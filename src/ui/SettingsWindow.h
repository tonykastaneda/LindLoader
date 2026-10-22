#pragma once

#include <QDialog>
#include <QList>

#include <functional>

class GameLibrary;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QStackedWidget;

// The frame of Lindy's Settings and Controller Settings windows, laid out like PCSX2's: pages on the
// left, the page on the right, and below them a help panel that describes the page, or the setting
// under the mouse. Settings save as they change (Lindy.ini).
class SettingsWindow : public QDialog
{
    Q_OBJECT
public:
    void showPage(int page);

signals:
    void settingsChanged();

protected:
    SettingsWindow(GameLibrary &library, const QString &title, QWidget *parent);
    bool eventFilter(QObject *watched, QEvent *event) override;

    void addPage(const QString &icon, const QString &title, const QString &description, QWidget *page);
    // Shows title and text in the help panel while the mouse is over widget.
    void help(QWidget *widget, const QString &title, const QString &text);
    // A checkbox for a true/false Lindy.ini key.
    QCheckBox *check(const QString &text, const QString &key, const QString &helpText);
    // Runs whenever the settings are (re)loaded, e.g. after Restore Defaults.
    void onLoad(const std::function<void()> &load);
    void changed(const QString &key, const QVariant &value);
    // Restore Defaults: these Lindy.ini groups ("interface", "audio", ...).
    void setDefaultsGroups(const QStringList &groups) { m_groups = groups; }
    QHBoxLayout *footer() const { return m_footer; }
    void reload();

    GameLibrary &m_library;
    bool m_loading = false;

private:
    void showHelp(const QString &title, const QString &text);

    QListWidget *m_list = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_helpTitle = nullptr;
    QLabel *m_helpText = nullptr;
    QHBoxLayout *m_footer = nullptr;
    QList<QPair<QString, QString>> m_pageHelp; // title, description per page
    QList<std::function<void()>> m_loaders;
    QStringList m_groups;
};
