#ifndef SPRITESTATE_H
#define SPRITESTATE_H

#include <QImage>
#include <QMap>
#include <QObject>
#include <QPixmap>
#include <QString>

class QFile;

#include "inputimage.h"
#include "lvkaframe.h"
#include "lvkanimation.h"
#include "lvkframe.h"
#include "types.h"

/// .lvks on-disk format versions historically supported by SpriteState.
///
/// Phase 2 of the upgrade plan added this enum so save() no longer
/// silently rewrites a v0.1 file into the v0.4 superset; the saver now
/// preserves the source version unless the data actually uses a feature
/// only representable in a newer version (image scale != 1.0 -> v0.2,
/// animation flags != 0 -> v0.3, aframe sticky -> v0.4).
enum class LvkVersion {
    V_01 = 1,
    V_02 = 2,
    V_03 = 3,
    V_04 = 4,
};

/// The SpriteState class contains all the information about
/// input images, frames and animations
class SpriteState : public QObject {
    Q_OBJECT

public:
    SpriteState(QObject *parent = 0);

    // Version tracking *********************************************************

    /// Version header read by the last successful load(). For a freshly
    /// constructed (never-loaded) instance this defaults to V_04 so that
    /// new documents are saved in the latest format.
    LvkVersion loadedVersion() const { return _loadedVersion; }

    /// Returns the minimum .lvks version required to faithfully represent
    /// the current in-memory state (scans aframes for sticky, animations
    /// for flags, and images for non-default scale). save() uses
    /// max(loadedVersion(), minimumVersion()) as the on-disk version so
    /// loaded-and-resaved files keep their original header unless the
    /// user has introduced data that genuinely requires a newer version.
    LvkVersion minimumVersion() const;

    /// Returns the canonical header literal for @p v (e.g. "LvkSprite version 0.1").
    static const char *headerLiteral(LvkVersion v);

    // hash getters ************************************************************

    /// get input images hash
    const QMap<Id, InputImage> &images() const { return _images; }

    /// get frames hash
    const QMap<Id, LvkFrame> &frames() const { return _frames; }

    /// get animations hash
    const QMap<Id, LvkAnimation> &animations() const { return _animations; }

    /// get aframes list from animation @param aniId
    ///
    /// Agent 7 fix: In Qt6, QMap::operator[](key) const returns the value
    /// *by value* (a copy), so the previous body
    ///   `return _animations[aniId]._aframes;`
    /// produced a dangling reference into a temporary copy — a real
    /// use-after-free. We now look up via a const iterator (whose
    /// referenced value lives inside the map) and fall back to a process-
    /// wide empty list when the animation id is unknown. The empty list
    /// has static storage duration, so the returned reference is always
    /// valid for the lifetime of the program.
    const QList<LvkAframe> &aframes(Id aniId) const {
        static const QList<LvkAframe> kEmpty;
        const auto it = _animations.constFind(aniId);
        if (it == _animations.constEnd()) {
            return kEmpty;
        }
        return it.value()._aframes;
    }

    /// get frame pixmaps hash
    const QMap<Id, QPixmap> &fpixmaps() const { return _fpixmaps; }

    // pixmap getters ***********************************************************
    //
    // Agent 7: these getters are now genuinely `const`. The previous code
    // commented out `const` because the bodies used `QMap::operator[]`,
    // which in Qt6 (a) returns by value in the const overload — i.e. a
    // dangling reference — and (b) is non-const-callable in the mutating
    // overload. We now look up via const_iterator and fall back to a
    // static null sentinel when the key is missing, so the returned
    // reference is always valid.

    /// get pixmap data from image @param imgId
    const QPixmap &ipixmap(Id imgId) const {
        if (imgId == NullId)
            return nullPixmap;
        const auto it = _images.constFind(imgId);
        return (it == _images.constEnd()) ? nullPixmap : it.value().pixmap;
    }

