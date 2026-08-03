#include "PdfDocument.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QElapsedTimer>

#include <fpdf_doc.h>
#include <fpdf_progressive.h>
#include <fpdf_text.h>
#include <qloggingcategory.h>

#include <QPolygonF>

#include <Document/API/DocumentRubrication.h>

namespace
{
    Q_GLOBAL_STATIC(QRecursiveMutex, pdfiumMutex);
    int FPDFium_refcount = 0;

    class PdfiumMutexLocker : public std::unique_lock<QRecursiveMutex>
    {
    public:
        PdfiumMutexLocker() : std::unique_lock<QRecursiveMutex>(*pdfiumMutex){}
    };
}

// TODO: add checks
// TODO: abstract out FPDFium (and its "locks")
// TODO: optimize where possible

struct PdfDocument::Private
{
    auto to_view(const FPDF_PAGE page, const int page_index, const double x, const double y) const -> QPointF
    {
        FS_SIZEF size;
        FPDF_GetPageSizeByIndexF(document, page_index, &size);
        const int width = size.width;
        const int height = size.height;

        int rx, ry;
        if (FPDF_PageToDevice(page, 0, 0, width, height, 0, x, y, &rx, &ry))
            return QPointF(rx, ry);
        return {};
    }

    auto to_view(const FPDF_PAGE page, const int page_index, const QPointF& point) const -> QPointF
    {
        return to_view(page, page_index, point.x(), point.y());
    }

    auto to_view(const FPDF_PAGE page, const int page_index, const double left, const double top, const double right, const double bottom) const -> QRectF
    {
        FS_SIZEF size;
        FPDF_GetPageSizeByIndexF(document, page_index, &size);
        const int width = size.width;
        const int height = size.height;

        int xfmLeft, xfmTop, xfmRight, xfmBottom;
        if (FPDF_PageToDevice(page, 0, 0, size.width, size.height, 0, left, top, &xfmLeft, &xfmTop) &&
            FPDF_PageToDevice(page, 0, 0, size.width, size.height, 0, right, bottom, &xfmRight, &xfmBottom))
            return QRectF(xfmLeft, xfmTop, xfmRight - xfmLeft, xfmBottom - xfmTop);
        return {};
    }

    auto to_view(const FPDF_PAGE page, const int page_index, const QRectF& rect) const -> QRectF
    {
        return to_view(page, page_index, rect.left(), rect.top(), rect.right(), rect.bottom());
    }

    FPDF_DOCUMENT document = nullptr;
};

PdfDocument::PdfDocument()
    : d(std::make_unique<Private>())
{
    PdfiumMutexLocker lock;
    if (FPDFium_refcount++ == 0)
        FPDF_InitLibrary();
}

PdfDocument::~PdfDocument()
{
    PdfiumMutexLocker lock;
    if (--FPDFium_refcount == 0)
        FPDF_DestroyLibrary();
}

void PdfDocument::load(const QString& path)
{
    PdfiumMutexLocker lock;

    if (d->document != nullptr)
        FPDF_CloseDocument(d->document);

    d->document = FPDF_LoadDocument(path.toLatin1(), nullptr); // TODO: async api (QPdfDocument-like load)
}

auto PdfDocument::pageCount() const -> std::size_t
{
    PdfiumMutexLocker lock;

    return FPDF_GetPageCount(d->document);
}

auto PdfDocument::pagePointSize(const int page) const -> QSizeF
{
    PdfiumMutexLocker lock;

    FS_SIZEF size;
    const auto status = FPDF_GetPageSizeByIndexF(d->document, page, &size);

    if (status != 0)
        return { size.width, size.height };

    return {};
}

auto PdfDocument::textReady(int) const -> bool
{
    return true;
}

auto PdfDocument::forceTextReadiness(int) const -> QFuture<void>
{
    return QtFuture::makeReadyVoidFuture();
}

auto PdfDocument::text(const int page, const int from, int count) const -> QString
{
    PdfiumMutexLocker lock;

    const FPDF_PAGE pdf_page = FPDF_LoadPage(d->document, page);
    const FPDF_TEXTPAGE text_page = FPDFText_LoadPage(pdf_page);

    if (count == -1)
        count = FPDFText_CountChars(text_page);

    const auto getText = [](const FPDF_TEXTPAGE p, const int f, const int c) -> QString
    {
        QList<ushort> buf(c + 1);
        // TODO is that enough space in case one unicode character is more than one in utf-16?
        int len = FPDFText_GetText(p, f, c, buf.data());
        Q_ASSERT(len - 1 <= c); // len is number of characters written, including the terminator
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(buf.constData()), len - 1);
    };

    const QString text = getText(text_page, from, count);

    FPDF_ClosePage(pdf_page);
    FPDFText_ClosePage(text_page);

    return text;
}

