// ExportController.cpp
//
// See ExportController.h. The export menu now offers three actions:
//
//   - File -> Export (Ctrl+E)           : Cocos2d (.lkob/.lkot/.h)
//   - File -> Export as... (Ctrl+Shift+E): Cocos2d, prompted destination
//   - The Export As dialog includes a format filter that lets the user
//     pick "JSON Atlas" or "All Formats" without us having to edit the
//     mainwindow.ui (Agent 9 owns UI styling - we stay disjoint).

#include "controllers/ExportController.h"

#include "dialogs.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QSet>

// F5.1: see the declaration in ExportController.h for the rationale.
// The helper is a public static member so it's reachable from unit
// tests that link against ExportController.cpp without us needing to
// expose any of the rest of the dialog plumbing.
bool ExportController::isAllFilesFilter(const QString &f) {
    return f.endsWith(QStringLiteral("(*)"));
}

ExportController::ExportController(MainWindow *mw, Ui::MainWindow *ui, SpriteState2 *state,
                                   QObject *parent)
    : QObject(parent), m_mw(mw), m_ui(ui), m_state(state),
      m_exportFormat(SpriteState::Cocos2d) {}

void ExportController::wireSignals() {
    connect(m_ui->actionExport, &QAction::triggered, this, &ExportController::exportFile);
    connect(m_ui->actionExportAs, &QAction::triggered, this, &ExportController::exportAsFile);
}

void ExportController::setCurrentExportFile(const QString &exportFileName, int format) {
    m_exportFileName = exportFileName;
    m_exportFormat = (format < 0) ? SpriteState::Cocos2d : format;
}

void ExportController::exportFile() {
    if (m_exportFileName.isEmpty()) {
        exportAsFile();
    } else {
        // Repeat the last export with the format the user picked then;
        // hardcoding Cocos2d here left JSON/All targets silently stale.
        runExport(m_exportFileName, m_exportFormat);
    }
}

void ExportController::exportAsFile() {
    static QString exportFileName = "";

    // Offer a filter that lets the user pick the export format implicitly
    // from the file extension/filter. We avoid a separate format dropdown
    // dialog to keep the UI surface area small and avoid touching
    // mainwindow.ui (which Agent 9 also edits).
    const QString cocosFilter = tr("Cocos2d (*.lkot *.lkob)");
    const QString jsonFilter = tr("JSON Atlas (*.json)");
    const QString allFilter = tr("All Formats (*.lvks *.json)");
    const QString allFiles = tr("All files (*)");
    const QString filters = cocosFilter + ";;" + jsonFilter + ";;" + allFilter + ";;" + allFiles;

    QString selectedFilter = cocosFilter;
    exportFileName = QFileDialog::getSaveFileName(m_mw, tr("Export file"),
                                                  QFileInfo(exportFileName).absolutePath(), filters,
                                                  &selectedFilter);

    if (exportFileName.isEmpty())
        return;

    int fmt = SpriteState::Cocos2d;
    if (selectedFilter == jsonFilter)
        fmt = SpriteState::Json;
    else if (selectedFilter == allFilter)
        fmt = SpriteState::All;

    // Phase 6b: QFileDialog::getSaveFileName auto-appends a filter's
    // extension only when the filter has a single "*.ext" pattern. The
    // Cocos2d filter has two ("*.lkot *.lkob") so users who typed a bare
    // basename would walk away with an extensionless file. Append a
    // sensible default based on the chosen filter: ".lkob" (the binary
    // plist Cocos2d's runtime loads -- .lkot is the human-readable
    // companion the exporter writes alongside it) for Cocos2d, ".json"
    // for JSON Atlas, ".lvks" for All Formats.
    //
    // D4.2: if the user explicitly picked the "All files (*)" filter
    // they are asking for raw control over the filename -- do not append
    // anything. The auto-suffix is only sensible when the user picked a
    // format-specific filter.
    //
    // D4.3: QFileInfo::suffix() is greedy after the last '.', so a
    // basename like "hero.v2" yields a "v2" suffix and the old
    // isEmpty() guard wrongly skipped the append, leaving the user with
    // a misclassified Cocos2d artifact. Whitelist the known export
    // extensions and append a default whenever the actual suffix is not
    // one of them.
    static const QSet<QString> knownExt = {
        QStringLiteral("lkob"), QStringLiteral("lkot"), QStringLiteral("h"),
        QStringLiteral("json"), QStringLiteral("lvks"),
    };
    const QString suffix = QFileInfo(exportFileName).suffix().toLower();
    // F5.1: locale-safe All Files detection -- match on the "(*)"
    // pattern suffix (untranslated) instead of the human label, which
    // QFileDialog returns localized under any non-English locale.
    if (!isAllFilesFilter(selectedFilter) && !knownExt.contains(suffix)) {
        QString defaultSuffix;
        if (fmt == SpriteState::Json)
            defaultSuffix = QStringLiteral("json");
        else if (fmt == SpriteState::All)
            defaultSuffix = QStringLiteral("lvks");
        else if (fmt == SpriteState::Cocos2d)
            defaultSuffix = QStringLiteral("lkob");
        if (!defaultSuffix.isEmpty())
            exportFileName.append(QLatin1Char('.') + defaultSuffix);
    }

    runExport(exportFileName, fmt);
    setCurrentExportFile(exportFileName, fmt);
}

void ExportController::exportAsJsonAtlas() {
    static QString last = "";
    const QString filename = QFileDialog::getSaveFileName(m_mw, tr("Export as JSON Atlas"),
                                                          QFileInfo(last).absolutePath(),
                                                          tr("JSON Atlas (*.json);;All files (*)"));
    if (filename.isEmpty())
        return;
    last = filename;
    runExport(filename, SpriteState::Json);
    setCurrentExportFile(filename, SpriteState::Json);
}

void ExportController::exportAllFormats() {
    static QString last = "";
    QString filename =
        QFileDialog::getSaveFileName(m_mw, tr("Export All Formats"), QFileInfo(last).absolutePath(),
                                     tr("All Formats (*.lvks *.json);;All files (*)"));
    if (filename.isEmpty())
        return;
    // Multi-pattern filter -> getSaveFileName won't auto-append; do it
    // manually so users who typed a bare basename get the canonical .lvks
    // master rather than an extensionless artifact.
    if (QFileInfo(filename).suffix().isEmpty())
        filename.append(QStringLiteral(".lvks"));
    last = filename;
    runExport(filename, SpriteState::All);
    setCurrentExportFile(filename, SpriteState::All);
}

void ExportController::runExport(const QString &filename, int format) {
    SpriteStateError err;
    if (!m_state->exportSprite(filename, QString(), QString(),
                               static_cast<SpriteState::ExportFormat>(format), &err)) {
        errorDialog(tr("Cannot export '") + filename + "' " + SpriteState::errorMessage(err), m_mw);
        return;
    }
}