    /// get pixmap data from frame @param frameId
    const QPixmap &fpixmap(Id frameId) const {
        if (frameId == NullId)
            return nullPixmap;
        const auto it = _fpixmaps.constFind(frameId);
        return (it == _fpixmaps.constEnd()) ? nullPixmap : it.value();
    }

    // basic const getters ******************************************************

    /// get const input image by Id
    const InputImage &const_image(Id imgId) const {
        static const InputImage kEmpty;
        const auto it = _images.constFind(imgId);
        return (it == _images.constEnd()) ? kEmpty : it.value();
    }

    /// get const frame by Id
    const LvkFrame &const_frame(Id frameId) const {
        static const LvkFrame kEmpty;
        const auto it = _frames.constFind(frameId);
        return (it == _frames.constEnd()) ? kEmpty : it.value();
    }

    /// get const animation by Id
    const LvkAnimation &const_animation(Id aniId) const {
        static const LvkAnimation kEmpty;
        const auto it = _animations.constFind(aniId);
        return (it == _animations.constEnd()) ? kEmpty : it.value();
    }

    /// get const aframe by Id
    const LvkAframe &const_aframe(Id aniId, Id aframeId) const {
        static const LvkAframe kEmpty;
        const auto it = _animations.constFind(aniId);
        return (it == _animations.constEnd()) ? kEmpty : it.value().aframe(aframeId);
    }

    // update *******************************************************************

    /// update image
    void updateImage(const InputImage &img) {
        _images[img.id] = img;
        reloadImagePixmap(img.id);
        reloadFramePixmaps(img.id);
    }

    /// update frame
    void updateFrame(const LvkFrame &frame) {
        _frames[frame.id] = frame;
        reloadFramePixmap(frame);
    }

    /// update animation
    void updateAnimation(const LvkAnimation &ani) { _animations[ani.id] = ani; }

    /// update aframe
    void updateAframe(const LvkAframe &aframe, Id aniId) {
        _animations[aniId].aframe(aframe.id) = aframe;
    }

    // add *********************************************************************

    /// Add new input image. If the image Id is null, then addImage() auto-asigns
    /// an unique Id
    void addImage(InputImage &img);

    /// Add new frame. If the frame Id is null, then addFrame() auto-asigns
    /// an unique Id
    void addFrame(LvkFrame &frame);

    /// Add new animation. If the animation Id is null, then addAnimation()
    /// auto-asigns an unique Id
    void addAnimation(LvkAnimation &ani);

    /// Add new aframe to the animation @param aniId. If the aframe Id is null,
    /// then addAframe() auto-asigns an unique Id
    void addAframe(LvkAframe &aframe, Id aniId);

    // remove ******************************************************************

    /// remove input image by id
    void removeImage(Id id) { _images.remove(id); }

    /// remove frame by id
    void removeFrame(Id id) {
        _frames.remove(id);
        _fpixmaps.remove(id);
    }

    /// remove animation by id
    void removeAnimation(Id id) { _animations.remove(id); }

    /// remove aframe @param id in animation @param aniId
    void removeAframe(Id aframeId, Id aniId) { _animations[aniId].removeAframe(aframeId); }

    // Custom header ***********************************************************

    void setCustomHeader(const QString &header) { _customHeader = header; }

    QString getCustomHeader() const { return _customHeader; }

    // Load, save, export ******************************************************

    /// Errors
    typedef enum {
        ErrNone = 0,
        ErrNullFilename,
        ErrFileDoesNotExist,
        ErrCantOpenReadMode,
        ErrCantOpenReadWriteMode,
        ErrInvalidFormat,
        ErrUnsafeOutputPath,
    } SpriteStateError;

    /// Export format flags (bit-mask). All == Cocos2d | Json.
    /// Default behavior preserves backward compatibility with pre-refactor
    /// callers: the legacy Cocos2d pipeline (.lkob / .lkot / .h) is emitted
    /// when no format is specified.
    enum ExportFormat {
        Cocos2d = 1,
        Json = 2,
        All = Cocos2d | Json,
    };

