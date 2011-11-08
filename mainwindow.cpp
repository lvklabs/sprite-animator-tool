#include <QString>
#include <QStringList>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QPixmap>
#include <QDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QGraphicsPixmapItem>
#include <QList>
#include <QWhatsThis>
#include <QMapIterator>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "lvkaction.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkaframe.h"
#include "settings.h"

// imgTableWidget columns
enum {
    ColImageId,
    ColImageCheckable,
    ColImageVisibleId,
    ColImageScale,
    ColImageFilename,
    ColImageTotal,
};

// framesTableWidget colums
enum {
    ColFrameId,
    ColFrameVisibleId,
    ColFrameOx,
    ColFrameOy,
    ColFrameW,
    ColFrameH,
    ColFrameImgId,
    ColFrameName,
    ColFrameTotal,
};

// aniTableWidget columns
enum {
    ColAniId,
    ColAniName,
    ColAniFlags,
    ColAniTotal,
};

// aframesTableWidget columns
enum {
    ColAframeId,
    ColAframeFrameId,
    ColAframeOx,
    ColAframeOy,
    ColAframeSticky,
    ColAframeDelay,
    ColAframeAniId,
    ColAframeTotal,
};

// transTableWidget columns
enum {
    ColTransAniId,
    ColTransAniCheckable,
    ColTransAniName,
    ColTransTotal,
};

// Blend modes
enum {
    BlendNone,
    BlendFrameRect,
    BlendExistentFrame,
    BlendFrameId,
    BlendModeTotal
};

bool isValidAniName(const QString & name, bool showErrorDialog)
{
    if (name.isEmpty())  {
        if (showErrorDialog) {
            infoDialog(QObject::tr("Cannot add an animation without name"));
        }
        return false;
    } else if (name.contains(",")) {
        if (showErrorDialog) {
            infoDialog(QObject::tr("Animation name cannot contain the character ','"));
        }
        return false;
    }
    return true;
}

bool isValidFrameName(const QString name, bool showErrorDialog)
{
    if (name.isEmpty())  {
        if (showErrorDialog) {
            infoDialog(QObject::tr("Cannot add a frame without name"));
        }
        return false;
    } else if (name.contains(",")) {
        if (showErrorDialog) {
            infoDialog(QObject::tr("Frame name cannot contain the character ','"));
        }
        return false;
    }
    return true;
}

QString toHexString(unsigned i)
{
    return "0x"  + QString::number(i, 16);
}

#ifdef MAC_OS_X
QString convertToMacKeys(const QString& str)
{
    QString tmp = str;
    tmp.replace("Shift + ", QString(QChar(0x21e7) /* ⇧ */), Qt::CaseInsensitive);
    tmp.replace("Ctrl + ",  QString(QChar(0x2318) /* ⌘ */), Qt::CaseInsensitive);
    tmp.replace("Alt + ",   QString(QChar(0x2325) /* ⌥ */), Qt::CaseInsensitive);
    tmp.replace("Shift",    QString(QChar(0x21e7) /* ⇧ */), Qt::CaseInsensitive);
    tmp.replace("Ctrl",     QString(QChar(0x2318) /* ⌘ */), Qt::CaseInsensitive);
    tmp.replace("Alt",      QString(QChar(0x2325) /* ⌥ */), Qt::CaseInsensitive);
    tmp.replace("F2",       QString(QChar(0x21a9) /* ↩ */), Qt::CaseInsensitive);

    return tmp;
}
#endif // MAC_OS_X

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      statusBarMousePos(new QLabel()), statusBarRectSize(new QLabel()),
      _blendFrameId(NullId)
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

    initSignals();
    initTables();
    initRecentFilesMenu();
    showFramesTab();
    hideFramePreview();

    resize(1204, 768);
    updateGeometry();
}

void MainWindow::initSignals()
{
    connect(&_sprState,                SIGNAL(loadProgress(QString)),this, SLOT(showLoadProgress(QString)));

    connect(ui->actionSave,            SIGNAL(triggered()),          this, SLOT(saveFile()));
    connect(ui->actionSaveAs,          SIGNAL(triggered()),          this, SLOT(saveAsFile()));
    connect(ui->actionOpen,            SIGNAL(triggered()),          this, SLOT(openFileDialog()));
    connect(ui->actionClose,           SIGNAL(triggered()),          this, SLOT(closeFile_checkUnsaved()));
    connect(ui->actionExport,          SIGNAL(triggered()),          this, SLOT(exportFile()));
    connect(ui->actionExportAs,        SIGNAL(triggered()),          this, SLOT(exportAsFile()));
    connect(ui->actionUndo,            SIGNAL(triggered()),          this, SLOT(undo()));
    connect(ui->actionRedo,            SIGNAL(triggered()),          this, SLOT(redo()));
    connect(ui->actionExit,            SIGNAL(triggered()),          this, SLOT(exit()));
    connect(ui->actionAbout,           SIGNAL(triggered()),          this, SLOT(about()));
    connect(ui->actionWhatsThis,       SIGNAL(triggered()),          this, SLOT(whatsThisMode()));
    //connect(ui->actionFramesTab,       SIGNAL(triggered()),          this, SLOT(showFramesTab()));
    //connect(ui->actionAnimationsTab,   SIGNAL(triggered()),          this, SLOT(showAnimationsTab()));
    connect(ui->actionAddImage,        SIGNAL(triggered()),          this, SLOT(addImageDialog()));
    connect(ui->actionAddFrame,        SIGNAL(triggered()),          this, SLOT(addFrameDialog()));
    connect(ui->actionAddAnimation,    SIGNAL(triggered()),          this, SLOT(addAnimationDialog()));
    //connect(ui->actionShowHideFramesPreview, SIGNAL(triggered()),    this, SLOT(hideShowFramePreview()));
    connect(ui->actionRemoveImage,     SIGNAL(triggered()),          this, SLOT(removeSelImage()));
    connect(ui->actionRemoveFrame,     SIGNAL(triggered()),          this, SLOT(removeSelFrame()));
    connect(ui->actionRemoveAnimation, SIGNAL(triggered()),          this, SLOT(removeSelAnimation()));
    connect(ui->actionRemoveAllUnusedFrames, SIGNAL(triggered()),    this, SLOT(removeAllUnusedFrames()));
    connect(ui->actionInvertAframesOrder, SIGNAL(triggered()),       this, SLOT(invertAframesOrder()));
    connect(ui->actionRefreshAnimation,SIGNAL(triggered()),          this, SLOT(previewAnimation()));

    connect(ui->addImageButton,        SIGNAL(clicked()),            this, SLOT(addImageDialog()));
    connect(ui->removeImageButton,     SIGNAL(clicked()),            this, SLOT(removeSelImage()));
    connect(ui->refreshImgButton,      SIGNAL(clicked()),            this, SLOT(reloadSelImage()));
    connect(ui->addFrameButton,        SIGNAL(clicked()),            this, SLOT(addFrameDialog()));
    connect(ui->removeFrameButton,     SIGNAL(clicked()),            this, SLOT(removeSelFrame()));
    connect(ui->removeAllUnusedFramesButton, SIGNAL(clicked()),      this, SLOT(removeAllUnusedFrames()));
    connect(ui->addAniButton,          SIGNAL(clicked()),            this, SLOT(addAnimationDialog()));
    connect(ui->removeAniButton,       SIGNAL(clicked()),            this, SLOT(removeSelAnimation()));
    connect(ui->refreshAniButton,      SIGNAL(clicked()),            this, SLOT(previewAnimation()));
    connect(ui->addAframeButton,       SIGNAL(clicked()),            this, SLOT(addAframeDialog()));
    connect(ui->removeAframeButton,    SIGNAL(clicked()),            this, SLOT(removeSelAframe()));
    connect(ui->aniDecSpeedButton,     SIGNAL(clicked()),            this, SLOT(decAniSpeed()));
    connect(ui->aniIncSpeedButton,     SIGNAL(clicked()),            this, SLOT(incAniSpeed()));
    connect(ui->hideFramePreviewButton,SIGNAL(clicked()),            this, SLOT(hideShowFramePreview()));
    connect(ui->moveDownAframeButton,  SIGNAL(clicked()),            this, SLOT(moveSelAframeDown()));
    connect(ui->moveUpAframeButton,    SIGNAL(clicked()),            this, SLOT(moveSelAframeUp()));
    connect(ui->invertAframesButton,   SIGNAL(clicked()),            this, SLOT(invertAframesOrder()));

    connect(ui->addAniTransButton,     SIGNAL(clicked()),            this, SLOT(addTransDialog()));
    connect(ui->refreshTransButton,    SIGNAL(clicked()),            this, SLOT(previewTransition()));
    //TODO
    //connect(ui->removeAniTransButton,  SIGNAL(clicked()),            this, SLOT(()));
    //connect(ui->removeAllAniTransButton, SIGNAL(clicked()),          this, SLOT(()));
    //connect(ui->moveAniTransDownButton,SIGNAL(clicked()),            this, SLOT(()));
    //connect(ui->moveAniTransUpButton,  SIGNAL(clicked()),            this, SLOT(()));

    connect(ui->scaleImageButton,      SIGNAL(clicked()),            this, SLOT(scaleCheckedImages()));
    connect(ui->quickModeButton,       SIGNAL(clicked()),            this, SLOT(switchQuickMode()));
    connect(ui->checkAllImagesButton,  SIGNAL(clicked()),            this, SLOT(checkAllImages()));
    connect(ui->invertCheckedImagesButton,  SIGNAL(clicked()),       this, SLOT(invertCheckedImages()));
    connect(ui->createQuickAniButton,  SIGNAL(clicked()),            this, SLOT(createQuickAnimation()));

    connect(ui->landscapeCheckBox,     SIGNAL(stateChanged(int)),    this, SLOT(switchLandscapeMode()));
    connect(ui->previewScrSizeCombo,   SIGNAL(activated(QString)),   this, SLOT(changePreviewScrSize(const QString &)));

    connect(ui->imgPreview,            SIGNAL(mousePositionChanged(int,int)),  this, SLOT(showMousePosition(int,int)));
    connect(ui->framePreview,          SIGNAL(mousePositionChanged(int,int)),  this, SLOT(showMousePosition(int,int)));
    connect(ui->aframePreview,         SIGNAL(mousePositionChanged(int,int)),  this, SLOT(showMousePosition(int,int)));
    connect(ui->imgPreview,            SIGNAL(mouseRectChanging(const QRect&)),       this, SLOT(showMouseRect(const QRect&)));
    connect(ui->imgPreview,            SIGNAL(frameRectChanging(const QRect&)),       this, SLOT(updateCurrentFrame_ui(const QRect&)));
    connect(ui->imgPreview,            SIGNAL(mouseRectChangeFinished(const QRect&)), this, SLOT(blendFrameRect()));
    connect(ui->imgPreview,            SIGNAL(mouseRectChangeFinished(const QRect&)), this, SLOT(showMouseRect(const QRect&)));
    connect(ui->imgPreview,            SIGNAL(frameRectChangeFinished(const QRect&)), this, SLOT(updateCurrentFrame(const QRect&)));

    connect(ui->imgZoomInButton,       SIGNAL(clicked()),  ui->imgPreview,    SLOT(zoomIn()));
    connect(ui->imgZoomOutButton,      SIGNAL(clicked()),  ui->imgPreview,    SLOT(zoomOut()));
    connect(ui->actionClearGuides,     SIGNAL(triggered()),ui->imgPreview,    SLOT(clearGuides()));
    connect(ui->frameZoomInButton,     SIGNAL(clicked()),  ui->framePreview,  SLOT(zoomIn()));
    connect(ui->frameZoomOutButton,    SIGNAL(clicked()),  ui->framePreview,  SLOT(zoomOut()));
    connect(ui->aframeZoomInButton,    SIGNAL(clicked()),  ui->aframePreview, SLOT(zoomIn()));
    connect(ui->aframeZoomOutButton,   SIGNAL(clicked()),  ui->aframePreview, SLOT(zoomOut()));

    connect(ui->imgTableWidget,        SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(showSelImage(int)));
    connect(ui->framesTableWidget,     SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(showSelFrame(int)));
    connect(ui->aframesTableWidget,    SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(showSelAframe(int)));
    connect(ui->aniTableWidget,        SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(showAframes(int)));

    connect(ui->saveCustomHeaderButton,    SIGNAL(clicked()),                       this, SLOT(saveCustomHeader()));
    connect(ui->restoreCustomHeaderButton, SIGNAL(clicked()),                       this, SLOT(restoreCustomHeader()));

    cellChangedSignals(true);
    blendComboBoxSignals(true);
}

