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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qrecentfileaction.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkaframe.h"
#include "lvkframegraphicsgroup.h"
#include "version.h"
#include <stdio.h>

/// imgTableWidget columns
enum {
    ColImageId          = 0,
    ColImageFilename    = 1,
};

/// framesTableWidget colums
enum {
    ColFrameId          = 0,
    ColFrameOx          = 1,
    ColFrameOy          = 2,
    ColFrameW           = 3,
    ColFrameH           = 4,
    ColFrameName        = 5,
    ColFrameImgId       = 6,
};

/// aniTableWidget columns
enum {
    ColAniId            = 0,
    ColAniName          = 1,
};

/// aframesTableWidget columns
enum {
    ColAframeId         = 0,
    ColAframeFrameId    = 1,
    ColAframeOx         = 2,
    ColAframeOy         = 3,
    ColAframeDelay      = 4,
    ColAframeAniId      = 5,
};

#define getImageId(row)         ui->imgTableWidget->item(row, ColImageId)->text().toInt()
#define getFrameId(row)         ui->framesTableWidget->item(row, ColFrameId)->text().toInt()
#define getAFrameId(row)        ui->aframesTableWidget->item(row, ColAframeId)->text().toInt()
#define getAFrameFrameId(row)   ui->aframesTableWidget->item(row, ColAframeFrameId)->text().toInt()
#define getFrameImgId(row)      ui->framesTableWidget->item(row, ColFrameImgId)->text().toInt()
#define getAnimationId(row)     ui->aniTableWidget->item(row, ColAniId)->text().toInt()

#define selectedImgId()         getImageId(ui->imgTableWidget->currentRow())
#define selectedAniId()         getAnimationId(ui->aniTableWidget->currentRow())


inline void infoDialog(const QString str)
{
    QMessageBox msg;
    msg.setText(str);
    msg.exec();
}

