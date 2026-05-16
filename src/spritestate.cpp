#include "spritestate.h"
#include "exporters/JsonAtlasExporter.h"

#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QDebug>
#include <QTextStream>
#include <QStringList>
#include <QImageWriter>
#include <QImageReader>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <iostream>
#include <algorithm>
#include <cctype>

// SECURITY (Agent 5): Audit of src/spritestate.cpp - 2026-05-16
// ---------------------------------------------------------------------------
// Findings:
//
//  [FIXED] Shell injection via --postprocessing-script (HIGH severity)
//      runPostprocessingScript() used to concatenate the user-supplied
//      postpScriptCmd with the input/output image paths into one QString and
//      passed it to QProcess::start(const QString&). The single-string
//      overload tokenises by whitespace and (in Qt5/early Qt6) is documented
//      as accepting a "shell-style" command line. A postpScript value
//      containing shell metacharacters (`;`, `|`, `&&`, `$()`, backticks,
//      glob `*`) - or a temp path containing whitespace - would have been
//      interpreted as separate tokens or as additional commands. We now use
//      QProcess::start(program, args) with QProcess::splitCommand() to split
//      ONLY the trusted-shape portion supplied by the operator, and append
//      the temp paths as explicit, opaque positional arguments so they can
//      never be re-parsed as shell tokens.
//
//  [FIXED] Predictable / world-writable temp path (MEDIUM severity)
//      writeTempImage() used a fixed path of QDir::tempPath()/tmpLvkImg.png
//      and {that}.ppi. Both are guessable, deterministic, single-user-hostile
//      (race on /tmp), and the .png + .ppi pair were not unlinked on failure.
//      Now we use QTemporaryFile, which gives randomised unique names, 0600
//      POSIX permissions, and RAII cleanup.
//
//  [FIXED] Script path validation (LOW severity)
//      The script command was invoked even if the program path did not exist
//      or was not executable. We now reject non-executable script paths
//      before spawning a QProcess and surface a clear error.
//
//  [INFO] Other QProcess / system() / popen() sites: NONE.
//      grep'd the rest of spritestate.cpp - the only process spawn is the
//      postprocessing script. mainwindow.cpp, dialogs.cpp, etc. contain only
//      QDialog::exec()/QMessageBox::exec() (modal event loops, not
//      subprocess exec).
//
//  [INFO] QFile::open() return values are checked in save()/load() and the
//      three exportSprite() outputs; no unchecked file-open bugs found.
//
//  [FIXME(agent-5)] exportSprite() writes binFileName/textFileName/headerFileName
//      derived from a user-supplied filename and outputDir without checking
//      that the resolved paths stay inside outputDir. A user (or sprite file
//      with a baseName containing "..") could overwrite arbitrary files the
//      process has rights to. This is low risk for a trusted-developer tool
//      but should be hardened in the Phase 3 refactor (Agent 8 / Agent 10).
//
//  [FIXME(agent-5)] writeImageWithPostprocessing() does not impose a maximum
//      output-image size before writePostprocImage() readAll()s it into
//      memory. A malicious postprocessing script could return a multi-GB
//      file. Acceptable for a trusted-developer tool, but worth capping.
// ---------------------------------------------------------------------------

#define HEADER_VER_01 "LvkSprite version 0.1"
#define HEADER_VER_02 "LvkSprite version 0.2"
#define HEADER_VER_03 "LvkSprite version 0.3"   // New: Custom header section
#define HEADER_VER_04 "LvkSprite version 0.4"   // New: Sticky flag
#define HEADER_LATEST HEADER_VER_04

#define setError(p, err_code) if (p) { *(p) = err_code; }

// Convert string to a new string containing only valid characters for macro names
QString getMacroName(const QString& name)
{
    QString macroName;

    QByteArray a = name.toLatin1();
    for (int i = 0; i < a.size(); ++i)
    {
        char c = a[i];
        if (isalpha(c)) {
            macroName.append(QChar(static_cast<char>(toupper(c))));
        } else if (isdigit(c)) {
            macroName.append(QChar(c));
        } else if (c == ' ' || c == '_' || c == '.') {
            macroName.append(QChar('_'));
        }
    }

    return macroName;
}

