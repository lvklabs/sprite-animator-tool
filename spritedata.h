#ifndef SPRITEDATA_H
#define SPRITEDATA_H

#include <QImage>
#include <QPixmap>
#include <QHash>

/// The SpriteData struct contains pixmap data
/// FIXME move this inside SpriteState
struct SpriteData
{
    SpriteData();

    /// clear hashes
    void clear();
};

#endif // SPRITEDATA_H
