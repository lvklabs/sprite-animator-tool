// Unit tests for ExportController + the JSON atlas exporter shape.
//
// Phase 5b: ExportController is a thin wrapper around
// SpriteState::exportSprite(). The data-class tests don't reach either, so
// we exercise both paths here:
//
//   1. Cocos2d export through the controller: construct MainWindow, load
//      examples/mario.lvks, call setCurrentExportFile() + exportFile() with
//      a non-empty filename so exportFile() bypasses the QFileDialog branch
//      and calls runExport(Cocos2d) directly. Verifies the controller wires
//      its filename + format dispatch correctly.
//
//   2. JSON atlas export: the controller's JSON entry points
//      (exportAsJsonAtlas / exportAsFile-with-JSON-filter) all show a
//      QFileDialog and can't be driven non-interactively without adding
//      new public API. Instead, we hit the same underlying pipeline
//      (SpriteState::exportSprite(..., Json, ...)) that runExport() ends
//      up calling, and verify the .png / .json outputs and the JSON
//      top-level shape (frames / animations / meta) the consumer
//      depends on.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "mainwindow.h"
#include "controllers/ExportController.h"
#include "spritestate2.h"

class TestExportController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCocos2dExportThroughController();
    void testJsonAtlasExportShape();
    void testCurrentExportFileRoundTrip();

private:
    QString examplesDir() const;
};

void TestExportController::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("LvkLabsTest"));
    QCoreApplication::setApplicationName(QStringLiteral("LvkSpriteEditorTest"));
    QSettings s;
    s.setValue(QStringLiteral("ui/showAboutOnStartup"), false);
    s.sync();
}

QString TestExportController::examplesDir() const
{
    return QDir::currentPath() + QDir::separator() + QStringLiteral("examples");
}

void TestExportController::testCocos2dExportThroughController()
{
    const QString src = examplesDir() + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Load mario.lvks; relative image paths in the .lvks resolve from CWD.
    const QString savedCwd = QDir::currentPath();
    QVERIFY(QDir::setCurrent(examplesDir()));

    MainWindow mw;
    QVERIFY(mw.openFile(src));

    ExportController* exporter = mw.exporter();
    QVERIFY(exporter != nullptr);

    // Drive the non-dialog path: setting a current export filename causes
    // exportFile() to call runExport(..., Cocos2d) directly without showing
    // a QFileDialog. The exported basename is "mario_export" so it can't
    // collide with the source fixture name.
    const QString exportTarget = tmp.path() + QDir::separator() + "mario_export.lkob";
    exporter->setCurrentExportFile(exportTarget);
    QCOMPARE(exporter->currentExportFile(), exportTarget);

    exporter->exportFile();

    // Cocos2d emits .lkob / .lkot / AnimNameDef_<base>.h, where <base> is
    // the basename of the filename passed to exportSprite (mario_export).
    const QString lkob   = tmp.path() + "/mario_export.lkob";
    const QString lkot   = tmp.path() + "/mario_export.lkot";
    const QString header = tmp.path() + "/AnimNameDef_mario_export.h";
    QVERIFY2(QFile::exists(lkob),   qPrintable("missing: " + lkob));
    QVERIFY2(QFile::exists(lkot),   qPrintable("missing: " + lkot));
    QVERIFY2(QFile::exists(header), qPrintable("missing: " + header));
    QVERIFY(QFileInfo(lkob).size() > 0);

    QDir::setCurrent(savedCwd);
}

void TestExportController::testJsonAtlasExportShape()
{
    // ExportController's JSON entry points all pop a QFileDialog, so we
    // exercise the JSON output pipeline via the exact same path runExport()
    // ends up taking: SpriteState::exportSprite(..., Json, ...). This is
    // the surface area the controller delegates to, and it's also where
    // the JsonAtlasExporter lives.
    const QString src = examplesDir() + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    const QString savedCwd = QDir::currentPath();
    QVERIFY(QDir::setCurrent(examplesDir()));

    MainWindow mw;
    QVERIFY(mw.openFile(src));

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    SpriteStateError err = SpriteState::ErrNone;
    const QString filenameInTmp = tmp.path() + QDir::separator() + "mario.lvks";
    QVERIFY2(mw.state().exportSprite(filenameInTmp, tmp.path(), QString(),
                                     SpriteState::Json, &err),
             qPrintable("JSON export failed: " + SpriteState::errorMessage(err)));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString png  = tmp.path() + QDir::separator() + "mario.png";
    const QString json = tmp.path() + QDir::separator() + "mario.json";
    QVERIFY(QFile::exists(png));
    QVERIFY(QFile::exists(json));

    QFile f(json);
    QVERIFY(f.open(QFile::ReadOnly));
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    QCOMPARE(pe.error, QJsonParseError::NoError);
    QVERIFY(doc.isObject());
    QJsonObject root = doc.object();

    // Required top-level keys (TexturePacker schema + LVK extension).
    QVERIFY(root.contains(QStringLiteral("frames")));
    QVERIFY(root.contains(QStringLiteral("animations")));
    QVERIFY(root.contains(QStringLiteral("meta")));

    // meta should reference the .png that was just written.
    QJsonObject meta = root.value(QStringLiteral("meta")).toObject();
    QCOMPARE(meta.value(QStringLiteral("image")).toString(), QStringLiteral("mario.png"));

    // mario.lvks has 3 animations; the JSON should list all of them and
    // each entry should be a non-empty array of frame filenames.
    QJsonObject anims = root.value(QStringLiteral("animations")).toObject();
    QCOMPARE(anims.size(), 3);
    for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
        QVERIFY(it.value().isArray());
        QVERIFY(it.value().toArray().size() > 0);
    }

    QDir::setCurrent(savedCwd);
}

void TestExportController::testCurrentExportFileRoundTrip()
{
    // Tiny smoke test for the simplest public accessor pair. Construction
    // is sufficient -- no MainWindow / no fixture loading needed.
    MainWindow mw;
    ExportController* exporter = mw.exporter();
    QVERIFY(exporter != nullptr);

    QVERIFY(exporter->currentExportFile().isEmpty());
    exporter->setCurrentExportFile("/some/path.lkob");
    QCOMPARE(exporter->currentExportFile(), QStringLiteral("/some/path.lkob"));
}

QTEST_MAIN(TestExportController)
#include "tst_export_controller.moc"
