#pragma once

#include <QList>
#include <QWidget>

#include "GameLibrary.h"
#include "IniFile.h"

class BindingButton;
class QComboBox;
struct BindingDef;

// The Controls page of Game Properties, laid out like PCSX2's controller settings: the binding
// groups around a picture of the controls, the system buttons underneath. Click a binding, then
// press a key or controller button; right-click clears it. Writes the game's controls.ini.
class ControllerSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    ControllerSettingsWidget(const Game &game, GameLibrary &library, QWidget *parent = nullptr);

private:
    QWidget *buildGroup(const QString &title, const QList<BindingDef> &binds);
    QString keyFor(const BindingButton *button) const;
    void refresh();
    void refreshControllers();
    void bind(BindingButton *button, const QString &binding, const QString &guid);
    void clear(BindingButton *button);
    void selectController(int index);
    void resetToDefaults();
    void save();

    Game m_game;
    GameLibrary &m_library;
    IniFile m_ini;
    QString m_section; // the game's section in controls.ini: Digital, Driving, ...
    int m_player = 1;
    QComboBox *m_controllers = nullptr;
    QList<BindingButton *> m_buttons;
};