void MainWindow::cellChangedSignals(bool connected)
{
    if (connected) {
        connect(ui->imgTableWidget,       SIGNAL(cellChanged(int,int)), this, SLOT(updateImgTable(int,int)));
        connect(ui->framesTableWidget,    SIGNAL(cellChanged(int,int)), this, SLOT(updateFramesTable(int,int)));
        connect(ui->aframesTableWidget,   SIGNAL(cellChanged(int,int)), this, SLOT(updateAframesTable(int,int)));
        connect(ui->aniTableWidget,       SIGNAL(cellChanged(int,int)), this, SLOT(updateAniTable(int,int)));
    } else {
        disconnect(ui->imgTableWidget,    SIGNAL(cellChanged(int,int)), this, SLOT(updateImgTable(int,int)));
        disconnect(ui->framesTableWidget, SIGNAL(cellChanged(int,int)), this, SLOT(updateFramesTable(int,int)));
        disconnect(ui->aframesTableWidget,SIGNAL(cellChanged(int,int)), this, SLOT(updateAframesTable(int,int)));
        disconnect(ui->aniTableWidget,    SIGNAL(cellChanged(int,int)), this, SLOT(updateAniTable(int,int)));
    }
}

void MainWindow::blendComboBoxSignals(bool connected)
{
    if (connected) {
        connect(ui->blendModeComboBox,    SIGNAL(currentIndexChanged(int)), this, SLOT(setBlendPixmap()));
    } else {
        disconnect(ui->blendModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setBlendPixmap()));
    }
}

void MainWindow::initTables()
{
    QStringList headersList;

    /* input images table */

    ui->imgTableWidget->setRowCount(0);
    ui->imgTableWidget->setColumnCount(ColImageTotal);
    ui->imgTableWidget->setColumnWidth(ColImageId, 30);
    ui->imgTableWidget->setColumnWidth(ColImageCheckable, 30);
    ui->imgTableWidget->setColumnWidth(ColImageVisibleId, 30);
    ui->imgTableWidget->setColumnWidth(ColImageScale, 40);
    headersList << tr("Id") << tr("Ch") << tr("Id") << tr("Scale") << tr("Filename");
    ui->imgTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
    ui->imgTableWidget->setColumnHidden(ColImageCheckable, true);
#ifndef DEBUG_SHOW_ID_COLS
    ui->imgTableWidget->setColumnHidden(ColImageId, true);
#endif

    /* frames table */

    ui->framesTableWidget->setRowCount(0);
    ui->framesTableWidget->setColumnCount(ColFrameTotal);
    ui->framesTableWidget->setColumnWidth(ColFrameId, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameVisibleId, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameOx, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameOy, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameW, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameH, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameImgId, 50);
    ui->framesTableWidget->ignoreColumn(ColFrameVisibleId);
    ui->framesTableWidget->ignoreColumn(ColFrameImgId);
    headersList << tr("Id") << tr("Id") << tr("ox") << tr("oy") << tr("w") << tr("h") << tr("Img Id") << tr("Name");
    ui->framesTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->framesTableWidget->setColumnHidden(ColFrameId, true);
#endif

    /* animations table */

    ui->aniTableWidget->setRowCount(0);
    ui->aniTableWidget->setColumnCount(ColAniTotal);
    ui->aniTableWidget->setColumnWidth(ColAniId, 30);
    ui->aniTableWidget->setColumnWidth(ColAniName, 270);
    ui->aniTableWidget->setColumnWidth(ColAniFlags, 30);
    headersList << "Id" << "Name" << "Flags";
    ui->aniTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();

    /* animation frames table */

    ui->aframesTableWidget->setRowCount(0);
    ui->aframesTableWidget->setColumnCount(ColAframeTotal);
    ui->aframesTableWidget->setColumnWidth(ColAframeId, 30);
    ui->aframesTableWidget->setColumnWidth(ColAframeFrameId, 60);
    ui->aframesTableWidget->setColumnWidth(ColAframeOx, 30);
    ui->aframesTableWidget->setColumnWidth(ColAframeOy, 30);
    ui->aframesTableWidget->setColumnWidth(ColAframeSticky, 50);
    ui->aframesTableWidget->setColumnWidth(ColAframeDelay, 50);
    ui->aframesTableWidget->setColumnWidth(ColAframeAniId, 30);
    ui->aframesTableWidget->ignoreColumn(ColAframeFrameId);
    headersList << tr("Id") << tr("Frame Id") << tr("ox") << tr("oy") << tr("Sticky") << tr("Delay") << tr("Animation Id");
    ui->aframesTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->aframesTableWidget->setColumnHidden(ColAframeId, true);
    ui->aframesTableWidget->setColumnHidden(ColAframeAniId, true);
#endif

    /* transitions table */

    ui->transTableWidget->setRowCount(0);
    ui->transTableWidget->setColumnCount(ColTransTotal);
    ui->transTableWidget->setColumnWidth(ColTransAniId, 30);
    ui->transTableWidget->setColumnWidth(ColTransAniCheckable, 30);
    headersList << tr("Animation Id") << tr("Ch") << tr("Animation Name");
    ui->transTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->transTableWidget->setColumnHidden(ColTransAniId, true);
#endif

}

Id MainWindow::getImageId(int row)
{
    const QTableWidget *t = ui->imgTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColImageId)->text().toInt() : NullId;
}

Id MainWindow::getFrameId(int row)
{
    const QTableWidget *t = ui->framesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColFrameId)->text().toInt() : NullId;
}

Id MainWindow::getAframeId(int row)
{
    const QTableWidget *t = ui->aframesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColAframeId)->text().toInt() : NullId;
}

Id MainWindow::getAframeFrameId(int row)
{
    const QTableWidget *t = ui->aframesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColAframeFrameId)->text().toInt() : NullId;
}

Id MainWindow::getAframeAniId(int row)
{
    const QTableWidget *t = ui->aframesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColAframeAniId)->text().toInt() : NullId;
}

Id MainWindow::getFrameImgId(int row)
{
    const QTableWidget *t = ui->framesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColFrameImgId)->text().toInt() : NullId;
}

Id MainWindow::getAnimationId(int row)
{
    const QTableWidget *t = ui->aniTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColAniId)->text().toInt() : NullId;
}

Id MainWindow::getTransAniId(int row)
{
    const QTableWidget *t = ui->transTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColTransAniId)->text().toInt() : NullId;
}

