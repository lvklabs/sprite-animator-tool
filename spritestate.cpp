#include "spritestate.h"

#include <QFile>
#include <QDebug>
#include <QTextStream>
#include <QStringList>


#define setError(p, err_code) if (p) { *p = err_code; }

SpriteState::SpriteState()
{
}

void SpriteState::clear()
{
    _images.clear();
    _frames.clear();
    _animations.clear();

    _fpixmaps.clear();
}

bool SpriteState::serialize(const QString& filename, int* error) const
{
    QFile file(filename);

    setError(error, 0);

    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() <<  "Error: SpriteState::serialize(): could not open"
                 << filename << "in rw mode";
        setError(error, ErrCantOpenReadWriteMode);
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

bool SpriteState::deserialize(const QString& filename, int* error)
{
    QFile file(filename);

    setError(error, 0);

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qDebug() <<  "Error: SpriteState::serialize(): could not open"
                 << filename << "in ro mode";
        setError(error, ErrCantOpenReadMode);
        return false;
    }

    _images.clear();

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
                qDebug() << "Error: SpriteState::deserialize(): Invalid LvkSprite file format"
                         << "at line" << lineNumber;
                setError(error, ErrInvalidFormat);
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
                qDebug() << "Error: SpriteState::deserialize(): Unspected token"
                         << line << "at line" << lineNumber;
                setError(error, ErrInvalidFormat);
                state = StError;
            } else {
                qDebug() << "Error: SpriteState::deserialize(): Unknow token"
                         << line << "at line" << lineNumber;
                setError(error, ErrInvalidFormat);
                state = StError;
            }
            break;

        case StTokenImages:
            if (line == ")") {
                state = StNoToken;
            } else {
                if (tmpImage.fromString(line)) {
                    _images.insert(tmpImage.id, tmpImage);
                } else {
                    qDebug() << "Error: SpriteState::deserialize(): invalid image entry"
                             << line << "at line" << lineNumber;
                    setError(error, ErrInvalidFormat);
                    state = StError;
                }
            }
            break;

        case StTokenFrames:
            if (line == ")") {
                state = StNoToken;
            } else {
                if (tmpFrame.fromString(line)) {
                    _frames.insert(tmpFrame.id, tmpFrame);
                } else {
                    qDebug() << "Error: SpriteState::deserialize(): invalid frame entry"
                             << line << "at line" << lineNumber;
                    setError(error, ErrInvalidFormat);
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
                    qDebug() << "Error: SpriteState::deserialize(): null animation id"
                             << "at line" << lineNumber;
                    setError(error, ErrInvalidFormat);
                    state = StError;
                }
            } else {
                if (tmpAni.fromString(line)) {
                    currentAniId = tmpAni.id;
                    _animations.insert(tmpAni.id, tmpAni);
                } else {
                    qDebug() << "Error: SpriteState::deserialize(): invalid animation entry"
                             << line << "at line" << lineNumber;
                    setError(error, ErrInvalidFormat);
                    state = StError;
                }
            }
            break;

        case StTokenAframes:
            if (line == ")") {
                state = StTokenAnimations;
            } else {
                if (tmpAframe.fromString(line)) {
                    _animations[currentAniId].aframes.insert(tmpAframe.id, tmpAframe);
                } else {
                    qDebug() << "Error: SpriteState::deserialize(): invalid aframe entry"
                             << line << "at line" << lineNumber;
                    setError(error, ErrInvalidFormat);
                    state = StError;
                }
            }
            break;

        default:
            qDebug() << "Warning: SpriteState::deserialize(): Unhandled state "
                     << state << "at line" << lineNumber;
            break;
        }
    } while (true);

    file.close();

    return (state != StError);
}
bool SpriteState::serializeOutput(const QString& filename, int* error) const
{
    if(_animations.count() == 0){
        qDebug() <<  "Nothing to export.";
        return false;
    }

    QString binFileName;
    QString textFileName;
    if(filename.endsWith(".lkot")){
        binFileName.append(filename);
        binFileName.replace(".lkot", ".lkob");
        textFileName.append(filename);
    }else{
        binFileName.append(filename + ".lkob");
        textFileName.append(filename + ".lkot");
    }

    QFile binOutput(binFileName);
    QFile textOutput(textFileName);

    setError(error, 0);

    if (!binOutput.open(QFile::WriteOnly)) {
        qDebug() <<  "Error: SpriteState::serializeOutput(): could not open "
                 << binOutput.fileName() << " in WriteOnly mode";
        setError(error, ErrCantOpenReadWriteMode);
        return false;
    }
    if (!textOutput.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() <<  "Error: SpriteState::serializeOutput(): could not open "
                 << textOutput.fileName() << " in WriteOnly mode";
        setError(error, ErrCantOpenReadWriteMode);
        return false;
    }

    QDataStream dataStream(&binOutput);
    QTextStream textStream(&textOutput);
    textStream << "### LvkSprite #########################################\n";
    textStream << "LvkSprite version 0.1\n\n";

    textStream << "# Images\n";
    textStream << "# format: imageId,offset(bytes),length(bytes)\n";
    textStream << "images(\n";
    qint64 offset = 0;
    for (QHashIterator<Id, InputImage> it(_images); it.hasNext();) {
        it.next();
        InputImage inImage = it.value();
        QFile imageFile(inImage.filename);
        if (!imageFile.open(QFile::ReadOnly)) {
            qDebug() <<  "Error: SpriteState::serializeOutput(): could not open "
                     << imageFile.fileName() << " in ReadOnly mode";
            setError(error, ErrCantOpenReadWriteMode);
            return false;
        }
        QByteArray imageContents(imageFile.readAll());
        dataStream.writeRawData(imageContents.constData(), imageContents.length());
        textStream << "\t" <<  inImage.id << "," <<  offset << "," << imageFile.size() << "\n";
        offset += imageFile.size();
        imageFile.close();
    }
    binOutput.close();
    textStream << ")\n\n";

    textStream << "# Frames\n";
    textStream << "# format: frameId,name,imageId,ox,oy,w,h\n";
    textStream << "frames(\n";
    for (QHashIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
        it.next();
        textStream << "\t" << it.value().toString() << "\n";
    }
    textStream << ")\n\n";

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

    textStream << "### End LvkSprite #####################################\n";

    textOutput.close();

    return true;
}