inline bool yesNoDialog(const QString str)
{
    QMessageBox msg;
    msg.setText(str);
    msg.addButton(QMessageBox::Yes);
    msg.addButton(QMessageBox::No);
    msg.exec();
    return msg.result(); // FIXME the return value is wrong
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), _imgId(0), _frameId(0), _aniId(0), _aframeId(0)
{
    ui->setupUi(this);

    QStringList headersList;

    connect(ui->actionSave,        SIGNAL(triggered()),          this, SLOT(saveFile()));
    connect(ui->actionSaveAs,      SIGNAL(triggered()),          this, SLOT(saveAsFile()));
    connect(ui->actionOpen,        SIGNAL(triggered()),          this, SLOT(openFileDialog()));
    connect(ui->actionClose,       SIGNAL(triggered()),          this, SLOT(closeFile()));
    connect(ui->actionExit,        SIGNAL(triggered()),          this, SLOT(exit()));
    connect(ui->actionAbout,       SIGNAL(triggered()),          this, SLOT(about()));

    connect(ui->addImageButton,    SIGNAL(clicked()),            this, SLOT(addImageDialog()));
    connect(ui->imgTableWidget,    SIGNAL(cellClicked(int,int)), this, SLOT(showSelImage(int)));
    connect(ui->imgZoomInButton,   SIGNAL(clicked()),            this, SLOT(imgZoomIn()));
    connect(ui->imgZoomOutButton,  SIGNAL(clicked()),            this, SLOT(imgZoomOut()));
    connect(ui->removeImageButton, SIGNAL(clicked()),            this, SLOT(removeSelImage()));

    connect(ui->addFrameButton,    SIGNAL(clicked()),            this, SLOT(addFrameFromImgRegion()));
    connect(ui->framesTableWidget, SIGNAL(cellClicked(int,int)), this, SLOT(showSelFrame(int)));
    connect(ui->removeFrameButton, SIGNAL(clicked()),            this, SLOT(removeSelFrame()));

    connect(ui->addAniButton,      SIGNAL(clicked()),            this, SLOT(addAnimationDialog()));
    connect(ui->aframesTableWidget,SIGNAL(cellClicked(int,int)), this, SLOT(showSelAframe(int)));
    connect(ui->removeAniButton,   SIGNAL(clicked()),            this, SLOT(removeSelAnimation()));
    connect(ui->addAframeButton,   SIGNAL(clicked()),            this, SLOT(addAframeDialog()));
    connect(ui->previewAniButton,  SIGNAL(clicked()),            this, SLOT(previewAnimation()));
    connect(ui->removeAframeButton,SIGNAL(clicked()),            this, SLOT(removeSelAframe()));


    /* input images table */

    ui->imgTableWidget->setRowCount(0);
    ui->imgTableWidget->setColumnCount(2);
    ui->imgTableWidget->setColumnWidth(ColImageId, 30);
    headersList << "Id" << "Filename";
    ui->imgTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->imgTableWidget->setColumnHidden(ColImageId, true);
#endif

    /* frames table */

    ui->framesTableWidget->setRowCount(0);
    ui->framesTableWidget->setColumnCount(7);
    ui->framesTableWidget->setColumnWidth(ColFrameId, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameOx, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameOy, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameW, 30);
    ui->framesTableWidget->setColumnWidth(ColFrameH, 30);
    headersList << "Id" << "ox" << "oy" << "w" << "h" << "Name" << "Image Id" ;
    ui->framesTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->framesTableWidget->setColumnHidden(ColFrameId, true);
    ui->framesTableWidget->setColumnHidden(ColFrameImgId, true);
#endif

    /* animations table */

    ui->aniTableWidget->setRowCount(0);
    ui->aniTableWidget->setColumnCount(2);
    ui->aniTableWidget->setColumnWidth(ColAniId, 30);
    headersList << "Id" << "Name";
    ui->aniTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->aniTableWidget->setColumnHidden(ColAniId, true);
#endif

    /* animation frames table */

    ui->aframesTableWidget->setRowCount(0);
    ui->aframesTableWidget->setColumnCount(5);
    ui->aframesTableWidget->setColumnWidth(ColAframeId, 30);
    ui->aframesTableWidget->setColumnWidth(ColAframeFrameId, 60);
    ui->aframesTableWidget->setColumnWidth(ColAframeOx, 30);
    ui->aframesTableWidget->setColumnWidth(ColAframeOy, 30);
    ui->aframesTableWidget->setColumnWidth(ColAframeDelay, 50);
    headersList << "Id" << "Frame Id" << "ox" << "oy" << "Delay" << "Animation Id";
    ui->aframesTableWidget->setHorizontalHeaderLabels(headersList);
    headersList.clear();
#ifndef DEBUG_SHOW_ID_COLS
    ui->aframesTableWidget->setColumnHidden(ColAframeId, true);
    ui->aframesTableWidget->setColumnHidden(ColAframeAniId, true);
#endif

    ui->tabWidget->setCurrentWidget(ui->framesTab);

    loadRecentFilesFromSettings();

    resize(1204, 768);
    updateGeometry();
}

void MainWindow::saveFile()
{
    if (_filename.isEmpty()) {
        saveAsFile();
    } else {
        if (!_sprState.serialize(_filename)) {
           infoDialog("Cannot save" + _filename);
           return;
        }
    }
}

void MainWindow::saveAsFile()
{
    static QString filename = "";

    filename = QFileDialog::getOpenFileName(
            this, tr("Save file"), QFileInfo(filename).absolutePath(), "");

    if (!filename.isEmpty()) {
        if (!_sprState.serialize(filename)) {
           infoDialog("Cannot save" + filename);
           return;
        }
        setCurrentFile(filename);
    }
}

void MainWindow::openFileDialog()
{
    static QString filename = "";

    filename = QFileDialog::getOpenFileName(
            this, tr("Open file"), QFileInfo(filename).absolutePath(), "");

    if (openFile(filename)) {
        addRecentFile(filename);
    }
}

