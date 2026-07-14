// ExportController.h
//
// Agent 8: owns File -> Export wiring and delegates to
// SpriteState::exportSprite() with the new ExportFormat enum
// (Cocos2d / Json / All) from the Phase 3 export overhaul.

#ifndef LVK_CONTROLLERS_EXPORT_CONTROLLER_H
#define LVK_CONTROLLERS_EXPORT_CONTROLLER_H

#include <QObject>
#include <QString>

class MainWindow;
class SpriteState2;
namespace Ui {
class MainWindow;
}

class ExportController : public QObject {
    Q_OBJECT
public:
    ExportController(MainWindow *mw, Ui::MainWindow *ui, SpriteState2 *state,
                     QObject *parent = nullptr);
    ~ExportController() override = default;

    void wireSignals();

    /// Remember the export target for File -> Export (Ctrl+E). The format
    /// is remembered alongside the filename: re-export must repeat the
    /// user's chosen format, or a JSON/All target silently degrades to a
    /// Cocos2d-only export leaving the .json/.png atlas stale. Defaults to
    /// Cocos2d for legacy callers (session restore of pre-format configs).
    void setCurrentExportFile(const QString &exportFileName, int format = -1);
    QString currentExportFile() const { return m_exportFileName; }
    int currentExportFormat() const { return m_exportFormat; }

    /// True when @p f is the "All files (*)" wildcard filter, regardless
    /// of locale.
    ///
    /// F5.1: The dialog filter list contains an "All files (*)" entry
    /// whose human label is translated by Qt at runtime ("Tous les
    /// fichiers (*)" under fr_FR, "Alle Dateien (*)" under de_DE, ...).
    /// QFileDialog returns the SELECTED filter as the localized string,
    /// so the previous literal compare against `tr("All files (*)")`
    /// silently regressed every non-English locale: the comparison
    /// failed and the "user picked All Files = keep their filename"
    /// guarantee (D4.2) silently disappeared. We match on the "(*)"
    /// pattern suffix instead -- that suffix is Qt filter syntax, never
    /// translated, and uniquely identifies the all-files wildcard among
    /// the filters we register.
    static bool isAllFilesFilter(const QString &f);

public slots:
    void exportFile();
    void exportAsFile();

    /// New menu entries (Agent 8): the format dropdown is shown in the
    /// QFileDialog as a filter; the chosen filter determines the export
    /// format. We avoid a custom QDialog so we don't have to edit
    /// mainwindow.ui (Agent 9 owns styling and we want to stay disjoint).
    void exportAsJsonAtlas();
    void exportAllFormats();

private:
    void runExport(const QString &filename, int format);

    MainWindow *m_mw = nullptr;
    Ui::MainWindow *m_ui = nullptr;
    SpriteState2 *m_state = nullptr;
    QString m_exportFileName;
    int m_exportFormat; // SpriteState::ExportFormat; set in the constructor
};

#endif // LVK_CONTROLLERS_EXPORT_CONTROLLER_H
