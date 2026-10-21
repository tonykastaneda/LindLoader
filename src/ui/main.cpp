#include <QApplication>
#include <QFile>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LindLoader");
    app.setStyle("Fusion");

    QFile qss(":/style.qss");
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    MainWindow window(QCoreApplication::applicationDirPath());
    window.show();
    return app.exec();
}