bool MainWindow::openFile(const QString& filename)
{
    if (!filename.isEmpty()) {
        SpriteState tmp;

        if (!tmp.deserialize(filename)) {
           infoDialog("Cannot open. See debug output for more info :)");
           return false;
        }

        closeFile();

        /* load input images */

        for (QHashIterator<Id, InputImage> it(tmp.images()); it.hasNext();) {
            it.next();
            const InputImage& image =  it.value();
            addImage(image);
            _imgId = std::max(_imgId, image.id + 1);
        }

        /* load frames */

        for (QHashIterator<Id, LvkFrame> it(tmp.frames()); it.hasNext();) {
            it.next();
            const LvkFrame& frame =  it.value();
            addFrame(frame);
            _frameId = std::max(_frameId, frame.id + 1);
        }

        /* load animations */

        for (QHashIterator<Id, LvkAnimation> it(tmp.animations()); it.hasNext();) {
            it.next();
            const LvkAnimation& ani =  it.value();
            addAnimation(ani);
            _aniId = std::max(_aniId, ani.id + 1);

            /* load aframes */

            for (QHashIterator<Id, LvkAframe> it2(it.value().aframes); it2.hasNext();) {
                it2.next();
                const LvkAframe& aframe =  it2.value();
                addAframe(aframe, ani.id);
                _aframeId = std::max(_aframeId, aframe.id + 1);
            }
        }

        setCurrentFile(filename);
        return true;
    } else {
        return false;
    }
}

void MainWindow::loadRecentFilesFromSettings()
{
    // FIXME only loads the last file opened
    QString recentFile = settings.value("RecentFiles/filename").toString();

    if (!recentFile.isEmpty()) {
       addRecentFile(recentFile);
    }
}

void MainWindow::addRecentFile(const QString& filename)
{
    ui->actionNoRecentFiles->setVisible(false);

    QRecentFileAction* action = new QRecentFileAction(filename);
    ui->actionOpenRecent->addAction(action);
    connect(action, SIGNAL(triggered(QString)), this, SLOT(openFile(QString)));

    // FIXME only stores the last file opened
    settings.setValue("RecentFiles/filename", filename);
}

void MainWindow::closeFile()
{
    _imgId    = 0;
    _frameId  = 0;
    _aniId    = 0;
    _aframeId = 0;

    _sprState.clear();

    ui->imgTableWidget->clearContents();
    ui->imgTableWidget->setRowCount(0);
    ui->framesTableWidget->clearContents();
    ui->framesTableWidget->setRowCount(0);
    ui->aniTableWidget->clearContents();
    ui->aniTableWidget->setRowCount(0);
    ui->aframesTableWidget->clearContents();
    ui->aframesTableWidget->setRowCount(0);

    ui->imgPreview->setPixmap(QPixmap());
    ui->framePreview->setPixmap(QPixmap());
    ui->frameAPreview->setPixmap(QPixmap());
}

void MainWindow::setCurrentFile(const QString& filename)
{
    _filename = filename;
    if (filename.isEmpty()) {
        setWindowTitle(QString(APP_NAME));
    } else  {
        setWindowTitle(QString(APP_NAME) + " - " + QFileInfo(filename).baseName());
    }
}

void MainWindow::addImageDialog()
{
    static QString filename = "";

    filename = QFileDialog::getOpenFileName(
            this, tr("Add Image"), QFileInfo(filename).absolutePath(), "");

    if (!filename.isEmpty()) {
        addImage(InputImage(_imgId++, filename));
    }
}

void MainWindow::addImage(const InputImage& image)
{
    /* State */

    QString filename(image.filename);

    QImage img(filename);

    if (img.isNull()) {
        infoDialog(filename + " has an invalid image format");
        return;
    }

    _sprState.addImage(image);

    /* UI */

    int rows = ui->imgTableWidget->rowCount();

    QTableWidgetItem* item_id       = new QTableWidgetItem(QString::number(image.id));
    QTableWidgetItem* item_filename = new QTableWidgetItem(QFileInfo(filename).fileName());
    ui->imgTableWidget->setRowCount(rows+1);
    ui->imgTableWidget->setItem(rows, ColImageId, item_id);
    ui->imgTableWidget->setItem(rows, ColImageFilename, item_filename);
    ui->imgTableWidget->setCurrentItem(item_id);

    showSelImage(rows);
    ui->removeImageButton->setEnabled(true);
    ui->addFrameButton->setEnabled(true);
}