Id MainWindow::selectedImgId()
{ return getImageId(ui->imgTableWidget->currentRow()); }

Id MainWindow::selectedFrameId()
{ return getFrameId(ui->framesTableWidget->currentRow()); }

Id MainWindow::selectedAniId()
{ return getAnimationId(ui->aniTableWidget->currentRow()); }

Id MainWindow::selectedAframeId()
{ return getAframeId(ui->aframesTableWidget->currentRow()); }


bool MainWindow::saveFile()
{
    bool success = true;

    if (_filename.isEmpty()) {
        success = saveAsFile();
    } else {
        SpriteStateError err;
        if (!_sprState.save(_filename, &err)) {
           infoDialog(tr("Cannot save") + _filename + ". " + SpriteState::errorMessage(err));
           success = false;
        }
    }

    return success;
}

bool MainWindow::saveAsFile()
{
    static QString lastDir = "";
    bool           success = true;
    QString        filename;

    filename = QFileDialog::getSaveFileName(
            this, tr("Save file"), lastDir, "*.lvks;; *.*");

    if (filename.isNull()) {
        success = false;
    } else {
        lastDir = QFileInfo(filename).absolutePath();

        SpriteStateError err;
        if (!_sprState.save(filename, &err)) {
           infoDialog(tr("Cannot save ") + filename + ". " + SpriteState::errorMessage(err));
           success = false;
        }
        setCurrentFile(filename);
    }
    return success;
}

DialogButton MainWindow::saveChangesDialog()
{
    QString msg;

    if (_filename.isEmpty()) {
        msg = tr("Save changes to file before closing?");
    } else {
        msg = tr("Save changes to file '") + _filename + tr("' before closing?");
    }

    DialogButton button =  yesNoCancelDialog(msg);

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
        if (saveChangesDialog() == CancelButton) {
            return;
        }
    }

    static QString lastDir = "";

    QString filename;

    filename = QFileDialog::getOpenFileName(
            this, tr("Open file"), lastDir, "*.lvks;; *.*");

    if (!filename.isNull()) {
        lastDir = QFileInfo(filename).absolutePath();
        openFile(filename);
    }
}

bool MainWindow::openFile_checkUnsaved(const QString& filename)
{
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton) {
            return false;
        }
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
        if (err) {
            *err = SpriteState::ErrFileDoesNotExist;
        }
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

    /* UI - tables and previews */

    refresh_ui();

    if (ui->imgTableWidget->rowCount() > 0) {
        ui->imgTableWidget->selectRow(0);
        showSelImage(0);
    } else {
        ui->imgPreview->setPixmap(QPixmap());
    }
    if (ui->framesTableWidget->rowCount() > 0) {
        // selectRow not working (?)
        //ui->framesTableWidget->selectRow(0);
        //showSelFrame(0);
    } else {
        ui->framePreview->setPixmap(QPixmap());
    }
    if (ui->aniTableWidget->rowCount() > 0) {
        ui->aniTableWidget->selectRow(0);
        showAframes(0);
    } else {
        clearPreviewAnimation();
    }
    if (ui->aframesTableWidget->rowCount() > 0) {
        ui->aframesTableWidget->selectRow(0);
        showSelAframe(0);
    } else {
        ui->aframePreview->setPixmap(QPixmap());
    }

    ui->customHeaderText->setPlainText(_sprState.getCustomHeader());

    return true;
}

void MainWindow::refresh_ui()
{
    refresh_imgTable();
    refresh_frameTable();
    refresh_aniTable();
    refresh_aframeTable();

    if (ui->aniPreview->isPlaying()) {
        previewAnimation();
    }
    if (ui->transPreview->isPlaying()) {
        previewTransition();
    }
}

void MainWindow::refresh_imgTable()
{
    cellChangedSignals(false);

    int row = ui->imgTableWidget->currentRow();
    int col = ui->imgTableWidget->currentColumn();

    ui->imgTableWidget->clearContents();
    ui->imgTableWidget->setRowCount(0);

    for (QMapIterator<Id, InputImage> it(_sprState.images()); it.hasNext();) {
        it.next();
        const InputImage& image =  it.value();
        addImage_ui(image);
    }

    ui->imgTableWidget->setCurrentCell(row, col);

    cellChangedSignals(true);
}

void MainWindow::refresh_frameTable()
{
    cellChangedSignals(false);

    int row = ui->framesTableWidget->currentRow();
    int col = ui->framesTableWidget->currentColumn();

    ui->framesTableWidget->clearContents();
    ui->framesTableWidget->setRowCount(0);

    for (QMapIterator<Id, LvkFrame> it(_sprState.frames()); it.hasNext();) {
        it.next();
        const LvkFrame& frame =  it.value();
        addFrame_ui(frame);
    }

    ui->framesTableWidget->setCurrentCell(row, col);

    cellChangedSignals(true);
}

void MainWindow::refresh_aniTable()
{
    cellChangedSignals(false);

    int row = ui->aniTableWidget->currentRow();
    int col = ui->aniTableWidget->currentColumn();

    ui->aniTableWidget->clearContents();
    ui->aniTableWidget->setRowCount(0);

    for (QMapIterator<Id, LvkAnimation> it(_sprState.animations()); it.hasNext();) {
        it.next();
        const LvkAnimation& ani =  it.value();
        addAnimation_ui(ani);
    }

    ui->aniTableWidget->setCurrentCell(row, col);

    cellChangedSignals(true);
}

void MainWindow::refresh_aframeTable()
{
    cellChangedSignals(false);

    int row = ui->aframesTableWidget->currentRow();
    int col = ui->aframesTableWidget->currentColumn();

    ui->aframesTableWidget->clearContents();
    ui->aframesTableWidget->setRowCount(0);

    int ani_row = ui->aniTableWidget->currentRow();
    if (ani_row != -1) {
        Id aniId = getAnimationId(ani_row);
        
        // TODO check why do I need to copy the aframes to iterate them
        // otherwise tha app crashes when undoing changes
        const QList<LvkAframe> aframes = _sprState.aframes(aniId);
        for (QListIterator<LvkAframe> it2(aframes); it2.hasNext();) {
            const LvkAframe& aframe =  it2.next();
            addAframe_ui(aframe, aniId);
        }
    }

    ui->aframesTableWidget->setCurrentCell(row, col);
    
    cellChangedSignals(true);
}

void MainWindow::storeRecentFile(const QString& filename)
{
    #define makeKey(str, i)         { str = KEY_RECENT_FILE; str.append(QString::number(i)); }

    QString key;

    /* search if filename is already stored */
    int found = -1;
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        makeKey(key, i);

        QString fileAlreadyStored = settings.value(key).toString();

        if (filename == fileAlreadyStored) {
            found = i;
            break;
        }
    }

    /* if found in the first position, nothing to do */
    if (found == 0) {
        return;
    }

    /* if not found, stored it in the last position */
    if (found == -1) {
        found = MAX_RECENT_FILES - 1;
        makeKey(key, found);
        settings.setValue(key, filename);
    }

    /* swap entries */
    QString key_;
    for (int i = found; i > 0; --i) {
        /* swap(i, i-1) */
        makeKey(key, i);
        makeKey(key_, i - 1);
        QString recentFile = settings.value(key).toString();
        QString recentFile_ = settings.value(key_).toString();
        settings.setValue(key_, recentFile);
        settings.setValue(key, recentFile_);
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

        if (!recentFile.isEmpty()) {
           addRecentFileMenu(recentFile);
        }
    }
}

void MainWindow::addRecentFileMenu(const QString& filename)
{
    ui->actionNoRecentFiles->setVisible(false);
    LvkAction* action = new LvkAction(filename);
    ui->actionOpenRecent->addAction(action);
    connect(action, SIGNAL(triggered(QString)), this, SLOT(openFile_checkUnsaved(QString)));
}

void MainWindow::closeFile_checkUnsaved()
{
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton) {
            return;
        }
    }
    closeFile();
}

void MainWindow::closeFile()
{
    _sprState.clear();
    setCurrentFile("");

    showFramesTab();

    cellChangedSignals(false);
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
    cellChangedSignals(true);

    ui->imgPreview->clear();
    ui->framePreview->clear();
    ui->aframePreview->clear();

    ui->customHeaderText->clear();

    clearPreviewAnimation();
    clearPreviewTransition();
}

void MainWindow::exportFile()
{
    if (_exportFileName.isEmpty()) {
        exportAsFile();
    } else {
        SpriteStateError err;
        if (!_sprState.exportSprite(_exportFileName, "", "", &err)) {
           infoDialog(tr("Cannot export '") + _exportFileName + "' "
                      + SpriteState::errorMessage(err));
           return;
        }
    }
}

void MainWindow::exportAsFile()
{
    static QString exportFileName = "";

    exportFileName = QFileDialog::getSaveFileName(
            this, tr("Export file"), QFileInfo(exportFileName).absolutePath(), "*.lkot *.lkob;; *.*");

    if (!exportFileName.isEmpty()) {
        SpriteStateError err;
        if (!_sprState.exportSprite(exportFileName, "", "", &err)) {
           infoDialog(tr("Cannot export '") + exportFileName + "' "
                      + SpriteState::errorMessage(err));
           return;
        }
        setCurrentExportFile(exportFileName);
    }
}