    /// save instance to @param filename
    /// NOTE: Input image filenames cannot contain the charater ',',
    ///       otherwise deserialize() will fail
    bool save(const QString &filename, SpriteStateError *err = 0);

    /// load instance from @param filename
    bool load(const QString &filename, SpriteStateError *err = 0);

    /// clear all hashes
    void clear();

    /// export sprite file @param filename.
    /// If @param outputDir is null, the sprite file directory is used.
    ///
    /// When @param format includes Cocos2d, writes .lkob/.lkot/.h files
    /// (byte-equivalent to the pre-refactor output). When @param format
    /// includes Json, also writes .png + .json (TexturePacker-compatible
    /// JSON Array atlas).
    ///
    /// Output paths are constrained to @param outputDir: any baseName
    /// derived from @param filename containing path separators or
    /// resolving outside the canonical outputDir will be rejected
    /// (returning false and setting err=ErrUnsafeOutputPath).
    bool exportSprite(const QString &filename, const QString &outputDir = QString(),
                      const QString &postpScript = "", ExportFormat format = Cocos2d,
                      SpriteStateError *err = 0) const;

    /// Backwards-compat overload preserving the pre-refactor 4-arg
    /// signature (filename, outputDir, postpScript, err). Defaults to
    /// Cocos2d. Kept so the legacy callers in src/main.cpp (Agent 10's
    /// lane) and any external tooling keep linking after Phase 3 lands.
    bool exportSprite(const QString &filename, const QString &outputDir, const QString &postpScript,
                      SpriteStateError *err) const {
        return exportSprite(filename, outputDir, postpScript, Cocos2d, err);
    }

    /// Map a CLI --format=... string to ExportFormat. Returns Cocos2d on
    /// unknown values (matches QCommandLineParser default behavior).
    /// Accepts: "cocos2d", "json", "all" (case-insensitive).
    static ExportFormat parseFormat(const QString &s);

    /// returns the error string of @param err
    static const QString &errorMessage(SpriteStateError err);

    // Force refresh pixmaps **************************************************

    /// force reload image pixmap
    void reloadImagePixmap(Id id);

    /// force reload all image pixmaps
    void reloadImagePixmaps();

    /// force reload frame pixmaps. If imgId is not null, then
    /// only reloads those frames using the image @param img
    void reloadFramePixmaps(Id imgId);

    // queries ****************************************************************

    /// Returns true if the frame is not used by any animation
    bool isFrameUnused(Id frameId) const;

signals:
    void loadProgress(QString progress);

protected:
    /// Counter. Next image id
    Id _imgId;

    /// Counter. Next frame id
    Id _frameId;

    /// Counter. Next animation id
    Id _aniId;

    /// Counter. Next animation frame id
    Id _aframeId;

    /// null pixmap
    QPixmap nullPixmap;

    /// input images hash
    QMap<Id, InputImage> _images;

    /// frames hash
    QMap<Id, LvkFrame> _frames;

    /// animations hash
    QMap<Id, LvkAnimation> _animations;

    // TODO (?) move frame pixmap into the LvkFrame classs
    /// Frame pixmaps
    QMap<Id, QPixmap> _fpixmaps;

    /// Custom data appended to the header
    QString _customHeader;

    /// On-disk version of the most recently loaded file. Defaults to V_04
    /// so new (never-loaded) SpriteStates save in the latest format. See
    /// loadedVersion() / minimumVersion() / save() for the policy.
    LvkVersion _loadedVersion = LvkVersion::V_04;

    // TODO (?) move this method inside LvkFrame
    /// force reload frame pixmap
    void reloadFramePixmap(const LvkFrame &frame);

private:
    bool writeImageWithPostprocessing(QFile &binOutput, const LvkFrame &frame,
                                      const QString &postpScript) const;
};

typedef SpriteState::SpriteStateError SpriteStateError;

#endif // SPRITESTATE_H
