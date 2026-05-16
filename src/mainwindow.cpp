// mainwindow.cpp - Agent 8 (Phase 3): tab logic now lives in
// src/controllers/{Image,Frame,Animation,Transition,Export}*Controller.{h,cpp}.
// MainWindow owns SpriteState2, Ui::MainWindow, the five controllers,
// file open/save/close, recent files, status bar, undo/redo.
// Pre-refactor LOC: 2,528. Target: <= 600.

#include <QString>
#include <QStringList>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QPixmap>
#include <QMessageBox>
#include <QWhatsThis>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "lvkaction.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkaframe.h"
#include "settings.h"
#include "controllers/ImageTabController.h"
#include "controllers/FrameTabController.h"
#include "controllers/AnimationTabController.h"
#include "controllers/TransitionTabController.h"
#include "controllers/ExportController.h"

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
static QString convertToMacKeys(const QString& str)
{
    QString tmp = str;
    tmp.replace("Shift + ", QString(QChar(0x21e7)), Qt::CaseInsensitive);
    tmp.replace("Ctrl + ",  QString(QChar(0x2318)), Qt::CaseInsensitive);
    tmp.replace("Alt + ",   QString(QChar(0x2325)), Qt::CaseInsensitive);
    tmp.replace("Shift",    QString(QChar(0x21e7)), Qt::CaseInsensitive);
    tmp.replace("Ctrl",     QString(QChar(0x2318)), Qt::CaseInsensitive);
    tmp.replace("Alt",      QString(QChar(0x2325)), Qt::CaseInsensitive);
    tmp.replace("F2",       QString(QChar(0x21a9)), Qt::CaseInsensitive);
    return tmp;
}
#endif // MAC_OS_X

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      statusBarMousePos(new QLabel(this)),
      statusBarRectSize(new QLabel(this))
{
    ui->setupUi(this);
    ui->statusBar->addWidget(statusBarMousePos);
    ui->statusBar->addWidget(statusBarRectSize);

    ui->imgPreview->setPixmap(QPixmap());
    ui->imgPreview->setBackground(QPixmap(":/bg/default-bg"));
    ui->framePreview->setPixmap(QPixmap());
    ui->framePreview->setBackground(QPixmap(":/bg/default-bg"));
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
    _imageCtl      = std::make_unique<ImageTabController>(this, ui.get(), &_sprState, this);
    _frameCtl      = std::make_unique<FrameTabController>(this, ui.get(), &_sprState, this);
    _animationCtl  = std::make_unique<AnimationTabController>(this, ui.get(), &_sprState, this);
    _transitionCtl = std::make_unique<TransitionTabController>(this, ui.get(), &_sprState, this);
    _exportCtl     = std::make_unique<ExportController>(this, ui.get(), &_sprState, this);

    initSignals();
    initTables();
    initRecentFilesMenu();

    _imageCtl->wireSignals();
    _frameCtl->wireSignals();
    _animationCtl->wireSignals();
    _transitionCtl->wireSignals();
    _exportCtl->wireSignals();

    showFramesTab();
    _frameCtl->hideFramePreview();

    resize(1204, 768);
    updateGeometry();

    about();
}

MainWindow::~MainWindow()
{
    // ui is unique_ptr; statusBar* labels are Qt-parented to this.
}

