#pragma once

#include <QGraphicsView>

class DocumentFacade;

// TODO: hide QGraphicsView
class DocumentView : public QGraphicsView
{
public:
    explicit DocumentView(QWidget* parent = nullptr);
    ~DocumentView() override;

    void setDocument(const std::shared_ptr<DocumentFacade>& document);

    using QGraphicsView::transformationAnchor;
    using QGraphicsView::setTransformationAnchor;

    QString getSelectedText() const;

    // TODO: remove it
    QGraphicsItem* page(int) const;

private:
    friend struct RenderFeedback;
    friend struct PageItemFeedback;

    struct Private;
    std::unique_ptr<Private> d;
};
