#include "PdfView.h"

#include <QGraphicsEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <QPdfDocument>
#include <QWheelEvent>
#include <QFuture>
#include <QPointer>

PdfView::PdfView(QWidget* parent)
    : QGraphicsView(parent)
{}

PdfView::~PdfView(){}

void PdfView::setDocument(QPdfDocument* document)
{
    auto* scene = new QGraphicsScene();
    scene->setBackgroundBrush(palette().brush(QPalette::Dark));

    constexpr auto documentMargins = 6;

    qreal yCursor = documentMargins;
    qreal maxPageWidth = std::numeric_limits<qreal>::min();

    for (int page = 0; page < document->pageCount(); ++page)
    {
        const auto item = new QGraphicsPixmapItem();
        QSizeF pagePointSize = document->pagePointSize(page);

        // create page
        QImage pageImage = document->render(page, pagePointSize.toSize());
        item->setPixmap(QPixmap::fromImage(pageImage));
        item->setPos(documentMargins, yCursor);

        yCursor += pagePointSize.height() + documentMargins;
        maxPageWidth = std::max(maxPageWidth, pagePointSize.width());

        // create background with shadow effect
        auto background = new QGraphicsRectItem(item->x(), item->y(), item->boundingRect().width(), item->boundingRect().height(), item); // geom

        background->setBrush(Qt::white);
        background->setPen(Qt::NoPen);
        auto shadowEffect = new QGraphicsDropShadowEffect();
        shadowEffect->setBlurRadius(10.0);
        shadowEffect->setColor(QColor(0, 0, 0, 150));
        shadowEffect->setOffset(2, 2);
        background->setGraphicsEffect(shadowEffect);

        scene->addItem(background);
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


