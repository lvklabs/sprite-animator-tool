#include "spritestate.h"

#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QTextStream>
#include <QStringList>
#include <QImageWriter>
#include <QImageReader>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <iostream>
#include <cctype>

#define HEADER_VER_01 "LvkSprite version 0.1"
#define HEADER_VER_02 "LvkSprite version 0.2"
#define HEADER_VER_03 "LvkSprite version 0.3"
#define HEADER_LATEST HEADER_VER_03

#define setError(p, err_code) if (p) { *(p) = err_code; }

// Convert string to a new string containing only valid characters for macro names
QString getMacroName(const QString& name)
{
    QString macroName;

    QByteArray a = name.toAscii();
    for (int i = 0; i < a.size(); ++i)
    {
        char c = a[i];
        if (isalpha(c)) {
            macroName.append(toupper(c));
        } else if (isnumber(c)) {
            macroName.append(c);
        } else if (c == ' ' || c == '_' || c == '.') {
            macroName.append('_');
        }
    }

    return macroName;
}

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
    _animations[aniId]._aframes.insert(aframe.id, aframe);
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

    _customHeader = "";
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
    stream << HEADER_LATEST "\n\n";

    stream << "# Images\n";
    stream << "# format: imageId,filename,scale\n";
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
        for (QListIterator<LvkAframe> it2(it.value()._aframes); it2.hasNext();) {
            stream << "\t\t" << it2.next().toString() << "\n";
        }
        stream << "\t)\n\n";
    }
    stream << ")\n\n";

    stream << "# Custom data appended to the header\n";
    stream << "custom_header(\n";
    stream << _customHeader << "\n";
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
        StTokenHeader     = 6,
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
        if (line.isEmpty() && state != StTokenHeader) {
            continue;
        }
        if (line.startsWith('#') && state != StTokenHeader) {
            continue;
        }

        if (state == StCheckVersion) {
            if (line == HEADER_VER_01 || line == HEADER_VER_02 || line == HEADER_VER_03) {
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
            } else if (line == "custom_header(") {
                state = StTokenHeader;
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
                    emit(loadProgress(tr("Image ") + tmpImage.filename));
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
                    emit(loadProgress(tr("Frame ") + tmpImage.filename));
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
                    emit(loadProgress(tr("Animation ") + tmpAni.name));
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

        case StTokenHeader:
            if (line == ")") {
                state = StNoToken;
            } else {
                _customHeader.append(line).append("\n");
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

bool SpriteState::exportSprite(const QString& filename, const QString& outputDir_,
                               const QString &postpScript, SpriteStateError* err) const
{
    setError(err, ErrNone);

    QFileInfo fileInfo(filename);

    QString outputDir;
    if (!outputDir_.isEmpty()) {
        outputDir = outputDir_;
    } else {
        outputDir = fileInfo.path();
    }

    QString binFileName  = outputDir + QDir::separator() + fileInfo.baseName() + ".lkob";
    QString textFileName = outputDir + QDir::separator() + fileInfo.baseName() + ".lkot";
    QString headerFileName = outputDir + QDir::separator() + "AnimNameDef_" + fileInfo.baseName() + ".h";

    QFile binOutput(binFileName);
    QFile textOutput(textFileName);
    QFile headerOutput(headerFileName);

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

    if (!headerOutput.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() <<  "Error: SpriteState::exportSprite(): could not open "
                 << headerOutput.fileName() << " in WriteOnly mode";
        setError(err, ErrCantOpenReadWriteMode);
        return false;
    }

    /////////////////////////////////////////////////////////////////////////////////
    // Export bin and text file

    QTextStream textStream(&textOutput);
    textStream << "### Exported LvkSprite ################################\n";
    textStream << "Exported LvkSprite version 0.1\n\n";

    textStream << "# Frame Pixmaps\n";
    textStream << "# format: frameId,offset(bytes),length(bytes)\n";
    textStream << "fpixmaps(\n";

    qint64 prevOffset = 0; /* previous offset */
    qint64 offset = 0;

    for (QHashIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
        LvkFrame frame = it.next().value();

        // export only those frames that are used at least in one animation
        if (!isFrameUnused(frame.id)) {
            prevOffset = offset;
            writeImageWithPostprocessing(binOutput, frame, postpScript);
            offset = binOutput.size();
            textStream << "\t" <<  frame.id << "," <<  prevOffset << "," << (offset - prevOffset) << "\n";
        }
        else
        {
            qDebug() << "Omitting unused frame " << frame.id;
        }
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
        for (QListIterator<LvkAframe> it2(it.value()._aframes); it2.hasNext();) {
            textStream << "\t\t" << it2.next().toString() << "\n";
        }
        textStream << "\t)\n\n";
    }
    textStream << ")\n\n";

    textStream << "### End Exported LvkSprite ############################\n";

    textOutput.close();

    /////////////////////////////////////////////////////////////////////////////////
    // Export header file

    QTextStream headerStream(&headerOutput);
    headerStream << "// File autogenerated by " HEADER_LATEST "\n";
    headerStream << "// -- DO NOT EDIT OR MODIFY THIS FILE --\n\n";
    headerStream << "#ifndef __" << getMacroName(headerFileName) << "__\n";
    headerStream << "#define __" << getMacroName(headerFileName) << "__\n\n";

    for (QHashIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        headerStream << "#define ANIM_" << getMacroName(it.value().name) << "\t\t\t\"" << it.value().name << "\"\n";
        headerStream << "#define ANIM_" << getMacroName(it.value().name) << "_FLAGS\t\t\t 0x" << QString::number(it.value().flags, 16) << "\n";
    }
    headerStream << "\n";

    if (!_customHeader.isEmpty()) {
        headerStream << "////////////////////////////////////////////////\n";
        headerStream << "// starting custom header data\n";
        headerStream << _customHeader << "\n";
        headerStream << "// end custom header data\n";
        headerStream << "////////////////////////////////////////////////\n";
        headerStream << "\n";
        headerStream << "\n";
    }

    headerStream << "#endif\n";

    headerOutput.close();

    return true;
}

bool writeTempImage(QString &tmpImgFilename, const QImage &image)
{
    const int IMG_COMPRESSION = 9; // min:0, max:9

    tmpImgFilename = QDir::tempPath() + QDir::separator() + "tmpLvkImg.png";

    if (QFile::exists(tmpImgFilename) && !QFile::remove(tmpImgFilename)) {
        qDebug() << "Could not remove temp file" << tmpImgFilename;
        return false;
    }

    QImageWriter imgWriter(tmpImgFilename, QByteArray("png"));
    imgWriter.setCompression(IMG_COMPRESSION);
    imgWriter.write(image);

    return true;
}

bool runPostprocessingScript(const QString &postpScriptCmd, const QString &inputImg, const QString &outputImg)
{
    const int TIMEOUT_START = 3;
    const int TIMEOUT_FINISH = 30;

    if (QFile::exists(outputImg) && !QFile::remove(outputImg)) {
        qDebug() << "Could not remove temp file" << outputImg;
        return false;
    }

    QString cmdLine =  postpScriptCmd + " " + inputImg + " " + outputImg;

    qDebug() << "Postprocessing script: " << cmdLine;

    QProcess postpScript;
    postpScript.start(cmdLine);
    if (!postpScript.waitForStarted(TIMEOUT_START*1000)) {
        qDebug() << "Could not start postprocessing script" << cmdLine;
        return false;
    }
    if (!postpScript.waitForFinished(TIMEOUT_FINISH*1000)) {
        qDebug() << "Postprocessing script took more than" << TIMEOUT_FINISH << " secs to finish. Aborting.";
        return false;
    }

    return true;
}

bool writePostprocImage(QFile &binOutput, const QString &postprocImgFilename)
{
    QFile postprocImg(postprocImgFilename);
    if (!postprocImg.open(QFile::ReadOnly)) {
        qDebug() << "Could not open postprocessed image" << postprocImgFilename;
        return false;
    }

    binOutput.write(postprocImg.readAll());

    postprocImg.close();

    return true;
}

bool SpriteState::writeImageWithPostprocessing(QFile &binOutput, const LvkFrame &frame, const QString &postpScript) const
{
    // create temp image from frame pixmap data

    std::cout << "Exporting frame " << frame.id << "..." << std::endl;

    QString tmpImgFilename;
    if (!writeTempImage(tmpImgFilename, _fpixmaps[frame.id].toImage())) {
        return false;
    }

    // run post processing script on temp image

    QString postpImgFilename = tmpImgFilename + ".ppi";
    if (!postpScript.isEmpty()) {
        qDebug() << "Postprocessing temp image...";

        if (!runPostprocessingScript(postpScript, tmpImgFilename, postpImgFilename)) {
            std::cout << "Error: Postprocess script '" << postpScript.toStdString()
                      << "' failed. Writing image without postprocessing." << std::endl;
            postpImgFilename = tmpImgFilename;
        }

        qDebug() << "Writing postprocessed image...";
    } else {
        postpImgFilename = tmpImgFilename;
    }

    // write postprocessed image in binOuput

    if (!writePostprocImage(binOutput, postpImgFilename)) {
        return false;
    }

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

bool SpriteState::isFrameUnused(Id frameId) const
{
    bool isUnused = true;

    QHashIterator<Id, LvkAnimation> aniIt(_animations);
    while (aniIt.hasNext() && isUnused) {
        const LvkAnimation &ani = aniIt.next().value();
        QListIterator<LvkAframe> aframeIt(ani._aframes);
        while (aframeIt.hasNext() && isUnused) {
            if (aframeIt.next().frameId == frameId) {
                isUnused = false;
            }
        }
    }

    return isUnused;
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

