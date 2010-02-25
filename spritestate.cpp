#include "spritestate.h"

#include <QFile>
#include <QDebug>
#include <QTextStream>
#include <QStringList>
#include <QImageWriter>
#include <QFileInfo>
#include <QDir>

#define setError(p, err_code) if (p) { *(p) = err_code; }

SpriteState::SpriteState(QObject* parent)
        : QObject(parent), _imgId(0), _frameId(0), _aniId(0), _aframeId(0)
{
}

void SpriteState::addImage(InputImage& img)
{
    if (img.id == NullId) {
        img.id = _imgId++;
    } else {
        _imgId = std::max(_imgId, img.id + 1);
    }
    _images.insert(img.id, img);
}

void SpriteState::addFrame(LvkFrame& frame)
{
    if (frame.id == NullId) {
        frame.id = _frameId++;
    } else {
        _frameId = std::max(_frameId, frame.id + 1);
    }
    reloadFramePixmap(frame);
    _frames.insert(frame.id, frame);
}

void SpriteState::addAnimation(LvkAnimation& ani)
{
    if (ani.id == NullId) {
        ani.id = _aniId++;
    } else {
        _aniId = std::max(_aniId, ani.id + 1);
    }
    _animations.insert(ani.id, ani);
}

void SpriteState::addAframe(LvkAframe& aframe, Id aniId)
{
    if (aframe.id == NullId) {
        aframe.id = _aframeId++;
    } else {
        _aframeId = std::max(_aframeId, aframe.id + 1);
    }
    _animations[aniId].aframes.insert(aframe.id, aframe);
}

void SpriteState::clear()
{
    _imgId    = 0;
    _frameId  = 0;
    _aniId    = 0;
    _aframeId = 0;

    _images.clear();
    _frames.clear();
    _animations.clear();
    _fpixmaps.clear();
}

bool SpriteState::save(const QString& filename, SpriteStateError* err)
{
    setError(err, ErrNone);

    QFile file(filename);

    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() <<  "Error: SpriteState::save(): could not open"
                 << filename << "in rw mode";
        setError(err, ErrCantOpenReadWriteMode);
        return false;
    }

    QTextStream stream(&file);
    stream << "### LvkSprite #########################################\n";
    stream << "LvkSprite version 0.1\n\n";

    stream << "# Images\n";
    stream << "# format: imageId,filename\n";
    stream << "images(\n";
    for (QHashIterator<Id, InputImage> it(_images); it.hasNext();) {
        it.next();
        stream << "\t" <<  it.value().toString() << "\n";
    }
    stream << ")\n\n";

    stream << "# Frames\n";
    stream << "# format: frameId,name,imageId,ox,oy,w,h\n";
    stream << "frames(\n";
    for (QHashIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
        it.next();
        stream << "\t" << it.value().toString() << "\n";
    }
    stream << ")\n\n";

    stream << "# Animations\n";
    stream << "# format: animationId,name\n";
    stream << "# Animation frames\n";
    stream << "# format: aframeId,frameId,delay,ox,oy\n";
    stream << "animations(\n";
    for (QHashIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        stream << "\t" << it.value().toString() << "\n";        
        stream << "\taframes(\n";
        for (QHashIterator<Id, LvkAframe> it2(it.value().aframes); it2.hasNext();) {
            it2.next();
            stream << "\t\t" << it2.value().toString() << "\n";
        }
        stream << "\t)\n\n";
    }
    stream << ")\n\n";

    stream << "### End LvkSprite #####################################\n";

    file.close();

    return true;
}

