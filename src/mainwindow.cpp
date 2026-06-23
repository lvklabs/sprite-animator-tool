// mainwindow.cpp - Agent 8 (Phase 3): tab logic now lives in
// src/controllers/{Image,Frame,Animation,Transition,Export}*Controller.{h,cpp}.
// MainWindow owns SpriteState2, Ui::MainWindow, the five controllers,
// file open/save/close, recent files, status bar, undo/redo.
// Pre-refactor LOC: 2,528. Target: <= 600.

#include "mainwindow.h"
#include "controllers/AnimationTabController.h"
#include "controllers/ExportController.h"
#include "controllers/FrameTabController.h"
#include "controllers/ImageTabController.h"
#include "controllers/TransitionTabController.h"
#include "inputimage.h"
#include "lvkaction.h"
#include "lvkaframe.h"
#include "lvkframe.h"
#include "settings.h"
#include "ui_mainwindow.h"
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHeaderView>
#include <QKeySequence>
#include <QMessageBox>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QWhatsThis>

// Column count enums kept here so initTables() (which configures all four
// data tables) stays self-contained without #include'ing the controllers.
enum { ColImageTotal = 5 };
enum { ColFrameTotal = 8 };
enum { ColAniTotal = 3 };
enum { ColAframeTotal = 7 };
enum { ColTransTotal = 3 };
enum { BlendFrameId = 3 };
enum { BlendNone = 0 };

