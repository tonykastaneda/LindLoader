#pragma once

#include <QDialog>

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
    QWidget *buildSummaryPage();
    QWidget *buildDisplayPage();
    QWidget *buildControlsPage();
    void save(const QString &key, const QString &value);

    Game m_game;
    GameLibrary &m_library;
    IniFile m_ini;
    bool m_loading = true;
};
