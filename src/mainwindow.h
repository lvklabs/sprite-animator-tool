#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCloseEvent>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QPixmap>
#include <QSettings>
#include <QTableWidget>

#include <memory>

#include "dialogs.h"
#include "inputimage.h"
#include "lvkaframe.h"
#include "lvkanimation.h"
#include "lvkframe.h"
#include "spritestate2.h"
#include "types.h"

namespace Ui {
class MainWindow;
}

class ImageTabController;
class FrameTabController;
class AnimationTabController;
class TransitionTabController;
class ExportController;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow() override;

public slots:
    /// Opens an sprite file
    /// @returns true if success, false otherwise
    bool openFile(const QString &filename);

public:
    // Public helpers used by controllers ------------------------------------
    //
    // These were `private` in the original god class but the controllers
    // need them. They're kept here (not duplicated on every controller) so
    // there's exactly one definition.

    /// Switch tabs (controllers call these when an operation needs the user
    /// to be looking at a specific tab).
    void showFramesTab();
    void showAnimationsTab();

    /// Repaint everything from m_state. Used by controllers when a large
    /// mutation happens (e.g. undo/redo).
    void refreshAll();

    /// Status-bar helpers (called from frame-def-widget signals).
    void showMousePosition(int x, int y);
    void showMouseRect(const QRect &rect);

    /// Refresh small previews after a frame edit (called by ImageTabController
    /// when an image is reloaded / scaled).
    void refreshPreviews();

    // Accessors -------------------------------------------------------------
    SpriteState2 &state() { return _sprState; }
    const SpriteState2 &state() const { return _sprState; }
    Ui::MainWindow *uiPtr() { return ui.get(); }

    ImageTabController *images() { return _imageCtl.get(); }
    FrameTabController *frames() { return _frameCtl.get(); }
    AnimationTabController *animations() { return _animationCtl.get(); }
    TransitionTabController *transitions() { return _transitionCtl.get(); }
    ExportController *exporter() { return _exportCtl.get(); }

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    // Agent 7: Ui::MainWindow is a plain struct (not a QObject); the Qt
    // parent-child tree does not own it. Use unique_ptr to make the
    // ownership explicit and free the manual `delete ui;` in ~MainWindow.
    std::unique_ptr<Ui::MainWindow> ui;

    /// current file open
    QString _filename;

    /// app settings
    QSettings settings;

    /// sets current file and updates the main window title.
    /// The filename will be stored in the recent files section in the config file
    void setCurrentFile(const QString &filename);

    /// current sprite state
    SpriteState2 _sprState;

    /// Labels to show information in the status bar
    QLabel *statusBarMousePos;
    QLabel *statusBarRectSize;

    // Agent 8 (Phase 3 refactor): MainWindow now delegates tab logic to
    // five thin controllers. Each is owned via unique_ptr so destruction
    // ordering is well-defined (controllers go before ui/_sprState).
    std::unique_ptr<ImageTabController> _imageCtl;
    std::unique_ptr<FrameTabController> _frameCtl;
    std::unique_ptr<AnimationTabController> _animationCtl;
    std::unique_ptr<TransitionTabController> _transitionCtl;
    std::unique_ptr<ExportController> _exportCtl;

    /// initialize recent files menu
    void initRecentFilesMenu();

    /// initialize signals not owned by controllers (file menu, app-level)
    void initSignals();

    /// initialize tables (column widths/headers) -- owned by MainWindow
    /// because the schemas are stable and shared across controllers.
    void initTables();

    /// opens an sprite file, returns the error in @param err if not a null pointer
    bool openFile_(const QString &filename, SpriteState::SpriteStateError *err = 0);

private slots:
    bool saveFile();
    bool saveAsFile();
    void openFileDialog();
    void closeFile();
    void exit();

    void showLoadProgress(const QString &progress);

    DialogButton saveChangesDialog();
    bool openFile_checkUnsaved(const QString &filename);
    void closeFile_checkUnsaved();

    void undo();
    void redo();

    void whatsThisMode();
    void about();

    /// Team H3: View > Theme menu handler. Calls Theme::apply() so the
    /// palette swaps live, then Theme::saveToSettings() so the choice
    /// survives a restart. The three actions live in an exclusive
    /// QActionGroup so exactly one is checked at a time.
    void onThemeActionTriggered();

    void addRecentFileMenu(const QString &filename);
    void storeRecentFile(const QString &filename);

    void saveCustomHeader();
    void restoreCustomHeader();
};

#endif // MAINWINDOW_H