auto PdfDocument::textBoxes(const int page, const int from, int count) const -> QList<QRectF>
{
    PdfiumMutexLocker lock;

    const FPDF_PAGE pdf_page = FPDF_LoadPage(d->document, page);
    const FPDF_TEXTPAGE text_page = FPDFText_LoadPage(pdf_page);

    if (count == -1)
        count = FPDFText_CountChars(text_page);

    QList<QRectF> rects;

    for (int i = from; i < from + count; i++)
    {
        double l, r, b, t;
        FPDFText_GetCharBox(text_page, i, &l, &r, &b, &t);
        QRectF viewCharBox = d->to_view(pdf_page, page, l, t, r, b);

        rects.append(viewCharBox);
    }

    FPDF_ClosePage(pdf_page);
    FPDFText_ClosePage(text_page);

    return rects;
}

auto PdfDocument::render(const int page, const qreal scale) const -> QFuture<QImage>
{
    struct ICancel
    {
        virtual ~ICancel() = default;
        virtual bool isCancelled() = 0;
    };

    const auto render_progressive = [](const FPDF_DOCUMENT document, const int number, const double factor, ICancel* cancel) -> QImage
    {
        PdfiumMutexLocker lock;

        FS_SIZEF originalSizeF;
        FPDF_GetPageSizeByIndexF(document, number, &originalSizeF);
        const auto sizeF = QSizeF(originalSizeF.width, originalSizeF.height) * factor;
        const auto size = sizeF.toSize();

        const QRect region { 0, 0, size.width(), size.height() };

        IFSDK_PAUSE pause;
        pause.version = 1;
        pause.user = cancel;

        // Link IFSDK_PAUSE interface with ICancel interface
        pause.NeedToPauseNow = [](IFSDK_PAUSE* pause) -> FPDF_BOOL
        {
            const auto cancel = static_cast<ICancel*>(pause->user);
            return cancel ? cancel->isCancelled() : false;
        };

        QImage result(size, QImage::Format_ARGB32);
        result.fill(Qt::transparent);
        FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(result.width(), result.height(), FPDFBitmap_BGRA, result.bits(), result.bytesPerLine());

        const FPDF_PAGE page = FPDF_LoadPage(document, number);
        const auto status = FPDF_RenderPageBitmap_Start(bitmap, page, region.left(), region.top(), region.width(), region.height(), 0, 0, &pause);

        FPDF_RenderPage_Close(page);
        FPDFBitmap_Destroy(bitmap);
        FPDF_ClosePage(page);

        if (status == FPDF_RENDER_DONE)
            return result;

        return {};
    };

    return QtConcurrent::run(
        [document=d->document, page, factor=scale, render_progressive](QPromise<QImage>& promise)
        {
            struct PromiseCancel final : ICancel
            {
                explicit PromiseCancel(QPromise<QImage>& promise) : m_promise(promise){}
                auto isCancelled() -> bool override { return m_promise.isCanceled(); }

            private:
                QPromise<QImage>& m_promise;
            };

            const auto cancel = std::make_unique<PromiseCancel>(promise);

            const QImage result = render_progressive(document, page, factor, cancel.get());
            promise.addResult(result);
        }
    );
}

namespace
{
    auto parse_action(const FPDF_DOCUMENT document,
                      const FPDF_DEST destination,
                      const FPDF_ACTION action) -> std::optional<DocumentAction>
    {
        switch (FPDFAction_GetType(action))
        {
        case PDFACTION_UNSUPPORTED:
        case PDFACTION_GOTO:
        {
            const int destinationPage = FPDFDest_GetDestPageIndex(document, destination);
            if (destinationPage < 0)
                return std::nullopt;

            FPDF_BOOL hasX, hasY, hasZoom;
            FS_FLOAT x, y, zoom;

            if (!FPDFDest_GetLocationInPage(destination, &hasX, &hasY, &hasZoom, &x, &y, &zoom))
                return std::nullopt;

            const float destinationZoom = hasZoom ? zoom : 1.0;
            const auto destinationLocation = QPointF(hasX ? x : 0, hasY ? y : 0);

            return JumpDocumentAction {
                destinationPage,
                destinationZoom,
                destinationLocation
            };

            break;
        }
        case PDFACTION_URI:
        {
            const unsigned long uriPathLength = FPDFAction_GetURIPath(document, action, nullptr, 0);
            if (uriPathLength < 1)
                return std::nullopt;

            QByteArray buffer(static_cast<qlonglong>(uriPathLength), 0);
            const unsigned long got = FPDFAction_GetURIPath(document, action, buffer.data(), uriPathLength);
            Q_ASSERT(got == uriPathLength);

            return OpenUrlDocumentAction {
                QString::fromLatin1(buffer)
            };

            break;
        }
        case PDFACTION_LAUNCH:
        case PDFACTION_REMOTEGOTO:
        {
            const unsigned long filePathLength = FPDFAction_GetFilePath(action, nullptr, 0);
            if (filePathLength < 1)
                return std::nullopt;

            QByteArray buffer(static_cast<qlonglong>(filePathLength), 0);
            const unsigned long got = FPDFAction_GetFilePath(action, buffer.data(), filePathLength);
            Q_ASSERT(got == filePathLength);

            return OpenUrlDocumentAction {
                QUrl::fromLocalFile(QString::fromLatin1(buffer))
            };
        }
        default:
            return std::nullopt;
        }
    }
}