void MainWindow::setCurrentFile(const QString& filename)
{
    if (filename.isEmpty()) {
        _filename = "";
        _exportFileName = "";

        setWindowTitle(QString(APP_NAME));
    } else  {
        QFileInfo fileInfo(filename);

        _filename = fileInfo.absoluteFilePath();
        _exportFileName = "";

        setWindowTitle(QString(APP_NAME) + " - " + fileInfo.fileName());

        storeRecentFile(fileInfo.absoluteFilePath());
        // NOTE: Workaround to get the files ordered by date in the recent files menu
        // addRecentFileMenu(filename);
        ui->actionOpenRecent->clear();
        initRecentFilesMenu();

        qDebug() << "Info: changing current app dir to" << fileInfo.absolutePath();
        QDir::setCurrent(fileInfo.absolutePath());
    }
}

void MainWindow::setCurrentExportFile(const QString& exportFileName)
{
    _exportFileName = exportFileName;
}

void MainWindow::showFramesTab()
{
    ui->tabWidget->setCurrentWidget(ui->framesTab);
}

void MainWindow::showAnimationsTab()
{
    ui->tabWidget->setCurrentWidget(ui->animationsTab);
}

QString MainWindow::toRelativePath(const QString &filePath)
{
    const QString sep = QDir::separator();
    QFileInfo fileInfo(filePath);
    QStringList dirs1 = fileInfo.absolutePath().split(sep, QString::SkipEmptyParts);
    QStringList dirs2 = QDir::currentPath().split(sep, QString::SkipEmptyParts);

    QString relFilePath;

    int i = 0;
    int minSize = std::min(dirs1.size(), dirs2.size());

    for (; i < minSize && dirs1[i] == dirs2[i]; ++i)
        ;
    for (int j = i; j < dirs2.size(); ++j) {
        relFilePath.append(".." + sep);
    }
    for (int j = i; j < dirs1.size(); ++j) {
        relFilePath.append(dirs1[j] + sep);
    }
    relFilePath.append(fileInfo.fileName());

    return relFilePath;
}

void MainWindow::addImageDialog()
{
    showFramesTab();

    static QString lastDir = "";

    QStringList filenames;

    filenames = QFileDialog::getOpenFileNames(
            this, tr("Add Image"), lastDir,
            "Images(*.png *.jpg *.jpeg *.xpm *.xbm *.bmp *.tif *.tiff);; *.*");

    if (filenames.size() > 0) {
        lastDir = QFileInfo(filenames[0]).absolutePath();

        _sprState.startTransaction();
        for (int i = 0; i < filenames.size(); ++i) {
            addImage(InputImage(NullId, toRelativePath(filenames[i])));
        }
        _sprState.endTransaction();
    }
}

Id MainWindow::addImage(const InputImage& image)
{
    InputImage image_ = image;

    /* State */
    if (!image_.filename.isEmpty()) {
        _sprState.addImage(image_);
    }

    /* UI */
    addImage_ui(image_);

    return image_.id;
}

void MainWindow::addImage_ui(const InputImage& image)
{
    QString filename(image.filename);

    if (filename.isEmpty()) {
        infoDialog(tr("Empty Filename"));
        return;
    }
    if (!QFileInfo(filename).exists()) {
        infoDialog(tr("File '") + filename + tr("' does not exist"));
    } else if (QImage(filename).isNull()) {
        infoDialog(filename + tr(" has an invalid image format"));
    }

    int rows = ui->imgTableWidget->rowCount();

    QTableWidgetItem* item_id        = new QTableWidgetItem(QString::number(image.id));
    QTableWidgetItem* item_checkable = new QTableWidgetItem();
    QTableWidgetItem* item_vid       = new QTableWidgetItem(QString::number(image.id));
    QTableWidgetItem* item_filename  = new QTableWidgetItem(filename);
    QTableWidgetItem* item_scale     = new QTableWidgetItem(QString::number(image.scale()));

    item_checkable->setCheckState(Qt::Unchecked);

    cellChangedSignals(false);
    ui->imgTableWidget->setRowCount(rows+1);
    ui->imgTableWidget->setItem(rows, ColImageId,        item_id);
    ui->imgTableWidget->setItem(rows, ColImageCheckable, item_checkable);
    ui->imgTableWidget->setItem(rows, ColImageVisibleId, item_vid);
    ui->imgTableWidget->setItem(rows, ColImageFilename,  item_filename);
    ui->imgTableWidget->setItem(rows, ColImageScale,     item_scale);
    ui->imgTableWidget->setCurrentItem(item_id);
    cellChangedSignals(true);

    showImage(image.id);
}

void MainWindow::showSelImage(int row)
{
    Id imgId = (row == -1) ? NullId : getImageId(row);
    showImage(imgId);
}

void MainWindow::showImage(Id imgId, bool clearPixmapCache /*= false*/)
{
    if (clearPixmapCache) {
        ui->imgPreview->clearPixmapCache(imgId);
        ui->imgPreview->clear(); // FIXME
    }
    const QPixmap& selPixmap = _sprState.ipixmap(imgId);
    ui->imgPreview->setPixmap(selPixmap, imgId);
}

void MainWindow::showSelImageWithFrameRect(int row, const QRect& rect)
{
    showSelImage(row);
    ui->imgPreview->setFrameRect(rect);
}

void MainWindow::removeSelImage()
{
    showFramesTab();

    int currentRow = ui->imgTableWidget->currentRow();

    if (currentRow == -1) {
        infoDialog(tr("No image selected"));
        return;
    }

    QString imgFilename = ui->imgTableWidget->item(currentRow, ColImageFilename)->text();
    if (!yesNoDialog(tr("Are you sure you want to remove the image '") + imgFilename  + tr("'?"))) {
        return;
    }

    removeImage(currentRow);

    ui->imgPreview->setPixmap(QPixmap());
}

void MainWindow::removeImage(int row)
{
    Id imgId = getImageId(row);
    qDebug() << ui->imgTableWidget->item(row, ColImageId)->text();
    qDebug() << ui->imgTableWidget->item(row, ColImageId)->text().toInt();

    cellChangedSignals(false);
    ui->imgTableWidget->removeRow(row);
    cellChangedSignals(true);

    _sprState.removeImage(imgId);

    /* remove frames that use this image*/
    for (int r = 0; r < ui->framesTableWidget->rowCount(); ++r) {
        qDebug() << "row " << r << " -- frameImgId " << getFrameImgId(r) << " == imgId " << imgId;
        if (getFrameImgId(r) == imgId) {
            removeFrame(r);
        }
    }
}

void MainWindow::reloadSelImage()
{
    if (ui->imgTableWidget->currentRow() == -1)
    {
        return;
    }

    reloadImage(selectedImgId());
}

void MainWindow::reloadImage(Id imgId)
{
    _sprState.reloadImagePixmap(imgId);
    _sprState.reloadFramePixmaps(imgId);

    ui->imgPreview->clearPixmapCache(imgId);
    refreshPreviews();
}

Id MainWindow::getFrameDialog(const QString &title)
{
    if (_sprState.frames().isEmpty()) {
        infoDialog(tr("No frames available.\n\nGo to the \"Frames\" tab to create one frame."));
        return NullId;
    }

    QStringList framesList;

    for (QMapIterator<Id, LvkFrame> it(_sprState.frames()); it.hasNext();) {
        it.next();
        const LvkFrame& frame = it.value();
        framesList << tr("Id: ") + QString::number(frame.id) +  tr(" Name: ") + frame.name;
    }
    framesList.sort();

    Id frameId = NullId;
    bool ok;
    QString frame_str = QInputDialog::getItem(this, title, tr("Choose a frame:"),
                                              framesList, 0, false, &ok);
    if (ok) {
        QStringList tokens = frame_str.split(" ");

        if (tokens.size() >= 2) {
            frameId = tokens.at(1).toInt();
        } else {
            infoDialog(tr("The selected frame could not be parsed"));
        }
    }

    return frameId;
}

bool MainWindow::addFrameDialog(const QString & defaultName /*= ""*/, bool promptName /*= true*/)
{
    showFramesTab();

    QString name = defaultName;

    if (promptName) {
        bool ok;
        name = QInputDialog::getText(this, tr("New frame"), tr("Frame name:"),
                                           QLineEdit::Normal, defaultName, &ok);
        if (!ok) {
            return false;
        }
    }

    name = name.trimmed();
    if (!isValidFrameName(name, true)) {
        return false;
    }

    Id imgId = selectedImgId();
    if (addFrameFromMouseRect(imgId, name) != NullId) {
        return true;
    } else {
        return false;
    }
}

Id MainWindow::addFrameFromMouseRect(Id imgId, const QString &name)
{
    QRect frameRect = ui->imgPreview->mouseFrameRect();
    bool validRect = true;

    int ox;
    int oy;
    int w;
    int h;

    if (frameRect.isNull()) {
        /* add frame using the whole image */
        ox = 0;
        oy = 0;
        w  = _sprState.ipixmap(imgId).width();
        h  = _sprState.ipixmap(imgId).height();
    } else if (frameRect.width() == 0) {
        infoDialog(tr("Cannot add a frame with null width"));
        validRect = false;
    } else if (frameRect.height() == 0) {
        infoDialog(tr("Cannot add a frame with null height"));
        validRect = false;
    } else {
        /* add frame from selection in the input image */
        ox = frameRect.x();
        oy = frameRect.y();
        w  = frameRect.width();
        h  = frameRect.height();
    }

    if (validRect) {
        return addFrame(LvkFrame(NullId, imgId, ox, oy, w, h, name));
    } else {
        return NullId;
    }
}

Id MainWindow::addFrame(const LvkFrame &frame)
{
    LvkFrame frame_ = frame;

    /* state */

    _sprState.addFrame(frame_);

    /* UI */
    addFrame_ui(frame_);

    return frame_.id;
}