void MainWindow::initSignals()
{
    // Wire only the slots that are NOT owned by a controller. The
    // controllers connect their own UI signals in wireSignals().
    connect(&_sprState, &SpriteState2::loadProgress, this, &MainWindow::showLoadProgress);

    connect(ui->actionSave,   &QAction::triggered, this, [this](bool){ saveFile(); });
    connect(ui->actionSaveAs, &QAction::triggered, this, [this](bool){ saveAsFile(); });
    connect(ui->actionOpen,   &QAction::triggered, this, &MainWindow::openFileDialog);
    connect(ui->actionClose,  &QAction::triggered, this, &MainWindow::closeFile_checkUnsaved);
    connect(ui->actionUndo,   &QAction::triggered, this, &MainWindow::undo);
    connect(ui->actionRedo,   &QAction::triggered, this, &MainWindow::redo);
    connect(ui->actionExit,   &QAction::triggered, this, &MainWindow::exit);
    connect(ui->actionAbout,  &QAction::triggered, this, &MainWindow::about);
    connect(ui->actionWhatsThis, &QAction::triggered, this, &MainWindow::whatsThisMode);

    // Zoom buttons: wire directly preview-to-preview.
    connect(ui->imgZoomInButton,    &QAbstractButton::clicked, ui->imgPreview,    &LvkInputImageWidget::zoomIn);
    connect(ui->imgZoomOutButton,   &QAbstractButton::clicked, ui->imgPreview,    &LvkInputImageWidget::zoomOut);
    connect(ui->actionClearGuides,  &QAction::triggered,        ui->imgPreview,    &LvkFrameDefWidget::clearGuides);
    connect(ui->frameZoomInButton,  &QAbstractButton::clicked, ui->framePreview,  &LvkInputImageWidget::zoomIn);
    connect(ui->frameZoomOutButton, &QAbstractButton::clicked, ui->framePreview,  &LvkInputImageWidget::zoomOut);
    connect(ui->aframeZoomInButton, &QAbstractButton::clicked, ui->aframePreview, &LvkInputImageWidget::zoomIn);
    connect(ui->aframeZoomOutButton,&QAbstractButton::clicked, ui->aframePreview, &LvkInputImageWidget::zoomOut);

    // Mouse-position status bar (shared across all three preview widgets).
    connect(ui->imgPreview,    &LvkInputImageWidget::mousePositionChanged, this, &MainWindow::showMousePosition);
    connect(ui->framePreview,  &LvkInputImageWidget::mousePositionChanged, this, &MainWindow::showMousePosition);
    connect(ui->aframePreview, &LvkInputImageWidget::mousePositionChanged, this, &MainWindow::showMousePosition);
    connect(ui->imgPreview,    &LvkFrameDefWidget::mouseRectChanging,      this, &MainWindow::showMouseRect);
    connect(ui->imgPreview,    &LvkFrameDefWidget::mouseRectChangeFinished,this, &MainWindow::showMouseRect);

    // Custom header save/restore lives at the MainWindow level (it's a
    // sprite-state field, not a tab-scoped concern).
    connect(ui->saveCustomHeaderButton,    &QAbstractButton::clicked, this, &MainWindow::saveCustomHeader);
    connect(ui->restoreCustomHeaderButton, &QAbstractButton::clicked, this, &MainWindow::restoreCustomHeader);
}

// Bottleneck hook (Agent 8): controllers wrap their setText() calls in
// local disconnect/reconnect pairs (mirrors legacy behavior). Kept for
// future cross-cutting concerns (e.g. read-only mode).
void MainWindow::cellChangedSignals(bool /*connected*/) {}

void MainWindow::initTables()
{
    QStringList headersList;

    /* input images table */
    ui->imgTableWidget->setRowCount(0);
    ui->imgTableWidget->setColumnCount(ColImageTotal);
    ui->imgTableWidget->setColumnWidth(0, 30);
    ui->imgTableWidget->setColumnWidth(1, 30);
    ui->imgTableWidget->setColumnWidth(2, 30);
    ui->imgTableWidget->setColumnWidth(3, 40);
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
    for (int i = 0; i < 6; ++i) ui->framesTableWidget->setColumnWidth(i, 30);
    ui->framesTableWidget->setColumnWidth(6, 50);
    ui->framesTableWidget->ignoreColumn(1);
    ui->framesTableWidget->ignoreColumn(6);
    headersList << tr("Id") << tr("Id") << tr("ox") << tr("oy") << tr("w") << tr("h") << tr("Img Id") << tr("Name");
    ui->framesTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->framesTableWidget->setColumnHidden(0, true);
#endif

    /* animations table */
    ui->aniTableWidget->setRowCount(0);
    ui->aniTableWidget->setColumnCount(ColAniTotal);
    ui->aniTableWidget->setColumnWidth(0, 30);
    ui->aniTableWidget->setColumnWidth(1, 270);
    ui->aniTableWidget->setColumnWidth(2, 30);
    headersList << "Id" << "Name" << "Flags";
    ui->aniTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();

    /* animation frames table */
    ui->aframesTableWidget->setRowCount(0);
    ui->aframesTableWidget->setColumnCount(ColAframeTotal);
    ui->aframesTableWidget->setColumnWidth(0, 30);
    ui->aframesTableWidget->setColumnWidth(1, 60);
    ui->aframesTableWidget->setColumnWidth(2, 30);
    ui->aframesTableWidget->setColumnWidth(3, 30);
    ui->aframesTableWidget->setColumnWidth(4, 50);
    ui->aframesTableWidget->setColumnWidth(5, 50);
    ui->aframesTableWidget->setColumnWidth(6, 30);
    ui->aframesTableWidget->ignoreColumn(1);
    headersList << tr("Id") << tr("Frame Id") << tr("ox") << tr("oy") << tr("Sticky") << tr("Delay") << tr("Animation Id");
    ui->aframesTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->aframesTableWidget->setColumnHidden(0, true);
    ui->aframesTableWidget->setColumnHidden(6, true);
#endif

    /* transitions table */
    ui->transTableWidget->setRowCount(0);
    ui->transTableWidget->setColumnCount(ColTransTotal);
    ui->transTableWidget->setColumnWidth(0, 30);
    ui->transTableWidget->setColumnWidth(1, 30);
    headersList << tr("Animation Id") << tr("Ch") << tr("Animation Name");
    ui->transTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->transTableWidget->setColumnHidden(0, true);
#endif
}

