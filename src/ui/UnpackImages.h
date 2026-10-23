#pragma once

class GameLibrary;
class QWidget;

// File > Unpack (.bin)...: picks disk images and unpacks each into a folder next to it, showing the
// progress. Then deletes the images or offers to (Settings > Game List > After Unpacking), offers to move
// a game unpacked outside the library into one of its folders, and rescans.
void unpackDiskImages(GameLibrary &library, QWidget *parent);
