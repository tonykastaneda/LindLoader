#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Lindy");
    app.setWindowIcon(QIcon(":/icons/lindy.png"));

    // Frontends start a game with Lindy as the emulator: Lindy.exe [--linked <cabinets>] <game>.
    QCommandLineParser parser;
    parser.setApplicationDescription("Sega Lindbergh games on Windows.");
    parser.addHelpOption();
    const QCommandLineOption linked("linked", "Linked play with <cabinets> cabinets, one controller each.", "cabinets");
    parser.addOption(linked);
    parser.addPositionalArgument("game",
                                 "Start this game without showing Lindy, and exit when it closes: the game's folder, "
                                 "a folder above it, or its disk image.",
                                 "[game]");
    parser.process(app);

    MainWindow window(QCoreApplication::applicationDirPath());
    if (!parser.positionalArguments().isEmpty())
    {
        int cabinets = 1;
        if (parser.isSet(linked))
        {
            bool ok = false;
            cabinets = parser.value(linked).toInt(&ok);
            if (!ok || cabinets < 2)
            {
                QMessageBox::warning(nullptr, "Lindy", "--linked takes the number of cabinets, 2 or more.");
                return 1;
            }
        }
        if (!window.runFromCommandLine(parser.positionalArguments().first(), cabinets))
            return 1;
        return app.exec();
    }
    window.show();
    window.scanLibrary();
    return app.exec();
}