bool MainWindow::saveFile()
{
    if (_filename.isEmpty()) return saveAsFile();
    SpriteStateError err;
    if (!_sprState.save(_filename, &err)) {
        infoDialog(tr("Cannot save") + _filename + ". " + SpriteState::errorMessage(err));
        return false;
    }
    return true;
}

bool MainWindow::saveAsFile()
{
    static QString lastDir = "";
    QString filename = QFileDialog::getSaveFileName(
            this, tr("Save file"), lastDir, "*.lvks;; *.*");
    if (filename.isNull()) return false;
    lastDir = QFileInfo(filename).absolutePath();

    SpriteStateError err;
    if (!_sprState.save(filename, &err)) {
        infoDialog(tr("Cannot save ") + filename + ". " + SpriteState::errorMessage(err));
        return false;
    }
    setCurrentFile(filename);
    return true;
}

DialogButton MainWindow::saveChangesDialog()
{
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

void MainWindow::openFileDialog()
{
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton) return;
    }
    static QString lastDir = "";
    QString filename = QFileDialog::getOpenFileName(this, tr("Open file"), lastDir, "*.lvks;; *.*");
    if (!filename.isNull()) {
        lastDir = QFileInfo(filename).absolutePath();
        openFile(filename);
    }
}

bool MainWindow::openFile_checkUnsaved(const QString& filename)
{
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton) return false;
    }
    return openFile(filename);
}

bool MainWindow::openFile(const QString& filename)
{
    SpriteStateError err;
    if (!openFile_(filename, &err)) {
        infoDialog(tr("Cannot open ") + filename + ". " + SpriteState::errorMessage(err));
        return false;
    }
    return true;
}

bool MainWindow::openFile_(const QString& filename_, SpriteStateError* err)
{
    if (!QFile::exists(filename_)) {
        if (err) *err = SpriteState::ErrFileDoesNotExist;
        return false;
    }
    QString filename = QFileInfo(filename_).absoluteFilePath();

    closeFile();
    setCurrentFile(filename);

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

void MainWindow::refreshAll()
{
    _imageCtl->refreshTable();
    _frameCtl->refreshTable();
    _animationCtl->refreshTables();

    if (ui->aniPreview->isPlaying())   _animationCtl->previewAnimation();
    if (ui->transPreview->isPlaying()) _transitionCtl->previewTransition();
}

void MainWindow::refreshPreviews()
{
    _imageCtl->showSelImage(ui->imgTableWidget->currentRow());
    _frameCtl->showSelFrame(ui->framesTableWidget->currentRow());
    _animationCtl->showSelAframe(ui->aframesTableWidget->currentRow());
    _animationCtl->previewAnimation();
    _transitionCtl->previewTransition();
}

void MainWindow::storeRecentFile(const QString& filename)
{
    #define makeKey(str, i) { str = KEY_RECENT_FILE; str.append(QString::number(i)); }
    QString key;
    int found = -1;
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        makeKey(key, i);
        if (filename == settings.value(key).toString()) { found = i; break; }
    }
    if (found == 0) return;
    if (found == -1) {
        found = MAX_RECENT_FILES - 1;
        makeKey(key, found);
        settings.setValue(key, filename);
    }
    QString key_;
    for (int i = found; i > 0; --i) {
        makeKey(key, i);
        makeKey(key_, i - 1);
        QString r  = settings.value(key).toString();
        QString r2 = settings.value(key_).toString();
        settings.setValue(key_, r);
        settings.setValue(key, r2);
    }
    #undef makeKey
}

