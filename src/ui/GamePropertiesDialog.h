#pragma once

#include <QDialog>
#include <QList>
#include <QPair>

#include "GameLibrary.h"
#include "IniFile.h"

class QCheckBox;
class QComboBox;

// Per-game settings, laid out like the game properties windows of DuckStation and PCSX2:
// categories on the left, the settings of the selected category on the right.
class GamePropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    GamePropertiesDialog(const Game &game, GameLibrary &library, QWidget *parent = nullptr);

private:
    using Choices = QList<QPair<QString, QString>>; // label, linuxloader.ini value

    QWidget *buildSummaryPage();
    QWidget *buildGraphicsPage();
    QWidget *buildEmulationPage();

    // A checkbox or combo box that edits one linuxloader.ini key.
    QCheckBox *check(const QString &text, const QString &key, bool fallback);
    QComboBox *combo(const QString &key, const QString &fallback, const Choices &choices);
    void save(const QString &key, const QString &value);

    Game m_game;
    GameLibrary &m_library;
    IniFile m_ini;
    bool m_loading = true;
};