#ifdef MAC_OS_X
static QString convertToMacKeys(const QString &str) {
    QString tmp = str;
    tmp.replace("Shift + ", QString(QChar(0x21e7)), Qt::CaseInsensitive);
    tmp.replace("Ctrl + ", QString(QChar(0x2318)), Qt::CaseInsensitive);
    tmp.replace("Alt + ", QString(QChar(0x2325)), Qt::CaseInsensitive);
    tmp.replace("Shift", QString(QChar(0x21e7)), Qt::CaseInsensitive);
    tmp.replace("Ctrl", QString(QChar(0x2318)), Qt::CaseInsensitive);
    tmp.replace("Alt", QString(QChar(0x2325)), Qt::CaseInsensitive);
    tmp.replace("F2", QString(QChar(0x21a9)), Qt::CaseInsensitive);
    return tmp;
}
#endif // MAC_OS_X

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), statusBarMousePos(new QLabel(this)),
      statusBarRectSize(new QLabel(this)) {
    ui->setupUi(this);
    ui->statusBar->addWidget(statusBarMousePos);
    ui->statusBar->addWidget(statusBarRectSize);

    ui->imgPreview->setPixmap(QPixmap());
    // Palette-driven background: follows the active color scheme so dark mode
    // stays dark instead of leaking the burned-in light checker PNG.
    ui->imgPreview->setBackgroundRole(QPalette::Base);
    ui->imgPreview->setAutoFillBackground(true);
    ui->imgPreview->setBackground(QPixmap());
    ui->framePreview->setPixmap(QPixmap());
    ui->framePreview->setBackgroundRole(QPalette::Base);
    ui->framePreview->setAutoFillBackground(true);
    ui->framePreview->setBackground(QPixmap());
    ui->aframePreview->setPixmap(QPixmap());

    ui->imgPreview->setScrollArea(ui->imgPreviewScroll);
    ui->framePreview->setScrollArea(ui->framePreviewScroll);
    ui->aframePreview->setScrollArea(ui->aframePreviewScroll);

    ui->createQuickAniButton->setVisible(false);
    ui->checkAllImagesButton->setVisible(false);
    ui->invertCheckedImagesButton->setVisible(false);
    ui->scaleImageButton->setVisible(false);

#ifdef MAC_OS_X
    ui->imgTableWidget->setWhatsThis(convertToMacKeys(ui->imgTableWidget->whatsThis()));
    ui->framesTableWidget->setWhatsThis(convertToMacKeys(ui->framesTableWidget->whatsThis()));
    ui->aframesTableWidget->setWhatsThis(convertToMacKeys(ui->aframesTableWidget->whatsThis()));
    ui->aniTableWidget->setWhatsThis(convertToMacKeys(ui->aniTableWidget->whatsThis()));
    ui->imgPreviewScroll->setWhatsThis(convertToMacKeys(ui->imgPreviewScroll->whatsThis()));

    const int tableFontSize = 11;
    ui->imgTableWidget->setFont(QFont("", tableFontSize));
    ui->framesTableWidget->setFont(QFont("", tableFontSize));
    ui->aniTableWidget->setFont(QFont("", tableFontSize));
    ui->aframesTableWidget->setFont(QFont("", tableFontSize));
    ui->transTableWidget->setFont(QFont("", tableFontSize));

    setWindowIcon(QIcon());
#endif // MAC_OS_X

    // Construct controllers. Each holds non-owning pointers to ui +
    // _sprState. Order matters: controllers may reference each other in
    // their slots (e.g. ImageTabController::removeImage forwards to
    // FrameTabController) -- the slots run after the constructor returns,
    // so by-name lookup via m_mw->frames() etc. is safe.
    _imageCtl = std::make_unique<ImageTabController>(this, ui.get(), &_sprState, this);
    _frameCtl = std::make_unique<FrameTabController>(this, ui.get(), &_sprState, this);
    _animationCtl = std::make_unique<AnimationTabController>(this, ui.get(), &_sprState, this);
    _transitionCtl = std::make_unique<TransitionTabController>(this, ui.get(), &_sprState, this);
    _exportCtl = std::make_unique<ExportController>(this, ui.get(), &_sprState, this);

    initSignals();
    initTables();
    initRecentFilesMenu();

    _imageCtl->wireSignals();
    _frameCtl->wireSignals();
    _animationCtl->wireSignals();
    _transitionCtl->wireSignals();
    _exportCtl->wireSignals();

    // Connect MainWindow's showMouseRect AFTER the controllers wire their
    // own mouseRectChangeFinished slots. Qt fires slots in connect order;
    // doing this last keeps FrameTabController::blendFrameRect (preview
    // repaint) ahead of showMouseRect (status-bar update), matching the
    // master branch's behavior and avoiding transient flicker.
    connect(ui->imgPreview, &LvkFrameDefWidget::mouseRectChangeFinished, this,
            &MainWindow::showMouseRect);

    showFramesTab();
    _frameCtl->hideFramePreview();

    // Phase 6a: persist window geometry / dock state across launches via
    // QSettings (keys "ui/mainwindow/geometry" + "ui/mainwindow/state").
    // Fall back to the Agent-9 80%-of-screen heuristic on first launch
    // (or after QSettings is cleared). Org/app name are set in main.cpp
    // so the QSettings lookup resolves to a stable per-user store.
    const QByteArray savedGeo =
        settings.value(QStringLiteral("ui/mainwindow/geometry")).toByteArray();
    const QByteArray savedState =
        settings.value(QStringLiteral("ui/mainwindow/state")).toByteArray();
    if (!savedGeo.isEmpty()) {
        restoreGeometry(savedGeo);
    } else if (QScreen *scr = screen()) {
        const QRect avail = scr->availableGeometry();
        const int w = std::max(1204, static_cast<int>(avail.width() * 0.8));
        const int h = std::max(768, static_cast<int>(avail.height() * 0.8));
        resize(w, h);
    } else {
        resize(1204, 768);
    }
    if (!savedState.isEmpty()) {
        restoreState(savedState);
    }
    updateGeometry();

    // Agent 10: gate the splash About dialog behind a QSettings flag.
    // Previously this was an unconditional modal `about()` call from the
    // constructor, which blocked `--version` / `--help` (see UPGRADE_NOTES.md
    // #9) and made MainWindow non-instantiable in headless contexts (tests,
    // CI, --export). Default-true preserves legacy behavior for existing
    // users; flip the key to disable.
    if (settings.value(QStringLiteral("ui/showAboutOnStartup"), true).toBool()) {
        about();
    }
}