void MainWindow::initRecentFilesMenu()
{
    QString baseKey(KEY_RECENT_FILE);
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        QString key = baseKey;
        key.append(QString::number(i));
        QString recentFile = settings.value(key).toString();
        if (!recentFile.isEmpty()) addRecentFileMenu(recentFile);
    }
}

void MainWindow::addRecentFileMenu(const QString& filename)
{
    ui->actionNoRecentFiles->setVisible(false);
    LvkAction* action = new LvkAction(filename, this);
    ui->actionOpenRecent->addAction(action);
    connect(action, QOverload<const QString&>::of(&LvkAction::triggered),
            this, [this](const QString& f){ openFile_checkUnsaved(f); });
}

void MainWindow::closeFile_checkUnsaved()
{
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton) return;
    }
    closeFile();
}

void MainWindow::closeFile()
{
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

void MainWindow::setCurrentFile(const QString& filename)
{
    if (filename.isEmpty()) {
        _filename = "";
        _exportCtl->setCurrentExportFile("");
        setWindowTitle(QString(APP_NAME));
    } else {
        QFileInfo fileInfo(filename);
        _filename = fileInfo.absoluteFilePath();
        _exportCtl->setCurrentExportFile("");
        setWindowTitle(QString(APP_NAME) + " - " + fileInfo.fileName());
        storeRecentFile(fileInfo.absoluteFilePath());
        ui->actionOpenRecent->clear();
        initRecentFilesMenu();
        qDebug() << "Info: changing current app dir to" << fileInfo.absolutePath();
        QDir::setCurrent(fileInfo.absolutePath());
    }
}

void MainWindow::showFramesTab()     { ui->tabWidget->setCurrentWidget(ui->framesTab); }
void MainWindow::showAnimationsTab() { ui->tabWidget->setCurrentWidget(ui->animationsTab); }

void MainWindow::showMousePosition(int x, int y)
{
    statusBarMousePos->setText(tr("Mouse x,y: ") + QString::number(x) + "," + QString::number(y));
}

void MainWindow::showMouseRect(const QRect& rect)
{
    int x = rect.x(), y = rect.y(), w = rect.width(), h = rect.height();
    if (w == 0 && h == 0) {
        statusBarRectSize->setText("");
    } else {
        statusBarRectSize->setText(tr("  Rect: x,y,w,h: ") +
                                   QString::number(x) + "," + QString::number(y) + "," +
                                   QString::number(w) + "," + QString::number(h));
    }
}

void MainWindow::saveCustomHeader()
{
    _sprState.setCustomHeader(ui->customHeaderText->toPlainText());
}

void MainWindow::restoreCustomHeader()
{
    ui->customHeaderText->setPlainText(_sprState.getCustomHeader());
}

void MainWindow::showLoadProgress(const QString& progress)
{
    statusBarRectSize->setFixedWidth(500);
    statusBarRectSize->setText("Loading " + progress);
    statusBarRectSize->repaint();
}

void MainWindow::undo()
{
    if (ui->tabWidget->currentWidget() == ui->transitionsTab) {
        infoDialog(tr("Actions in the \"Transitions\" tab cannot be undone or redone"));
    } else if (_sprState.canUndo()) {
        _sprState.undo();
        refreshAll();
    }
}

void MainWindow::redo()
{
    if (ui->tabWidget->currentWidget() == ui->transitionsTab) {
        infoDialog(tr("Actions in the \"Transitions\" tab cannot be undone or redone"));
    } else if (_sprState.canRedo()) {
        _sprState.redo();
        refreshAll();
    }
}

void MainWindow::whatsThisMode()
{
    QWhatsThis::enterWhatsThisMode();
}

void MainWindow::about()
{
    QMessageBox msg;
    msg.setText(QString(APP_ABOUT));
    msg.setIconPixmap(QPixmap(":/icons/app-icon-128x128"));
    msg.exec();
}

void MainWindow::exit()
{
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton) return;
    }
    QCoreApplication::exit(0);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();
    exit();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) ui->imgPreview->update();
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) ui->imgPreview->update();
}