void MainWindow::showSelImage(int row)
{
    Id imgId = getImageId(row);
    const QPixmap& selPixmap = _sprState.ipixmap(imgId);
    int w = selPixmap.width();
    int h = selPixmap.height();

    ui->imgPreview->setPixmap(selPixmap);
    ui->imgPreview->setGeometry(0, 0, w, h);
    ui->imgPreview->updateGeometry();
}

void MainWindow::imgZoomIn()
{
    ui->imgPreview->resize(ui->imgPreview->size()*2);
    ui->imgPreview->updateGeometry();
}

void MainWindow::imgZoomOut()
{
    ui->imgPreview->resize(ui->imgPreview->size()/2);
    ui->imgPreview->updateGeometry();
}

void MainWindow::removeSelImage()
{
// FIX yesNoDialog
//    if (!yesNoDialog(tr("Are you sure you want to remove the image?"))) {
//        return;
//    }

    int currentRow = ui->imgTableWidget->currentRow();

    if (currentRow == -1) {
        infoDialog("No image selected");
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

    ui->imgTableWidget->removeRow(row);
    if (ui->imgTableWidget->rowCount() == 0) {
        ui->removeImageButton->setEnabled(false);
        ui->addFrameButton->setEnabled(false);
    }

    _sprState.removeImage(imgId);

    /* remove frames that use this image*/
    for (int r = 0; r < ui->framesTableWidget->rowCount(); ++r) {
        qDebug() << "row " << r << " -- frameImgId " << getFrameImgId(r) << " == imgId " << imgId;
        if (getFrameImgId(r) == imgId) {
            removeFrame(r);
        }
    }
}

bool MainWindow::addFrameFromImgRegion()
{
    if (ui->imgTableWidget->currentRow() == -1) {
        infoDialog("No input image selected");
        return false;
    }

    bool ok;
    QString name = QInputDialog::getText(this, tr("New frame"),
                                         tr("Frame name:"),
                                         QLineEdit::Normal, "", &ok);
    if (ok) {
        name = name.trimmed();
        if (name.isEmpty())  {
            infoDialog("Cannot add a frame without name");
            return false;
        }

        Id imgId = selectedImgId();

        // TODO when available, this vars must be initialized from
        //  a rectangular region in the input image
        int ox = 0;
        int oy = 0;
        int w  = _sprState.ipixmap(imgId).width();
        int h  = _sprState.ipixmap(imgId).height();

        addFrame(LvkFrame(_frameId++, imgId, ox, oy, w, h, name));

        return true;
    }
    return false;
}

void MainWindow::addFrame(const LvkFrame &frame)
{
    /* state */

    _sprState.addFrame(frame);

    /* UI */

    QTableWidgetItem* item_id   = new QTableWidgetItem(QString::number(frame.id));
    QTableWidgetItem* item_ox   = new QTableWidgetItem(QString::number(frame.ox));
    QTableWidgetItem* item_oy   = new QTableWidgetItem(QString::number(frame.oy));
    QTableWidgetItem* item_w    = new QTableWidgetItem(QString::number(frame.w));
    QTableWidgetItem* item_h    = new QTableWidgetItem(QString::number(frame.h));
    QTableWidgetItem* item_iid  = new QTableWidgetItem(QString::number(frame.imgId));
    QTableWidgetItem* item_name = new QTableWidgetItem(frame.name);

    int rows = ui->framesTableWidget->rowCount();

    ui->framesTableWidget->setRowCount(rows+1);
    ui->framesTableWidget->setItem(rows, ColFrameId,    item_id);
    ui->framesTableWidget->setItem(rows, ColFrameOx,    item_ox);
    ui->framesTableWidget->setItem(rows, ColFrameOy,    item_oy);
    ui->framesTableWidget->setItem(rows, ColFrameW,     item_w);
    ui->framesTableWidget->setItem(rows, ColFrameH,     item_h);
    ui->framesTableWidget->setItem(rows, ColFrameImgId, item_iid);
    ui->framesTableWidget->setItem(rows, ColFrameName,  item_name);
    ui->imgTableWidget->setCurrentItem(item_id);

    showSelFrame(rows);
    ui->removeFrameButton->setEnabled(true);
}

void MainWindow::showSelFrame(int row)
{
    int frameId = getFrameId(row);

    const QPixmap& selPixmap = _sprState.fpixmap(frameId);
    int w = selPixmap.width();
    int h = selPixmap.height();
    
    ui->framePreview->setPixmap(selPixmap);
    ui->framePreview->setGeometry(0, 0, w, h);
    ui->framePreview->updateGeometry();
}

void MainWindow::removeSelFrame()
{
// FIX yesNoDialog
//    if (!yesNoDialog(tr("Are you sure you want to remove the frame?"))) {
//        return;
//    }

    int currentRow = ui->framesTableWidget->currentRow();

    if (currentRow == -1) {
        infoDialog("No frame selected");
        return;
    }
    removeFrame(currentRow);
}

void MainWindow::removeFrame(int row)
{
    Id frameId = getFrameId(row);

    ui->framesTableWidget->removeRow(row);
    if (ui->framesTableWidget->rowCount() == 0) {
        ui->removeFrameButton->setEnabled(false);
    }
    ui->framePreview->setPixmap(QPixmap());

    _sprState.removeFrame(frameId);
}

void MainWindow::addAnimationDialog()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("New animation"),
                                         tr("Animation name:"),
                                         QLineEdit::Normal, "", &ok);
    if (ok) {
        name = name.trimmed();
        if (name.isEmpty())  {
            infoDialog("Cannot add an animation without name");
            return;
        }
        addAnimation(LvkAnimation(_aniId++, name));
    }
}

