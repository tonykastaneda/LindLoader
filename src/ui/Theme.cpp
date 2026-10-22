#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QSvgRenderer>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace
{
// The icons are drawn in #dcdcdc; this swaps that for the palette's text colour when painting.
class TintedIconEngine : public QIconEngine
{
public:
    TintedIconEngine(const QString &path, const QString &onPath) : m_path(path), m_onPath(onPath) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const bool on = state == QIcon::On && !m_onPath.isEmpty();
        QFile f(on ? m_onPath : m_path);
        if (!f.open(QIODevice::ReadOnly))
            return;
        QByteArray svg = f.readAll();
        if (!on)
        {
            const QPalette p = QApplication::palette();
            const QColor c = mode == QIcon::Disabled ? p.color(QPalette::Disabled, QPalette::WindowText)
                             : mode == QIcon::Selected ? p.color(QPalette::HighlightedText)
                                                       : p.color(QPalette::WindowText);
            svg.replace("#dcdcdc", c.name().toLatin1());
        }
        QSvgRenderer(svg).render(painter, rect);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap pm(size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        paint(&p, QRect(QPoint(0, 0), size), mode, state);
        return pm;
    }

    QIconEngine *clone() const override { return new TintedIconEngine(m_path, m_onPath); }

private:
    QString m_path, m_onPath;
};

QPalette darkPalette()
{
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
    return p;
}
} // namespace

namespace Theme
{
QList<QPair<QString, QString>> choices()
{
    return {{"Dark (Default)", "dark"}, {"Light", "light"}, {"Windows", "windows"}};
}

void apply(const QString &name)
{
    // Light and Windows pick their colours whatever Windows' own dark/light mode is: Windows follows it.
    qApp->styleHints()->setColorScheme(name == "light" ? Qt::ColorScheme::Light : Qt::ColorScheme::Unknown);
    if (name == "windows")
    {
        QApplication::setStyle(QStyleFactory::create("windows11"));
        QApplication::setPalette(QApplication::style()->standardPalette());
    }
    else
    {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(name == "light" ? QApplication::style()->standardPalette() : darkPalette());
    }
    qApp->setStyleSheet("QToolBar { border: none; padding: 2px 4px; spacing: 2px; }"
                        "QToolButton { padding: 4px 8px; }"
                        "QTreeView::item { height: 26px; }");
}

QIcon icon(const QString &name, const QString &onName)
{
    return QIcon(new TintedIconEngine(":/icons/" + name + ".svg", onName.isEmpty() ? QString() : ":/icons/" + onName + ".svg"));
}
} // namespace Theme
