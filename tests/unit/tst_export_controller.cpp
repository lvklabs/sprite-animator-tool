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
#include <QScopeGuard>

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
    void testReopenSameFilePreservesExportTarget();
    void testIsAllFilesFilterLocaleSafe();

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
    auto restoreCwd = qScopeGuard([savedCwd]() { QDir::setCurrent(savedCwd); });
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
    auto restoreCwd = qScopeGuard([savedCwd]() { QDir::setCurrent(savedCwd); });
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

void TestExportController::testReopenSameFilePreservesExportTarget()
{
    // Regression test for the Phase 6b openFile_ fix.
    //
    // Before the fix, openFile_() unconditionally called closeFile() which
    // itself called setCurrentFile("") -- that wiped the export target and
    // cleared _filename. The subsequent setCurrentFile(realPath) then saw
    // _filename == "" != realPath and wiped the export target a second
    // time. The same-file guard inside setCurrentFile could never engage.
    //
    // After the fix, the previous _filename and export target are captured
    // before closeFile() and the export target is restored when the file
    // path being re-opened matches the previous one.
    //
    // Team D3.3: the previous version of this test only exercised the
    // POSITIVE case (same-file reopen preserves the target). That is
    // necessary but not sufficient: a buggy implementation that NEVER
    // cleared the export target on open would also pass. The negative
    // case is the actual safety invariant: opening a DIFFERENT .lvks
    // file MUST clear the export target so the user does not
    // accidentally overwrite the previous document's export artifact.
    // We construct a second .lvks fixture by copying mario.lvks to
    // tmp/other.lvks and assert opening it after setting a target on
    // mario clears the target.
    const QString src = examplesDir() + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    const QString savedCwd = QDir::currentPath();
    auto restoreCwd = qScopeGuard([savedCwd]() { QDir::setCurrent(savedCwd); });
    QVERIFY(QDir::setCurrent(examplesDir()));

    MainWindow mw;
    QVERIFY(mw.openFile(src));

    ExportController* exporter = mw.exporter();
    QVERIFY(exporter != nullptr);

    // Simulate the user picking an Export-As target after opening mario.lvks.
    const QString exportTarget = QStringLiteral("/tmp/mario_export.lkob");
    exporter->setCurrentExportFile(exportTarget);
    QCOMPARE(exporter->currentExportFile(), exportTarget);

    // Re-open the same file (e.g. from the recent-files menu).
    QVERIFY(mw.openFile(src));

    // The export target must survive the same-file reopen.
    QCOMPARE(exporter->currentExportFile(), exportTarget);

    // Re-open again, still the same file.
    QVERIFY(mw.openFile(src));
    QCOMPARE(exporter->currentExportFile(), exportTarget);

    // NEGATIVE CASE: opening a DIFFERENT .lvks must clear the export
    // target. We copy mario.lvks under a fresh name to get a real,
    // openable second fixture. Image-path references inside the copy
    // are relative; they resolve against CWD (which we just set to
    // examplesDir), so the copy is loadable without further setup.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString otherPath = tmp.path() + QDir::separator()
        + QStringLiteral("other.lvks");
    QVERIFY2(QFile::copy(src, otherPath),
             qPrintable("failed to copy fixture to " + otherPath));
    QVERIFY(QFile::exists(otherPath));

    // Re-confirm the export target is still set (sanity before the
    // negative case).
    QCOMPARE(exporter->currentExportFile(), exportTarget);

    // Opening a different file path MUST wipe the export target.
    QVERIFY(mw.openFile(otherPath));
    QVERIFY2(exporter->currentExportFile().isEmpty(),
             qPrintable("opening a different file must clear the export target; "
                        "got '" + exporter->currentExportFile() + "'"));
}

void TestExportController::testIsAllFilesFilterLocaleSafe()
{
    // F5.1: ExportController::isAllFilesFilter must recognise the
    // wildcard "(*)" pattern at the END of the filter string regardless
    // of the human-readable label, which Qt translates per-locale.
    //
    // Pre-fix code did a literal `selectedFilter == tr("All files (*)")`
    // compare; under any non-English locale Qt returns the translated
    // form ("Tous les fichiers (*)", "Alle Dateien (*)", ...) and the
    // compare silently failed, regressing the D4 "user picked All Files
    // = keep their literal filename" guarantee. The new helper matches
    // on the pattern suffix instead, so every locale's all-files filter
    // is correctly identified.
    //
    // We don't install a QTranslator -- we just hand the helper the
    // literal strings Qt would return after translation. That keeps the
    // test deterministic across CI hosts where the system locale and
    // Qt's available .qm files aren't guaranteed.
    QVERIFY(ExportController::isAllFilesFilter(
        QStringLiteral("All files (*)")));
    QVERIFY(ExportController::isAllFilesFilter(
        QStringLiteral("Tous les fichiers (*)")));   // fr_FR
    QVERIFY(ExportController::isAllFilesFilter(
        QStringLiteral("Alle Dateien (*)")));        // de_DE
    QVERIFY(ExportController::isAllFilesFilter(
        QStringLiteral("Todos los archivos (*)")));  // es_ES
    QVERIFY(ExportController::isAllFilesFilter(
        QStringLiteral("Tutti i file (*)")));        // it_IT
    QVERIFY(ExportController::isAllFilesFilter(
        QStringLiteral("\xE3\x81\x99\xE3\x81\xB9\xE3\x81\xA6\xE3\x81\xAE"
                       "\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB (*)")));  // ja_JP

    // Negative cases: format-specific filters must NOT be misidentified
    // as All Files even though they contain a parenthesised pattern.
    QVERIFY(!ExportController::isAllFilesFilter(
        QStringLiteral("Cocos2d (*.lkot *.lkob)")));
    QVERIFY(!ExportController::isAllFilesFilter(
        QStringLiteral("JSON Atlas (*.json)")));
    QVERIFY(!ExportController::isAllFilesFilter(
        QStringLiteral("All Formats (*.lvks *.json)")));
    // Edge: empty string is not the all-files filter.
    QVERIFY(!ExportController::isAllFilesFilter(QString()));
}

QTEST_MAIN(TestExportController)
#include "tst_export_controller.moc"
