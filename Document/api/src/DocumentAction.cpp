#include "DocumentAction.h"

OpenUrlDocumentAction::OpenUrlDocumentAction(const QUrl& url): m_url(url)
{}

auto OpenUrlDocumentAction::url() const -> const QUrl&
{
    return m_url;
}

JumpDocumentAction::JumpDocumentAction(const int destinationPage, const float destinationZoom, const QPointF destinationLocation)
    : m_destinationPage(destinationPage)
    , m_destinationZoom(destinationZoom)
    , m_destinationLocation(destinationLocation)
{}

auto JumpDocumentAction::destinationPage() const -> int
{
    return m_destinationPage;
}

auto JumpDocumentAction::destinationZoom() const -> float
{
    return m_destinationZoom;
}

auto JumpDocumentAction::destinationLocation() const -> QPointF
{
    return m_destinationLocation;
}

DocumentAction::DocumentAction(OpenUrlDocumentAction&& action)
    : std::variant<OpenUrlDocumentAction, JumpDocumentAction>(std::move(action)) {}

DocumentAction::DocumentAction(JumpDocumentAction&& action)
    : std::variant<OpenUrlDocumentAction, JumpDocumentAction>(std::move(action)) {}