#include "PdfViewPageProvider.h"

#include <QPdfDocument>
#include <QtConcurrent/QtConcurrentRun>

PdfViewPageProvider::PdfViewPageProvider()
{
    setCacheLimit(1 /*GiB*/ * 1024 /*MiB*/ * 1024 /*KiB*/ * 1024 /*B*/);
}

void PdfViewPageProvider::setDocument(QPdfDocument* document)
{
    _document = document;
}

QPdfDocument* PdfViewPageProvider::document() const
{
    return _document;
}

void PdfViewPageProvider::setPixelRatio(const qreal ratio)
{
    // TODO: invalidate cache (?) or take ratio in account with {scale} (!)
    _pixelRatio = ratio;
}

void PdfViewPageProvider::setCacheLimit(const qreal bytes) const
{
    _cache.setMaxCost(bytes);
}

std::variant<PdfViewPageProvider::RenderResponse::Cached, PdfViewPageProvider::RenderResponse::Scheduled> PdfViewPageProvider::requestRender(int page, qreal scale)
{
    const CacheKey key { page, scale };

    if (const QImage* image = _cache.object(key); image)
    {
        qDebug() << "Cache hit: page =" << key.first << "scale =" << key.second;
        return RenderResponse::Cached { *image };
    }

    // TODO: Do not drop the active job completely but cancel and move it to the second place in the queue.
    //       Item with cancelled render job will try to ::update() itself, and if item is visible,
    //       the request must be marked as "actual" to not be dropped completely.

    _activeRenderRequestJob.cancel();

    // Rendering is done in separate thread
    _activeRenderRequestJob = QtConcurrent::run([document=_document, ratio=_pixelRatio, page, scale](QPromise<QImage>& promise)
    {
        struct PromiseCancel : QPdfDocument::ICancel {
            explicit PromiseCancel(QPromise<QImage>& promise) : m_promise(promise) {}
            bool isCancelled() final
            {
                return m_promise.isCanceled();
            }

        private:
            QPromise<QImage>& m_promise;
        };

        const auto pointSize = document->pagePointSize(page);
        const auto renderSize = pointSize * scale * ratio;
        const auto size = renderSize.toSize();

        const auto cancel = std::make_unique<PromiseCancel>(promise);
        const QImage result = document->render2(page, size, cancel.get());
        promise.addResult(result);
    });

    // Caching is done in main thread
    const auto chain = _activeRenderRequestJob.then(QThread::currentThread(), [this, key](const QImage& image){
        _cache.insert(key, new QImage(image), image.sizeInBytes());
    });

    return RenderResponse::Scheduled { std::nullopt, chain };
}
