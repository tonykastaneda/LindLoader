#pragma once

#include <QString>
#include <QStringList>

// Edits linuxloader.ini / controls.ini in place: values change, comments and layout stay.
// An empty section means "the first matching key in any section".
class IniFile
{
public:
    bool load(const QString &path);
    bool save() const;
    QString path() const { return m_path; }

    QString value(const QString &key, const QString &fallback = QString()) const { return value({}, key, fallback); }
    void setValue(const QString &key, const QString &value) { setValue({}, key, value); }

    QString value(const QString &section, const QString &key, const QString &fallback) const;
    void setValue(const QString &section, const QString &key, const QString &value);
    void removeValue(const QString &section, const QString &key);
    QStringList keys(const QString &section) const;

private:
    int findKey(const QString &section, const QString &key) const;
    int findSection(const QString &section) const;

    QString m_path;
    QStringList m_lines;
};
