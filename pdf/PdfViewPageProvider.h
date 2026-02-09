#pragma once

#include <QFuture>
#include <QGraphicsView>

#include "../custom/QCacheExt.h"

class QPdfDocument;

class PdfViewPageProvider
{
public:
    PdfViewPageProvider();

    void setDocument(QPdfDocument* document);
    void setView(QGraphicsView* view);

    QPdfDocument* document() const;

    void setPixelRatio(qreal ratio);
    void setCacheLimit(qreal bytes) const;

    std::optional<QImage> request(QGraphicsItem* requester, int page, qreal scale);

private:
    class RenderCache
    {
    public:
        RenderCache();

        QImage* object(int page, qreal scale) const;
        QImage* nearestObject(int page, qreal targetScale) const;

        bool insert(int page, qreal scale, QImage* image);

        void setLimit(std::size_t bytes);

    private:
        mutable QCacheExt<std::pair<int, qreal>, QImage> _storage;
        mutable QHash<int, std::set<qreal>> _keySets;
    };

    struct RenderRequest
    {
        int Page;
        qreal Scale;
        QGraphicsItem* Requester;

        bool operator==(const RenderRequest& other) const
        {
            return Page == other.Page && qFuzzyCompare(Scale, other.Scale);
        }
    };

    struct RenderState {
        RenderRequest Request;
        QFuture<void> Future;

        RenderState(const RenderRequest& parameters, QFuture<void> future)
            : Request(parameters)
            , Future(std::move(future))
        {}

        ~RenderState()
        {
            Future.cancelChain();
        }
    };

    bool isRequesterActual(const QGraphicsItem* item) const;

    std::optional<QImage> findNearestImage(int page, qreal scale);

    void enqueueRenderRequest(RenderRequest&& request);
    void tryDequeueRenderRequest();

    QGraphicsView* _view = nullptr;
    QPdfDocument* _document = nullptr;
    qreal _pixelRatio = 1.0;

    mutable RenderCache _cache;

    std::list<RenderRequest> _requests;
    std::optional<RenderState> _renderState;
};
