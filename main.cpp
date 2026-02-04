#include <QApplication>

#include "pdf/PdfView.h"
#include <QPdfDocument>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    PdfView view;
    QPdfDocument document;
    document.load(qEnvironmentVariable("DOCUMENT"));

    view.setDocument(&document);
    view.setWheelZooming(true);
    view.show();

    return QApplication::exec();
}
