#include "PdfPageProvider.h"

#include <QElapsedTimer>
#include <QGraphicsItem>
#include <QPdfDocument>
#include <QScreen>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include "custom/QCacheExt.h"

namespace
{
    template<typename Set>
    auto closest_element(Set& set, const typename Set::value_type& value)
        -> decltype(set.begin())
    {
        const auto it = set.lower_bound(value);
        if (it == set.begin())
            return it;

        const auto prev_it = std::prev(it);
        return (it == set.end() || value - *prev_it <= *it - value) ? prev_it : it;
    }

    class RenderCache
    {
    public:
        RenderCache()
        {
            _storage.setOnEraseFn([this](const std::pair<int, qreal>& key)
            {
                _keySets[key.first].erase(key.second);
            });
        }

        QImage* object(int page, qreal scale) const
        {
            return _storage.object({ page, scale });
        }

        QImage* nearestObject(int page, const qreal targetScale) const
        {
            const auto& scales = _keySets[page];
            const auto closestScaleIt = closest_element(scales, targetScale);

            if (closestScaleIt == scales.end())
                return nullptr;

            return _storage.object({ page, *closestScaleIt });
        }

        bool insert(int page, qreal scale, QImage* image) const
        {
            if (const bool inserted = _storage.insert({ page, scale }, image, image->sizeInBytes()); Q_LIKELY(inserted))
            {
                _keySets[page].insert(scale);
                return true;
            }
            return false;
        }

        void setLimit(std::size_t bytes) const
        {
            _storage.setMaxCost(bytes);
        }

    private:
        mutable QCacheExt<std::pair<int, qreal>, QImage> _storage;
        mutable QHash<int, std::set<qreal>> _keySets;
    };

    struct RenderRequest
    {
        int Page;
        qreal Scale;
        PdfPageProvider::Interface::RequesterID Requester;

        bool operator==(const RenderRequest& other) const
        {
            return Page == other.Page && qFuzzyCompare(Scale, other.Scale);
        }
    };

    struct RenderState
    {
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
}

struct PdfPageProvider::Private
{
    friend class PdfPageProvider;

    Private()
    {
        dequeueDelayTimer.setSingleShot(true);
        dequeueDelayTimer.setInterval(50);
        QObject::connect(&dequeueDelayTimer, &QTimer::timeout, [this]{ tryDequeueRenderRequest(); });
    }

    std::optional<QImage> request(const Interface::RequesterID requester, const int page, const qreal scale)
    {
        if (const QImage* image = cache.object(page, scale); image)
        {
            qDebug() << "Cache hit: page =" << page << "scale =" << scale;
            return *image;
        }

        const RenderRequest parameters { page, scale, requester };
        const std::optional<QImage> nearestImage = findNearestImage(page, scale);

        // Check active render request for duplication
        if (renderState)
        {
            if (renderState->Request == parameters)
            {
                return *nearestImage;
            }

            if (renderState->Request.Page == parameters.Page)
            {
                renderState.reset();
            }
        }

        // Check pending render requests for duplication
        for (RenderRequest& request : requests)
        {
            if (request.Page == parameters.Page)
            {
                request.Scale = parameters.Scale;
                return *nearestImage;
            }
        }

        // Enqueue new request
        enqueueRenderRequest(RenderRequest {parameters});

        // Dequeue it delayed to start render and let for some requests to be outdated
        if (!renderState)
            tryDequeueRenderRequestDelayed();

        return nearestImage;
    }

private:
    std::optional<QImage> findNearestImage(const int page, const qreal scale) const
    {
        if (auto* image = cache.nearestObject(page, scale); image)
            return *image;

        return std::nullopt;
    }

    void enqueueRenderRequest(RenderRequest&& request)
    {
        requests.emplace_back(std::move(request));
    }

    void tryDequeueRenderRequestDelayed()
    {
        dequeueDelayTimer.start();
    }

    void tryDequeueRenderRequest()
    {
        if (renderState && !interface->isActual(renderState->Request.Requester))
        {
            renderState.reset();
        }

        // Erase unactual requests
        const auto firstActualIt = std::find_if(requests.begin(), requests.end(), [this](const RenderRequest& request)
        {
            return interface->isActual(request.Requester);
        });

        const auto prevSize = requests.size();
        requests.erase(requests.begin(), firstActualIt);

        if (const auto diff = prevSize - requests.size(); diff) qDebug() << "Erased" << diff << "elements";

        if (requests.empty()) return;

        // Take first request in queue
        RenderRequest request = std::move(requests.front());
        requests.pop_front();

        QFuture<void> future = QtConcurrent::run(
            [document=document, page=request.Page, scale=request.Scale, ratio=pixelRatio](QPromise<QImage>& promise)
            {
                struct PromiseCancel : QPdfDocument::ICancel {
                    explicit PromiseCancel(QPromise<QImage>& promise) : m_promise(promise) {}
                    bool isCancelled() final
                    {
                        return m_promise.isCanceled();
                    }

                private:
                    QPromise<QImage>& m_promise;
                };

                const auto pointSize = document->pagePointSize(page);
                const auto renderSize = pointSize * scale * ratio;
                const auto size = renderSize.toSize();

                const auto cancel = std::make_unique<PromiseCancel>(promise);

                QElapsedTimer timer;
                timer.start();
                const QImage result = document->render2(page, size, cancel.get());

                if (!result.isNull())
                    qDebug() << "Render finished: page =" << page << "scale =" << scale << " time =" << timer.elapsed() << "ms";

                promise.addResult(result);
            }
        )
        .then(QThread::currentThread(), [this, request](const QImage& image){
            (void) cache.insert(request.Page, request.Scale, new QImage(image));
            interface->notify(request.Requester);

            renderState.reset();
            tryDequeueRenderRequest();
        });

        renderState.emplace(request, future);
    }

    Interface* interface = nullptr;
    QPdfDocument* document = nullptr;
    qreal pixelRatio = 1.0;

    QTimer dequeueDelayTimer;

    mutable RenderCache cache;

    std::list<RenderRequest> requests;
    std::optional<RenderState> renderState;
};

PdfPageProvider::PdfPageProvider()
    : d_ptr(new Private)
{
    setCacheLimit(512 /*MiB*/ * 1024 /*KiB*/ * 1024 /*B*/);
}

PdfPageProvider::~PdfPageProvider() = default;

void PdfPageProvider::setDocument(QPdfDocument* document) const
{
    d_ptr->document = document;
}

void PdfPageProvider::setInterface(Interface* interface) const
{
    d_ptr->interface = interface;
}

PdfPageProvider::Interface* PdfPageProvider::interface() const
{
    return d_ptr->interface;
}

QPdfDocument* PdfPageProvider::document() const
{
    return d_ptr->document;
}

void PdfPageProvider::setPixelRatio(const qreal ratio) const
{
    // TODO: invalidate cache (?) or take ratio in account with {scale} (!)
    d_ptr->pixelRatio = ratio;
}

void PdfPageProvider::setCacheLimit(const qreal bytes) const
{
    d_ptr->cache.setLimit(bytes);
}

void PdfPageProvider::setRenderDelay(const int ms) const
{
    d_ptr->dequeueDelayTimer.setInterval(ms);
}

std::optional<QImage> PdfPageProvider::request(const Interface::RequesterID requester, const int page, const qreal scale) const
{
    return d_ptr->request(requester, page, scale);
}