void MainWindow::addFrame_ui(const LvkFrame &frame)
{
    QTableWidgetItem* item_id   = new QTableWidgetItem(QString::number(frame.id));
    QTableWidgetItem* item_vid  = new QTableWidgetItem(QString::number(frame.id));
    QTableWidgetItem* item_ox   = new QTableWidgetItem(QString::number(frame.ox));
    QTableWidgetItem* item_oy   = new QTableWidgetItem(QString::number(frame.oy));
    QTableWidgetItem* item_w    = new QTableWidgetItem(QString::number(frame.w));
    QTableWidgetItem* item_h    = new QTableWidgetItem(QString::number(frame.h));
    QTableWidgetItem* item_iid  = new QTableWidgetItem(QString::number(frame.imgId));
    QTableWidgetItem* item_name = new QTableWidgetItem(frame.name);

    int rows = ui->framesTableWidget->rowCount();

    cellChangedSignals(false);
    ui->framesTableWidget->setRowCount(rows+1);
    ui->framesTableWidget->setItem(rows, ColFrameId,        item_id);
    ui->framesTableWidget->setItem(rows, ColFrameVisibleId, item_vid);
    ui->framesTableWidget->setItem(rows, ColFrameOx,        item_ox);
    ui->framesTableWidget->setItem(rows, ColFrameOy,        item_oy);
    ui->framesTableWidget->setItem(rows, ColFrameW,         item_w);
    ui->framesTableWidget->setItem(rows, ColFrameH,         item_h);
    ui->framesTableWidget->setItem(rows, ColFrameImgId,     item_iid);
    ui->framesTableWidget->setItem(rows, ColFrameName,      item_name);
    cellChangedSignals(true);

    showFrame(frame.id);

    ui->framesTableWidget->setFocus();
    ui->framesTableWidget->setCurrentCell(rows, ColFrameOx);
}

void MainWindow::showSelFrame(int row)
{
    Id frameId = (row == -1) ? NullId : getFrameId(row);
    showFrame(frameId);
}

void MainWindow::showFrame(Id frameId)
{
    /* show frame preview */
    const QPixmap& selPixmap = _sprState.fpixmap(frameId);
    ui->framePreview->setPixmap(selPixmap);

    /* show frame rect in the input image*/
    if (frameId == NullId) {
        showSelImageWithFrameRect(-1, QRect());
    } else {
        const LvkFrame frame = _sprState.const_frame(frameId);
        for (int r = 0; r < ui->imgTableWidget->rowCount(); r++) {
            if (frame.imgId == getImageId(r)) {
                ui->imgTableWidget->selectRow(r);
                showSelImageWithFrameRect(r, frame.rect());
                break;
            }
        }
        ui->imgPreview->scrollToFrame(frame);
    }
}

void MainWindow::removeSelFrame()
{
    showFramesTab();

    int currentRow = ui->framesTableWidget->currentRow();

    if (currentRow == -1) {
        infoDialog(tr("No frame selected"));
        return;
    }

    QString frameName = ui->framesTableWidget->item(currentRow, ColFrameName)->text();
    if (!yesNoDialog(tr("Are you sure you want to remove the frame '") + frameName  + tr("'?"))) {
        return;
    }

    removeFrame(currentRow);
}

void MainWindow::removeFrame(int row)
{
    Id frameId = getFrameId(row);

    cellChangedSignals(false);
    ui->framesTableWidget->removeRow(row);
    cellChangedSignals(true);

    ui->framePreview->setPixmap(QPixmap());

    _sprState.removeFrame(frameId);
}

void MainWindow::updateCurrentFrame(const QRect &rect)
{
    if (ui->framesTableWidget->currentRow() == -1) {
        return;
    }

    LvkFrame frame = _sprState.const_frame(selectedFrameId());
    frame.setRect(rect);
    _sprState.updateFrame(frame);

    updateCurrentFrame_ui(rect);
    showFrame(frame.id);
}

void MainWindow::updateCurrentFrame_ui(const QRect &rect)
{
    int currentRow = ui->framesTableWidget->currentRow();

    if (currentRow == -1) {
        return;
    }

    cellChangedSignals(false);
    ui->framesTableWidget->item(currentRow, ColFrameOx)->setText(QString::number(rect.x()));
    ui->framesTableWidget->item(currentRow, ColFrameOy)->setText(QString::number(rect.y()));
    ui->framesTableWidget->item(currentRow, ColFrameW)->setText(QString::number(rect.width()));
    ui->framesTableWidget->item(currentRow, ColFrameH)->setText(QString::number(rect.height()));
    cellChangedSignals(true);
}

void MainWindow::removeAllUnusedFrames()
{
    if (!yesNoDialog(tr("Are you sure you want to remove all unused animations?"))) {
        return;
    }

    QVector<int> unusedRows;

    // iterate frames and find all unused frames
    for (int row = 0; row < ui->framesTableWidget->rowCount(); ++row) {
        Id frameId = getFrameId(row);
        if (_sprState.isFrameUnused(frameId)) {
            unusedRows.append(row);
        }
    }

    _sprState.startTransaction();

    // iterate in reverse order to not alter row numbers after removing one row
    for (int i = unusedRows.size() - 1; i >= 0; --i) {
        removeFrame(unusedRows[i]);
    }
    _sprState.endTransaction();

    infoDialog(QString::number(unusedRows.size()) + tr(" unused frame(s) were removed."));
}

void MainWindow::addAnimationDialog()
{
    showAnimationsTab();

    bool ok;
    QString name = QInputDialog::getText(this, tr("New animation"),
                                         tr("Animation name:"),
                                         QLineEdit::Normal, "", &ok);
    if (ok) {
        name = name.trimmed();
        if (name.isEmpty())  {
            infoDialog(tr("Cannot add an animation without name"));
            return;
        }
        addAnimation(LvkAnimation(NullId, name));
    }
}

void MainWindow::hideFramePreview()
{
    ui->tabLayout->setColumnStretch(12, 0);     //1,0,2,0,0,0,0,0,0,0,0,0,*0*
    ui->hSpacerFramePreview->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Preferred);
    ui->frameZoomInButton->hide();
    ui->frameZoomOutButton->hide();
    ui->framePreviewScroll->hide();
    ui->blendModeComboBox->hide();
    ui->blendModeLabel->hide();
    ui->hideFramePreviewButton->setIcon(QIcon(":/buttons/button-show"));
    ui->hideFramePreviewButton->setToolTip(tr("Show frames preview"));
    ui->framePreviewLayout->update();
}

void MainWindow::showFramePreview()
{
    ui->tabLayout->setColumnStretch(12, 2); //1,0,2,0,0,0,0,0,0,0,0,0,*2*
    ui->hSpacerFramePreview->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->frameZoomInButton->show();
    ui->frameZoomOutButton->show();
    ui->framePreviewScroll->show();
    ui->blendModeComboBox->show();
    ui->blendModeLabel->show();
    ui->hideFramePreviewButton->setIcon(QIcon(":/buttons/button-hide"));
    ui->hideFramePreviewButton->setToolTip(tr("Hide frames preview"));
    ui->framePreviewLayout->update();
}

void MainWindow::hideShowFramePreview()
{
    static bool visible = false;

    if (visible) {
        hideFramePreview();
    } else {
        showFramePreview();
    }
    visible = !visible;
}

Id MainWindow::addAnimation(const LvkAnimation& ani)
{
    LvkAnimation ani_ = ani;

    /* state */
    _sprState.addAnimation(ani_);

    /* UI */
    addAnimation_ui(ani_);

    return ani_.id;
}

void MainWindow::addAnimation_ui(const LvkAnimation& ani)
{
    int rows = ui->aniTableWidget->rowCount();

    QTableWidgetItem* item_id   = new QTableWidgetItem(QString::number(ani.id));
    QTableWidgetItem* item_name = new QTableWidgetItem(ani.name);
    QTableWidgetItem* item_flags = new QTableWidgetItem(toHexString(ani.flags));

    cellChangedSignals(false);
    ui->aniTableWidget->setRowCount(rows+1);
    ui->aniTableWidget->setItem(rows, ColAniId,    item_id);
    ui->aniTableWidget->setItem(rows, ColAniName,  item_name);
    ui->aniTableWidget->setItem(rows, ColAniFlags, item_flags);
    ui->aniTableWidget->setCurrentItem(item_id);
    cellChangedSignals(true);

    showAframes(rows);
    clearPreviewAnimation();
}

void MainWindow::showAframes(int row)
{
    ui->aniPreview->stop();

    cellChangedSignals(false);

    ui->aframePreview->setEnabled(false);
    ui->aframesTableWidget->setEnabled(false);
    ui->aframesTableWidget->clearContents();
    ui->aframesTableWidget->setRowCount(0);

    if (row == -1) {
        return;
    }

    int animationId = getAnimationId(row);
    LvkAnimation ani = _sprState.animations().value(animationId);
    for (QListIterator<LvkAframe> it(ani._aframes); it.hasNext();){
        LvkAframe aFrame = it.next();
        addAframe_ui(aFrame,animationId);
    }

    if (ui->aframesTableWidget->rowCount() > 0) {
        ui->aframesTableWidget->selectRow(0);
        showSelAframe(0);
    }

    ui->aframePreview->setEnabled(true);
    ui->aframesTableWidget->setEnabled(true);

    cellChangedSignals(true);

    previewAnimation(); /* automatic preview */

}

void MainWindow::refreshPreviews()
{
    showSelImage(ui->imgTableWidget->currentRow());
    showSelFrame(ui->framesTableWidget->currentRow());
    showSelAframe(ui->aframesTableWidget->currentRow());
    previewAnimation();
    previewTransition();
}

