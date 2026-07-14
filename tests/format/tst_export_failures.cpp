// tst_export_failures.cpp (Round 7)
//
// exportSprite() must FAIL -- with ErrCantExportFrame and no partial
// artifacts left on disk -- when a used frame's image cannot be written.
// Before the Round 7 fix the writeImageWithPostprocessing() return value
// was discarded: a .lvks referencing a missing image asset exported
// "successfully" (exit 0) with a zero-length record in the .lkob/.lkot
// pair, handing downstream Cocos2d loaders a corrupt atlas with no error
// signal anywhere.

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QScopeGuard>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>

#include "spritestate.h"

class TstExportFailures : public QObject
{
    Q_OBJECT

private slots:
    void missingImageFailsCocos2dExport();
    void missingImageLeavesNoPartialArtifacts();
    void presentImageStillExports();

private:
    // Write a minimal one-image/one-frame/one-animation sprite whose
    // frame IS used by the animation. @p imageName is written verbatim
    // into the images() block.
    static QString writeSprite(const QTemporaryDir &dir, const QString &name,
                               const QString &imageName);
};

QString TstExportFailures::writeSprite(const QTemporaryDir &dir, const QString &name,
                                       const QString &imageName)
{
    const QString path = dir.path() + QDir::separator() + name;
    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text)) {
        return QString();
    }
    QTextStream ts(&f);
    ts << "### LvkSprite ##\n";
    ts << "LvkSprite version 0.4\n\n";
    ts << "images(\n";
    ts << "\t0," << imageName << ",1\n";
    ts << ")\n\n";
    ts << "frames(\n";
    ts << "\t0,solo,0,0,0,16,16\n";
    ts << ")\n\n";
    ts << "animations(\n";
    ts << "\t0,walk,0\n";
    ts << "\taframes(\n";
    ts << "\t\t0,0,200,0,0,0\n";
    ts << "\t)\n";
    ts << ")\n\n";
    ts << "### End LvkSprite ##\n";
    return path;
}

void TstExportFailures::missingImageFailsCocos2dExport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString src =
        writeSprite(dir, QStringLiteral("missing.lvks"), QStringLiteral("nonexistent.png"));
    QVERIFY(!src.isEmpty());

    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    // The load itself succeeds: a missing-on-disk asset is a legal
    // editing state (the pixmap is null and the GUI shows a blank).
    QVERIFY2(st.load(src, &err), qPrintable(SpriteState::errorMessage(err)));

    err = SpriteState::ErrNone;
    QVERIFY2(!st.exportSprite(src, dir.path(), QString(), SpriteState::Cocos2d, &err),
             "export of a used frame with a missing image reported success");
    QCOMPARE(err, SpriteState::ErrCantExportFrame);
}

void TstExportFailures::missingImageLeavesNoPartialArtifacts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString src =
        writeSprite(dir, QStringLiteral("missing.lvks"), QStringLiteral("nonexistent.png"));
    QVERIFY(!src.isEmpty());

    SpriteState st;
    QVERIFY(st.load(src, nullptr));
    QVERIFY(!st.exportSprite(src, dir.path(), QString(), SpriteState::Cocos2d, nullptr));

    // A failed export must not be mistakable for a finished one.
    QVERIFY(!QFile::exists(dir.path() + QStringLiteral("/missing.lkob")));
    QVERIFY(!QFile::exists(dir.path() + QStringLiteral("/missing.lkot")));
    QVERIFY(!QFile::exists(dir.path() + QStringLiteral("/AnimNameDef_missing.h")));
}

void TstExportFailures::presentImageStillExports()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Same sprite, but the backing image actually exists.
    {
        QImage pixels(16, 16, QImage::Format_ARGB32);
        pixels.fill(Qt::blue);
        QVERIFY(pixels.save(dir.path() + QStringLiteral("/present.png")));
    }
    const QString src =
        writeSprite(dir, QStringLiteral("ok.lvks"), QStringLiteral("present.png"));
    QVERIFY(!src.isEmpty());

    // Relative image paths resolve against the CWD (the GUI/CLI chdir to
    // the sprite's directory before loading).
    const QString savedCwd = QDir::currentPath();
    QVERIFY(QDir::setCurrent(dir.path()));
    auto restoreCwd = qScopeGuard([&] { QDir::setCurrent(savedCwd); });

    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(src, &err), qPrintable(SpriteState::errorMessage(err)));

    err = SpriteState::ErrNone;
    QVERIFY2(st.exportSprite(src, dir.path(), QString(), SpriteState::Cocos2d, &err),
             qPrintable(SpriteState::errorMessage(err)));
    QCOMPARE(err, SpriteState::ErrNone);

    const QFileInfo lkob(dir.path() + QStringLiteral("/ok.lkob"));
    QVERIFY(lkob.exists());
    QVERIFY(lkob.size() > 0);
    QVERIFY(QFile::exists(dir.path() + QStringLiteral("/ok.lkot")));
    QVERIFY(QFile::exists(dir.path() + QStringLiteral("/AnimNameDef_ok.h")));
}

QTEST_MAIN(TstExportFailures)
#include "tst_export_failures.moc"
