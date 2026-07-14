#include "spritestate.h"
#include "exporters/JsonAtlasExporter.h"
#include "image_validation.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>
#include <algorithm>
#include <cctype>
#include <iostream>

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
//  [RESOLVED(agent-8/phase-4)] exportSprite() output paths are validated by
//      isSafeExportPath() (canonicalised containment check, '..'-segment and
//      NUL rejection, prefix-confusion defense). Covered by
//      tests/format/tst_path_traversal.cpp and
//      tests/security/tst_path_traversal_hard.cpp.
//
//  [RESOLVED(round-7)] writePostprocImage() now caps the postprocessed
//      output image at 256 MB (matching the QImageReader allocation limit
//      set in main.cpp) before readAll()ing it into memory.
// ---------------------------------------------------------------------------

#define HEADER_VER_01 "LvkSprite version 0.1"
#define HEADER_VER_02 "LvkSprite version 0.2"
#define HEADER_VER_03 "LvkSprite version 0.3" // New: Custom header section
#define HEADER_VER_04 "LvkSprite version 0.4" // New: Sticky flag
#define HEADER_LATEST HEADER_VER_04

#define setError(p, err_code)                                                                      \
    if (p) {                                                                                       \
        *(p) = err_code;                                                                           \
    }

// Convert string to a new string containing only valid characters for macro names.
//
// Phase 6b (Item 26): Replaces the old toLatin1()+isalpha()/isdigit() pipeline
// which silently dropped or mangled non-ASCII letters (e.g. "salto-mortal_é",
// "飛び蹴り") and produced empty / colliding `#define ANIM_` lines that failed
// to compile downstream. The new implementation:
//
//   * Walks the QString as Unicode codepoints (not Latin-1 bytes).
//   * Uses QChar::isLetterOrNumber() to classify letters/digits across scripts.
//   * Collapses every non-ASCII letter, digit, or separator to a single '_'
//     so the output is always a valid C identifier (ASCII-only, no leading
//     digit, no embedded whitespace, no doubled underscores).
//   * Returns "UNNAMED" for a name that sanitizes to the empty string, so the
//     emitted `#define ANIM_` line is never empty.
//
// Collision disambiguation (e.g. two animations whose names both collapse to
// "FLY") is the caller's responsibility -- see the export loop in
// exportSprite() which appends "_2", "_3", ... suffixes.
static QString getMacroName(const QString &name) {
    QString clean;
    clean.reserve(name.size());
    for (QChar ch : name) {
        const ushort u = ch.unicode();
        if (u < 128) {
            // ASCII fast path: preserve [A-Za-z0-9_], uppercase letters,
            // collapse everything else (space, dot, punctuation) to '_'.
            const char c = static_cast<char>(u);
            if (isalpha(static_cast<unsigned char>(c))) {
                clean.append(QChar(static_cast<char>(toupper(c))));
            } else if (isdigit(static_cast<unsigned char>(c))) {
                clean.append(ch);
            } else {
                clean.append(QLatin1Char('_'));
            }
        } else {
            // Non-ASCII codepoint: any letter/digit (Japanese, Cyrillic,
            // accented Latin, ...) collapses to a single '_'. This keeps the
            // macro ASCII-only and a valid C identifier; collisions are
            // handled by the caller.
            clean.append(QLatin1Char('_'));
        }
    }

    // De-duplicate runs of '_' to keep the output readable and avoid trivial
    // collisions between names that differ only in punctuation runs.
    QString collapsed;
    collapsed.reserve(clean.size());
    QChar prev;
    for (QChar ch : clean) {
        if (ch == QLatin1Char('_') && prev == QLatin1Char('_')) {
            continue;
        }
        collapsed.append(ch);
        prev = ch;
    }

    // A leading digit makes the result an invalid C identifier; prefix '_'.
    if (!collapsed.isEmpty() && collapsed.at(0).isDigit()) {
        collapsed.prepend(QLatin1Char('_'));
    }

    if (collapsed.isEmpty() || collapsed == QLatin1String("_")) {
        return QStringLiteral("UNNAMED");
    }
    return collapsed;
}

SpriteState::SpriteState(QObject *parent)
    : QObject(parent), _imgId(0), _frameId(0), _aniId(0), _aframeId(0) {}

const char *SpriteState::headerLiteral(LvkVersion v) {
    // Single source of truth mapping LvkVersion -> header literal. Used
    // by save() to write the chosen version, by the version-preservation
    // round-trip test to assert on it, and by future consumers.
    switch (v) {
    case LvkVersion::V_01:
        return HEADER_VER_01;
    case LvkVersion::V_02:
        return HEADER_VER_02;
    case LvkVersion::V_03:
        return HEADER_VER_03;
    case LvkVersion::V_04:
        return HEADER_VER_04;
    }
    // Defensive: an out-of-range LvkVersion is a programming error; we
    // pick the latest so a partly-broken caller still emits a parseable
    // file instead of a corrupted header.
    return HEADER_LATEST;
}