void MainWindow::previewAnimation()
{
    if (ui->aniTableWidget->currentRow() == -1) {
        //infoDialog(tr("No animation selected"));
        return;
    }

    LvkAnimation selectedAni = _sprState.animations().value(selectedAniId());
    ui->aniPreview->setAnimation(selectedAni, _sprState.fpixmaps());
    ui->aniPreview->play();
}

void MainWindow::clearPreviewAnimation()
{
    ui->aniPreview->clear();
}

void MainWindow::previewTransition()
{
    QList<LvkAnimation> aniList;
    for (int row = 0; row < ui->transTableWidget->rowCount(); row++) {
        aniList << _sprState.animations().value(getTransAniId(row));
    }
    ui->transPreview->setAnimations(aniList, _sprState.fpixmaps());
    ui->transPreview->play();
}

void MainWindow::clearPreviewTransition()
{
    ui->transPreview->clear();
}

void MainWindow::switchLandscapeMode()
{
    changePreviewScrSize(ui->previewScrSizeCombo->currentText());
}

void MainWindow::changePreviewScrSize(const QString &text)
{
    QString res = text.mid(0, text.indexOf(' ')); // Remove comment and keep only resolution
    bool ok = false;

    bool custom = false;
    if (res == tr("Custom...")) {
        custom = true;
        res = QInputDialog::getText(this,
                              tr("Insert custom screen resolution"),
                              tr("Insert custom screen resolution in format <width>x<height>"),
                              QLineEdit::Normal, "", &ok);
        if (!ok) {
            return;
        }
    }

    QStringList split = res.split("x");

    if (split.size() != 2) {
        infoDialog(tr("Invalid resolution. Use <width>x<height>."));
        return;
    }

    int w = QString(split.at(0)).toInt(&ok);
    if (!ok || w <= 0) {
        infoDialog(tr("Invalid width size."));
        return;
    }

    int h = QString(split.at(1)).toInt(&ok);
    if (!ok || h <= 0) {
        infoDialog(tr("Invalid height size."));
        return;
    }

    if (ui->landscapeCheckBox->isChecked()) {
        ui->aniPreview->setScreenSize(h, w);
        ui->transPreview->setScreenSize(h, w);
    } else {
        ui->aniPreview->setScreenSize(w, h);
        ui->transPreview->setScreenSize(w, h);
    }

    if (custom) {
        bool found = false;
        for (int i = 0; i < ui->previewScrSizeCombo->count(); ++i) {
            if (res == ui->previewScrSizeCombo->itemText(i)) {
                found = true;
                break;
            }
        }
        if (!found) {
            ui->previewScrSizeCombo->addItem(res);
        }
    }
}

void MainWindow::removeSelAnimation()
{
    showAnimationsTab();

    int currentRow = ui->aniTableWidget->currentRow();

    if (currentRow == -1) {
        infoDialog(tr("No animation selected"));
        return;
    }

    QString aniName = ui->aniTableWidget->item(currentRow, ColAniName)->text();
    if (!yesNoDialog(tr("Are you sure you want to remove the animation '") + aniName  + tr("'?"))) {
        return;
    }

    removeAnimation(currentRow);

    showAframes(ui->aniTableWidget->currentRow());
}

void MainWindow::removeAnimation(int row)
{
    Id aniId = getAnimationId(row);

    cellChangedSignals(false);
    ui->aniTableWidget->removeRow(row);
    cellChangedSignals(true);

    clearPreviewAnimation();

    _sprState.removeAnimation(aniId);
}

void MainWindow::addAframeDialog()
{
    if (ui->aniTableWidget->currentRow() == -1) {
        infoDialog(tr("No animation selected"));
        return;
    }

    Id frameId = getFrameDialog(tr("Add animation frame"));
    if (frameId != NullId) {
        addAframe(LvkAframe(NullId, frameId), selectedAniId());
    }
}


Id MainWindow::addAframe(const LvkAframe& aframe, Id aniId)
{
    LvkAframe aframe_ = aframe;

    /* state */
    _sprState.addAframe(aframe_, aniId);
    
    /* UI */
    addAframe_ui(aframe_, aniId);

    return aframe_.id;
}

void MainWindow::addAframe_ui(const LvkAframe& aframe, Id aniId)
{
    QTableWidgetItem* item_id     = new QTableWidgetItem(QString::number(aframe.id));
    QTableWidgetItem* item_fid    = new QTableWidgetItem(QString::number(aframe.frameId));
    QTableWidgetItem* item_delay  = new QTableWidgetItem(QString::number(aframe.delay));
    QTableWidgetItem* item_sticky = new QTableWidgetItem(QString::number(aframe.sticky));
    QTableWidgetItem* item_ox     = new QTableWidgetItem(QString::number(aframe.ox));
    QTableWidgetItem* item_oy     = new QTableWidgetItem(QString::number(aframe.oy));
    QTableWidgetItem* item_aniId  = new QTableWidgetItem(QString::number(aniId));

    int rows = ui->aframesTableWidget->rowCount();

    cellChangedSignals(false);
    ui->aframesTableWidget->setRowCount(rows+1);
    ui->aframesTableWidget->setItem(rows, ColAframeId,      item_id);
    ui->aframesTableWidget->setItem(rows, ColAframeFrameId, item_fid);
    ui->aframesTableWidget->setItem(rows, ColAframeDelay,   item_delay);
    ui->aframesTableWidget->setItem(rows, ColAframeSticky,  item_sticky);
    ui->aframesTableWidget->setItem(rows, ColAframeOx,      item_ox);
    ui->aframesTableWidget->setItem(rows, ColAframeOy,      item_oy);
    ui->aframesTableWidget->setItem(rows, ColAframeAniId,   item_aniId);
    ui->aframesTableWidget->setCurrentItem(item_id);
    cellChangedSignals(true);

    showAframe(aframe.id);

    previewAnimation();

    ui->aframesTableWidget->setFocus();
    ui->aframesTableWidget->setCurrentCell(rows, ColAframeFrameId);
}


void MainWindow::showSelAframe(int row)
{
    Id frameId = (row == -1) ? NullId : getAframeFrameId(row);
    showAframe(frameId);
}

void MainWindow::showAframe(Id frameId)
{
    const QPixmap& selPixmap = _sprState.fpixmap(frameId);
    int w = selPixmap.width();
    int h = selPixmap.height();

    ui->aframePreview->setPixmap(selPixmap);
    ui->aframePreview->setGeometry(0, 0, w, h);
    ui->aframePreview->updateGeometry();
}

void MainWindow::removeSelAframe()
{
    showAnimationsTab();

    int currentRow = ui->aframesTableWidget->currentRow();

    if (currentRow == -1) {
        infoDialog(tr("No frame selected"));
        return;
    }

    if (!yesNoDialog(tr("Are you sure you want to remove the selected frame?"))) {
        return;
    }

    removeAframe(currentRow);
}

void MainWindow::removeAframe(int row)
{
    Id frameId = getAframeId(row);
    Id aniId = selectedAniId();
    _sprState.removeAframe(frameId, aniId);

    cellChangedSignals(false);
    ui->aframesTableWidget->removeRow(row);
    cellChangedSignals(true);

    ui->aframePreview->setPixmap(QPixmap());

    previewAnimation();
}

void MainWindow::moveSelAframeUp()
{
    moveSelAframe(1);
}

void MainWindow::moveSelAframeDown()
{
    moveSelAframe(-1);
}

void MainWindow::moveSelAframe(int offset)
{
    LvkTableWidget *table = ui->aframesTableWidget;

    if (table->currentRow() == -1) {
        infoDialog(tr("No frame selected"));
        return;
    } else if (ui->aniTableWidget->currentRow() == -1) {
        infoDialog(tr("No animation selected"));
        return;
    }

    int currentRow = table->currentRow();
    int targetRow = table->currentRow() - offset;

    if (targetRow < 0 || targetRow >= table->rowCount()) {
        return;
    }


    LvkAnimation ani = _sprState.const_animation(selectedAniId());
    ani.swapAframes(getAframeId(currentRow), getAframeId(targetRow));
    _sprState.updateAnimation(ani);

    cellChangedSignals(false);
    table->swapRows(currentRow, targetRow);
    cellChangedSignals(true);

    previewAnimation();
    table->setCurrentCell(targetRow, table->currentColumn());
}

void MainWindow::invertAframesOrder()
{
    LvkTableWidget *table = ui->aframesTableWidget;
    LvkAnimation ani = _sprState.const_animation(selectedAniId());
    int rowCount = table->rowCount();

    cellChangedSignals(false);
    for (int r = 0; r < rowCount/2; r++) {
        int r2 = rowCount - r - 1;
        ani.swapAframes(getAframeId(r), getAframeId(r2));
        table->swapRows(r, r2);
    }
    cellChangedSignals(true);

    _sprState.updateAnimation(ani);

    previewAnimation();
}

void MainWindow::setBlendPixmap()
{
    blendComboBoxSignals(false);

    switch (ui->blendModeComboBox->currentIndex()) {
    case BlendNone:
        blendNone();
        break;
    case BlendExistentFrame:
        blendExistentFrame();
        break;
    case BlendFrameRect:
        blendFrameRect();
        break;
    case BlendFrameId:
        blendFrameId();
        break;
    default:
        blendNone();
        break;
    }

    blendComboBoxSignals(true);

    ui->framePreview->repaint();
}

void MainWindow::blendNone()
{
    ui->framePreview->setBlendPixmap(QPixmap());
}

