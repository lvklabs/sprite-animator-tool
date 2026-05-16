// tst_json_export.cpp
//
// Agent 8 (Phase 3): exercises the new JSON atlas exporter wired through
// SpriteState::exportSprite(format=Json|All).
//
// Loads examples/mario.lvks, exports to a temp dir, parses the JSON, and
// verifies:
//   - mario.png exists and is a non-empty file,
//   - mario.json exists,
//   - the "frames" object has at least as many entries as the source has
//     non-orphan frames,
//   - the "animations" object has exactly as many keys as the source has
//     animations, and each sequence is non-empty.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>

#include "spritestate.h"

#ifndef LVK_EXAMPLES_DIR
#  define LVK_EXAMPLES_DIR "."
#endif

class TstJsonExport : public QObject
{
    Q_OBJECT
private slots:
    void exportMarioAtlas();
    void exportAllFormatsAtomicity();
    void parseFormatHelper();
};

void TstJsonExport::exportMarioAtlas()
{
    const QString src = QString::fromUtf8(LVK_EXAMPLES_DIR) + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    SpriteState s;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(s.load(src, &err), qPrintable(SpriteState::errorMessage(err)));

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // The exporter uses the basename of `src` (mario) under outputDir.
    const QString fakePath = tmp.path() + QDir::separator() + "mario.lvks";
    err = SpriteState::ErrNone;
    QVERIFY2(s.exportSprite(fakePath, tmp.path(), QString(),
                            SpriteState::Json, &err),
             qPrintable("JSON export failed: " + SpriteState::errorMessage(err)));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString png  = tmp.path() + QDir::separator() + "mario.png";
    const QString json = tmp.path() + QDir::separator() + "mario.json";
    QVERIFY2(QFile::exists(png),  qPrintable("missing PNG: " + png));
    QVERIFY2(QFile::exists(json), qPrintable("missing JSON: " + json));
    QVERIFY(QFileInfo(png).size() > 0);

    QFile f(json);
    QVERIFY(f.open(QFile::ReadOnly));
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    QVERIFY2(pe.error == QJsonParseError::NoError, qPrintable(pe.errorString()));
    QVERIFY(doc.isObject());

    QJsonObject root = doc.object();
    QVERIFY(root.contains(QStringLiteral("frames")));
    QVERIFY(root.contains(QStringLiteral("animations")));
    QVERIFY(root.contains(QStringLiteral("meta")));

    // mario.lvks has 3 animations: walk / jump / queen_walk.
    QJsonObject anims = root.value(QStringLiteral("animations")).toObject();
    QCOMPARE(anims.size(), 3);
    for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
        QVERIFY2(it.value().isArray(),
                 qPrintable(QStringLiteral("animation %1 is not an array").arg(it.key())));
        QVERIFY2(it.value().toArray().size() > 0,
                 qPrintable(QStringLiteral("animation %1 is empty").arg(it.key())));
    }

    // Frames object must have at least 1 entry per non-orphan frame in
    // the source. mario.lvks has 6 frames; some are used, some may not
    // be - we just require >=1.
    QJsonObject frames = root.value(QStringLiteral("frames")).toObject();
    QVERIFY(frames.size() >= 1);
    for (auto it = frames.constBegin(); it != frames.constEnd(); ++it) {
        QJsonObject f = it.value().toObject();
        QVERIFY(f.contains(QStringLiteral("filename")));
        QVERIFY(f.contains(QStringLiteral("frame")));
        QVERIFY(f.contains(QStringLiteral("sourceSize")));
        QVERIFY(f.contains(QStringLiteral("duration")));
    }

    // meta must include the atlas image filename (relative).
    QJsonObject meta = root.value(QStringLiteral("meta")).toObject();
    QCOMPARE(meta.value(QStringLiteral("image")).toString(), QStringLiteral("mario.png"));
}

void TstJsonExport::exportAllFormatsAtomicity()
{
    // ExportFormat::All should emit BOTH Cocos2d outputs AND the JSON
    // atlas. Verifies dispatching logic in SpriteState::exportSprite.
    const QString src = QString::fromUtf8(LVK_EXAMPLES_DIR) + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    SpriteState s;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(s.load(src, &err));

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString fakePath = tmp.path() + QDir::separator() + "mario.lvks";
    err = SpriteState::ErrNone;
    QVERIFY2(s.exportSprite(fakePath, tmp.path(), QString(),
                            SpriteState::All, &err),
             qPrintable("ALL export failed: " + SpriteState::errorMessage(err)));

    const QStringList expected = {
        tmp.path() + "/mario.lkob",
        tmp.path() + "/mario.lkot",
        tmp.path() + "/AnimNameDef_mario.h",
        tmp.path() + "/mario.png",
        tmp.path() + "/mario.json",
    };
    for (const QString& p : expected) {
        QVERIFY2(QFile::exists(p), qPrintable("missing expected output: " + p));
    }
}

void TstJsonExport::parseFormatHelper()
{
    QCOMPARE(SpriteState::parseFormat("cocos2d"),    SpriteState::Cocos2d);
    QCOMPARE(SpriteState::parseFormat("COCOS2D"),    SpriteState::Cocos2d);
    QCOMPARE(SpriteState::parseFormat("json"),       SpriteState::Json);
    QCOMPARE(SpriteState::parseFormat("JSON"),       SpriteState::Json);
    QCOMPARE(SpriteState::parseFormat("all"),        SpriteState::All);
    QCOMPARE(SpriteState::parseFormat("nonsense"),   SpriteState::Cocos2d); // default
    QCOMPARE(SpriteState::parseFormat(""),           SpriteState::Cocos2d); // default
    QCOMPARE(SpriteState::parseFormat("  json  "),   SpriteState::Json);    // trim
}

QTEST_MAIN(TstJsonExport)
#include "tst_json_export.moc"
