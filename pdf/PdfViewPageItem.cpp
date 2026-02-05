#include "PdfViewPageItem.h"

#include <QPainter>
#include <QPdfDocument>
#include <QWidget>

#include "PdfViewPageProvider.h"

PdfViewPageItem::PdfViewPageItem(const int number, PdfViewPageProvider* provider)
    : _provider(provider)
    , _number(number)
    , _pointSize(_provider->document()->pagePointSize(number))
{
    assert(number >= 0 && number < _provider->document()->pageCount());
}

QRectF PdfViewPageItem::boundingRect() const
{
    return QRectF(QPointF(0, 0), _pointSize);
}

// TODO: move out
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

void PdfViewPageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);

    const qreal scale = painter->worldTransform().m11();

    painter->fillRect(boundingRect(), Qt::white);

    // TODO: Draw blurry image based on outdated render from cache.
    //       ~
    //       It makes sense to do this only for those renders whose background task is significantly longer
    //       than the time it takes to draw a blurry image based of outdated render from the cache.
    //       ~
    //       Since only the PdfViewPageProvider can answer the question of whether such an operation is appropriate,
    //       this action must occur in complete coordination with the asynchronous render request.

    auto response = _provider->requestRender(_number, scale);

    std::visit(overloads {
        [&](const PdfViewPageProvider::RenderResponse::Cached& cached)
        {
            painter->drawImage(boundingRect(), cached.Image);
        },
        [&](PdfViewPageProvider::RenderResponse::Scheduled& scheduled)
        {
            if (scheduled.NearestImage)
                painter->drawImage(boundingRect(), scheduled.NearestImage.value());

            scheduled.Signal.then([this]{ update(); });
        },
    }, response);
}
