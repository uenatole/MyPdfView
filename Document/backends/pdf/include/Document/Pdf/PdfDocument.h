#pragma once

#include <Document/API/Document.h>

class PdfDocument : public Document
{
public:
    PdfDocument();
    ~PdfDocument() override;

    void load(const QString& path);

    auto pageCount() const -> std::size_t final;
    auto pagePointSize(int page) const -> QSizeF final;

    auto textReady(int page) const -> bool final;
    auto forceTextReadiness(int page) const -> QFuture<void> final;

    auto text(int page, int from, int count) const -> QString final;
    auto textBoxes(int page, int from, int count) const -> QList<QRectF> final;

    auto render(int page, qreal scale) const -> QFuture<QImage> final;

    auto links(int page) const -> QList<DocumentLink> final;

    auto rubrication() const -> DocumentRubrication final;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
