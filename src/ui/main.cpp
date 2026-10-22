#include <QApplication>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Lindy");
    app.setWindowIcon(QIcon(":/icons/lindy.png"));

    MainWindow window(QCoreApplication::applicationDirPath());
    window.show();
    return app.exec();
}