MainWindow::~MainWindow() {
    // ui is unique_ptr; statusBar* labels are Qt-parented to this.
}

void MainWindow::initSignals() {
    // Wire only the slots that are NOT owned by a controller. The
    // controllers connect their own UI signals in wireSignals().
    connect(&_sprState, &SpriteState2::loadProgress, this, &MainWindow::showLoadProgress);

    connect(ui->actionSave, &QAction::triggered, this, [this](bool) { saveFile(); });
    connect(ui->actionSaveAs, &QAction::triggered, this, [this](bool) { saveAsFile(); });
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFileDialog);
    connect(ui->actionClose, &QAction::triggered, this, &MainWindow::closeFile_checkUnsaved);
    connect(ui->actionUndo, &QAction::triggered, this, &MainWindow::undo);
    connect(ui->actionRedo, &QAction::triggered, this, &MainWindow::redo);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::exit);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);
    connect(ui->actionWhatsThis, &QAction::triggered, this, &MainWindow::whatsThisMode);

    // Phase 6a: bind the file/edit QActions to Qt's portable StandardKey
    // sequences so platform conventions (e.g. Cmd+O on macOS) are honored
    // even though the .ui file pins them to the literal "Ctrl+..." strings.
    // QKeySequence::Open / Save already correspond to Ctrl+O / Ctrl+S on
    // X11/Win, so behavior is unchanged on Linux; the win is portability
    // and a single source of truth for the binding.
    ui->actionOpen->setShortcut(QKeySequence::Open);
    ui->actionSave->setShortcut(QKeySequence::Save);
    ui->actionSaveAs->setShortcut(QKeySequence::SaveAs);
    ui->actionUndo->setShortcut(QKeySequence::Undo);
    ui->actionRedo->setShortcut(QKeySequence::Redo);
    ui->actionClose->setShortcut(QKeySequence::New);
    ui->actionExit->setShortcut(QKeySequence::Quit);
    // Ctrl+E for Export has no QKeySequence::StandardKey equivalent; keep
    // the literal binding via the QAction itself. A previous version also
    // registered a parallel QShortcut with Qt::ApplicationShortcut context
    // as a "backup", but that just triggered Qt's "ambiguous shortcut
    // overload" warning and could dead-key the binding -- the action's own
    // shortcut already fires from menu, toolbar, and application context.
    ui->actionExport->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    ui->actionExportAs->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+E")));

    // Zoom buttons: wire directly preview-to-preview.
    connect(ui->imgZoomInButton, &QAbstractButton::clicked, ui->imgPreview,
            &LvkInputImageWidget::zoomIn);
    connect(ui->imgZoomOutButton, &QAbstractButton::clicked, ui->imgPreview,
            &LvkInputImageWidget::zoomOut);
    connect(ui->actionClearGuides, &QAction::triggered, ui->imgPreview,
            &LvkFrameDefWidget::clearGuides);
    connect(ui->frameZoomInButton, &QAbstractButton::clicked, ui->framePreview,
            &LvkInputImageWidget::zoomIn);
    connect(ui->frameZoomOutButton, &QAbstractButton::clicked, ui->framePreview,
            &LvkInputImageWidget::zoomOut);
    connect(ui->aframeZoomInButton, &QAbstractButton::clicked, ui->aframePreview,
            &LvkInputImageWidget::zoomIn);
    connect(ui->aframeZoomOutButton, &QAbstractButton::clicked, ui->aframePreview,
            &LvkInputImageWidget::zoomOut);

    // Mouse-position status bar (shared across all three preview widgets).
    connect(ui->imgPreview, &LvkInputImageWidget::mousePositionChanged, this,
            &MainWindow::showMousePosition);
    connect(ui->framePreview, &LvkInputImageWidget::mousePositionChanged, this,
            &MainWindow::showMousePosition);
    connect(ui->aframePreview, &LvkInputImageWidget::mousePositionChanged, this,
            &MainWindow::showMousePosition);
    connect(ui->imgPreview, &LvkFrameDefWidget::mouseRectChanging, this,
            &MainWindow::showMouseRect);
    // showMouseRect/mouseRectChangeFinished is wired in the constructor
    // AFTER controllers' wireSignals(), so FrameTabController::blendFrameRect
    // (which repaints the preview) runs before showMouseRect (which only
    // updates the status bar). Slot-order matches master and avoids
    // transient preview flicker on rect commit.

    // Custom header save/restore lives at the MainWindow level (it's a
    // sprite-state field, not a tab-scoped concern).
    connect(ui->saveCustomHeaderButton, &QAbstractButton::clicked, this,
            &MainWindow::saveCustomHeader);
    connect(ui->restoreCustomHeaderButton, &QAbstractButton::clicked, this,
            &MainWindow::restoreCustomHeader);
}

