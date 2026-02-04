#pragma once

#include <QGraphicsItem>

class QPdfDocument;

class PdfViewPageItem : public QGraphicsItem
{
public:
    PdfViewPageItem(int number, QPdfDocument* document);

    QRectF boundingRect() const override;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

private:
    const int _number;
    QPdfDocument* const _document;
    const QSizeF _pointSize;

    qreal _scaleCache;
    QImage _imageCache;
};