bool SpriteState::load(const QString& filename, SpriteStateError* err)
{
    setError(err, ErrNone);

    if (filename.isEmpty()) {
        setError(err, ErrNullFilename);
        return false;
    }

    QFile file(filename);

    if (!file.exists()) {
        setError(err, ErrFileDoesNotExist);
        return false;
    }

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qDebug() <<  "Error: SpriteState::load(): could not open"
                 << filename << "in ro mode";
        setError(err, ErrCantOpenReadMode);
        return false;
    }

    clear();

    enum {
        StCheckVersion    = 0,
        StNoToken         = 1,
        StTokenImages     = 2,
        StTokenFrames     = 3,
        StTokenAnimations = 4,
        StTokenAframes    = 5,
        StError           = 999,
    } state = StCheckVersion;

    QTextStream stream(&file);
    QString     line;
    QStringList tokens;

    int lineNumber = -1;

    InputImage   tmpImage;
    LvkFrame     tmpFrame;
    LvkAnimation tmpAni;
    LvkAframe    tmpAframe;

    Id currentAniId = NullId;

    do {
        line = stream.readLine().trimmed();
        ++lineNumber;

        if (line.isNull()) {
            break; /* end of stream */
        }
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith('#')) {
            continue;
        }

        if (state == StCheckVersion) {
            if (line == "LvkSprite version 0.1") {
                state = StNoToken;
                continue;
            } else {
                qDebug() << "Error: SpriteState::load(): Invalid LvkSprite file format"
                         << "at line" << lineNumber;
                setError(err, ErrInvalidFormat);
                state = StError;
                break;
            }
        }

        switch (state) {
        case StNoToken:
            if (line == "images(") {
                state = StTokenImages;
            } else if (line == "frames(") {
                state = StTokenFrames;
            } else if (line == "animations(") {
                state = StTokenAnimations;
            } else if (line == "aframes(") {
                qDebug() << "Error: SpriteState::load(): Unspected token"
                         << line << "at line" << lineNumber;
                setError(err, ErrInvalidFormat);
                state = StError;
            } else {
                qDebug() << "Error: SpriteState::load(): Unknown token"
                         << line << "at line" << lineNumber;
                setError(err, ErrInvalidFormat);
                state = StError;
            }
            break;

        case StTokenImages:
            if (line == ")") {
                state = StNoToken;
            } else {
                if (tmpImage.fromString(line)) {
                    addImage(tmpImage);
                } else {
                    qDebug() << "Error: SpriteState::load(): invalid image entry"
                             << line << "at line" << lineNumber;
                    setError(err, ErrInvalidFormat);
                    state = StError;
                }
            }
            break;

        case StTokenFrames:
            if (line == ")") {
                state = StNoToken;
            } else {
                if (tmpFrame.fromString(line)) {
                    addFrame(tmpFrame);
                } else {
                    qDebug() << "Error: SpriteState::load(): invalid frame entry"
                             << line << "at line" << lineNumber;
                    setError(err, ErrInvalidFormat);
                    state = StError;
                }
            }
            break;

        case StTokenAnimations:
            if (line == ")") {
                currentAniId = NullId;
                state = StNoToken;
            } else if (line == "aframes(") {
                if (currentAniId != NullId) {
                    state = StTokenAframes;
                } else {
                    qDebug() << "Error: SpriteState::load(): null animation id"
                             << "at line" << lineNumber;
                    setError(err, ErrInvalidFormat);
                    state = StError;
                }
            } else {
                if (tmpAni.fromString(line)) {
                    currentAniId = tmpAni.id;
                    addAnimation(tmpAni);
                } else {
                    qDebug() << "Error: SpriteState::load(): invalid animation entry"
                             << line << "at line" << lineNumber;
                    setError(err, ErrInvalidFormat);
                    state = StError;
                }
            }
            break;

        case StTokenAframes:
            if (line == ")") {
                state = StTokenAnimations;
            } else {
                if (tmpAframe.fromString(line)) {
                    addAframe(tmpAframe, currentAniId);
                } else {
                    qDebug() << "Error: SpriteState::load(): invalid aframe entry"
                             << line << "at line" << lineNumber;
                    setError(err, ErrInvalidFormat);
                    state = StError;
                }
            }
            break;

        default:
            qDebug() << "Warning: SpriteState::load(): Unhandled state "
                     << (int)state << "at line" << lineNumber;
            break;
        }
    } while (true);

    file.close();

    return (state != StError);
}
bool SpriteState::exportSprite(const QString& filename, const QString& outputDir_, SpriteStateError* err) const
{
    setError(err, ErrNone);

    QFileInfo fileInfo(filename);

    QString outputDir;
    if (!outputDir_.isNull()) {
        outputDir = outputDir_;
    } else {
        outputDir = fileInfo.path();
    }

    QString binFileName  = outputDir + QDir::separator() + fileInfo.baseName() + ".lkob";
    QString textFileName = outputDir + QDir::separator() + fileInfo.baseName() + ".lkot";

    QFile binOutput(binFileName);
    QFile textOutput(textFileName);

    if (binOutput.exists()) {
        if (!binOutput.remove()) {
            qDebug() <<  "Error: SpriteState::exportSprite():"
                     << binOutput.fileName() << "already exists and cannot be removed";
            setError(err, ErrCantOpenReadWriteMode);
            return false;
        }
    }

    if (!binOutput.open(QFile::WriteOnly | QFile::Append)) {
        qDebug() <<  "Error: SpriteState::exportSprite(): could not open "
                 << binOutput.fileName() << " in WriteOnly mode";
        setError(err, ErrCantOpenReadWriteMode);
        return false;
    }

    if (!textOutput.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() <<  "Error: SpriteState::exportSprite(): could not open "
                 << textOutput.fileName() << " in WriteOnly mode";
        setError(err, ErrCantOpenReadWriteMode);
        return false;
    }

    QTextStream textStream(&textOutput);
    textStream << "### Exported LvkSprite ################################\n";
    textStream << "Exported LvkSprite version 0.1\n\n";

    textStream << "# Frame Pixmaps\n";
    textStream << "# format: frameId,offset(bytes),length(bytes)\n";
    textStream << "fpixmaps(\n";

    QImageWriter imgWriter(&binOutput, QByteArray("png"));
    qint64 prevOffset = 0; /* previous offset */
    qint64 offset = 0;

    for (QHashIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
        LvkFrame frame = it.next().value();
        prevOffset = offset;
        imgWriter.write(_fpixmaps[frame.id].toImage());
        offset = binOutput.size();
        textStream << "\t" <<  frame.id << "," <<  prevOffset << "," << (offset - prevOffset) << "\n";
    }
    textStream << ")\n\n";

    binOutput.close();

    textStream << "# Animations\n";
    textStream << "# format: animationId,name\n";
    textStream << "# Animation frames\n";
    textStream << "# format: aframeId,frameId,delay\n";
    textStream << "animations(\n";
    for (QHashIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        textStream << "\t" << it.value().toString() << "\n";
        textStream << "\taframes(\n";
        for (QHashIterator<Id, LvkAframe> it2(it.value().aframes); it2.hasNext();) {
            it2.next();
            textStream << "\t\t" << it2.value().toString() << "\n";
        }
        textStream << "\t)\n\n";
    }
    textStream << ")\n\n";

    textStream << "### End Exported LvkSprite ############################\n";

    textOutput.close();

    return true;
}