void MainWindow::initTables() {
    QStringList headersList;

    // Phase 6a (HiDPI): Convert the legacy fixed-pixel column widths
    // (setColumnWidth(col, 30) etc.) into font-metrics-driven widths so
    // they scale with the system DPI and user font size. The rule of thumb
    // for narrow numeric columns is "header text + 2 'M's of padding";
    // wider name columns use ResizeToContents on the stretchable trailing
    // section. The previous 30/40/50px constants were burned in for the
    // 96-dpi Qt 4 era and clip badly at 200%+ scaling.
    const auto colWidthFor = [this](const QString &header) {
        const QFontMetrics fm = fontMetrics();
        return fm.horizontalAdvance(header + QStringLiteral("MM"));
    };

    /* input images table */
    ui->imgTableWidget->setRowCount(0);
    ui->imgTableWidget->setColumnCount(ColImageTotal);
    ui->imgTableWidget->setColumnWidth(0, colWidthFor(tr("Id")));
    ui->imgTableWidget->setColumnWidth(1, colWidthFor(tr("Ch")));
    ui->imgTableWidget->setColumnWidth(2, colWidthFor(tr("Id")));
    ui->imgTableWidget->setColumnWidth(3, colWidthFor(tr("Scale")));
    headersList << tr("Id") << tr("Ch") << tr("Id") << tr("Scale") << tr("Filename");
    ui->imgTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
    ui->imgTableWidget->setColumnHidden(1, true);
#ifndef DEBUG_SHOW_ID_COLS
    ui->imgTableWidget->setColumnHidden(0, true);
#endif

    /* frames table */
    ui->framesTableWidget->setRowCount(0);
    ui->framesTableWidget->setColumnCount(ColFrameTotal);
    ui->framesTableWidget->setColumnWidth(0, colWidthFor(tr("Id")));
    ui->framesTableWidget->setColumnWidth(1, colWidthFor(tr("Id")));
    ui->framesTableWidget->setColumnWidth(2, colWidthFor(tr("ox")));
    ui->framesTableWidget->setColumnWidth(3, colWidthFor(tr("oy")));
    ui->framesTableWidget->setColumnWidth(4, colWidthFor(tr("w")));
    ui->framesTableWidget->setColumnWidth(5, colWidthFor(tr("h")));
    ui->framesTableWidget->setColumnWidth(6, colWidthFor(tr("Img Id")));
    ui->framesTableWidget->ignoreColumn(1);
    ui->framesTableWidget->ignoreColumn(6);
    headersList << tr("Id") << tr("Id") << tr("ox") << tr("oy") << tr("w") << tr("h")
                << tr("Img Id") << tr("Name");
    ui->framesTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->framesTableWidget->setColumnHidden(0, true);
#endif

    /* animations table */
    ui->aniTableWidget->setRowCount(0);
    ui->aniTableWidget->setColumnCount(ColAniTotal);
    ui->aniTableWidget->setColumnWidth(0, colWidthFor(tr("Id")));
    // Name column: stretchable last section handles overflow, so let
    // ResizeToContents pick a sensible default at construction time.
    ui->aniTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->aniTableWidget->setColumnWidth(2, colWidthFor(tr("Flags")));
    headersList << tr("Id") << tr("Name") << tr("Flags");
    ui->aniTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();

    /* animation frames table */
    ui->aframesTableWidget->setRowCount(0);
    ui->aframesTableWidget->setColumnCount(ColAframeTotal);
    ui->aframesTableWidget->setColumnWidth(0, colWidthFor(tr("Id")));
    ui->aframesTableWidget->setColumnWidth(1, colWidthFor(tr("Frame Id")));
    ui->aframesTableWidget->setColumnWidth(2, colWidthFor(tr("ox")));
    ui->aframesTableWidget->setColumnWidth(3, colWidthFor(tr("oy")));
    ui->aframesTableWidget->setColumnWidth(4, colWidthFor(tr("Sticky")));
    ui->aframesTableWidget->setColumnWidth(5, colWidthFor(tr("Delay")));
    ui->aframesTableWidget->setColumnWidth(6, colWidthFor(tr("Animation Id")));
    ui->aframesTableWidget->ignoreColumn(1);
    headersList << tr("Id") << tr("Frame Id") << tr("ox") << tr("oy") << tr("Sticky") << tr("Delay")
                << tr("Animation Id");
    ui->aframesTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->aframesTableWidget->setColumnHidden(0, true);
    ui->aframesTableWidget->setColumnHidden(6, true);
#endif

    /* transitions table */
    ui->transTableWidget->setRowCount(0);
    ui->transTableWidget->setColumnCount(ColTransTotal);
    ui->transTableWidget->setColumnWidth(0, colWidthFor(tr("Animation Id")));
    ui->transTableWidget->setColumnWidth(1, colWidthFor(tr("Ch")));
    headersList << tr("Animation Id") << tr("Ch") << tr("Animation Name");
    ui->transTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->transTableWidget->setColumnHidden(0, true);
#endif
}

