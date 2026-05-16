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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dialogs.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

ExportController::ExportController(MainWindow* mw, Ui::MainWindow* ui,
                                   SpriteState2* state, QObject* parent)
    : QObject(parent), m_mw(mw), m_ui(ui), m_state(state)
{
}

void ExportController::wireSignals()
{
    connect(m_ui->actionExport,   &QAction::triggered, this, &ExportController::exportFile);
    connect(m_ui->actionExportAs, &QAction::triggered, this, &ExportController::exportAsFile);
}

void ExportController::setCurrentExportFile(const QString& exportFileName)
{
    m_exportFileName = exportFileName;
}

void ExportController::exportFile()
{
    if (m_exportFileName.isEmpty()) {
        exportAsFile();
    } else {
        runExport(m_exportFileName, SpriteState::Cocos2d);
    }
}

void ExportController::exportAsFile()
{
    static QString exportFileName = "";

    // Offer a filter that lets the user pick the export format implicitly
    // from the file extension/filter. We avoid a separate format dropdown
    // dialog to keep the UI surface area small and avoid touching
    // mainwindow.ui (which Agent 9 also edits).
    const QString cocosFilter = tr("Cocos2d (*.lkot *.lkob)");
    const QString jsonFilter  = tr("JSON Atlas (*.json)");
    const QString allFilter   = tr("All Formats (*.lvks *.json)");
    const QString allFiles    = tr("All files (*)");
    const QString filters = cocosFilter + ";;" + jsonFilter + ";;" + allFilter + ";;" + allFiles;

    QString selectedFilter = cocosFilter;
    exportFileName = QFileDialog::getSaveFileName(
            m_mw, tr("Export file"),
            QFileInfo(exportFileName).absolutePath(),
            filters, &selectedFilter);

    if (exportFileName.isEmpty()) return;

    int fmt = SpriteState::Cocos2d;
    if (selectedFilter == jsonFilter)      fmt = SpriteState::Json;
    else if (selectedFilter == allFilter)  fmt = SpriteState::All;

    runExport(exportFileName, fmt);
    setCurrentExportFile(exportFileName);
}

void ExportController::exportAsJsonAtlas()
{
    static QString last = "";
    const QString filename = QFileDialog::getSaveFileName(
            m_mw, tr("Export as JSON Atlas"), QFileInfo(last).absolutePath(),
            tr("JSON Atlas (*.json);;All files (*)"));
    if (filename.isEmpty()) return;
    last = filename;
    runExport(filename, SpriteState::Json);
}

void ExportController::exportAllFormats()
{
    static QString last = "";
    const QString filename = QFileDialog::getSaveFileName(
            m_mw, tr("Export All Formats"), QFileInfo(last).absolutePath(),
            tr("All Formats (*.lvks *.json);;All files (*)"));
    if (filename.isEmpty()) return;
    last = filename;
    runExport(filename, SpriteState::All);
}

void ExportController::runExport(const QString& filename, int format)
{
    SpriteStateError err;
    if (!m_state->exportSprite(filename, QString(), QString(),
                               static_cast<SpriteState::ExportFormat>(format), &err)) {
        infoDialog(tr("Cannot export '") + filename + "' "
                   + SpriteState::errorMessage(err));
        return;
    }
}
