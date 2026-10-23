#include "UnpackImages.h"

#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QInputDialog>
#include <QLocale>
#include <QMessageBox>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrentRun>
#include <atomic>
#include <functional>
#include <memory>

#include "DiskImage.h"
#include "GameLibrary.h"

namespace
{
using Unpacked = QList<QPair<QString, QString>>; // image, the folder it unpacked to (empty if it failed)

bool inLibrary(const GameLibrary &library, const QString &path)
{
    const QString dir = QDir::cleanPath(path);
    for (const QString &f : library.folders())
    {
        const QString root = QDir::cleanPath(f);
        if (dir.compare(root, Qt::CaseInsensitive) == 0 || dir.startsWith(root + "/", Qt::CaseInsensitive))
            return true;
    }
    return false;
}

// Copies the files under from into to (folders included); progress gets the bytes copied so far.
bool copyTree(const QString &from, const QString &to, qint64 *copied, const std::function<void(qint64)> &progress)
{
    if (!QDir().mkpath(to))
        return false;
    for (const QFileInfo &fi : QDir(from).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
    {
        const QString target = to + "/" + fi.fileName();
        if (fi.isDir() && !fi.isSymLink())
        {
            if (!copyTree(fi.filePath(), target, copied, progress))
                return false;
            continue;
        }
        if (!QFile::copy(fi.filePath(), target))
            return false;
        *copied += fi.size();
        progress(*copied);
    }
    return true;
}

// Moves an unpacked game's folder into a library folder: a rename on the same drive, otherwise a copy
// (showing its progress) and then removing the original.
void moveToLibrary(QWidget *parent, const QString &from, const QString &to)
{
    if (QFileInfo::exists(to))
    {
        QMessageBox::warning(parent, "Move to Library",
                             QStringLiteral("%1 already exists, so the game stays in %2.")
                                 .arg(QDir::toNativeSeparators(to), QDir::toNativeSeparators(from)));
        return;
    }
    if (QDir().rename(from, to))
        return;
    qint64 total = 0;
    for (QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories); it.hasNext();)
        total += it.nextFileInfo().size();
    QProgressDialog progress(QStringLiteral("Moving %1 to %2...").arg(QFileInfo(from).fileName(), QDir::toNativeSeparators(to)),
                             QString(), 0, 1000, parent);
    progress.setWindowTitle("Move to Library");
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    QFutureWatcher<bool> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<bool>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run([from, to, total, &progress] {
        qint64 copied = 0;
        return copyTree(from, to, &copied, [total, &progress](qint64 done) {
            const int value = total ? int(done * 1000 / total) : 1000;
            QMetaObject::invokeMethod(&progress, [&progress, value] { progress.setValue(value); });
        });
    }));
    loop.exec();
    progress.close();
    // Only a complete copy replaces the original; a partial one is removed again.
    if (watcher.result())
        QDir(from).removeRecursively();
    else
    {
        QDir(to).removeRecursively();
        QMessageBox::warning(parent, "Move to Library",
                             QStringLiteral("Could not copy the game to %1, so it stays in %2.")
                                 .arg(QDir::toNativeSeparators(to), QDir::toNativeSeparators(from)));
    }
}