bool MainWindow::saveFile() {
    if (_filename.isEmpty())
        return saveAsFile();
    SpriteStateError err;
    if (!_sprState.save(_filename, &err)) {
        infoDialog(tr("Cannot save") + _filename + ". " + SpriteState::errorMessage(err));
        return false;
    }
    return true;
}

bool MainWindow::saveAsFile() {
    static QString lastDir = "";
    QString filename = QFileDialog::getSaveFileName(this, tr("Save file"), lastDir,
                                                    tr("Lvks files (*.lvks);;All files (*)"));
    if (filename.isNull())
        return false;
    lastDir = QFileInfo(filename).absolutePath();

    SpriteStateError err;
    if (!_sprState.save(filename, &err)) {
        infoDialog(tr("Cannot save ") + filename + ". " + SpriteState::errorMessage(err));
        return false;
    }
    setCurrentFile(filename);
    return true;
}

DialogButton MainWindow::saveChangesDialog() {
    QString msg = _filename.isEmpty()
                      ? tr("Save changes to file before closing?")
                      : tr("Save changes to file '") + _filename + tr("' before closing?");
    DialogButton button = yesNoCancelDialog(msg);
    if (button == YesButton) {
        if (ui->transTableWidget->rowCount() > 0) {
            infoDialog(tr("Warning: transitions won't be saved."));
        }
        if (!saveFile()) {
            button = CancelButton;
        }
    }
    return button;
}

void MainWindow::openFileDialog() {
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton)
            return;
    }
    static QString lastDir = "";
    QString filename = QFileDialog::getOpenFileName(this, tr("Open file"), lastDir,
                                                    tr("Lvks files (*.lvks);;All files (*)"));
    if (!filename.isNull()) {
        lastDir = QFileInfo(filename).absolutePath();
        openFile(filename);
    }
}