SpriteState::SpriteState(QObject* parent)
        : QObject(parent), _imgId(0), _frameId(0), _aniId(0), _aframeId(0)
{
}

const char* SpriteState::headerLiteral(LvkVersion v)
{
    // Single source of truth mapping LvkVersion -> header literal. Used
    // by save() to write the chosen version, by the version-preservation
    // round-trip test to assert on it, and by future consumers.
    switch (v) {
    case LvkVersion::V_01: return HEADER_VER_01;
    case LvkVersion::V_02: return HEADER_VER_02;
    case LvkVersion::V_03: return HEADER_VER_03;
    case LvkVersion::V_04: return HEADER_VER_04;
    }
    // Defensive: an out-of-range LvkVersion is a programming error; we
    // pick the latest so a partly-broken caller still emits a parseable
    // file instead of a corrupted header.
    return HEADER_LATEST;
}

LvkVersion SpriteState::minimumVersion() const
{
    // Start at v0.1 and ratchet up as we encounter data that the older
    // versions cannot represent. Order of checks doesn't matter — the
    // final value is just the max over all data-introduced minimums.
    LvkVersion v = LvkVersion::V_01;

    // sticky was introduced in v0.4. ox/oy on aframes were introduced
    // in v0.2; if either is nonzero we have to bump to at least v0.2.
    for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        const QList<LvkAframe>& aframes = it.value()._aframes;
        for (int i = 0; i < aframes.size(); ++i) {
            const LvkAframe& af = aframes.at(i);
            if (af.sticky) {
                v = std::max(v, LvkVersion::V_04);
            } else if ((af.ox != 0 || af.oy != 0) && v < LvkVersion::V_02) {
                v = std::max(v, LvkVersion::V_02);
            }
        }
        if (v >= LvkVersion::V_04) {
            break; // No higher version exists; short-circuit.
        }
    }

    // animation flags were introduced in v0.3.
    if (v < LvkVersion::V_03) {
        for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
            it.next();
            if (it.value().flags != 0) {
                v = std::max(v, LvkVersion::V_03);
                break;
            }
        }
    }

    // image scale (!= 1.0) was introduced in v0.2.
    if (v < LvkVersion::V_02) {
        for (QMapIterator<Id, InputImage> it(_images); it.hasNext();) {
            it.next();
            if (it.value().scale() != 1.0) {
                v = std::max(v, LvkVersion::V_02);
                break;
            }
        }
    }

    // The custom_header() section was introduced in v0.3. We still emit
    // it for any version on save (it's an open-ended trailer), but if
    // the in-memory document carries one then a v0.1/v0.2 reader cannot
    // parse the resulting file -- so we must bump to v0.3.
    if (v < LvkVersion::V_03 && !_customHeader.isEmpty()) {
        v = std::max(v, LvkVersion::V_03);
    }

    return v;
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
    // Bug #4 (Agent 7): the previous implementation called
    // QList::insert(aframe.id, aframe). QList::insert(int index, T) treats
    // the first argument as a *position*, not a key. With non-dense ids
    // (e.g. mario.lvks uses id 2 in animation 0, then ids 1,5,6,7 in
    // animation 1) the call was OOB / heap-corrupting. Aframe order is
    // already determined by parser/save iteration order, so append is the
    // correct semantics — matches LvkAnimation::addAframe (push_back).
    _animations[aniId]._aframes.append(aframe);
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

    // A cleared SpriteState is logically a fresh document. Match the
    // default-constructed state so a clear()-and-build cycle saves in
    // the latest format (load() will overwrite this immediately if it
    // reads a file).
    _loadedVersion = LvkVersion::V_04;
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

    // Phase 2: preserve the loaded version on save. Only bump if the
    // in-memory data actually uses a feature that the loaded version
    // cannot express (see minimumVersion()). When we do bump, emit a
    // visible note so CLI users / log readers know the file's header
    // moved forward.
    const LvkVersion target = std::max(_loadedVersion, minimumVersion());
    if (target > _loadedVersion) {
        qWarning().noquote()
            << "Note:" << filename
            << "uses features requiring" << headerLiteral(target)
            << "(was" << headerLiteral(_loadedVersion) << ");"
            << "auto-bumping the saved header to preserve data fidelity.";
    }

    QTextStream stream(&file);
    stream << "### LvkSprite #########################################\n";
    stream << headerLiteral(target) << "\n\n";

    stream << "# Images\n";
    // The schema comment matches what the data actually emits, so a
    // hand-eyeball of the on-disk file lines up with the column count.
    if (target < LvkVersion::V_02) {
        stream << "# format: imageId,filename\n";
    } else {
        stream << "# format: imageId,filename,scale\n";
    }
    stream << "images(\n";
    for (QMapIterator<Id, InputImage> it(_images); it.hasNext();) {
        it.next();
        stream << "\t" <<  it.value().toString(target) << "\n";
    }
    stream << ")\n\n";

    stream << "# Frames\n";
    stream << "# format: frameId,name,imageId,ox,oy,w,h\n";
    stream << "frames(\n";
    for (QMapIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
        it.next();
        stream << "\t" << it.value().toString() << "\n";
    }
    stream << ")\n\n";

    stream << "# Animations\n";
    if (target < LvkVersion::V_03) {
        stream << "# format: animationId,name\n";
    } else {
        stream << "# format: animationId,name,flags\n";
    }
    stream << "# Animation frames\n";
    if (target < LvkVersion::V_02) {
        stream << "# format: aframeId,frameId,delay\n";
    } else if (target < LvkVersion::V_04) {
        stream << "# format: aframeId,frameId,delay,ox,oy\n";
    } else {
        stream << "# format: aframeId,frameId,delay,ox,oy,sticky\n";
    }
    stream << "animations(\n";
    for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        stream << "\t" << it.value().toString(target) << "\n";
        stream << "\taframes(\n";
        for (QListIterator<LvkAframe> it2(it.value()._aframes); it2.hasNext();) {
            stream << "\t\t" << it2.next().toString(target) << "\n";
        }
        stream << "\t)\n\n";
    }
    stream << ")\n\n";

    stream << "# Custom data appended to the header\n";
    stream << "custom_header(\n";
    // Bug #5 fix (Agent 8): load() appends "\n" after every header line
    // (see line ~409 in this file), so _customHeader always ends in '\n'
    // when populated by a previous load. Writing `_customHeader << "\n"`
    // then adds a SECOND trailing newline; reload re-appends one per line
    // (including the now-empty trailing line) and the on-disk size of the
    // header block grew by one '\n' per round-trip. Strip trailing
    // whitespace and write exactly one terminating '\n' so the canonical
    // form is a fixed point.
    {
        QString headerOut = _customHeader;
        while (!headerOut.isEmpty() &&
               (headerOut.endsWith(QLatin1Char('\n')) ||
                headerOut.endsWith(QLatin1Char('\r')) ||
                headerOut.endsWith(QLatin1Char(' ')) ||
                headerOut.endsWith(QLatin1Char('\t')))) {
            headerOut.chop(1);
        }
        if (!headerOut.isEmpty()) {
            stream << headerOut << "\n";
        }
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
            // Phase 2: remember the on-disk version so save() can write
            // the file back out in the same version (unless the user
            // introduces newer-only data; see SpriteState::save()).
            if (line == HEADER_VER_01) {
                _loadedVersion = LvkVersion::V_01;
                state = StNoToken;
                continue;
            } else if (line == HEADER_VER_02) {
                _loadedVersion = LvkVersion::V_02;
                state = StNoToken;
                continue;
            } else if (line == HEADER_VER_03) {
                _loadedVersion = LvkVersion::V_03;
                state = StNoToken;
                continue;
            } else if (line == HEADER_VER_04) {
                _loadedVersion = LvkVersion::V_04;
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

    // Preserve legacy playback order: hand-edited / post-delete-saves can
    // have non-sequential aframe ids on disk. addAframe() appends in file
    // order, so we re-sort each animation's aframes by id here. Matches
    // the historical QList::insert(id, ...) semantics where the key was
    // used as a sort position (modulo the OOB bug fixed in Bug #4).
    for (auto it = _animations.begin(); it != _animations.end(); ++it) {
        QList<LvkAframe>& aframes = it.value()._aframes;
        std::sort(aframes.begin(), aframes.end(),
                  [](const LvkAframe& a, const LvkAframe& b) { return a.id < b.id; });
    }

    return (state != StError);
}

// SECURITY (Agent 8 + Phase 4): Validate that @param sourceFilename /
// outputDir produce output paths inside the canonical outputDir.
//
// Phase 4 hardening (vs the original Agent 8 implementation):
//   - validate the FULL last-segment filename (post cleanPath), not just
//     QFileInfo::baseName() which is "everything before the first dot".
//     The legacy baseName check trivially passed
//     `legit...../../../etc/passwd.lvks` because baseName == "legit".
//   - use QDir::canonicalPath() to resolve outputDir symlinks before
//     the containment check; refuse empty canonical (broken symlink or
//     non-existent dir).
//   - append a trailing separator to canonOutDir before startsWith() so
//     that "/tmp/safe-evil/..." cannot prefix-confuse "/tmp/safe".
//   - reject NUL byte and EITHER slash flavor in the filename (Windows
//     attackers can smuggle backslashes through a Linux build's QDir
//     which only treats '/' as a separator).
//
// Rejects:
//   - empty filename, or filename containing '/', '\\', NUL, or "..",
//   - absolute filename,
//   - outputDir that does not exist / cannot canonicalize,
//   - resolved candidate path that escapes canonicalised outputDir.
// Returns true on safe; false on unsafe (with reasonOut optionally set).
static bool isSafeExportPath(const QString& sourceFilename,
                             const QString& outputDir,
                             QString*       reasonOut)
{
    auto fail = [&](const QString& reason) {
        if (reasonOut) *reasonOut = reason;
        qDebug() << "isSafeExportPath: REJECTED -" << reason;
        return false;
    };

    // 1. Reject the raw input for NUL bytes and ".." or alt-separator
    // traversal BEFORE QDir::cleanPath rewrites them away. cleanPath
    // collapses "/tmp/safe/.." to "/tmp", which would then look safe to
    // a naive last-segment check.
    if (sourceFilename.contains(QChar('\0'))) {
        return fail(QStringLiteral("filename contains NUL byte"));
    }
    if (sourceFilename.contains(QStringLiteral(".."))) {
        return fail(QStringLiteral("filename contains '..' traversal: ") + sourceFilename);
    }

    // 2. Extract and validate the final filename component.
    //
    // We deliberately do NOT use QFileInfo::baseName() here: it stops at
    // the first dot, so an attacker filename of
    //   legit...../../../etc/passwd.lvks
    // returns baseName == "legit", a name the old check accepted while the
    // outer "..." segments would still let the operating system traverse.
    //
    // The regex below matches BOTH '/' and '\\' so a Windows-flavored
    // attack ("a\\..\\..\\b") still gets split into its components on a
    // POSIX build (where QDir::separator is '/').
    const QString cleaned = QDir::cleanPath(sourceFilename);
    const QString filenameOnly =
        cleaned.section(QRegularExpression(QStringLiteral("[\\\\/]")), -1,
                        -1, QString::SectionSkipEmpty);

    if (filenameOnly.isEmpty()) {
        return fail(QStringLiteral("filename is empty after cleanPath: ") + sourceFilename);
    }
    if (filenameOnly.contains(QLatin1Char('/')) ||
        filenameOnly.contains(QLatin1Char('\\'))) {
        // Belt-and-braces: the section() above should have stripped these,
        // but a defense-in-depth check is cheap.
        return fail(QStringLiteral("filename contains a path separator: ") + filenameOnly);
    }
    if (filenameOnly == QStringLiteral(".") || filenameOnly == QStringLiteral("..")) {
        return fail(QStringLiteral("filename is a directory traversal: ") + filenameOnly);
    }
    if (QFileInfo(filenameOnly).isAbsolute()) {
        return fail(QStringLiteral("filename is an absolute path: ") + filenameOnly);
    }

    // 2. Canonicalise outputDir, resolving any symlinks. This refuses
    // operator-confusion attacks like "/tmp/safe -> /etc".
    const QString canonicalOutputDir = QDir(outputDir).canonicalPath();
    if (canonicalOutputDir.isEmpty()) {
        return fail(QStringLiteral("could not canonicalize outputDir (missing or broken symlink): ") + outputDir);
    }
    QFileInfo canonOutInfo(canonicalOutputDir);
    if (!canonOutInfo.exists() || !canonOutInfo.isDir()) {
        return fail(QStringLiteral("canonical outputDir does not exist or is not a directory: ")
                    + canonicalOutputDir);
    }

    // 3. Build the candidate output path and verify it stays inside
    // canonicalOutputDir. Always compare against the canonical dir WITH
    // a trailing separator to defeat prefix confusion:
    //   /tmp/safe         vs.  /tmp/safe-evil/x  -> rejected
    // Without the trailing separator the second startsWith() would
    // succeed because "/tmp/safe-evil/x".startsWith("/tmp/safe") is true.
    const QString sep = QDir::separator();
    const QString canonOutWithSep = canonicalOutputDir + sep;
    const QString candidate = QDir::cleanPath(canonicalOutputDir + sep + filenameOnly);
    if (!candidate.startsWith(canonOutWithSep) &&
        candidate != canonicalOutputDir) {
        return fail(QStringLiteral("resolved path escapes outputDir: ") + candidate);
    }
    return true;
}

bool SpriteState::exportSprite(const QString& filename, const QString& outputDir_,
                               const QString &postpScript, ExportFormat format,
                               SpriteStateError* err) const
{
    setError(err, ErrNone);

    QFileInfo fileInfo(filename);

    QString outputDir;
    if (!outputDir_.isEmpty()) {
        outputDir = outputDir_;
    } else {
        outputDir = fileInfo.path();
    }

    // SECURITY (Phase 4): Validate the FULL filename (post-cleanPath) rather
    // than the baseName-before-first-dot legacy behavior. The new
    // isSafeExportPath() does the full-name check; we only fall back to
    // baseName() AFTER the check passes so that the .lkob/.lkot/.h artifact
    // basenames remain identical to the pre-Phase-4 output.
    {
        QString reason;
        if (!isSafeExportPath(filename, outputDir, &reason)) {
            qDebug() << "SpriteState::exportSprite():" << reason;
            setError(err, ErrUnsafeOutputPath);
            return false;
        }
    }
    const QString baseName = fileInfo.baseName();
    if (baseName.isEmpty()) {
        // A filename like "...lvks" has baseName "" -- still trigger the
        // safe-path rejection so we don't open(outputDir + "" + ".lkob")
        // which writes to the directory itself.
        qDebug() << "SpriteState::exportSprite(): baseName is empty for"
                 << filename;
        setError(err, ErrUnsafeOutputPath);
        return false;
    }

    // Dispatch JSON-only exports to the JSON exporter and return early.
    // The Cocos2d path below is the original behavior (preserved for
    // byte-equivalence of the .lkob / .lkot / .h artifacts).
    const bool wantCocos2d = (format & Cocos2d) != 0;
    const bool wantJson    = (format & Json)    != 0;

    if (!wantCocos2d && !wantJson) {
        qDebug() << "SpriteState::exportSprite(): no format flags set";
        return false;
    }

    if (wantJson) {
        JsonAtlasExporter jsonExp;
        const QString jsonBase =
            QDir::cleanPath(QFileInfo(outputDir).canonicalFilePath() +
                            QDir::separator() + baseName);
        if (!jsonExp.exportAtlas(jsonBase, *this)) {
            qDebug() << "SpriteState::exportSprite(): JSON atlas export failed for"
                     << jsonBase;
            setError(err, ErrCantOpenReadWriteMode);
            return false;
        }
        if (!wantCocos2d) {
            return true;
        }
    }

    QString binFileName  = outputDir + QDir::separator() + baseName + ".lkob";
    QString textFileName = outputDir + QDir::separator() + baseName + ".lkot";
    QString headerFileName = outputDir + QDir::separator() + "AnimNameDef_" + baseName + ".h";

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
    textStream << "Exported " HEADER_VER_04 "\n\n";

    textStream << "# Frame Pixmaps\n";
    textStream << "# format: frameId,offset(bytes),length(bytes)\n";
    textStream << "fpixmaps(\n";

    qint64 prevOffset = 0; /* previous offset */
    qint64 offset = 0;

    for (QMapIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
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
    textStream << "# format: aframeId,frameId,delay,ox,oy,sticky\n";
    textStream << "animations(\n";
    for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
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

    QString headerFileMacroName = "__" + getMacroName(headerFileName) + "__";

    QTextStream headerStream(&headerOutput);
    headerStream << "// File autogenerated by " HEADER_LATEST "\n";
    headerStream << "// -- DO NOT EDIT OR MODIFY THIS FILE --\n\n";
    headerStream << "#ifndef " << headerFileMacroName << "\n";
    headerStream << "#define " << headerFileMacroName << "\n\n";

    for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        headerStream << "#define ANIM_" << getMacroName(it.value().name) << "\t\t\t\"" << it.value().name << "\"\n";
        headerStream << "#define ANIM_" << getMacroName(it.value().name) << "_FLAGS\t\t\t 0x" << QString::number(it.value().flags, 16) << "\n";
    }
    headerStream << "\n";

    if (!_customHeader.isEmpty()) {
        headerStream << "////////////////////////////////////////////////\n";
        headerStream << "// starting custom header data\n\n";
        headerStream << _customHeader << "\n";
        headerStream << "// end custom header data\n";
        headerStream << "////////////////////////////////////////////////\n";
        headerStream << "\n";
        headerStream << "\n";
    }

    headerStream << "#endif //" << headerFileMacroName << "\n";

    headerOutput.close();

    return true;
}

// SECURITY (Agent 5): write a frame's pixmap to a unique secure tempfile.
//
// QTemporaryFile gives us:
//   - randomised, unpredictable filename (no /tmp race),
//   - 0600 POSIX permissions by default,
//   - RAII cleanup if we let the object go out of scope without disowning,
//   - no need to QFile::remove() leftovers from a previous crashed run.
//
// We disable autoRemove only because writeImageWithPostprocessing() needs the
// file to outlive this helper so that the postprocessing subprocess can read
// it. The caller MUST remove the file itself; see the RAII guard in
// writeImageWithPostprocessing().
static bool writeTempImage(QString &tmpImgFilename, const QImage &image)
{
    const int IMG_COMPRESSION = 9; // min:0, max:9

    QTemporaryFile tmp(QDir::tempPath() + QDir::separator() + "lvk-frame-XXXXXX.png");
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        qDebug() << "writeTempImage: could not create secure temp file in"
                 << QDir::tempPath();
        return false;
    }
    tmpImgFilename = tmp.fileName();
    tmp.close(); // QImageWriter wants to own the handle

    QImageWriter imgWriter(tmpImgFilename, QByteArray("png"));
    imgWriter.setCompression(IMG_COMPRESSION);
    if (!imgWriter.write(image)) {
        qDebug() << "writeTempImage: failed to encode PNG to" << tmpImgFilename
                 << "-" << imgWriter.errorString();
        QFile::remove(tmpImgFilename);
        tmpImgFilename.clear();
        return false;
    }
    return true;
}