auto PdfDocument::links(const int page) const -> QList<DocumentLink>
{
    QList<DocumentLink> links;

    const PdfiumMutexLocker lock;

    const FPDF_PAGE pdf_page = FPDF_LoadPage(d->document, page);
    Q_ASSERT(pdf_page != nullptr);

    // Ordinary links
    FPDF_LINK link;
    int link_index = 0;

    while(FPDFLink_Enumerate(pdf_page, &link_index, &link))
    {
        FS_RECTF rect;

        if (!FPDFLink_GetAnnotRect(link, &rect))
        {
            // Skip link with invalid rect
            continue;
        }

        // If rect is flipped - normalize it.
        if (rect.right < rect.left) std::swap(rect.right, rect.left);
        if (rect.top < rect.bottom) std::swap(rect.top, rect.bottom);

        QList<QRectF> geometry;

        if (const int quadPointsCount = FPDFLink_CountQuadPoints(link) > 0)
        {
            for (int i = 0; i < quadPointsCount; ++i)
            {
                FS_QUADPOINTSF point;
                if (FPDFLink_GetQuadPoints(link, i, &point))
                {
                    const auto poly = QPolygonF
                    {{
                        QPointF(point.x1, point.y1),
                        QPointF(point.x2, point.y2),
                        QPointF(point.x3, point.y3),
                        QPointF(point.x4, point.y4)
                    }};

                    geometry += d->to_view(pdf_page, page, poly.boundingRect());
                }
            }
        }
        else
        {
            geometry << d->to_view(pdf_page, page, rect.left, rect.top, rect.right, rect.bottom);
        }

        const FPDF_DEST destination = FPDFLink_GetDest(d->document, link);
        const FPDF_ACTION action = FPDFLink_GetAction(link);

        if (const auto actionOpt = parse_action(d->document, destination, action); actionOpt.has_value())
            links << DocumentLink(page, geometry, *actionOpt);
    }

    const FPDF_TEXTPAGE text_page = FPDFText_LoadPage(pdf_page);
    Q_ASSERT(text_page != nullptr);

    // Web links
    if (const FPDF_PAGELINK webLinks = FPDFLink_LoadWebLinks(text_page))
    {
        for (int i = 0; i < FPDFLink_CountWebLinks(webLinks); ++i)
        {
            QList<QRectF> geometry;

            for (int r = 0; r < FPDFLink_CountRects(webLinks, i); ++r)
            {
                double left, top, right, bottom;

                if (!FPDFLink_GetRect(webLinks, i, r, &left, &top, &right, &bottom))
                {
                    // skip link with bad geometry
                    continue;
                }

                geometry << d->to_view(pdf_page, page, left, top, right, bottom);
            }

            const int urlPathLength = FPDFLink_GetURL(webLinks, i, nullptr, 0);

            if (urlPathLength < 1)
            {
                // NOTE: skip link with bad url path
                continue;
            }

            QList<unsigned short> buffer(urlPathLength, 0);
            const unsigned long got = FPDFLink_GetURL(webLinks, i, buffer.data(), urlPathLength);
            Q_ASSERT(got == urlPathLength);

            links << DocumentLink(
                page,
                geometry,
                OpenUrlDocumentAction {
                    QString::fromUtf16(buffer.data(), static_cast<qsizetype>(got) - 1)
                }
            );
        }

        FPDFLink_CloseWebLinks(webLinks);
    }

    FPDFText_ClosePage(text_page);
    FPDF_ClosePage(pdf_page);

    return links;
}

namespace
{
    void traverse_bookmarks(const FPDF_DOCUMENT doc, const FPDF_BOOKMARK parent, QList<DocumentRubric>& list)
    {
        FPDF_BOOKMARK bookmark = FPDFBookmark_GetFirstChild(doc, parent);
        while (bookmark != nullptr)
        {
            const unsigned long titleLength = FPDFBookmark_GetTitle(bookmark, nullptr, 0);
            if (titleLength < 1) continue;

            QList<unsigned short> buffer(static_cast<qlonglong>(titleLength), 0);
            const unsigned long got = FPDFBookmark_GetTitle(bookmark, buffer.data(), titleLength);
            Q_ASSERT(got == titleLength);

            const auto title = QString::fromUtf16(buffer.data(), static_cast<qsizetype>(got) - 1);

            const auto destination = FPDFBookmark_GetDest(doc, bookmark);
            const auto action = FPDFBookmark_GetAction(bookmark);
            const auto actionOpt = parse_action(doc, destination, action);

            DocumentRubric& rubric = list.emplace_back();
            rubric.Title = title;
            rubric.Action = actionOpt;

            traverse_bookmarks(doc, bookmark, rubric.Children);

            bookmark = FPDFBookmark_GetNextSibling(doc, bookmark);
        }
    }
}

auto PdfDocument::rubrication() const -> DocumentRubrication
{
    DocumentRubrication rubrication;
    traverse_bookmarks(d->document, nullptr, rubrication.Children);
    return rubrication;
}