bool MainWindow::openFile_checkUnsaved(const QString &filename) {
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton)
            return false;
    }
    return openFile(filename);
}

bool MainWindow::openFile(const QString &filename) {
    SpriteStateError err;
    if (!openFile_(filename, &err)) {
        infoDialog(tr("Cannot open ") + filename + ". " + SpriteState::errorMessage(err));
        return false;
    }
    return true;
}

bool MainWindow::openFile_(const QString &filename_, SpriteStateError *err) {
    if (!QFile::exists(filename_)) {
        if (err)
            *err = SpriteState::ErrFileDoesNotExist;
        return false;
    }
    QString filename = QFileInfo(filename_).absoluteFilePath();

    // Phase 6b: closeFile() unconditionally clears the export target via
    // setCurrentFile(""). If the user is re-opening the *same* .lvks (e.g.
    // from the recent-files menu after touching it on disk) the same-file
    // guard in setCurrentFile cannot engage because _filename has already
    // been wiped to "". Capture the previous identity + export target up
    // front so we can restore the export filename when this is a same-file
    // reopen, preserving the user's per-document Export-As selection.
    const QString previousFile = _filename;
    const QString savedExportFile =
        _exportCtl ? _exportCtl->currentExportFile() : QString();

    closeFile();
    setCurrentFile(filename);

    if (!previousFile.isEmpty() && filename == previousFile && _exportCtl &&
        !savedExportFile.isEmpty()) {
        _exportCtl->setCurrentExportFile(savedExportFile);
    }

    setCursor(QCursor(Qt::BusyCursor));
    bool success = _sprState.load(filename, err);
    statusBarRectSize->setText("");
    setCursor(QCursor(Qt::ArrowCursor));

    if (!success) {
        closeFile();
        return false;
    }

    refreshAll();

    if (ui->imgTableWidget->rowCount() > 0) {
        ui->imgTableWidget->selectRow(0);
        _imageCtl->showSelImage(0);
    } else {
        ui->imgPreview->setPixmap(QPixmap());
    }
    if (ui->framesTableWidget->rowCount() == 0) {
        ui->framePreview->setPixmap(QPixmap());
    }
    if (ui->aniTableWidget->rowCount() > 0) {
        ui->aniTableWidget->selectRow(0);
        _animationCtl->showAframes(0);
    } else {
        _animationCtl->clearPreviewAnimation();
    }
    if (ui->aframesTableWidget->rowCount() > 0) {
        ui->aframesTableWidget->selectRow(0);
        _animationCtl->showSelAframe(0);
    } else {
        ui->aframePreview->setPixmap(QPixmap());
    }

    ui->customHeaderText->setPlainText(_sprState.getCustomHeader());

    return true;
}

void MainWindow::refreshAll() {
    _imageCtl->refreshTable();
    _frameCtl->refreshTable();
    _animationCtl->refreshTables();

    if (ui->aniPreview->isPlaying())
        _animationCtl->previewAnimation();
    if (ui->transPreview->isPlaying())
        _transitionCtl->previewTransition();
}

void MainWindow::refreshPreviews() {
    _imageCtl->showSelImage(ui->imgTableWidget->currentRow());
    _frameCtl->showSelFrame(ui->framesTableWidget->currentRow());
    _animationCtl->showSelAframe(ui->aframesTableWidget->currentRow());
    _animationCtl->previewAnimation();
    _transitionCtl->previewTransition();
}