// SECURITY (Agent 5): Validate that a script command resolves to an
// executable file before we let QProcess try to spawn it.
//
// Accepts the user-supplied postpScriptCmd, which may include extra args
// (e.g. "python3 /path/to/script.py --flag"). We only validate the program
// portion (the first token of the tokenised command). If that token is a
// relative or absolute path, it must exist and be executable. If it's a bare
// name (e.g. "convert"), it must resolve via PATH.
static bool resolvePostprocessingProgram(const QString &postpScriptCmd,
                                         QString *resolvedProgram,
                                         QStringList *extraArgs,
                                         QString *errorOut)
{
    const QStringList tokens = QProcess::splitCommand(postpScriptCmd);
    if (tokens.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("postprocessing script command is empty");
        return false;
    }

    const QString program = tokens.first();
    const QStringList args = tokens.mid(1);

    // Step 1: locate the binary on disk.
    //
    // SECURITY (Phase 4): use canonicalFilePath() rather than
    // absoluteFilePath() so the path passed to QProcess::start has
    // already had its symlinks resolved. Without this, a writable
    // symlink dir on PATH (or in the user's chosen script path) is a
    // TOCTOU primitive: between the existence/executable checks below
    // and the QProcess::start() down in runPostprocessingScript() an
    // attacker can swap the symlink target. By snapshotting the
    // canonical path here we pin the file inode (modulo a separate
    // bind-mount or unlink-replace race, which the OS itself would
    // have to surface) for the subsequent QProcess::start call.
    QString resolved;
    QFileInfo fi(program);
    if (program.contains(QDir::separator()) || fi.isAbsolute()) {
        // Path-like: must exist on disk exactly as given.
        if (!fi.exists() || !fi.isFile()) {
            if (errorOut) {
                *errorOut = QStringLiteral("postprocessing script '%1' does not exist").arg(program);
            }
            return false;
        }
        resolved = fi.canonicalFilePath();
        if (resolved.isEmpty()) {
            // canonicalFilePath returns "" if the file doesn't exist or a
            // symlink in the chain is broken. We already proved existence
            // above; an empty result here means a broken symlink.
            if (errorOut) {
                *errorOut = QStringLiteral("postprocessing script '%1' has a broken symlink chain").arg(program);
            }
            return false;
        }
    } else {
        // Bare name: search PATH, then canonicalise.
        const QString pathResolved = QStandardPaths::findExecutable(program);
        if (pathResolved.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("postprocessing script '%1' not found on PATH").arg(program);
            }
            return false;
        }
        resolved = QFileInfo(pathResolved).canonicalFilePath();
        if (resolved.isEmpty()) {
            // PATH lookup found something, but it canonicalises to nothing
            // (broken symlink). Reject rather than execute the dangling
            // target.
            if (errorOut) {
                *errorOut = QStringLiteral("postprocessing script '%1' has a broken symlink chain on PATH").arg(program);
            }
            return false;
        }
    }

    // Step 2: must be executable (POSIX) - on Windows isExecutable defers to extension.
    QFileInfo resolvedInfo(resolved);
    if (!resolvedInfo.isExecutable()) {
        if (errorOut) {
            *errorOut = QStringLiteral("postprocessing script '%1' is not executable").arg(resolved);
        }
        return false;
    }

    if (resolvedProgram) *resolvedProgram = resolved;
    if (extraArgs)       *extraArgs       = args;
    return true;
}

