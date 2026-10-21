#pragma once

#include <QString>

#include <functional>

// Game disk images in ext2/ext3/ext4 format, as Batocera uses for Lindbergh games (game.bin).
// Windows can't mount them and the loader needs plain files, so LindLoader unpacks them once.
namespace DiskImage
{
// True if the file is an ext2/3/4 filesystem image.
bool isImage(const QString &path);

// Copies every file in the image to dest. progress gets the bytes written so far and the total.
bool extract(const QString &image, const QString &dest, const std::function<void(qint64, qint64)> &progress,
             QString *error);
} // namespace DiskImage