void MainWindow::storeRecentFile(const QString &filename) {
#define makeKey(str, i)                                                                            \
    {                                                                                              \
        str = KEY_RECENT_FILE;                                                                     \
        str.append(QString::number(i));                                                            \
    }
    QString key;
    int found = -1;
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        makeKey(key, i);
        if (filename == settings.value(key).toString()) {
            found = i;
            break;
        }
    }
    if (found == 0)
        return;
    if (found == -1) {
        found = MAX_RECENT_FILES - 1;
        makeKey(key, found);
        settings.setValue(key, filename);
    }
    QString key_;
    for (int i = found; i > 0; --i) {
        makeKey(key, i);
        makeKey(key_, i - 1);
        QString r = settings.value(key).toString();
        QString r2 = settings.value(key_).toString();
        settings.setValue(key_, r);
        settings.setValue(key, r2);
    }
#undef makeKey
}

void MainWindow::initRecentFilesMenu() {
    // Phase 6b: every rebuild of the recent-files list starts by re-adding
    // the "<no recent files>" placeholder. setCurrentFile() runs
    // QMenu::clear() on actionOpenRecent which removes the placeholder
    // (parented to MainWindow via the .ui file, so it survives the clear
    // but is no longer in the menu). If the loop below adds any real
    // entries, addRecentFileMenu() will hide the placeholder; if not, the
    // user still sees the disabled placeholder rather than an empty submenu
    // that looks broken.
    ui->actionOpenRecent->addAction(ui->actionNoRecentFiles);
    ui->actionNoRecentFiles->setVisible(true);

    QString baseKey(KEY_RECENT_FILE);
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        QString key = baseKey;
        key.append(QString::number(i));
        QString recentFile = settings.value(key).toString();
        if (!recentFile.isEmpty())
            addRecentFileMenu(recentFile);
    }
}

void MainWindow::addRecentFileMenu(const QString &filename) {
    ui->actionNoRecentFiles->setVisible(false);
    // Parent to the QMenu (actionOpenRecent) rather than MainWindow. The
    // recent-files list is rebuilt on every setCurrentFile() via
    // QMenu::clear(), which only deletes actions whose parent is the menu
    // itself. With MainWindow as the parent, clear() unhooked the actions
    // from the menu but left them alive on the MainWindow until shutdown,
    // leaking O(opens) LvkAction instances per session.
    LvkAction *action = new LvkAction(filename, ui->actionOpenRecent);
    ui->actionOpenRecent->addAction(action);
    connect(action, QOverload<const QString &>::of(&LvkAction::triggered), this,
            [this](const QString &f) { openFile_checkUnsaved(f); });
}

void MainWindow::closeFile_checkUnsaved() {
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton)
            return;
    }
    closeFile();
}

void MainWindow::closeFile() {
    _sprState.clear();
    setCurrentFile("");

    showFramesTab();

    ui->imgTableWidget->clearContents();
    ui->imgTableWidget->setRowCount(0);
    ui->framesTableWidget->clearContents();
    ui->framesTableWidget->setRowCount(0);
    ui->aniTableWidget->clearContents();
    ui->aniTableWidget->setRowCount(0);
    ui->aframesTableWidget->clearContents();
    ui->aframesTableWidget->setRowCount(0);
    ui->transTableWidget->clearContents();
    ui->transTableWidget->setRowCount(0);
    ui->blendModeComboBox->removeItem(BlendFrameId);
    ui->blendModeComboBox->setCurrentIndex(BlendNone);

    ui->imgPreview->clear();
    ui->framePreview->clear();
    ui->aframePreview->clear();

    ui->customHeaderText->clear();

    _animationCtl->clearPreviewAnimation();
    _transitionCtl->clearPreviewTransition();
}

