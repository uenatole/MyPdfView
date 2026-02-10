#include "PdfView.h"

#include <QPdfDocument>
#include <QGraphicsScene>
#include <QGraphicsEffect>
#include <QWheelEvent>

#include "PdfViewPageItem.h"
#include "PdfViewPageProvider.h"

PdfView::PdfView(QWidget* parent)
    : QGraphicsView(parent)
    , m_provider(new PdfViewPageProvider())
{
    m_provider->setView(this);
    // TODO: keep track QWindow::screenChanged
    m_provider->setPixelRatio(devicePixelRatio());
}

PdfView::~PdfView(){}

void PdfView::setDocument(QPdfDocument* document)
{
    m_provider->setDocument(document);

    auto* scene = new QGraphicsScene();
    scene->setBackgroundBrush(palette().brush(QPalette::Dark));

    constexpr auto documentMargins = 6;

    qreal yCursor = documentMargins;
    qreal maxPageWidth = std::numeric_limits<qreal>::min();

    for (int page = 0; page < document->pageCount(); ++page)
    {
        QSizeF pagePointSize = document->pagePointSize(page);

        const auto item = new PdfViewPageItem(page, m_provider.get());
        item->setPos(documentMargins, yCursor);

        yCursor += pagePointSize.height() + documentMargins;
        maxPageWidth = std::max(maxPageWidth, pagePointSize.width());

        // NOTE: According to the performance profiler, this causes large lags when scaling large
        // const auto shadowEffect = new QGraphicsDropShadowEffect();
        // shadowEffect->setBlurRadius(10.0);
        // shadowEffect->setColor(QColor(0, 0, 0, 150));
        // shadowEffect->setOffset(2, 2);
        // item->setGraphicsEffect(shadowEffect);

        scene->addItem(item);
    }

    setScene(scene);
    setSceneRect(0, 0, maxPageWidth + 2 * documentMargins, yCursor);

    centerOn(0, 0);
    setTransformationAnchor(AnchorUnderMouse);
}

void PdfView::setWheelZooming(bool enabled)
{
    m_wheelZoomingDisabled = !enabled;
}

bool PdfView::wheelZooming() const
{
    return m_wheelZoomingDisabled;
}

void PdfView::wheelEvent(QWheelEvent* event)
{
    if (m_wheelZoomingDisabled)
    {
        QGraphicsView::wheelEvent(event);
        return;
    }

    constexpr auto calculateZoomStep = [](const qreal currentZoomFactor, const int sign) -> qreal
    {
        constexpr qreal baseStep = 0.1;
        constexpr qreal ceilStep = 0.9;

        const auto factor = std::ceil(currentZoomFactor + sign * baseStep);
        return qMin(baseStep * factor, ceilStep);
    };

    if (event->modifiers() & Qt::ControlModifier)
    {
        const int delta = event->angleDelta().y();

        const ViewportAnchor anchor = transformationAnchor();
        setTransformationAnchor(AnchorUnderMouse);

        const qreal currentZoomFactor = transform().m11();
        const qreal stepSize = calculateZoomStep(currentZoomFactor, (delta > 0) ? +1 : -1);

        const qreal zoomChange = (delta > 0) ? stepSize : -stepSize;
        const qreal newZoomFactor = qBound(0.1, currentZoomFactor + zoomChange, 10.0);

        const auto ff = newZoomFactor / currentZoomFactor;

        scale(ff, ff);
        setTransformationAnchor(anchor);

        event->accept();
    }
    else
    {
        QGraphicsView::wheelEvent(event);
    }
}