// SECURITY (Agent 5): Spawn the postprocessing script with explicit argv,
// never a shell-style joined string.
//
// Returns true iff the script ran AND exited successfully (exit code 0,
// normal exit).
static bool runPostprocessingScript(const QString &postpScriptCmd,
                                    const QString &inputImg,
                                    const QString &outputImg)
{
    const int TIMEOUT_START  = 3;
    const int TIMEOUT_FINISH = 30;

    if (QFile::exists(outputImg) && !QFile::remove(outputImg)) {
        qDebug() << "runPostprocessingScript: could not remove stale output"
                 << outputImg;
        return false;
    }

    QString program;
    QStringList args;
    QString validationError;
    if (!resolvePostprocessingProgram(postpScriptCmd, &program, &args, &validationError)) {
        qDebug() << "runPostprocessingScript:" << validationError;
        return false;
    }

    // Append the two image paths as explicit positional arguments. They are
    // NEVER subjected to shell parsing -- QProcess passes argv straight to
    // execve() on POSIX (and a CreateProcess()-with-quoting on Windows).
    args << inputImg << outputImg;

    qDebug() << "runPostprocessingScript: program=" << program
             << " argc=" << args.size();

    QProcess postpScript;
    postpScript.start(program, args);
    if (!postpScript.waitForStarted(TIMEOUT_START * 1000)) {
        qDebug() << "Could not start postprocessing script" << program;
        return false;
    }
    if (!postpScript.waitForFinished(TIMEOUT_FINISH * 1000)) {
        qDebug() << "Postprocessing script took more than" << TIMEOUT_FINISH
                 << "secs to finish. Aborting.";
        postpScript.kill();
        postpScript.waitForFinished(1000);
        return false;
    }
    if (postpScript.exitStatus() != QProcess::NormalExit
        || postpScript.exitCode() != 0)
    {
        qDebug() << "Postprocessing script exited with status="
                 << postpScript.exitStatus()
                 << "code=" << postpScript.exitCode();
        return false;
    }

    return true;
}

