#pragma once

#include <QIcon>
#include <QList>
#include <QPair>
#include <QString>

// Lindy's look: "dark" (Fusion with a dark palette, like DuckStation's and PCSX2's dark themes),
// "light" (Fusion) or "windows" (the native Windows style).
namespace Theme
{
QList<QPair<QString, QString>> choices(); // label, name
void apply(const QString &name);

// An icon from :/icons/<name>.svg drawn in the theme's text colour, so it follows theme changes.
// onName: the icon for the checked state (e.g. a filled star), drawn in its own colours.
QIcon icon(const QString &name, const QString &onName = QString());
} // namespace Theme
