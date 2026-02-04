#include "PdfViewPageItem.h"

#include <QPainter>
#include <QPdfDocument>

PdfViewPageItem::PdfViewPageItem(const int number, QPdfDocument* document)
    : _number(number)
    , _document(document)
    , _pointSize(_document->pagePointSize(number))
    , _scaleCache(0.0)
{
    assert(number >= 0 && number < _document->pageCount());
}

QRectF PdfViewPageItem::boundingRect() const
{
    return QRectF(QPointF(0, 0), _pointSize);
}

void PdfViewPageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const qreal scale = painter->worldTransform().m11();

    painter->fillRect(boundingRect(), Qt::white);

    if (qFuzzyCompare(_scaleCache, scale)) {
        painter->drawImage(boundingRect(), _imageCache);
    }
    else
    {
        const QSizeF size = _pointSize * scale;
        const QImage image = _document->render(_number, size.toSize());
        painter->drawImage(boundingRect(), image);

        _scaleCache = scale;
        _imageCache = image;
    }
}
