#include "DocumentLink.h"

#include <utility>

DocumentLink::DocumentLink(const int page, const QList<QRectF>& geometry, DocumentAction action)
    : m_page(page)
    , m_geometry(geometry)
    , m_action(std::move(action))
{}

auto DocumentLink::page() const -> int
{
    return m_page;
}

auto DocumentLink::geometry() const -> const QList<QRectF>&
{
    return m_geometry;
}

auto DocumentLink::action() const -> const DocumentAction&
{
    return m_action;
}

auto DocumentLink::toString() const -> QString
{
    QString contentStr = "?";

    if (const auto content = std::get_if<OpenUrlDocumentAction>(&m_action); content)
    {
        contentStr = QString("Url { uri = %1 }")
            .arg(content->url().toString());
    }
    else if (const auto content = std::get_if<JumpDocumentAction>(&m_action); content)
    {
        contentStr = QString("Jump { page = %1, point = (%2, %3), zoom = %4 }")
                     .arg(content->destinationPage())
                     .arg(content->destinationLocation().x())
                     .arg(content->destinationLocation().y())
                     .arg(content->destinationZoom());
    }

    return QString("DocumentLink { location { page = %1, geometry = (%2,%3 %4x%5) }, contents = %6 }")
           .arg(m_page)
           .arg(m_geometry[0].x())
           .arg(m_geometry[0].y())
           .arg(m_geometry[0].width())
           .arg(m_geometry[0].height())
           .arg(contentStr);
}