// After unpacking: the images go (asked first, unless the setting says), and folders outside the game list join it.
void finish(GameLibrary &library, QWidget *parent, const Unpacked &results)
{
    QStringList images, folders;
    qint64 bytes = 0;
    for (const auto &[image, folder] : results)
    {
        if (folder.isEmpty())
            continue;
        for (const QString &done : library.unpackedImages(image))
        {
            images << done;
            bytes += QFileInfo(done).size();
        }
        folders << QDir::toNativeSeparators(folder);
    }
    if (images.isEmpty())
        return;

    const QString after = library.setting("library/afterUnpack").toString();
    const QString size = QLocale().formattedDataSize(bytes);
    bool remove = after == "delete";
    if (after == "ask")
        remove = QMessageBox::question(parent, "Unpack (.bin)",
                                       QStringLiteral("Unpacked into:\n%1\n\nDelete the disk image%2 to free %3? Lindy "
                                                      "runs the unpacked folder%2, not the image%2.")
                                           .arg(folders.join("\n"), images.size() > 1 ? "s" : "", size)) ==
                 QMessageBox::Yes;
    if (remove)
    {
        QStringList failed;
        for (const QString &image : images)
            if (!QFile::remove(image))
                failed << QDir::toNativeSeparators(image);
        if (!failed.isEmpty())
            QMessageBox::warning(parent, "Unpack (.bin)", "Could not delete:\n" + failed.join("\n"));
    }

    // The game list shows the games in its library folders: a game unpacked elsewhere can move into one
    // (or, with no library folders yet, its folder can become one).
    for (const auto &[image, folder] : results)
    {
        if (folder.isEmpty() || inLibrary(library, folder))
            continue;
        const QString game = QFileInfo(folder).fileName();
        if (library.folders().isEmpty())
        {
            const QString dir = QFileInfo(folder).absolutePath();
            if (QMessageBox::question(parent, "Unpack (.bin)",
                                      QStringLiteral("Add %1 to the game list?").arg(QDir::toNativeSeparators(dir))) ==
                QMessageBox::Yes)
                library.addFolder(dir);
            continue;
        }
        QStringList libraries;
        for (const QString &f : library.folders())
            libraries << QDir::toNativeSeparators(f);
        bool ok = false;
        const QString to = QInputDialog::getItem(parent, "Move to Library",
                                                 QStringLiteral("%1 isn't in your game library yet. Move it to:").arg(game),
                                                 libraries, 0, false, &ok);
        if (ok)
            moveToLibrary(parent, folder, QDir::fromNativeSeparators(to) + "/" + game);
    }
    library.rescan();
}
} // namespace

void unpackDiskImages(GameLibrary &library, QWidget *parent)
{
    QStringList images = QFileDialog::getOpenFileNames(parent, "Unpack (.bin)", QString(),
                                                       "Disk images (*.bin *.img *.ext2 *.ext3 *.ext4)");
    QStringList notImages;
    for (const QString &image : images)
        if (!DiskImage::isImage(image))
            notImages << QDir::toNativeSeparators(image);
    if (!notImages.isEmpty())
        QMessageBox::warning(parent, "Unpack (.bin)", "Not a disk image Lindy can unpack:\n" + notImages.join("\n"));
    images.removeIf([&notImages](const QString &image) { return notImages.contains(QDir::toNativeSeparators(image)); });
    if (images.isEmpty())
        return;

    // No Cancel: an image is unpacked to a temporary folder that only becomes the game's when it's complete.
    auto *progress = new QProgressDialog("Unpacking...", QString(), 0, 100 * images.size(), parent);
    progress->setWindowTitle("Unpack (.bin)");
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setValue(0);
    auto current = std::make_shared<std::atomic_int>(0);
    QObject::connect(&library, &GameLibrary::extracting, progress, [progress, current](const QString &name, int percent) {
        progress->setLabelText(QStringLiteral("Unpacking %1... %2%").arg(name).arg(percent));
        progress->setValue(qMax(progress->value(), *current * 100 + percent));
    });

    auto *watcher = new QFutureWatcher<Unpacked>(progress);
    QObject::connect(watcher, &QFutureWatcher<Unpacked>::finished, parent, [&library, parent, progress, watcher] {
        const Unpacked results = watcher->result();
        progress->close();
        progress->deleteLater();
        finish(library, parent, results);
    });
    watcher->setFuture(QtConcurrent::run([&library, images, current] {
        Unpacked results;
        for (int i = 0; i < images.size(); ++i)
        {
            *current = i;
            results << qMakePair(images[i], library.unpackImage(images[i]));
        }
        return results;
    }));
}
