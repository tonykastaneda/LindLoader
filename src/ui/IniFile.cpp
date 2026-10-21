#include "IniFile.h"

#include <QFile>
#include <QTextStream>

namespace
{
bool isComment(const QString &line)
{
    return line.startsWith('#') || line.startsWith(';');
}

// The section name if the line is a [Section] header, otherwise a null string.
QString sectionName(const QString &line)
{
    if (line.startsWith('[') && line.endsWith(']'))
        return line.mid(1, line.size() - 2).trimmed();
    return QString();
}

QString keyOf(const QString &line)
{
    const int eq = line.indexOf('=');
    return eq > 0 ? line.left(eq).trimmed() : QString();
}
} // namespace

bool IniFile::load(const QString &path)
{
    m_path = path;
    m_lines.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream in(&f);
    while (!in.atEnd())
        m_lines << in.readLine();
    return true;
}

bool IniFile::save() const
{
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream out(&f);
    for (const QString &line : m_lines)
        out << line << '\n';
    return true;
}

int IniFile::findSection(const QString &section) const
{
    for (int i = 0; i < m_lines.size(); ++i)
        if (sectionName(m_lines[i].trimmed()).compare(section, Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

int IniFile::findKey(const QString &section, const QString &key) const
{
    QString current;
    for (int i = 0; i < m_lines.size(); ++i)
    {
        const QString line = m_lines[i].trimmed();
        if (isComment(line))
            continue;
        const QString name = sectionName(line);
        if (!name.isNull())
        {
            current = name;
            continue;
        }
        if (!section.isEmpty() && current.compare(section, Qt::CaseInsensitive) != 0)
            continue;
        if (keyOf(line).compare(key, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

QString IniFile::value(const QString &section, const QString &key, const QString &fallback) const
{
    const int i = findKey(section, key);
    if (i < 0)
        return fallback;
    const QString &line = m_lines[i];
    return line.mid(line.indexOf('=') + 1).trimmed();
}

void IniFile::setValue(const QString &section, const QString &key, const QString &value)
{
    const int i = findKey(section, key);
    if (i >= 0)
    {
        m_lines[i] = key + " = " + value;
        return;
    }
    int header = section.isEmpty() ? -1 : findSection(section);
    if (!section.isEmpty() && header < 0)
    {
        if (!m_lines.isEmpty() && !m_lines.last().trimmed().isEmpty())
            m_lines << QString();
        m_lines << "[" + section + "]";
        header = m_lines.size() - 1;
    }
    if (header < 0)
    {
        m_lines << key + " = " + value;
        return;
    }
    // Insert after the last non-empty line of the section.
    int last = header;
    for (int j = header + 1; j < m_lines.size() && sectionName(m_lines[j].trimmed()).isNull(); ++j)
        if (!m_lines[j].trimmed().isEmpty())
            last = j;
    m_lines.insert(last + 1, key + " = " + value);
}

void IniFile::removeValue(const QString &section, const QString &key)
{
    const int i = findKey(section, key);
    if (i >= 0)
        m_lines.removeAt(i);
}

QStringList IniFile::keys(const QString &section) const
{
    QStringList result;
    QString current;
    for (const QString &raw : m_lines)
    {
        const QString line = raw.trimmed();
        if (isComment(line))
            continue;
        const QString name = sectionName(line);
        if (!name.isNull())
            current = name;
        else if (current.compare(section, Qt::CaseInsensitive) == 0 && !keyOf(line).isEmpty())
            result << keyOf(line);
    }
    return result;
}