void MainWindow::blendExistentFrame()
{
    Id frameId = getFrameDialog("Blend pixmap");
    if (frameId != NullId) {
        _blendFrameId = frameId;
        ui->framePreview->setBlendPixmap(_sprState.fpixmap(frameId));

        // add new item in combo box with current frame Id
        if (ui->blendModeComboBox->count() == BlendModeTotal - 1) {
            ui->blendModeComboBox->addItem("");
        }

        // update combo box
        if (ui->blendModeComboBox->count() == BlendModeTotal) {
            ui->blendModeComboBox->setItemText(BlendFrameId, tr("Selected frame with frame id ") + QString::number(frameId));
            ui->blendModeComboBox->setCurrentIndex(BlendFrameId);
        }
    } else { // if cancel button
        blendNone();
        ui->blendModeComboBox->setCurrentIndex(BlendNone);
    }
}

void MainWindow::blendFrameRect()
{
    // FIXME this method is doing two things: Updating the blend pixmap
    // if blendModeComboBox == BlendFrameRect, or udpating the
    // main pixmap if blendModeComboBox == BlendNone

    QRect frameRect = ui->imgPreview->mouseFrameRect();

    // blend only if it's a valid rect
    if (!frameRect.isNull() && frameRect.width() > 0 && frameRect.height() > 0) {
        Id imgId = selectedImgId();
        if (imgId != NullId) {
            const QPixmap & imgPixmap = _sprState.images().value(imgId).pixmap;
            QPixmap mouseRectPixmap(imgPixmap.copy(frameRect));

            // this method may also be invoked from a signal, we need to check this:
            if (ui->blendModeComboBox->currentIndex() == BlendFrameRect) {
                ui->framePreview->setBlendPixmap(mouseRectPixmap);
            } else if (ui->blendModeComboBox->currentIndex() == BlendNone){
                ui->framePreview->setPixmap(mouseRectPixmap);
            }
           ui->framePreview->repaint();
        }
    }
}

void MainWindow::blendFrameId()
{
    // Check if frame id is still valid. e.g. it was not removed.
    if (_blendFrameId != NullId && _sprState.fpixmaps().contains(_blendFrameId)) {
        ui->framePreview->setBlendPixmap(_sprState.fpixmap(_blendFrameId));
    } else {
        infoDialog(tr("Frame id ") + QString::number(_blendFrameId) + tr(" is no longer available"));
        _blendFrameId = NullId;
    }

    // if frame id is not valid, then udpate combo box
    if (_blendFrameId == NullId) {
        ui->blendModeComboBox->removeItem(BlendFrameId);
        ui->blendModeComboBox->setCurrentIndex(BlendNone);
    }
}

void MainWindow::incAniSpeed(int ms)
{
    if (ui->aniTableWidget->currentRow() == -1) {
        infoDialog(tr("No animation selected"));
        return;
    }

    Id aniId = selectedAniId();

    for (int r = 0; r < ui->aframesTableWidget->rowCount(); ++r) {
        LvkAframe aframe = _sprState.const_aframe(aniId, getAframeId(r));
        aframe.delay -= ms;
        if (aframe.delay < 0) {
            aframe.delay = 0;
        }
        _sprState.updateAframe(aframe, aniId);
        ui->aframesTableWidget->item(r, ColAframeDelay)->setText(QString::number(aframe.delay));
    }
    previewAnimation();
}

void MainWindow::decAniSpeed(int ms)
{
    incAniSpeed(-ms);
}

void MainWindow::addTransDialog()
{
    if (_sprState.animations().isEmpty()) {
        infoDialog(tr("No animations available.\n\nGo to the \"Animations\" tab to create one animation."));
        return;
    }

    QStringList anisList;

    for (QMapIterator<Id, LvkAnimation> it(_sprState.animations()); it.hasNext();) {
        it.next();
        const LvkAnimation & ani = it.value();
        anisList << tr("Id: ") + QString::number(ani.id) +  tr(" Name: ") + ani.name;
    }
    anisList.sort();

    bool ok;
    QString ani_str = QInputDialog::getItem(this, tr("Add animation"),
                                            tr("Choose animation:"),
                                            anisList, 0, false, &ok);
    if (ok) {
        QStringList tokens = ani_str.split(" ");

        if (tokens.size() < 2) {
            infoDialog(tr("Cannot add animation. The selected animation could not be parsed"));
            return;
        }
        Id aniId = tokens.at(1).toInt();

        addTrans(aniId);
    }
}

void MainWindow::addTrans(Id aniId)
{
    /* state */
    // TODO: _sprState.addAniTrans(aniId);

    /* UI */
    addTrans_ui(aniId);
}

void MainWindow::addTrans_ui(Id aniId)
{
    LvkAnimation ani = _sprState.animations().value(aniId);

    QTableWidgetItem* item_aniId      = new QTableWidgetItem(QString::number(ani.id));
    QTableWidgetItem* item_aniName    = new QTableWidgetItem(ani.name);
    QTableWidgetItem* item_aniChecked = new QTableWidgetItem(""); // FIXME

    int rows = ui->transTableWidget->rowCount();

    ui->transTableWidget->setRowCount(rows+1);
    ui->transTableWidget->setItem(rows, ColTransAniId,        item_aniId);
    ui->transTableWidget->setItem(rows, ColTransAniName,      item_aniName);
    ui->transTableWidget->setItem(rows, ColTransAniCheckable, item_aniChecked);
    ui->transTableWidget->setCurrentItem(item_aniId);

    previewTransition();
}

void MainWindow::updateImgTable(int row, int col)
{
    QTableWidget* table    = ui->imgTableWidget;
    QString       newValue = getItem(table, row, col);
    Id            imgId    = getIdItem(table, row, ColImageId);
    InputImage    img      = _sprState.const_image(imgId);

    bool ok = true;
    double newScale = 0;

    if (col == ColImageScale) {
        newScale = newValue.toDouble(&ok);
    }

    switch (col) {
    case ColImageId:
    case ColImageVisibleId:
        infoDialog(tr("Column \"Id\" is not editable"));
        setItem(table, row, col, img.id);
        break;
    case ColImageFilename:
        if (newValue.isEmpty()) {
            infoDialog(tr("Image filename cannot be empty"));
            setItem(table, row, col, img.filename);
        } else if (newValue.contains(',')) {
            infoDialog(tr("Image filename cannot contain the character ','"));
            setItem(table, row, col, img.filename);
        } else if (newValue != img.filename) {
            img.filename = newValue;
            img.reloadImage();
            if (!QFileInfo(newValue).exists()) {
                infoDialog(tr("The file does not exist"));
            } else if (img.pixmap.isNull()) {
                infoDialog(tr("The file contains an invalid image format"));
            }
            _sprState.updateImage(img);
            setItem(table, row, col, img.filename);
        }
        break;
    case ColImageScale:
        if (!ok) {
            infoDialog(tr("Invalid image scale"));
            setItem(table, row, col, img.scale());
        } else if (newScale != img.scale()) {
            img.scale(newScale);
            setItem(table, row, col, newScale);
        }
        break;
    }

    if (ok) {
        switch (col) {
        case ColImageId:
        case ColImageVisibleId:
            //nothing to do
            break;
        case ColImageFilename:
        case ColImageScale:
                _sprState.updateImage(img);
                ui->imgPreview->clearPixmapCache(img.id);
                refreshPreviews();
            break;
        }
    }
}

void MainWindow::updateFramesTable(int row, int col)
{
    QTableWidget* table    = ui->framesTableWidget;
    QString       newValue = getItem(table, row, col);
    Id            frameId  = getIdItem(table, row, ColFrameId);
    LvkFrame      frame    = _sprState.const_frame(frameId);

    bool ok = true;
    int i = 0;

    if (col != ColFrameName) {
        i = newValue.toInt(&ok);
    }

    switch (col) {
    case ColFrameId:
    case ColFrameVisibleId:
        ok = false;
        infoDialog(tr("Column \"Id\" is not editable"));
        setItem(table, row, col, frame.id);
        break;
    case ColFrameImgId:
        if (ok && _sprState.images().contains(i)) {
            frame.imgId = i;
        } else {
            infoDialog(tr("Invalid image Id"));
            setItem(table, row, col, frame.imgId);
        }
        break;
    case ColFrameOx:
        if (ok) {
            frame.ox = i;
        } else {
            infoDialog(tr("Invalid frame offset."));
            setItem(table, row, col, frame.ox);
        }
        break;
    case ColFrameOy:
        if (ok) {
            frame.oy = i;
        } else {
            infoDialog(tr("Invalid frame offset."));
            setItem(table, row, col, frame.oy);
        }
        break;
    case ColFrameW:
        if (ok) {
            frame.w = i;
        } else {
            infoDialog(tr("Invalid frame width."));
            setItem(table, row, col, frame.w);
        }
        break;
    case ColFrameH:
        if (ok) {
            frame.h = i;
        } else {
            infoDialog(tr("Invalid frame height."));
            setItem(table, row, col, frame.h);
        }
        break;
    case ColFrameName:
        if (newValue.isEmpty()) {
            ok = false;
            infoDialog(tr("Frame name cannot be empty"));
        } else if (newValue.contains(',')) {
            ok = false;
            infoDialog(tr("Frame name cannot contain the character ','"));
        }
        if (ok) {
            frame.name = newValue;
        } else {
            setItem(table, row, col, frame.name);
        }
        break;
    }

    if (ok) {
        switch (col) {
        case ColFrameOx:
        case ColFrameOy:
        case ColFrameW:
        case ColFrameH:
            ui->imgPreview->setFrameRect(frame.rect());
        case ColFrameImgId:
        case ColFrameName:
            _sprState.updateFrame(frame);
            showFrame(frame.id);
            break;
        default:
            break;
        }
    }
}

