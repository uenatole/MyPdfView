#include "DocumentZoomer.h"

#include <QWheelEvent>

#include "DocumentView.h"

DocumentZoomer::DocumentZoomer(DocumentView* parent)
    : QObject(parent)
    , m_view(parent)
{
    m_view->viewport()->installEventFilter(this);
}

bool DocumentZoomer::eventFilter(QObject* object, QEvent* event)
{
    if (object != m_view->viewport())
        return QObject::eventFilter(object, event);

    if (const auto wheel = dynamic_cast<QWheelEvent*>(event); wheel)
    {
        constexpr auto calculateZoomStep = [](const qreal currentZoomFactor, const int sign) -> qreal
        {
            constexpr qreal baseStep = 0.1;
            constexpr qreal ceilStep = 0.9;

            const auto factor = std::ceil(currentZoomFactor + sign * baseStep);
            return qMin(baseStep * factor, ceilStep);
        };

        if (wheel->modifiers() & Qt::ControlModifier)
        {
            const int delta = wheel->angleDelta().y();

            const QGraphicsView::ViewportAnchor anchor = m_view->transformationAnchor();
            m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

            const qreal currentZoomFactor = m_view->transform().m11();
            const qreal stepSize = calculateZoomStep(currentZoomFactor, (delta > 0) ? +1 : -1);

            const qreal zoomChange = (delta > 0) ? stepSize : -stepSize;
            const qreal newZoomFactor = qBound(0.1, currentZoomFactor + zoomChange, 10.0);

            const auto ff = newZoomFactor / currentZoomFactor;

            m_view->scale(ff, ff);
            m_view->setTransformationAnchor(anchor);

            return true;
        }
    }

    return QObject::eventFilter(object, event);
}
