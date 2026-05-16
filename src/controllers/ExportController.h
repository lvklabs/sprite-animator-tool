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
namespace Ui { class MainWindow; }

class ExportController : public QObject
{
    Q_OBJECT
public:
    ExportController(MainWindow* mw, Ui::MainWindow* ui, SpriteState2* state,
                     QObject* parent = nullptr);
    ~ExportController() override = default;

    void wireSignals();

    void setCurrentExportFile(const QString& exportFileName);
    QString currentExportFile() const { return m_exportFileName; }

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
    void runExport(const QString& filename, int format);

    MainWindow*     m_mw    = nullptr;
    Ui::MainWindow* m_ui    = nullptr;
    SpriteState2*   m_state = nullptr;
    QString         m_exportFileName;
};

#endif // LVK_CONTROLLERS_EXPORT_CONTROLLER_H
