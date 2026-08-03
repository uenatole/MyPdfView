#include <QApplication>
#include <QShortcut>
#include <QClipboard>
#include <QSplitter>
#include <QTreeView>
#include <QHeaderView>

#include <Document/API/DocumentFacade.h>

#include <QtDocumentView/DocumentView.h>
#include <QtDocumentView/DocumentZoomer.h>
#include <QtDocumentView/DocumentSelector.h>

#include <Document/Pdf/PdfDocument.h>

#include <Document/Ocr/OcrDocument.h>
#include <Document/Ocr/engines/TesseractOcrEngine.h>

#include <Document/Std/StandardDocumentParser.h>
#include <Document/Std/StandardDocumentRenderer.h>

#include "rubrication/DocumentRubricationModel.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    const auto pdf = std::make_shared<PdfDocument>();
    pdf->load(qEnvironmentVariable("DOCUMENT"));

    const auto ocr = std::make_shared<OcrDocument>();
    ocr->setSource(pdf);
    ocr->setEngine(std::make_unique<TesseractOcrEngine>());

    const auto renderer = std::make_shared<StandardDocumentRenderer>();
    const auto parser = std::make_shared<StandardDocumentParser>();

    const auto document = std::make_shared<DocumentFacade>();
    document->setDocument(pdf);
    document->setRenderer(renderer);
    document->setParser(parser);

    const auto documentView = new DocumentView;
    documentView->setDocument(document);

    const auto selector = new DocumentSelector(documentView);
    const auto zoomer = new DocumentZoomer(documentView);

    const QShortcut copyShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), documentView);
    QObject::connect(&copyShortcut, &QShortcut::activated, [&]
    {
        QGuiApplication::clipboard()->setText(documentView->getSelectedText(), QClipboard::Clipboard);
    });

    const auto rubrication = pdf->rubrication();

    const auto rubricationModel = new DocumentRubricationModel;
    rubricationModel->setRubrication(rubrication);

    const auto rubricationView = new QTreeView;
    rubricationView->setModel(rubricationModel);
    rubricationView->setExpandsOnDoubleClick(false);

    QObject::connect(rubricationView, &QTreeView::clicked, [&](const QModelIndex& index)
    {
        if (const auto action = rubricationModel->getAction(index); action.has_value())
            documentView->execute(action.value());
    });

    const auto splitter = new QSplitter;
    splitter->addWidget(rubricationView);
    splitter->addWidget(documentView);

    splitter->show();

    return QApplication::exec();
}
