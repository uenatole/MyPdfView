#pragma once

#include <QList>

#include "DocumentAction.h"

struct DocumentRubric
{
    QString Title;
    std::optional<DocumentAction> Action;
    QList<DocumentRubric> Children;
};

struct DocumentRubrication
{
    QList<DocumentRubric> Children;
};
