#pragma once

#include <QList>
#include <QWidget>

#include "ControlLayouts.h"
#include "GameLibrary.h"
#include "IniFile.h"

class BindingButton;
class QComboBox;

// The Controls page of Game Properties, laid out like PCSX2x6's controller settings: Bindings and
// Settings views, the controller and Automap per player, System Buttons, and the game's layout
// (Standard, Racing, Lightgun or Flight) with player 1 and player 2 side by side. Click a binding,
// then press a key or controller button; right-click clears it. Writes the game's controls.ini.
class ControllerSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    ControllerSettingsWidget(const Game &game, GameLibrary &library, QWidget *parent = nullptr);

private:
    QWidget *buildBindingsPage();
    QWidget *buildSettingsPage();
    QWidget *buildGroup(const ControlLayouts::Group &group);
    void refresh();
    void refreshControllers();
    void selectController(int player, int index);
    void refreshMice();
    void selectMouse(int player, const QString &path);
    void automap(int player);
    void bind(BindingButton *button, const QString &binding, const QString &guid);
    void clear(BindingButton *button);
    void copyFrom();
    void resetToDefaults();
    void save();

    Game m_game;
    GameLibrary &m_library;
    IniFile m_ini;
    const ControlLayouts::Layout *m_layout; // null for games without a controls page (mahjong/quiz)
    QList<QComboBox *> m_controllers;       // per player
    QList<QComboBox *> m_mice;              // per player, lightgun games
    bool m_refreshingMice = false;
    QList<BindingButton *> m_buttons;
};
