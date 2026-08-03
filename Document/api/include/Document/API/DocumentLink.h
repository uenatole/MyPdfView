#pragma once

#include <QRectF>
#include <QUrl>

#include "DocumentAction.h"

struct DocumentLink
{
    DocumentLink(int page, const QList<QRectF>& geometry, DocumentAction action);

    auto page() const -> int;
    auto geometry() const -> const QList<QRectF>&;
    auto action() const -> const DocumentAction&;
    auto toString() const -> QString;

private:
    int m_page;
    QList<QRectF> m_geometry;
    DocumentAction m_action;
};
