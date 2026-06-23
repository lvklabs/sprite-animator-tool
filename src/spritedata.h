#ifndef SPRITEDATA_H
#define SPRITEDATA_H

#include <QHash>
#include <QImage>
#include <QPixmap>

/// The SpriteData struct contains pixmap data
/// FIXME move this inside SpriteState
struct SpriteData {
    SpriteData();

    /// clear hashes
    void clear();
};

#endif // SPRITEDATA_H