void MainWindow::setCurrentFile(const QString &filename) {
    // Re-opening the same .lvks must NOT clobber the user's export target:
    // the export filename is a per-document preference, and a no-op
    // "open" (e.g. from the recent-files menu, or open-while-already-open)
    // should preserve it. We only reset the export filename when the
    // identity of the current document actually changes.
    if (filename.isEmpty()) {
        if (!_filename.isEmpty()) {
            _exportCtl->setCurrentExportFile("");
        }
        _filename = "";
        setWindowTitle(QString(APP_NAME));
    } else {
        QFileInfo fileInfo(filename);
        const QString newAbs = fileInfo.absoluteFilePath();
        if (newAbs != _filename) {
            _exportCtl->setCurrentExportFile("");
        }
        _filename = newAbs;
        setWindowTitle(QString(APP_NAME) + " - " + fileInfo.fileName());
        storeRecentFile(newAbs);
        ui->actionOpenRecent->clear();
        initRecentFilesMenu();
        qDebug() << "Info: changing current app dir to" << fileInfo.absolutePath();
        QDir::setCurrent(fileInfo.absolutePath());
    }
}

void MainWindow::showFramesTab() {
    ui->tabWidget->setCurrentWidget(ui->framesTab);
}
void MainWindow::showAnimationsTab() {
    ui->tabWidget->setCurrentWidget(ui->animationsTab);
}

void MainWindow::showMousePosition(int x, int y) {
    statusBarMousePos->setText(tr("Mouse x,y: ") + QString::number(x) + "," + QString::number(y));
}

void MainWindow::showMouseRect(const QRect &rect) {
    int x = rect.x(), y = rect.y(), w = rect.width(), h = rect.height();
    if (w == 0 && h == 0) {
        statusBarRectSize->setText("");
    } else {
        statusBarRectSize->setText(tr("  Rect: x,y,w,h: ") + QString::number(x) + "," +
                                   QString::number(y) + "," + QString::number(w) + "," +
                                   QString::number(h));
    }
}

void MainWindow::saveCustomHeader() {
    _sprState.setCustomHeader(ui->customHeaderText->toPlainText());
}

void MainWindow::restoreCustomHeader() {
    ui->customHeaderText->setPlainText(_sprState.getCustomHeader());
}

void MainWindow::showLoadProgress(const QString &progress) {
    statusBarRectSize->setFixedWidth(500);
    statusBarRectSize->setText(tr("Loading %1").arg(progress));
    statusBarRectSize->repaint();
}

void MainWindow::undo() {
    if (ui->tabWidget->currentWidget() == ui->transitionsTab) {
        infoDialog(tr("Actions in the \"Transitions\" tab cannot be undone or redone"));
    } else if (_sprState.canUndo()) {
        _sprState.undo();
        refreshAll();
    }
}

void MainWindow::redo() {
    if (ui->tabWidget->currentWidget() == ui->transitionsTab) {
        infoDialog(tr("Actions in the \"Transitions\" tab cannot be undone or redone"));
    } else if (_sprState.canRedo()) {
        _sprState.redo();
        refreshAll();
    }
}

void MainWindow::whatsThisMode() {
    QWhatsThis::enterWhatsThisMode();
}

void MainWindow::about() {
    QMessageBox msg;
    msg.setWindowTitle(tr("About %1").arg(APP_NAME));
    msg.setText(QString(APP_ABOUT) + "\n\n" + tr("Modernized 2026 — Qt 6 port, 10-agent upgrade."));
    msg.setIconPixmap(QPixmap(":/icons/app-icon-128x128"));
    msg.exec();
}

void MainWindow::exit() {
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton)
            return;
    }
    QCoreApplication::exit(0);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Phase 6b: only persist geometry/state when the user actually accepts
    // the close. If they hit Cancel on the unsaved-changes prompt we leave
    // the previously stored placement untouched -- otherwise a mid-resize
    // window state (potentially partially off-screen) would clobber the
    // last good layout. Mirrors the gating logic in exit() so the two
    // paths agree on what counts as "closing".
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton) {
            event->ignore();
            return;
        }
    }

    settings.setValue(QStringLiteral("ui/mainwindow/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/mainwindow/state"), saveState());

    event->accept();
    QCoreApplication::exit(0);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->modifiers() & Qt::ControlModifier)
        ui->imgPreview->update();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    if (event->modifiers() & Qt::ControlModifier)
        ui->imgPreview->update();
}