void MainWindow::addAnimation(const LvkAnimation& ani)
{
    /* state */

    _sprState.addAnimation(ani);

    /* UI */

    int rows = ui->aniTableWidget->rowCount();

    QTableWidgetItem* item_id   = new QTableWidgetItem(QString::number(ani.id));
    QTableWidgetItem* item_name = new QTableWidgetItem(ani.name);
    ui->aniTableWidget->setRowCount(rows+1);
    ui->aniTableWidget->setItem(rows, ColAniId, item_id);
    ui->aniTableWidget->setItem(rows, ColAniName, item_name);
    ui->aniTableWidget->setCurrentItem(item_id);

    showSelAnimation(rows);
    ui->removeAniButton->setEnabled(true);
    ui->addAframeButton->setEnabled(true);
}

void MainWindow::showSelAnimation(int row)
{
    ui->previewAniButton->setEnabled(true);
}

void MainWindow::previewAnimation()
{
    if (ui->aniTableWidget->currentRow() == -1) {
        return;
    }

    // TODO optimize this! If the animation did not change, do not delete and recreate
    // scene and animation

    static QGraphicsScene* scene = 0;
    static LvkFrameGraphicsGroup* animation = 0;

    if (ui->previewAniButton->text() == "Play") { // TODO do not compare strings
        if (scene) {
            if (animation) {
                scene->removeItem(animation);
                delete animation;
            }
            delete scene;
        }

        LvkAnimation selectedAni = _sprState.animations().value(selectedAniId());
        animation = new LvkFrameGraphicsGroup(selectedAni, _sprState.fpixmaps());

        scene = new QGraphicsScene;
        scene->addItem(animation);
        ui->animationGraphicsView->setScene(scene);
        ui->animationGraphicsView->show();
        animation->startAnimation();
        ui->previewAniButton->setText(tr("Stop"));
    } else {
        animation->stopAnimation();
        ui->previewAniButton->setText(tr("Play"));
    }
}

void MainWindow::removeSelAnimation()
{
// FIX yesNoDialog
//    if (!yesNoDialog(tr("Are you sure you want to remove the animation?"))) {
//        return;
//    }

    int currentRow = ui->aniTableWidget->currentRow();

    if (currentRow == -1) {
        infoDialog("No animation selected");
        return;
    }
    removeAnimation(currentRow);
}