LvkVersion SpriteState::minimumVersion() const {
    // Start at v0.1 and ratchet up as we encounter data that the older
    // versions cannot represent. Order of checks doesn't matter — the
    // final value is just the max over all data-introduced minimums.
    LvkVersion v = LvkVersion::V_01;

    // sticky was introduced in v0.4.
    //
    // Phase 2 revisited: ox/oy on aframes are NOT a v0.2-distinguishing
    // feature. The on-disk parser accepts 5-field aframes under any
    // header (LvkAframe::fromString does not gate on version), and
    // examples/mario.lvks ships as "LvkSprite version 0.1" with three
    // aframes carrying nonzero ox/oy values. The "v0.2 added ox/oy"
    // story was inferred from a misleading row in docs/lvks-format.md's
    // field-count table; the format-header summary at lvks-format.md:68
    // is authoritative ("no on-disk field change" for v0.2). Treating
    // nonzero ox/oy as a v0.2 marker would silently rewrite every v0.1
    // file containing offsets to a v0.2 header on save — exactly the
    // headline regression Phase 2 promised to stop. So we only bump
    // for sticky (a true v0.4-only field).
    for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        const QList<LvkAframe> &aframes = it.value()._aframes;
        for (int i = 0; i < aframes.size(); ++i) {
            const LvkAframe &af = aframes.at(i);
            if (af.sticky) {
                v = std::max(v, LvkVersion::V_04);
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

void SpriteState::addImage(InputImage &img) {
    if (img.id == NullId) {
        img.id = _imgId++;
    } else {
        _imgId = std::max(_imgId, img.id + 1);
    }
    _images.insert(img.id, img);
}

void SpriteState::addFrame(LvkFrame &frame) {
    if (frame.id == NullId) {
        frame.id = _frameId++;
    } else {
        _frameId = std::max(_frameId, frame.id + 1);
    }
    reloadFramePixmap(frame);
    _frames.insert(frame.id, frame);
}

void SpriteState::addAnimation(LvkAnimation &ani) {
    if (ani.id == NullId) {
        ani.id = _aniId++;
    } else {
        _aniId = std::max(_aniId, ani.id + 1);
    }
    _animations.insert(ani.id, ani);
}

void SpriteState::addAframe(LvkAframe &aframe, Id aniId) {
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

void SpriteState::insertAframe(const LvkAframe &aframe, Id aniId, int index) {
    const auto it = _animations.find(aniId);
    if (it == _animations.end()) {
        qWarning("SpriteState::insertAframe: animation %d not found; insert ignored", aniId);
        return;
    }
    _aframeId = std::max(_aframeId, aframe.id + 1);
    QList<LvkAframe> &aframes = it.value()._aframes;
    if (index < 0 || index > aframes.size()) {
        index = aframes.size(); // out-of-range position degrades to append
    }
    aframes.insert(index, aframe);
}

void SpriteState::clear() {
    _imgId = 0;
    _frameId = 0;
    _aniId = 0;
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

    // H4.3: a fresh document has never seen a `transitions(...)` block,
    // so save() must NOT emit the "intentionally dropped" comment.
    // load() will set this back to true if the source file contained
    // one.
    _loadedTransitions = false;

    // J3.1: forget the previously-loaded source path so Save-As of a
    // brand-new (post-clear) document doesn't inherit Save semantics
    // from a stale identity.
    _loadFilename.clear();
}

bool SpriteState::save(const QString &filename, SpriteStateError *err) {
    setError(err, ErrNone);

    // Team F1 (F1.1): atomic-save via Qt's purpose-built QSaveFile.
    //
    // History:
    //   Pre-D2 used QFile::open(filename, WriteOnly|Text), which truncated
    //   the original file BEFORE the empty-record check; a failed save
    //   (e.g. an image filename with a comma) destroyed the user's data.
    //   D2.2 introduced a hand-rolled "<filename>.save-tmp" + remove+rename
    //   pattern. This was atomic on POSIX (rename(2)) but NOT on Windows
    //   (QFile::rename refuses to clobber, so the sequence was
    //   remove(filename); rename(tmp, filename) -- the remove() succeeded
    //   then the rename failed under AV-hold/disk-full, leaving BOTH files
    //   gone). The fixed ".save-tmp" suffix also collided with concurrent
    //   saves of the same file.
    //
    // QSaveFile fixes all three:
    //   - commit() uses POSIX rename(2) on Linux/macOS and on Windows uses
    //     ReplaceFile / MoveFileEx(MOVEFILE_REPLACE_EXISTING) -- both are
    //     OS-level atomic replace operations (no remove-then-rename gap).
    //   - tmp filename is randomised internally so concurrent saves cannot
    //     collide on the temp path.
    //   - cancelWriting()+commit() removes the tmp; the original is never
    //     touched.
    //   - the destructor of QSaveFile rolls back automatically if commit()
    //     is never called (safety net on early-return / exception paths).
    //
    // Team H2 (H2.1): QSaveFile's atomic rename(2) on the original path
    // would REPLACE a symlink at @p filename with a regular file at the
    // link's own inode, silently breaking the link's intent (the user
    // pointed save() at the link expecting the TARGET to receive the
    // bytes). Resolve symlinks ourselves before constructing QSaveFile
    // so the atomic write lands at the link's destination. A broken
    // symlink (target missing or unresolvable) falls back to the
    // original path so the failure surfaces as a normal open() error
    // rather than a mysterious write to "".
    //
    // Team J1 (J1.1): use canonicalFilePath() rather than symLinkTarget()
    // so a CHAIN of symlinks (entry -> proxy -> real) resolves to the
    // final real target, not just one hop. symLinkTarget() would return
    // "proxy" and we'd then atomic-replace the proxy link with a regular
    // file -- the real.lvks would never be written and the proxy link
    // would silently disappear. canonicalFilePath() walks the whole chain
    // and returns "" for broken/dangling links, in which case we keep
    // the original filename so open() fails visibly.
    QString targetPath = filename;
    QFileInfo fileInfo(filename);
    if (fileInfo.isSymLink()) {
        // canonicalFilePath() resolves the FULL chain (entry -> proxy
        // -> real), not just one hop. symLinkTarget() would leave us
        // replacing the proxy.
        const QString canonical = fileInfo.canonicalFilePath();
        if (!canonical.isEmpty()) {
            targetPath = canonical;
        }
        // else: broken symlink -- write to the symlink location so
        // open() fails visibly instead of writing to an empty path.
    }
    QSaveFile file(targetPath);

    // Team H2 (H2.2): allow degradation to a non-atomic write on
    // filesystems that can't honour rename(2)/ReplaceFile atomicity
    // (FAT32, SMB, sshfs, some overlay mounts). Without this the
    // commit() returns false on those filesystems even though the user
    // clearly wants the bytes on disk -- their workflow is just "save
    // my work" and they don't care about crash-consistent renames.
    // Trade-off: on those filesystems a crash between the open and the
    // commit may leave a partially-written file at the destination;
    // on POSIX/NTFS the atomic-replace path is taken and the original
    // guarantee holds. Best-effort atomicity beats hard-failure when
    // the user has no other choice of storage.
    file.setDirectWriteFallback(true);

    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() << "Error: SpriteState::save(): could not open" << filename << "in rw mode";
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
        qWarning().noquote() << "Note:" << filename << "uses features requiring"
                             << headerLiteral(target) << "(was" << headerLiteral(_loadedVersion)
                             << ");" << "auto-bumping the saved header to preserve data fidelity.";
    }

    // Phase 6b follow-up: LvkAnimation/LvkFrame/InputImage::toString()
    // refuse to serialize a name containing ',' or NUL by returning an
    // empty QString. The legacy save loop then wrote a bare "\t\n" line
    // where the record should be, the reload step's fromString("") then
    // failed and skipped the record, save() still returned true, and the
    // user got silent data loss. Detect the empty-record case BEFORE
    // committing anything to disk, surface a qWarning, and return false
    // from save() with ErrInvalidFormat so the caller can prompt the
    // user. Note: we still write any records that came BEFORE the bad
    // one (the QTextStream has already flushed them), but the resulting
    // file will be left truncated and save() returns false -- the
    // caller's standard "save failed" UX (overwrite-on-retry) handles
    // cleanup. The key invariant is that save() NEVER returns true on
    // a file that drops records.
    bool sawInvalidRecord = false;

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
        const QString rec = it.value().toString(target);
        if (rec.isEmpty()) {
            qWarning() << "SpriteState::save(): refusing to save image record with invalid"
                       << "filename (comma or NUL):" << it.value().filename
                       << "id=" << it.value().id;
            sawInvalidRecord = true;
            continue;
        }
        stream << "\t" << rec << "\n";
    }
    stream << ")\n\n";

    stream << "# Frames\n";
    stream << "# format: frameId,name,imageId,ox,oy,w,h\n";
    stream << "frames(\n";
    for (QMapIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
        it.next();
        const QString rec = it.value().toString();
        if (rec.isEmpty()) {
            qWarning() << "SpriteState::save(): refusing to save frame record with invalid"
                       << "name (comma or NUL):" << it.value().name << "id=" << it.value().id;
            sawInvalidRecord = true;
            continue;
        }
        stream << "\t" << rec << "\n";
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
    // Phase B1.3: sort aframes by id at SAVE time (instead of LOAD time)
    // so the on-disk file is byte-equivalent on round-trip for arbitrary
    // input orderings. User-added aframes get normalized into id-order on
    // save; in-memory order during a session matches the order the user
    // built them (append semantics).
    for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        const QString rec = it.value().toString(target);
        if (rec.isEmpty()) {
            qWarning() << "SpriteState::save(): refusing to save animation record with invalid"
                       << "name (comma or NUL):" << it.value().name << "id=" << it.value().id;
            sawInvalidRecord = true;
            // We still need to emit the aframes block for this animation,
            // but doing so without a header line would corrupt the parser
            // state. Skip both the animation and its aframes; save() will
            // return false so the file is treated as invalid anyway.
            continue;
        }
        stream << "\t" << rec << "\n";
        stream << "\taframes(\n";
        QList<LvkAframe> sortedAframes = it.value()._aframes;
        std::sort(sortedAframes.begin(), sortedAframes.end(),
                  [](const LvkAframe &a, const LvkAframe &b) { return a.id < b.id; });
        for (QListIterator<LvkAframe> it2(sortedAframes); it2.hasNext();) {
            // Aframes have no name field, so toString never returns
            // empty for valid data -- but keep the defensive check
            // symmetric with the other records.
            const LvkAframe &af = it2.next();
            const QString arec = af.toString(target);
            if (arec.isEmpty()) {
                qWarning() << "SpriteState::save(): refusing to save aframe record"
                           << "id=" << af.id;
                sawInvalidRecord = true;
                continue;
            }
            stream << "\t\t" << arec << "\n";
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
               (headerOut.endsWith(QLatin1Char('\n')) || headerOut.endsWith(QLatin1Char('\r')) ||
                headerOut.endsWith(QLatin1Char(' ')) || headerOut.endsWith(QLatin1Char('\t')))) {
            headerOut.chop(1);
        }
        if (!headerOut.isEmpty()) {
            stream << headerOut << "\n";
        }
    }
    stream << ")\n\n";

    // F5.2 + H4.3: emit a visible breadcrumb explaining that the
    // `transitions()` block reserved in the format spec is not yet
    // implemented -- BUT only when the loaded file actually contained
    // such a block (the load() path sets _loadedTransitions in that
    // case). Without the gate, the comment was emitted on every save(),
    // so a v0.1 file (e.g. mario.lvks) that never had transitions grew
    // a noise line per round-trip cycle. The user-visible value of the
    // breadcrumb is "where did the transitions data go?" -- there's no
    // payoff in stamping that on files that never had any.
    //
    // J3.1: additional gate -- the breadcrumb is meaningful only for an
    // OVERWRITE of the same source file (the user saved over the file
    // that originally contained the dropped block). For a Save-As to a
    // different path the destination is a brand-new derived document
    // that never carried a transitions block of its own, so emitting
    // the comment would mislead a reader of the new file into thinking
    // *that* file once had transitions. Compare the canonical save path
    // against the canonical load path; only when they refer to the same
    // file do we inherit the flag.
    bool emitTransitionsBreadcrumb = false;
    if (_loadedTransitions && !_loadFilename.isEmpty()) {
        const QString saveCanon = QFileInfo(filename).absoluteFilePath();
        emitTransitionsBreadcrumb = (saveCanon == _loadFilename);
    }
    if (emitTransitionsBreadcrumb) {
        stream << "# transitions intentionally dropped -- feature not implemented\n\n";
    }

    stream << "### End LvkSprite #####################################\n";

    // Flush the stream so QSaveFile sees all bytes before commit/cancel.
    stream.flush();

    if (sawInvalidRecord) {
        // Team F1 (F1.1): discard the staged write; the original file at
        // @p filename is untouched. We still call commit() so QSaveFile
        // cleans up its randomised tmp -- per Qt docs, after
        // cancelWriting() the subsequent commit() returns true without
        // touching the destination and removes the tmp file.
        file.cancelWriting();
        file.commit();
        setError(err, ErrInvalidFormat);
        return false;
    }

    // Team F1 (F1.1): atomic publish. On POSIX this is rename(2); on
    // Windows it's ReplaceFile / MoveFileEx with MOVEFILE_REPLACE_EXISTING
    // -- both are OS-atomic, so there is no observable moment in which
    // the destination is missing or partially written. Failure paths
    // (full disk, AV hold, perms revoked between open and commit) leave
    // the original file intact; QSaveFile removes its tmp automatically.
    if (!file.commit()) {
        qDebug() << "Error: SpriteState::save(): commit() failed for" << filename << "-"
                 << file.errorString();
        setError(err, ErrCantOpenReadWriteMode);
        return false;
    }
    return true;
}

bool SpriteState::load(const QString &filename, SpriteStateError *err, int *rejectedCount) {
    setError(err, ErrNone);
    if (rejectedCount) {
        *rejectedCount = 0;
    }

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
        qDebug() << "Error: SpriteState::load(): could not open" << filename << "in ro mode";
        setError(err, ErrCantOpenReadMode);
        return false;
    }

    // Team F2 (F2.2): resolve image-record filenames against the
    // .lvks file's own directory, not the process CWD. The previous
    // call site fed tmpImage.filename verbatim to QFileInfo / the
    // validator, which used QFileInfo(relPath).exists() -- a CWD-
    // relative resolution. That made load() non-deterministic
    // across invocations (a relative "evil.png" sitting in CWD
    // would be sniffed instead of the sibling-to-the-.lvks file
    // the format actually means), and broke portability of .lvks
    // files between users with different working directories.
    //
    // We compute the absolute base directory once, here, and pass
    // resolved absolute paths into validateImageFile from now on.
    const QString spriteFileDir = QFileInfo(filename).absolutePath();

    clear();

    enum {
        StCheckVersion = 0,
        StNoToken = 1,
        StTokenImages = 2,
        StTokenFrames = 3,
        StTokenAnimations = 4,
        StTokenAframes = 5,
        StTokenHeader = 6,
        // F5.2: the .lvks format reserves a `transitions(...)` block for a
        // feature that is not yet implemented. The UI button is disabled
        // (D4), but a hand-edited file or a forward-compatible producer
        // could still include such a block. Previously the loader hit
        // StNoToken's "Unknown token" branch and failed the whole load
        // with ErrInvalidFormat. We now recognise the block, warn the
        // operator that the data is being dropped, and consume lines
        // until the matching ')'. save() emits a corresponding comment
        // so the drop is visible end-to-end.
        StTokenTransitions = 7,
        StError = 999,
    } state = StCheckVersion;

    QTextStream stream(&file);
    QString line;
    QStringList tokens;

    int lineNumber = -1;

    InputImage tmpImage;
    LvkFrame tmpFrame;
    LvkAnimation tmpAni;
    LvkAframe tmpAframe;

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
                qDebug() << "Error: SpriteState::load(): Invalid LvkSprite file format" << "at line"
                         << lineNumber;
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
            } else if (line == "transitions(") {
                // F5.2: transitions are not yet implemented. Warn loudly
                // so a silent drop is visible on stderr, then skip the
                // block (StTokenTransitions consumes lines until ')').
                // save() emits a comment so the user sees the drop in the
                // round-tripped file as well.
                //
                // H4.3: record that we saw a transitions block so save()
                // gates the "intentionally dropped" breadcrumb on it.
                // Without the gate the comment was emitted on every
                // save() -- including round-trips of files that never
                // had the block -- and grew the on-disk text by a noise
                // line each cycle.
                qWarning() << "SpriteState::load(): transitions() block found in" << filename
                           << "at line" << lineNumber
                           << "but transitions are not yet implemented; "
                              "block contents will be dropped on save.";
                _loadedTransitions = true;
                // J3.2: bump rejectedCount so the GUI surfaces the drop via
                // the same status-bar + infoDialog plumbing that already
                // fires for rejected image/frame records (see
                // MainWindow::openFile_). Pre-J3 the warning was silent at
                // stderr only -- a transitions-bearing file appeared to
                // load cleanly in the editor and the user had no signal
                // that data was lost. The headless CLI's --export exit
                // code (non-zero on rejectedCount > 0) also picks this up.
                if (rejectedCount) {
                    ++(*rejectedCount);
                }
                state = StTokenTransitions;
            } else if (line == "aframes(") {
                qDebug() << "Error: SpriteState::load(): Unspected token" << line << "at line"
                         << lineNumber;
                setError(err, ErrInvalidFormat);
                state = StError;
            } else {
                qDebug() << "Error: SpriteState::load(): Unknown token" << line << "at line"
                         << lineNumber;
                setError(err, ErrInvalidFormat);
                state = StError;
            }
            break;

        case StTokenImages:
            if (line == ")") {
                state = StNoToken;
            } else {
                if (tmpImage.fromString(line)) {
                    // Team D2 (D2.1) + Team F1 (F1.2): enforce the GUI
                    // dialog's image-format whitelist on load. Without
                    // this, a malicious .lvks with a record like
                    //     0,evil.eps,1
                    // survives InputImage::fromString's isSafeImagePath
                    // check (no NUL, no "..", clean relative path), gets
                    // inserted into state, and is rendered unchecked by
                    // the controller's refreshTable -- the "advisory
                    // dialog" hole B3 closed in the dialog path remained
                    // open through the load path.
                    //
                    // F1.2 closes a follow-up TOCTOU: the pre-F1 loader
                    // skipped the whitelist whenever the file was
                    // ABSENT, so a .lvks referencing "evil.eps" (file
                    // not on disk at load time) admitted the record;
                    // an attacker who then dropped evil.eps got it
                    // decoded on the next refresh. ExistenceCheck::
                    // Optional now gates on the extension even when the
                    // file is absent, while still tolerating the
                    // legitimate broken-asset-link case (a .lvks
                    // referencing "nonexistent.png" still admits the
                    // record -- only the pixmap will be null).
                    //
                    // Rejected records are SKIPPED (we don't abort the
                    // whole load) and a counter is bumped that the
                    // headless CLI inspects in D2.3 to surface a
                    // non-zero exit code.
                    // Team F1 + F2 merged: resolve filename against the
                    // .lvks file's directory (F2.2: CWD-independent), then
                    // call the validator with ExistenceCheck::Optional
                    // (F1.2: closes the TOCTOU bypass — a malicious .lvks
                    // referencing evil.eps that doesn't exist at load
                    // time is rejected on extension alone). The
                    // validator transparently falls back to extension-
                    // only when the file is absent, so a broken-asset
                    // .lvks with a missing `.png` is still admitted
                    // (the pixmap will be null but the record survives).
                    if (!tmpImage.filename.isEmpty()) {
                        const QString absImgPath = QDir(spriteFileDir)
                                                       .absoluteFilePath(tmpImage.filename);
                        QString errMsg;
                        if (!lvk::validateImageFile(absImgPath, &errMsg,
                                                    lvk::ExistenceCheck::Optional)) {
                            qWarning() << "SpriteState::load(): rejected image record"
                                       << "(format whitelist):" << errMsg;
                            if (rejectedCount) {
                                ++(*rejectedCount);
                            }
                            break; // skip this record, keep loading
                        }
                    }
                    emit(loadProgress(tr("Image ") + tmpImage.filename));
                    addImage(tmpImage);
                } else {
                    qDebug() << "Error: SpriteState::load(): invalid image entry" << line
                             << "at line" << lineNumber;
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
                    // Team F2 (F2.3): a bad frame record (e.g. out-of-bounds
                    // w/h flagged by LvkFrame::fromString's Phase 4 cap) used
                    // to abort the entire load with ErrInvalidFormat. That's
                    // inconsistent with the image-record path, which skips
                    // and counts (see StTokenImages above) so partial-load
                    // surfaces to the user via rejectedCount. Treat frame
                    // rejections the same way: warn, bump the counter, and
                    // keep parsing. The user gets a sprite missing the bad
                    // frame plus a banner / non-zero CLI exit; the alternative
                    // (silent abort + ErrInvalidFormat for the whole file) is
                    // strictly worse for the "one tampered record in a
                    // 1000-frame sprite" case.
                    qWarning() << "SpriteState::load(): rejected frame entry" << line
                               << "at line" << lineNumber;
                    if (rejectedCount) {
                        ++(*rejectedCount);
                    }
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
                    qDebug() << "Error: SpriteState::load(): null animation id" << "at line"
                             << lineNumber;
                    setError(err, ErrInvalidFormat);
                    state = StError;
                }
            } else {
                if (tmpAni.fromString(line)) {
                    currentAniId = tmpAni.id;
                    addAnimation(tmpAni);
                    emit(loadProgress(tr("Animation ") + tmpAni.name));
                } else {
                    qDebug() << "Error: SpriteState::load(): invalid animation entry" << line
                             << "at line" << lineNumber;
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
                    qDebug() << "Error: SpriteState::load(): invalid aframe entry" << line
                             << "at line" << lineNumber;
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

        case StTokenTransitions:
            // F5.2: drop transitions content until we see the closing
            // ')'. We do NOT preserve the content -- see save() for the
            // corresponding "feature not implemented" comment that tells
            // a user inspecting the file in a text editor what happened.
            if (line == ")") {
                state = StNoToken;
            }
            break;

        default:
            qDebug() << "Warning: SpriteState::load(): Unhandled state " << (int)state << "at line"
                     << lineNumber;
            break;
        }
    } while (true);

    file.close();

    // Reaching EOF in any state other than StNoToken means the file is
    // truncated or was never a .lvks at all:
    //   - StCheckVersion: no "LvkSprite version" header was seen (covers
    //     empty and completely foreign files),
    //   - any StToken*: an images(/frames(/animations(/aframes(/
    //     custom_header( block was never closed (file cut mid-section).
    // Accepting these silently produced "successful" loads of partial
    // data -- and a headless --export then wrote empty/partial artifacts
    // with exit code 0.
    if (state != StError && state != StNoToken) {
        qWarning() << "SpriteState::load(): unexpected end of file (truncated or invalid"
                   << ".lvks), parser state" << (int)state << "at line" << lineNumber << "of"
                   << filename;
        setError(err, ErrInvalidFormat);
        state = StError;
    }

    // Phase B1.3: aframes are now sorted by id at SAVE time, not LOAD
    // time. The previous post-load sort here caused round-trip byte
    // changes for files where on-disk aframe order was not sequential by
    // id (since save() iterates the in-memory list as-is). Moving the
    // sort to save() means:
    //   - In-memory order during a session reflects the file order (or
    //     the user's append order for newly created aframes), which is
    //     intuitive for the editor.
    //   - The on-disk form is canonical: a load->save cycle is a fixed
    //     point because save normalizes to id-order before writing.
    //   - The legacy playback ordering claim (id-as-position) is
    //     preserved: the file written by save() is in id-order, and the
    //     loader appends in file order, so a freshly-loaded sprite has
    //     id-ordered aframes -- exactly what the legacy code achieved
    //     by sorting on load.

    // J3.1: record the canonical source path for the Save-As detector
    // in save(). Only do this on a successful parse so a half-loaded
    // file (StError above) does not poison the next save's "is this
    // the same file?" check.
    if (state != StError) {
        _loadFilename = QFileInfo(filename).absoluteFilePath();
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
static bool isSafeExportPath(const QString &sourceFilename, const QString &outputDir,
                             QString *reasonOut) {
    auto fail = [&](const QString &reason) {
        if (reasonOut)
            *reasonOut = reason;
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
    // Only a ".." path SEGMENT is traversal. A plain substring test would
    // also fire on legitimate names with consecutive dots ("hero..final
    // .lvks", a parent directory named "v1..v2"), rejecting safe exports
    // with a misleading error. Split on both separator flavors so a
    // Windows-style "a\\..\\b" is still caught on a POSIX build.
    {
        static const QRegularExpression kSepRe(QStringLiteral("[\\\\/]"));
        const QStringList rawSegments = sourceFilename.split(kSepRe, Qt::SkipEmptyParts);
        for (const QString &segment : rawSegments) {
            if (segment == QStringLiteral("..")) {
                return fail(QStringLiteral("filename contains '..' traversal: ") + sourceFilename);
            }
        }
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
    const QString filenameOnly = cleaned.section(QRegularExpression(QStringLiteral("[\\\\/]")), -1,
                                                 -1, QString::SectionSkipEmpty);

    if (filenameOnly.isEmpty()) {
        return fail(QStringLiteral("filename is empty after cleanPath: ") + sourceFilename);
    }
    if (filenameOnly.contains(QLatin1Char('/')) || filenameOnly.contains(QLatin1Char('\\'))) {
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
        return fail(
            QStringLiteral("could not canonicalize outputDir (missing or broken symlink): ") +
            outputDir);
    }
    QFileInfo canonOutInfo(canonicalOutputDir);
    if (!canonOutInfo.exists() || !canonOutInfo.isDir()) {
        return fail(QStringLiteral("canonical outputDir does not exist or is not a directory: ") +
                    canonicalOutputDir);
    }

    // 3. Build the candidate output path and verify it stays inside
    // canonicalOutputDir. Always compare against the canonical dir WITH
    // a trailing separator to defeat prefix confusion:
    //   /tmp/safe         vs.  /tmp/safe-evil/x  -> rejected
    // Without the trailing separator the second startsWith() would
    // succeed because "/tmp/safe-evil/x".startsWith("/tmp/safe") is true.
    //
    // Phase B1.4: use '/' as the separator regardless of platform.
    // QDir::canonicalPath() always returns '/'-delimited paths on every
    // platform (including Windows, where Qt normalizes backslashes to
    // forward slashes in canonical paths). Concatenating QDir::separator()
    // -- which IS '\' on Windows -- to canonicalOutputDir produced a
    // canonOutWithSep like "C:/safe\" which never matched the all-'/' form
    // of `candidate`, so every legitimate Windows export was rejected.
    // Using '/' here keeps the containment check correct on every host.
    const QChar sep = QLatin1Char('/');
    const QString canonOutWithSep = canonicalOutputDir + sep;
    const QString candidate = QDir::cleanPath(canonicalOutputDir + sep + filenameOnly);
    if (!candidate.startsWith(canonOutWithSep) && candidate != canonicalOutputDir) {
        return fail(QStringLiteral("resolved path escapes outputDir: ") + candidate);
    }
    return true;
}

bool SpriteState::exportSprite(const QString &filename, const QString &outputDir_,
                               const QString &postpScript, ExportFormat format,
                               SpriteStateError *err) const {
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
        qDebug() << "SpriteState::exportSprite(): baseName is empty for" << filename;
        setError(err, ErrUnsafeOutputPath);
        return false;
    }

    // Dispatch JSON-only exports to the JSON exporter and return early.
    // The Cocos2d path below is the original behavior (preserved for
    // byte-equivalence of the .lkob / .lkot / .h artifacts).
    const bool wantCocos2d = (format & Cocos2d) != 0;
    const bool wantJson = (format & Json) != 0;

    if (!wantCocos2d && !wantJson) {
        qDebug() << "SpriteState::exportSprite(): no format flags set";
        return false;
    }

    if (wantJson) {
        JsonAtlasExporter jsonExp;
        // Phase B1.4: see isSafeExportPath. canonicalFilePath() returns
        // '/'-delimited paths on every platform; QDir::separator() is '\'
        // on Windows. Mixing them produced a path like "C:/safe\base"
        // that QFile::write would still open but that broke any consumer
        // doing prefix comparisons against the canonical form. Use '/'.
        const QString jsonBase =
            QDir::cleanPath(QFileInfo(outputDir).canonicalFilePath() + QLatin1Char('/') + baseName);
        if (!jsonExp.exportAtlas(jsonBase, *this)) {
            qDebug() << "SpriteState::exportSprite(): JSON atlas export failed for" << jsonBase;
            setError(err, ErrCantOpenReadWriteMode);
            return false;
        }
        if (!wantCocos2d) {
            return true;
        }
    }

    // Phase B1.4: the Cocos2d output filenames here are derived directly
    // from `outputDir` (caller-supplied, not canonicalised), not from
    // canonicalPath(). QFile::open accepts either '/' or '\\' on Windows
    // so this site does NOT actually break; we leave QDir::separator()
    // here to preserve byte-equivalence of pre-Phase-2 export output.
    // The canonical-vs-platform-separator mismatch only matters for the
    // containment / prefix-comparison sites (isSafeExportPath above and
    // the JSON exporter base path that gets passed to canonicalising
    // consumers).
    QString binFileName = outputDir + QDir::separator() + baseName + ".lkob";
    QString textFileName = outputDir + QDir::separator() + baseName + ".lkot";
    QString headerFileName = outputDir + QDir::separator() + "AnimNameDef_" + baseName + ".h";

    QFile binOutput(binFileName);
    QFile textOutput(textFileName);
    QFile headerOutput(headerFileName);

    if (binOutput.exists()) {
        if (!binOutput.remove()) {
            qDebug() << "Error: SpriteState::exportSprite():" << binOutput.fileName()
                     << "already exists and cannot be removed";
            setError(err, ErrCantOpenReadWriteMode);
            return false;
        }
    }

    if (!binOutput.open(QFile::WriteOnly | QFile::Append)) {
        qDebug() << "Error: SpriteState::exportSprite(): could not open " << binOutput.fileName()
                 << " in WriteOnly mode";
        setError(err, ErrCantOpenReadWriteMode);
        return false;
    }

    if (!textOutput.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() << "Error: SpriteState::exportSprite(): could not open " << textOutput.fileName()
                 << " in WriteOnly mode";
        setError(err, ErrCantOpenReadWriteMode);
        return false;
    }

    if (!headerOutput.open(QFile::WriteOnly | QFile::Text)) {
        qDebug() << "Error: SpriteState::exportSprite(): could not open " << headerOutput.fileName()
                 << " in WriteOnly mode";
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
            if (!writeImageWithPostprocessing(binOutput, frame, postpScript)) {
                // A frame that cannot be written (missing/unreadable source
                // image, disk full, postprocessing failure) must fail the
                // whole export: emitting a zero-length fpixmaps record and
                // returning success would hand consumers a silently corrupt
                // .lkob/.lkot pair. Remove the partial artifacts so a failed
                // export cannot be mistaken for a finished one.
                qDebug() << "Error: SpriteState::exportSprite(): failed to write frame"
                         << frame.id << "-- aborting export";
                binOutput.close();
                textOutput.close();
                headerOutput.close();
                binOutput.remove();
                textOutput.remove();
                headerOutput.remove();
                setError(err, ErrCantExportFrame);
                return false;
            }
            offset = binOutput.size();
            textStream << "\t" << frame.id << "," << prevOffset << "," << (offset - prevOffset)
                       << "\n";
        } else {
            qDebug() << "Omitting unused frame " << frame.id;
        }
    }
    textStream << ")\n\n";

    binOutput.close();

    textStream << "# Animations\n";
    textStream << "# format: animationId,name,flags\n";
    textStream << "# Animation frames\n";
    textStream << "# format: aframeId,frameId,delay,ox,oy,sticky\n";
    textStream << "animations(\n";
    // Phase B1.3 parity (Team D3.5): the .lvks save() path sorts aframes by id
    // at write time so a load->save round-trip is byte-equivalent. The Cocos2d
    // .lkot exporter iterates _aframes directly here, so for any non-sequential
    // input ordering (hand-edited file, post-delete-save, or just data added in
    // a non-id order during a session) the exported aframe rows would come out
    // in the in-memory order instead of canonical id-order, and post-B1
    // .lkot files would differ from pre-B1. Build a sorted copy per animation
    // and iterate that so the export matches save()'s sort policy.
    for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        textStream << "\t" << it.value().toString() << "\n";
        textStream << "\taframes(\n";
        QList<LvkAframe> sortedAframes = it.value()._aframes;
        std::sort(sortedAframes.begin(), sortedAframes.end(),
                  [](const LvkAframe &a, const LvkAframe &b) { return a.id < b.id; });
        for (QListIterator<LvkAframe> it2(sortedAframes); it2.hasNext();) {
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

    // Phase 6b (Item 26): two animation names may sanitize to the same macro
    // (e.g. "fly", "FLY", or two distinct CJK names both collapsing to "_").
    // Disambiguate by appending "_2", "_3", ... while preserving the original
    // name in the emitted string literal so the runtime API still resolves.
    QSet<QString> usedMacroNames;
    for (QMapIterator<Id, LvkAnimation> it(_animations); it.hasNext();) {
        it.next();
        QString base = getMacroName(it.value().name);
        QString uniq = base;
        int suffix = 2;
        while (usedMacroNames.contains(uniq)) {
            uniq = base + QStringLiteral("_") + QString::number(suffix++);
        }
        usedMacroNames.insert(uniq);
        headerStream << "#define ANIM_" << uniq << "\t\t\t\"" << it.value().name << "\"\n";
        headerStream << "#define ANIM_" << uniq << "_FLAGS\t\t\t 0x"
                     << QString::number(it.value().flags, 16) << "\n";
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
static bool writeTempImage(QString &tmpImgFilename, const QImage &image) {
    const int IMG_COMPRESSION = 9; // min:0, max:9

    QTemporaryFile tmp(QDir::tempPath() + QDir::separator() + "lvk-frame-XXXXXX.png");
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        qDebug() << "writeTempImage: could not create secure temp file in" << QDir::tempPath();
        return false;
    }
    tmpImgFilename = tmp.fileName();
    tmp.close(); // QImageWriter wants to own the handle

    QImageWriter imgWriter(tmpImgFilename, QByteArray("png"));
    imgWriter.setCompression(IMG_COMPRESSION);
    if (!imgWriter.write(image)) {
        qDebug() << "writeTempImage: failed to encode PNG to" << tmpImgFilename << "-"
                 << imgWriter.errorString();
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
static bool resolvePostprocessingProgram(const QString &postpScriptCmd, QString *resolvedProgram,
                                         QStringList *extraArgs, QString *errorOut) {
    const QStringList tokens = QProcess::splitCommand(postpScriptCmd);
    if (tokens.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("postprocessing script command is empty");
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
                *errorOut =
                    QStringLiteral("postprocessing script '%1' does not exist").arg(program);
            }
            return false;
        }
        resolved = fi.canonicalFilePath();
        if (resolved.isEmpty()) {
            // canonicalFilePath returns "" if the file doesn't exist or a
            // symlink in the chain is broken. We already proved existence
            // above; an empty result here means a broken symlink.
            if (errorOut) {
                *errorOut = QStringLiteral("postprocessing script '%1' has a broken symlink chain")
                                .arg(program);
            }
            return false;
        }
    } else {
        // Bare name: search PATH, then canonicalise.
        const QString pathResolved = QStandardPaths::findExecutable(program);
        if (pathResolved.isEmpty()) {
            if (errorOut) {
                *errorOut =
                    QStringLiteral("postprocessing script '%1' not found on PATH").arg(program);
            }
            return false;
        }
        resolved = QFileInfo(pathResolved).canonicalFilePath();
        if (resolved.isEmpty()) {
            // PATH lookup found something, but it canonicalises to nothing
            // (broken symlink). Reject rather than execute the dangling
            // target.
            if (errorOut) {
                *errorOut =
                    QStringLiteral("postprocessing script '%1' has a broken symlink chain on PATH")
                        .arg(program);
            }
            return false;
        }
    }

    // Step 2: must be executable (POSIX) - on Windows isExecutable defers to extension.
    QFileInfo resolvedInfo(resolved);
    if (!resolvedInfo.isExecutable()) {
        if (errorOut) {
            *errorOut =
                QStringLiteral("postprocessing script '%1' is not executable").arg(resolved);
        }
        return false;
    }

    if (resolvedProgram)
        *resolvedProgram = resolved;
    if (extraArgs)
        *extraArgs = args;
    return true;
}

// SECURITY (Agent 5): Spawn the postprocessing script with explicit argv,
// never a shell-style joined string.
//
// Returns true iff the script ran AND exited successfully (exit code 0,
// normal exit).
static bool runPostprocessingScript(const QString &postpScriptCmd, const QString &inputImg,
                                    const QString &outputImg) {
    const int TIMEOUT_START = 3;
    const int TIMEOUT_FINISH = 30;

    if (QFile::exists(outputImg) && !QFile::remove(outputImg)) {
        qDebug() << "runPostprocessingScript: could not remove stale output" << outputImg;
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

    qDebug() << "runPostprocessingScript: program=" << program << " argc=" << args.size();

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
    if (postpScript.exitStatus() != QProcess::NormalExit || postpScript.exitCode() != 0) {
        qDebug() << "Postprocessing script exited with status=" << postpScript.exitStatus()
                 << "code=" << postpScript.exitCode();
        return false;
    }

    return true;
}

static bool writePostprocImage(QFile &binOutput, const QString &postprocImgFilename) {
    QFile postprocImg(postprocImgFilename);
    if (!postprocImg.open(QFile::ReadOnly)) {
        qDebug() << "Could not open postprocessed image" << postprocImgFilename;
        return false;
    }

    // SECURITY: cap the bytes read back from the (user-supplied)
    // postprocessing script's output before readAll() buffers them in
    // memory. 256 MB matches the QImageReader allocation limit set in
    // main.cpp -- a legitimate postprocessed frame is orders of magnitude
    // smaller.
    constexpr qint64 kMaxPostprocImageBytes = 256 * 1024 * 1024;
    if (postprocImg.size() > kMaxPostprocImageBytes) {
        qWarning() << "writePostprocImage: refusing postprocessed image of"
                   << postprocImg.size() << "bytes (cap" << kMaxPostprocImageBytes << "):"
                   << postprocImgFilename;
        return false;
    }

    const QByteArray bytes = postprocImg.readAll();
    if (binOutput.write(bytes) != bytes.size()) {
        qWarning() << "writePostprocImage: short write to" << binOutput.fileName();
        return false;
    }

    postprocImg.close();

    return true;
}

bool SpriteState::writeImageWithPostprocessing(QFile &binOutput, const LvkFrame &frame,
                                               const QString &postpScript) const {
    // create temp image from frame pixmap data

    // Progress goes to stderr like every other diagnostic: stdout stays
    // reserved for downstream tooling (see runHeadlessExport in main.cpp).
    std::cerr << "Exporting frame " << frame.id << "..." << std::endl;

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
            if (!a.isEmpty())
                QFile::remove(a);
            if (!b.isEmpty())
                QFile::remove(b);
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
        const QString tmplDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QTemporaryFile postpTmp(tmplDir + QDir::separator() +
                                QStringLiteral("lvk-export-XXXXXX.ppi"));
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
            std::cerr << "Error: Postprocess script '" << postpScript.toStdString()
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

void SpriteState::reloadFramePixmap(const LvkFrame &frame) {
    if (frame.id != NullId) {
        QPixmap tmp(ipixmap(frame.imgId));
        QPixmap fpixmap(tmp.copy(frame.ox, frame.oy, frame.w, frame.h));
        _fpixmaps.insert(frame.id, fpixmap);
    }
}

void SpriteState::reloadImagePixmap(Id imgId) {
    if (imgId != NullId) {
        _images[imgId].reloadImage();
    }
}

void SpriteState::reloadImagePixmaps() {
    for (QMutableMapIterator<Id, InputImage> it(_images); it.hasNext();) {
        it.next();
        reloadImagePixmap(it.value().id);
    }
}

void SpriteState::reloadFramePixmaps(Id imgId) {
    for (QMapIterator<Id, LvkFrame> it(_frames); it.hasNext();) {
        it.next();
        const LvkFrame &frame = it.value();
        if (imgId == NullId || frame.imgId == imgId) {
            reloadFramePixmap(frame);
        }
    }
}

bool SpriteState::isFrameUnused(Id frameId) const {
    bool isUnused = true;

    QMapIterator<Id, LvkAnimation> aniIt(_animations);
    while (aniIt.hasNext() && isUnused) {
        // aniIt.next() returns a temporary Item proxy; bind by value to
        // avoid the -Wdangling-reference warning on Qt6's iterator API.
        const LvkAnimation ani = aniIt.next().value();
        QListIterator<LvkAframe> aframeIt(ani._aframes);
        while (aframeIt.hasNext() && isUnused) {
            if (aframeIt.next().frameId == frameId) {
                isUnused = false;
            }
        }
    }

    return isUnused;
}

const QString &SpriteState::errorMessage(SpriteStateError err) {
    static const QString strErrNone = tr("No error");
    static const QString strErrCantOpenReadMode = tr("Cannot read file");
    static const QString strErrOpenReadWriteMode = tr("Cannot write file");
    static const QString strErrInvalidFormat = tr("The file has an invalid sprite format");
    static const QString strErrNullFilename = tr("Empty filename");
    static const QString strErrFileDoesNotExist = tr("File does not exist");
    static const QString strErrUnsafeOutputPath =
        tr("Output path escapes the destination directory");
    static const QString strErrCantExportFrame =
        tr("Cannot export a frame image (missing or unreadable source image?)");
    static const QString strErrUnknown = tr("Unknown error");

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
    case ErrCantExportFrame:
        return strErrCantExportFrame;
    default:
        return strErrUnknown;
    }
}

SpriteState::ExportFormat SpriteState::parseFormat(const QString &s) {
    const QString v = s.trimmed().toLower();
    if (v == QStringLiteral("json"))
        return Json;
    if (v == QStringLiteral("all"))
        return All;
    // "cocos2d" or unknown -> Cocos2d (legacy default).
    return Cocos2d;
}
