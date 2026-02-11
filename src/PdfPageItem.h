#pragma once

#include <QGraphicsItem>

class PdfPageProvider;

class PdfPageItem : public QGraphicsItem
{
public:
    PdfPageItem(PdfPageProvider* provider, int number);
    ~PdfPageItem() override;

    QRectF boundingRect() const override;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

private:
    struct Private;
    std::unique_ptr<Private> d_ptr;
};