void MainWindow::removeAnimation(int row)
{
    Id aniId = getAnimationId(row);

    ui->aniTableWidget->removeRow(row);
    if (ui->aniTableWidget->rowCount() == 0) {
        ui->removeAniButton->setEnabled(false);
        ui->addAframeButton->setEnabled(false);
    }

    _sprState.removeAnimation(aniId);
}

void MainWindow::addAframeDialog()
{
    if (_sprState.frames().isEmpty()) {
        infoDialog(tr("No frames available.\n\nGo to the \"Frames\" tab and create at least one frame."));
        return;
    }
    if (ui->aniTableWidget->currentRow() == -1) {
        infoDialog("No animation selected");
        return;
    }

    QStringList framesList;

    for (QHashIterator<int, LvkFrame> it(_sprState.frames()); it.hasNext();) {
        it.next();
        const LvkFrame& frame = it.value();
        framesList << "Id: " + QString::number(frame.id) +  " Name: " + frame.name;
        framesList.sort();
    }

    bool ok;
    QString frame_str = QInputDialog::getItem(this, tr("New Animation frame"),
                                          tr("Choose a frame:"),
                                          framesList, 0, false, &ok);
    if (ok) {
        QStringList tokens = frame_str.split(" ");

        if (tokens.size() < 2) {
            infoDialog("Cannot add frame. The selected frame could not be parsed");
            return;
        }
        Id frameId = tokens.at(1).toInt();

        addAframe(LvkAframe(_aframeId++, frameId, 1.0), selectedAniId());
    }
}


void MainWindow::addAframe(const LvkAframe& aframe, Id aniId)
{

    /* state */
    
    _sprState.addAframe(aframe, aniId);

    /* UI */

    QTableWidgetItem* item_id    = new QTableWidgetItem(QString::number(aframe.id));
    QTableWidgetItem* item_fid   = new QTableWidgetItem(QString::number(aframe.frameId));
    QTableWidgetItem* item_delay = new QTableWidgetItem(QString::number(aframe.delay));

    int rows = ui->aframesTableWidget->rowCount();

    ui->aframesTableWidget->setRowCount(rows+1);
    ui->aframesTableWidget->setItem(rows, ColAframeId,      item_id);
    ui->aframesTableWidget->setItem(rows, ColAframeFrameId, item_fid);
    ui->aframesTableWidget->setItem(rows, ColAframeDelay,   item_delay);
    ui->aframesTableWidget->setCurrentItem(item_id);

    showSelAframe(rows);
    ui->removeAframeButton->setEnabled(true);
}


void MainWindow::showSelAframe(int row)
{
    int frameId = getAFrameFrameId(row);

    const QPixmap& selPixmap = _sprState.fpixmap(frameId);
    int w = selPixmap.width();
    int h = selPixmap.height();

    ui->frameAPreview->setPixmap(selPixmap);
    ui->frameAPreview->setGeometry(0, 0, w, h);
    ui->frameAPreview->updateGeometry();
}

void MainWindow::removeSelAframe()
{
// FIX yesNoDialog
//    if (!yesNoDialog(tr("Are you sure you want to remove the frame?"))) {
//        return;
//    }

    int currentRow = ui->aframesTableWidget->currentRow();

    if (currentRow == -1) {
        infoDialog("No frame selected");
        return;
    }
    removeAframe(currentRow);
}

void MainWindow::removeAframe(int row)
{
    Id frameId = getAFrameId(row);

    ui->aframesTableWidget->removeRow(row);
    if (ui->aframesTableWidget->rowCount() == 0) {
        ui->removeAframeButton->setEnabled(false);
    }
    ui->frameAPreview->setPixmap(QPixmap());

    _sprState.removeFrame(frameId);
}

void MainWindow::about()
{
    infoDialog(QString(APP_NAME)  + " " + QString(APP_VERSION));
}

 void MainWindow::exit()
 {
     QCoreApplication::exit(0);
 }

MainWindow::~MainWindow()
{
    delete ui;
}




