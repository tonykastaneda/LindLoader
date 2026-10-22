#pragma once

#include "SettingsWindow.h"

class BindingButton;

// Controller Settings: Global Settings (where controller input comes from), Lightgun (one mouse per
// player, crosshairs) and Hotkeys (Toggle Fullscreen, Exit Game, ...), the same for every game. Each
// game's buttons are bound on its own Controls page.
class ControllerSettingsDialog : public SettingsWindow
{
    Q_OBJECT
public:
    enum Page
    {
        GlobalSettings,
        Lightgun,
        Hotkeys,
    };
    ControllerSettingsDialog(GameLibrary &library, Page page = GlobalSettings, QWidget *parent = nullptr);

private:
    QWidget *buildGlobalPage();
    QWidget *buildLightgunPage();
    QWidget *buildHotkeysPage();

    QList<BindingButton *> m_hotkeys;
};