static bool writePostprocImage(QFile &binOutput, const QString &postprocImgFilename)
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

    // RAII cleanup of tmp files even on early return / exception paths.
    // (QTemporaryFile would auto-clean if we'd kept the object alive, but
    // we need the file to outlive the helper so the postprocessing process
    // can read it - so we clean explicitly here.)
    struct TempCleanup {
        QString a, b;
        ~TempCleanup() {
            if (!a.isEmpty()) QFile::remove(a);
            if (!b.isEmpty()) QFile::remove(b);
        }
    };
    TempCleanup cleanup{tmpImgFilename, QString()};

    // run post processing script on temp image
    //
    // SECURITY (Phase 4): The legacy code derived the postprocessed-image
    // path by string-appending ".ppi" to the input tempfile -- which made
    // it 100% predictable (and trivial for a co-located attacker to
    // pre-create / symlink). Use QTemporaryFile to get an OS-chosen unique
    // path with 0600 perms instead. setAutoRemove(false) because the path
    // has to outlive this QTemporaryFile (the subprocess will write it,
    // we'll read it back, and the surrounding TempCleanup will unlink it).
    QString postpImgFilename;
    {
        const QString tmplDir = QStandardPaths::writableLocation(
                                    QStandardPaths::TempLocation);
        QTemporaryFile postpTmp(tmplDir + QDir::separator()
                                + QStringLiteral("lvk-export-XXXXXX.ppi"));
        postpTmp.setAutoRemove(false);
        if (!postpTmp.open()) {
            qDebug() << "writeImageWithPostprocessing: could not create secure "
                     << "postp tempfile in" << tmplDir;
            return false;
        }
        postpImgFilename = postpTmp.fileName();
        postpTmp.close();
    }
    cleanup.b = postpImgFilename;

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
    for (QMutableMapIterator<Id, InputImage> it(_images); it.hasNext();) {
        it.next();
        reloadImagePixmap(it.value().id);
    }
}

void SpriteState::reloadFramePixmaps(Id imgId)
{
    for (QMapIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
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

    QMapIterator<Id, LvkAnimation> aniIt(_animations);
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
    static const QString strErrUnsafeOutputPath     = tr("Output path escapes the destination directory");
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
    case ErrUnsafeOutputPath:
        return strErrUnsafeOutputPath;
    default:
        return strErrUnknown;
    }
}

SpriteState::ExportFormat SpriteState::parseFormat(const QString& s)
{
    const QString v = s.trimmed().toLower();
    if (v == QStringLiteral("json"))    return Json;
    if (v == QStringLiteral("all"))     return All;
    // "cocos2d" or unknown -> Cocos2d (legacy default).
    return Cocos2d;
}