void SpriteState::reloadFramePixmap(const LvkFrame& frame)
{
    if (frame.id != NullId) {
        QPixmap tmp(ipixmap(frame.imgId));
        QPixmap fpixmap(tmp.copy(frame.ox, frame.oy, frame.w, frame.h));
        _fpixmaps.insert(frame.id, fpixmap);
    }
}


void SpriteState::reloadImagePixmap(Id imgId)
{
    if (imgId != NullId) {
        _images[imgId].reloadImage();
    }
}

void SpriteState::reloadImagePixmaps()
{
    for (QMutableHashIterator<Id, InputImage> it(_images); it.hasNext();) {
        it.next();
        reloadImagePixmap(it.value().id);
    }
}

void SpriteState::reloadFramePixmaps(Id imgId)
{
    for (QHashIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
        it.next();
        const LvkFrame& frame =  it.value();
        if (imgId == NullId || frame.imgId == imgId) {
            reloadFramePixmap(frame);
        }
    }
}

const QString& SpriteState::errorMessage(SpriteStateError err)
{
    static const QString strErrNone                 = tr("No error");
    static const QString strErrCantOpenReadMode     = tr("Cannot read file");
    static const QString strErrOpenReadWriteMode    = tr("Cannot write file");
    static const QString strErrInvalidFormat        = tr("The file has an invalid sprite format");
    static const QString strErrNullFilename         = tr("Empty filename");
    static const QString strErrFileDoesNotExist     = tr("File does not exist");
    static const QString strErrUnknown              = tr("Unknown error");

    switch (err) {
    case ErrNone:
        return strErrNone;
    case ErrCantOpenReadMode:
        return strErrCantOpenReadMode;
    case ErrCantOpenReadWriteMode:
        return strErrOpenReadWriteMode;
    case ErrInvalidFormat:
        return strErrInvalidFormat;
    case ErrNullFilename:
        return strErrNullFilename;
    case ErrFileDoesNotExist:
        return strErrFileDoesNotExist;
    default:
        return strErrUnknown;
    }
}

