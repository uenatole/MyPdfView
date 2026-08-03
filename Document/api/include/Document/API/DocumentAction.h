#pragma once

#include <QPointF>
#include <QUrl>

struct OpenUrlDocumentAction
{
    explicit OpenUrlDocumentAction(const QUrl& url);

    auto url() const -> const QUrl&;

private:
    QUrl m_url;
};

struct JumpDocumentAction
{
    JumpDocumentAction(int destinationPage, float destinationZoom, QPointF destinationLocation);

    auto destinationPage() const -> int;
    auto destinationZoom() const -> float;
    auto destinationLocation() const -> QPointF;

private:
    int m_destinationPage;
    float m_destinationZoom;
    QPointF m_destinationLocation;
};

struct DocumentAction : std::variant<OpenUrlDocumentAction, JumpDocumentAction>
{
    DocumentAction(OpenUrlDocumentAction&& action);
    DocumentAction(JumpDocumentAction&& action);
};
