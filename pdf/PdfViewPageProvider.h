#pragma once

#include <QImage>
#include <QFuture>
#include <QCache>

class QPdfDocument;

class PdfViewPageProvider
{
public:
    PdfViewPageProvider();

    void setDocument(QPdfDocument* document);
    QPdfDocument* document() const;

    void setPixelRatio(qreal ratio);
    void setCacheLimit(qreal bytes) const;

    struct RenderResponse
    {
        struct Cached
        {
            QImage Image;
        };

        struct Scheduled
        {
            std::optional<QImage> NearestImage;
            QFuture<void> Signal;
        };
    };

    std::variant<RenderResponse::Cached, RenderResponse::Scheduled> requestRender(int page, qreal scale);

private:
    QPdfDocument* _document = nullptr;
    qreal _pixelRatio = 1.0;

    using CacheKey = std::pair<int, qreal>;
    mutable QCache<CacheKey, QImage> _cache;

    mutable QFuture<QImage> _activeRenderRequestJob;
};
