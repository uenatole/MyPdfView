#pragma once

#include <QObject>

class DocumentView;

class DocumentZoomer : public QObject
{
public:
    explicit DocumentZoomer(DocumentView* parent);

protected:
    bool eventFilter(QObject*, QEvent*) final;

private:
    DocumentView* const m_view;
};
