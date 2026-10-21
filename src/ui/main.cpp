#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

#include "MainWindow.h"

// The dark Fusion theme, like DuckStation's and PCSX2's dark themes.
static void applyDarkFusion(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));
    const QColor window(45, 45, 48), base(30, 30, 32), alternate(38, 38, 41), text(220, 220, 222),
        disabled(120, 120, 125), highlight(42, 130, 218);
    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alternate);
    p.setColor(QPalette::ToolTipBase, window);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, window);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, highlight);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    app.setPalette(p);
    app.setStyleSheet("QToolBar { border: none; padding: 2px 4px; spacing: 2px; }"
                      "QToolButton { padding: 4px 8px; }"
                      "QTreeView::item { height: 26px; }");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LindLoader");
    applyDarkFusion(app);

    MainWindow window(QCoreApplication::applicationDirPath());
    window.show();
    return app.exec();
}