void MainWindow::updateAframesTable(int row, int col)
{
    QTableWidget* table    = ui->aframesTableWidget;
    QString       newValue = getItem(table, row, col);
    Id            aniId    = getIdItem(table, row, ColAframeAniId);
    Id            aframeId = getIdItem(table, row, ColAframeId);
    LvkAframe     aframe   = _sprState.const_aframe(aniId, aframeId);

    bool ok = true;
    int i = newValue.toInt(&ok);

    switch (col) {
    case ColAframeId:
        ok = false;
        infoDialog(tr("Column \"Id\" is not editable"));
        //setItem(table, row, col, aframe.id);
        break;
    case ColAframeAniId:
        ok = false;
        infoDialog(tr("Column \"Animation Id\" is not editable."));
        setItem(table, row, col, aniId);
        break;
    case ColAframeFrameId:
        if (ok && _sprState.frames().contains(i)) {
            aframe.frameId = i;
        } else {
            infoDialog(tr("Invalid frame id."));
            setItem(table, row, col, aframe.frameId);
        }
        break;
    case ColAframeOx:
        if (ok) {
            aframe.ox = i;
        } else {
            infoDialog(tr("Invalid frame offset."));
            setItem(table, row, col, aframe.ox);
        }
        break;
    case ColAframeOy:
        if (ok) {
            aframe.oy = i;
        } else {
            infoDialog(tr("Invalid frame offset."));
            setItem(table, row, col, aframe.oy);
        }
        break;
    case ColAframeSticky:
        if (ok && (i == 0 || i == 1)) {
            aframe.sticky = i;
        } else {
            infoDialog(tr("Invalid sticky value. Use: 1 = On, 0 = Off"));
            setItem(table, row, col, aframe.sticky);
        }
        break;
    case ColAframeDelay:
        if (ok && i >= 0) {
            aframe.delay = i;
        } else {
            infoDialog(tr("Invalid frame delay."));
            setItem(table, row, col, aframe.delay);
        }
        break;
    }

    if (ok) {
        _sprState.updateAframe(aframe, aniId);
        if (ui->aniPreview->isPlaying()) {
            previewAnimation(); /* force refresh animation */
        }
    }
}

void MainWindow::updateAniTable(int row, int col)
{
    QTableWidget* table    = ui->aniTableWidget;
    QString       newValue = getItem(table, row, col);
    Id            aniId    = getIdItem(table, row, ColAniId);
    LvkAnimation  ani      = _sprState.const_animation(aniId);

    switch (col) {
    case ColAniId:
        infoDialog(tr("Column \"Id\" is not editable"));
        //setItem(table, row, col, ani.id);
        break;
    case ColAniName:
        if (newValue.isEmpty()) {
            infoDialog(tr("Animation name cannot be empty"));
            setItem(table, row, col, ani.name);
        } else if (newValue.contains(',')) {
            infoDialog(tr("Animation name cannot contain the character ','"));
            setItem(table, row, col, ani.name);
        } else {
            ani.name = newValue;
            _sprState.updateAnimation(ani);
        }
        break;
    case ColAniFlags:
        if (newValue.isEmpty()) {
            ani.flags = 0;
            _sprState.updateAnimation(ani);
            setItem(table, row, col, toHexString(ani.flags));
        } else {
            bool ok = false;
            unsigned flags = newValue.toInt(&ok, 16);
            if (ok) {
                ani.flags = flags;
                _sprState.updateAnimation(ani);
            } else {
                infoDialog(tr("Animation flags must be a 32 bits hex number"));
            }
            setItem(table, row, col, toHexString(ani.flags));
        }
        break;
    }
}

void MainWindow::switchQuickMode()
{
    if (ui->quickModeButton->isChecked()) {
        ui->imgTableWidget->setColumnHidden(ColImageCheckable, false);
        ui->createQuickAniButton->setVisible(true);
        ui->checkAllImagesButton->setVisible(true);
        ui->invertCheckedImagesButton->setVisible(true);
        ui->scaleImageButton->setVisible(true);
    } else {
        ui->imgTableWidget->setColumnHidden(ColImageCheckable, true);
        ui->createQuickAniButton->setVisible(false);
        ui->checkAllImagesButton->setVisible(false);
        ui->invertCheckedImagesButton->setVisible(false);
        ui->scaleImageButton->setVisible(false);
    }
}

void MainWindow::checkAllImages()
{
    for (int row = 0; row < ui->imgTableWidget->rowCount(); ++row) {
            ui->imgTableWidget->item(row, ColImageCheckable)->setCheckState(Qt::Checked);
    }
}


void MainWindow::invertCheckedImages()
{
    for (int row = 0; row < ui->imgTableWidget->rowCount(); ++row) {
        if (ui->imgTableWidget->item(row, ColImageCheckable)->checkState() == Qt::Checked) {
            ui->imgTableWidget->item(row, ColImageCheckable)->setCheckState(Qt::Unchecked);
        } else {
            ui->imgTableWidget->item(row, ColImageCheckable)->setCheckState(Qt::Checked);
        }
    }
}

bool MainWindow::hasImagesChecked()
{
    for (int row = 0; row < ui->imgTableWidget->rowCount(); ++row) {
        if (ui->imgTableWidget->item(row, ColImageCheckable)->checkState() == Qt::Checked) {
            return true;
        }
    }
    return false;
}

void MainWindow::scaleCheckedImages()
{
    if (!hasImagesChecked()) {
        infoDialog(tr("This operation requires at least one image selected"));
        return;
    }

    // prompt animation name
    bool ok;
    QString input = QInputDialog::getText(this, tr("Scale all images"), tr("Scale size:"),
                                          QLineEdit::Normal, "", &ok);
    if (!ok) {
        return;
    }

    // validate input
    double scale = input.toDouble(&ok);
    if (!ok || scale <= 0) {
        infoDialog(tr("Invalid scale. The scale must be a floating point number greater than zero."));
        return;
    }

    // scale checked images
    for (int row = 0; row < ui->imgTableWidget->rowCount(); ++row) {
        if (ui->imgTableWidget->item(row, ColImageCheckable)->checkState() == Qt::Checked) {
            InputImage img = _sprState.const_image(getImageId(row));
            img.scale(scale);
            _sprState.updateImage(img);
            ui->imgPreview->clearPixmapCache(img.id);
            setItem(ui->imgTableWidget, row, ColImageScale, scale);
        }
    }
    refreshPreviews();
}

void MainWindow::createQuickAnimation()
{
    if (!hasImagesChecked()) {
        infoDialog(tr("This operation requires at least one image selected"));
        return;
    }

    // prompt animation name
    bool ok;
    QString aniName = QInputDialog::getText(this, tr("New animation"), tr("Animation name:"),
                                            QLineEdit::Normal, "", &ok);
    aniName = aniName.trimmed();
    if (!ok || !isValidAniName(aniName, true)) {
        return;
    }

    bool addReverseFrames = yesNoDialog(tr("After finishing the animationm,"
                                           "do you want to add frames to reverse the animation?"));

    _sprState.startTransaction();

    // add new animation
    Id aniId = addAnimation(LvkAnimation(NullId, aniName));

    QList<Id> newFrameIds; // list of new ids to reverse animation

    // create new frame for each selected img. Add frame to animation
    for (int row = 0; row < ui->imgTableWidget->rowCount(); ++row) {
        if (ui->imgTableWidget->item(row, ColImageCheckable)->checkState() == Qt::Checked) {
            QString frameName = aniName + "_frame_" + QString::number(row);
            // add frame
            Id frameId = addFrameFromMouseRect(getImageId(row), frameName);
            // add aframe if not null
            if (frameId != NullId) {
                addAframe(LvkAframe(NullId, frameId), aniId);
            }
            newFrameIds << frameId;
        }
    }

    // add reverse frames
    if (addReverseFrames) {
        for (int i = newFrameIds.size() - 1; i >= 0 ; --i) {
            addAframe(LvkAframe(NullId, newFrameIds[i]), aniId);
        }
    }

    _sprState.endTransaction();
}

void MainWindow::showMousePosition(int x, int y)
{
    statusBarMousePos->setText(tr("Mouse x,y: ") + QString::number(x) + "," + QString::number(y));
}

void MainWindow::showMouseRect(const QRect& rect)
{
    int x = rect.x();
    int y = rect.y();
    int w = rect.width();
    int h = rect.height();

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

void MainWindow::showLoadProgress(const QString &progress)
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
        refresh_ui();
    }
}

void MainWindow::redo()
{
    if (ui->tabWidget->currentWidget() == ui->transitionsTab) {
        infoDialog(tr("Actions in the \"Transitions\" tab cannot be undone or redone"));
    } else if (_sprState.canRedo()) {
        _sprState.redo();
        refresh_ui();
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
    msg.setIconPixmap(QPixmap(":/icons/app-icon-128x128"));;
    msg.exec();
}

void MainWindow::exit()
{
    if (_sprState.hasUnsavedChanges()) {
        if (saveChangesDialog() == CancelButton) {
            return;
        }
    }
    QCoreApplication::exit(0);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    exit();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        ui->imgPreview->update();
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        ui->imgPreview->update();
    }
}

MainWindow::~MainWindow()
{
    delete statusBarRectSize;
    delete statusBarMousePos;
    delete ui;
}

