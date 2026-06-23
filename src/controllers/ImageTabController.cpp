// ImageTabController.cpp
//
// Agent 8 (Phase 3): see ImageTabController.h.
//
// Code in this file is lifted ~verbatim from the pre-refactor
// MainWindow slots. Behavior must be byte-equivalent so that the
// existing tests (golden round-trip, undo/redo, security) keep passing.

#include "controllers/ImageTabController.h"

#include "controllers/AnimationTabController.h"
#include "controllers/FrameTabController.h"
#include "dialogs.h"
#include "image_validation.h"
#include "lvkinputimagewidget.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QMapIterator>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace {

// Column layout used to live as a file-scope enum in mainwindow.cpp.
// We mirror it here so the controller is self-contained.
enum {
    ColImageId,
    ColImageCheckable,
    ColImageVisibleId,
    ColImageScale,
    ColImageFilename,
    ColImageTotal,
};

} // namespace

ImageTabController::ImageTabController(MainWindow *mw, Ui::MainWindow *ui, SpriteState2 *state,
                                       QObject *parent)
    : QObject(parent), m_mw(mw), m_ui(ui), m_state(state) {}

void ImageTabController::wireSignals() {
    connect(m_ui->actionAddImage, &QAction::triggered, this, &ImageTabController::addImageDialog);
    connect(m_ui->actionRemoveImage, &QAction::triggered, this,
            &ImageTabController::removeSelImage);

    connect(m_ui->addImageButton, &QAbstractButton::clicked, this,
            &ImageTabController::addImageDialog);
    connect(m_ui->removeImageButton, &QAbstractButton::clicked, this,
            &ImageTabController::removeSelImage);
    connect(m_ui->refreshImgButton, &QAbstractButton::clicked, this,
            &ImageTabController::reloadSelImage);

    connect(m_ui->scaleImageButton, &QAbstractButton::clicked, this,
            &ImageTabController::scaleCheckedImages);
    connect(m_ui->quickModeButton, &QAbstractButton::clicked, this,
            &ImageTabController::switchQuickMode);
    connect(m_ui->checkAllImagesButton, &QAbstractButton::clicked, this,
            &ImageTabController::checkAllImages);
    connect(m_ui->invertCheckedImagesButton, &QAbstractButton::clicked, this,
            &ImageTabController::invertCheckedImages);
    connect(m_ui->createQuickAniButton, &QAbstractButton::clicked, this,
            &ImageTabController::createQuickAnimation);

    connect(m_ui->imgTableWidget, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { showSelImage(row); });
    connect(m_ui->imgTableWidget, &QTableWidget::cellChanged, this,
            &ImageTabController::updateImgTable);
}

void ImageTabController::refreshTable() {
    QSignalBlocker blocker(m_ui->imgTableWidget);

    int row = m_ui->imgTableWidget->currentRow();
    int col = m_ui->imgTableWidget->currentColumn();

    m_ui->imgTableWidget->clearContents();
    m_ui->imgTableWidget->setRowCount(0);

    for (QMapIterator<Id, InputImage> it(m_state->images()); it.hasNext();) {
        it.next();
        const InputImage &image = it.value();
        addImage_ui(image);
    }

    m_ui->imgTableWidget->setCurrentCell(row, col);
}

Id ImageTabController::getImageId(int row) const {
    const QTableWidget *t = m_ui->imgTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColImageId)->text().toInt() : NullId;
}

Id ImageTabController::selectedImgId() const {
    return getImageId(m_ui->imgTableWidget->currentRow());
}

QString ImageTabController::toRelativePath(const QString &filePath) const {
    const QString sep = QDir::separator();
    QFileInfo fileInfo(filePath);
    QStringList dirs1 = fileInfo.absolutePath().split(sep, Qt::SkipEmptyParts);
    QStringList dirs2 = QDir::currentPath().split(sep, Qt::SkipEmptyParts);

    QString relFilePath;
    int i = 0;
    int minSize = std::min(dirs1.size(), dirs2.size());

    for (; i < minSize && dirs1[i] == dirs2[i]; ++i) {
    }
    for (int j = i; j < dirs2.size(); ++j) {
        relFilePath.append(".." + sep);
    }
    for (int j = i; j < dirs1.size(); ++j) {
        relFilePath.append(dirs1[j] + sep);
    }
    relFilePath.append(fileInfo.fileName());
    return relFilePath;
}

void ImageTabController::addImageDialog() {
    m_mw->showFramesTab();

    static QString lastDir = "";

    // Team F1 (F1.3): align the file-dialog filter with the validator
    // whitelist in src/image_validation.cpp. Pre-F1 the dialog only
    // advertised png/jpg/jpeg/xpm/xbm/bmp/tif/tiff while the validator
    // also accepted gif/webp/svg -- so a user could legitimately pick a
    // webp through the dialog only if they switched to "All files (*)",
    // and SVG was admitted through both paths despite XXE / scripted-
    // SVG concerns. We now expand the dialog to include gif/webp and
    // drop SVG from BOTH the dialog AND the validator (see
    // src/image_validation.cpp). Final whitelist: png, jpg, jpeg, bmp,
    // gif, webp, xpm, xbm, tif, tiff (10 formats).
    QStringList filenames = QFileDialog::getOpenFileNames(
        m_mw, tr("Add Image"), lastDir,
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.xpm *.xbm *.tif *.tiff);;"
           "All files (*)"));

    if (filenames.size() > 0) {
        lastDir = QFileInfo(filenames[0]).absolutePath();

        m_state->startTransaction();
        for (int i = 0; i < filenames.size(); ++i) {
            addImage(InputImage(NullId, toRelativePath(filenames[i])));
        }
        m_state->endTransaction();
    }
}

Id ImageTabController::addImage(const InputImage &image) {
    InputImage image_ = image;

    // SECURITY (Team B3 / Phase 6c): the file-format whitelist used to be
    // advisory only -- addImage_ui showed an "unsupported format" dialog
    // and then added the row anyway, so a rejected format was still
    // attached to the sprite. Validate BEFORE state mutation so a
    // rejection means "nothing added to state, nothing added to UI".
    //
    // We skip the validation for empty filenames (legacy "blank slot"
    // construction is allowed; the dialog warns about it from
    // addImage_ui) and pass non-empty paths through validateImageFile.
    if (!image_.filename.isEmpty()) {
        QString errMsg;
        if (!validateImageFile(image_.filename, &errMsg)) {
            errorDialog(errMsg, m_mw);
            return NullId; // gate: reject => no state, no UI
        }
        m_state->addImage(image_);
    }
    addImage_ui(image_);
    return image_.id;
}

bool ImageTabController::validateImageFile(const QString &filename, QString *errMsg) const {
    // Team D2 (D2.1): Delegate to the free-function validator in
    // image_validation.{h,cpp} so that BOTH the GUI dialog path (here)
    // AND the SpriteState::load() path enforce the same format whitelist.
    // The pre-D2 code kept this validator inside the controller, which
    // meant SpriteState::load could insert images that the format
    // whitelist would have rejected -- the "advisory dialog" hole B3
    // claimed to close was still open through the load route. Now the
    // single source of truth lives in lvk::validateImageFile().
    return lvk::validateImageFile(filename, errMsg);
}

void ImageTabController::addImage_ui(const InputImage &image) {
    QString filename(image.filename);

    if (filename.isEmpty()) {
        infoDialog(tr("Empty Filename"), m_mw);
        return;
    }
    // NOTE (Team B3): file-existence / format checks moved into
    // validateImageFile(), called from addImage() BEFORE state mutation.
    // This function is reused by refreshTable() to redraw rows from
    // already-validated state; running file IO here on every refresh
    // produced UX noise when assets were moved on disk.

    int rows = m_ui->imgTableWidget->rowCount();

    QTableWidgetItem *item_id = new QTableWidgetItem(QString::number(image.id));
    QTableWidgetItem *item_checkable = new QTableWidgetItem();
    QTableWidgetItem *item_vid = new QTableWidgetItem(QString::number(image.id));
    QTableWidgetItem *item_filename = new QTableWidgetItem(filename);
    QTableWidgetItem *item_scale = new QTableWidgetItem(QString::number(image.scale()));

    item_checkable->setCheckState(Qt::Unchecked);

    {
        QSignalBlocker blocker(m_ui->imgTableWidget);
        m_ui->imgTableWidget->setRowCount(rows + 1);
        m_ui->imgTableWidget->setItem(rows, ColImageId, item_id);
        m_ui->imgTableWidget->setItem(rows, ColImageCheckable, item_checkable);
        m_ui->imgTableWidget->setItem(rows, ColImageVisibleId, item_vid);
        m_ui->imgTableWidget->setItem(rows, ColImageFilename, item_filename);
        m_ui->imgTableWidget->setItem(rows, ColImageScale, item_scale);
        m_ui->imgTableWidget->setCurrentItem(item_id);
    }

    showImage(image.id);
}

void ImageTabController::showSelImage(int row) {
    Id imgId = (row == -1) ? NullId : getImageId(row);
    showImage(imgId);
}

void ImageTabController::showImage(Id imgId, bool clearPixmapCache) {
    if (clearPixmapCache) {
        m_ui->imgPreview->clearPixmapCache(imgId);
        m_ui->imgPreview->clear(); // FIXME
    }
    const QPixmap &selPixmap = m_state->ipixmap(imgId);
    m_ui->imgPreview->setPixmap(selPixmap, imgId);
}

void ImageTabController::showSelImageWithFrameRect(int row, const QRect &rect) {
    showSelImage(row);
    m_ui->imgPreview->setFrameRect(rect);
}

void ImageTabController::removeSelImage() {
    m_mw->showFramesTab();

    int currentRow = m_ui->imgTableWidget->currentRow();
    if (currentRow == -1) {
        infoDialog(tr("No image selected"), m_mw);
        return;
    }
    QString imgFilename = m_ui->imgTableWidget->item(currentRow, ColImageFilename)->text();
    if (!yesNoDialog(tr("Are you sure you want to remove the image '") + imgFilename + tr("'?"),
                     m_mw)) {
        return;
    }
    removeImage(currentRow);
    m_ui->imgPreview->setPixmap(QPixmap());
}

void ImageTabController::removeImage(int row) {
    Id imgId = getImageId(row);

    {
        QSignalBlocker blocker(m_ui->imgTableWidget);
        m_ui->imgTableWidget->removeRow(row);
    }

    m_state->removeImage(imgId);

    // remove frames that use this image
    FrameTabController *frames = m_mw->frames();
    for (int r = 0; r < m_ui->framesTableWidget->rowCount(); ++r) {
        if (frames->getFrameImgId(r) == imgId) {
            frames->removeFrame(r);
        }
    }

    emit imageRemoved(imgId);
}

void ImageTabController::reloadSelImage() {
    if (m_ui->imgTableWidget->currentRow() == -1) {
        return;
    }
    reloadImage(selectedImgId());
}

void ImageTabController::reloadImage(Id imgId) {
    m_state->reloadImagePixmap(imgId);
    m_state->reloadFramePixmaps(imgId);

    m_ui->imgPreview->clearPixmapCache(imgId);
    m_mw->refreshPreviews();
}

void ImageTabController::updateImgTable(int row, int col) {
    QTableWidget *table = m_ui->imgTableWidget;
    QString newValue = table->item(row, col)->text();
    Id imgId = table->item(row, ColImageId)->text().toInt();
    InputImage img = m_state->const_image(imgId);

    auto setCellInt = [&](int c, int v) {
        QSignalBlocker blocker(table);
        table->item(row, c)->setText(QString::number(v));
    };
    auto setCellStr = [&](int c, const QString &v) {
        QSignalBlocker blocker(table);
        table->item(row, c)->setText(v);
    };
    auto setCellDbl = [&](int c, double v) {
        QSignalBlocker blocker(table);
        table->item(row, c)->setText(QString::number(v));
    };

    bool ok = true;
    double newScale = 0;

    if (col == ColImageScale) {
        newScale = newValue.toDouble(&ok);
    }

    switch (col) {
    case ColImageId:
    case ColImageVisibleId:
        infoDialog(tr("Column \"Id\" is not editable"), m_mw);
        setCellInt(col, img.id);
        break;
    case ColImageFilename:
        if (newValue.isEmpty()) {
            infoDialog(tr("Image filename cannot be empty"), m_mw);
            setCellStr(col, img.filename);
        } else if (newValue.contains(',')) {
            infoDialog(tr("Image filename cannot contain the character ','"), m_mw);
            setCellStr(col, img.filename);
        } else if (newValue != img.filename) {
            img.filename = newValue;
            img.reloadImage();
            if (!QFileInfo(newValue).exists()) {
                errorDialog(tr("The file does not exist"), m_mw);
            } else if (img.pixmap.isNull()) {
                errorDialog(tr("The file contains an invalid image format"), m_mw);
            }
            m_state->updateImage(img);
            setCellStr(col, img.filename);
        }
        break;
    case ColImageScale:
        if (!ok) {
            infoDialog(tr("Invalid image scale"), m_mw);
            setCellDbl(col, img.scale());
        } else if (newScale != img.scale()) {
            img.scale(newScale);
            setCellDbl(col, newScale);
        }
        break;
    }

    if (ok) {
        switch (col) {
        case ColImageId:
        case ColImageVisibleId:
            break;
        case ColImageFilename:
        case ColImageScale:
            m_state->updateImage(img);
            m_ui->imgPreview->clearPixmapCache(img.id);
            m_mw->refreshPreviews();
            break;
        }
    }
}

void ImageTabController::switchQuickMode() {
    if (m_ui->quickModeButton->isChecked()) {
        m_ui->imgTableWidget->setColumnHidden(ColImageCheckable, false);
        m_ui->createQuickAniButton->setVisible(true);
        m_ui->checkAllImagesButton->setVisible(true);
        m_ui->invertCheckedImagesButton->setVisible(true);
        m_ui->scaleImageButton->setVisible(true);
    } else {
        m_ui->imgTableWidget->setColumnHidden(ColImageCheckable, true);
        m_ui->createQuickAniButton->setVisible(false);
        m_ui->checkAllImagesButton->setVisible(false);
        m_ui->invertCheckedImagesButton->setVisible(false);
        m_ui->scaleImageButton->setVisible(false);
    }
}

void ImageTabController::checkAllImages() {
    for (int row = 0; row < m_ui->imgTableWidget->rowCount(); ++row) {
        m_ui->imgTableWidget->item(row, ColImageCheckable)->setCheckState(Qt::Checked);
    }
}

void ImageTabController::invertCheckedImages() {
    for (int row = 0; row < m_ui->imgTableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = m_ui->imgTableWidget->item(row, ColImageCheckable);
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    }
}

bool ImageTabController::hasImagesChecked() const {
    for (int row = 0; row < m_ui->imgTableWidget->rowCount(); ++row) {
        if (m_ui->imgTableWidget->item(row, ColImageCheckable)->checkState() == Qt::Checked) {
            return true;
        }
    }
    return false;
}

void ImageTabController::scaleCheckedImages() {
    if (!hasImagesChecked()) {
        infoDialog(tr("This operation requires at least one image selected"), m_mw);
        return;
    }

    bool ok;
    QString input = QInputDialog::getText(m_mw, tr("Scale all images"), tr("Scale size:"),
                                          QLineEdit::Normal, "", &ok);
    if (!ok) {
        return;
    }
    double scale = input.toDouble(&ok);
    if (!ok || scale <= 0) {
        infoDialog(
            tr("Invalid scale. The scale must be a floating point number greater than zero."),
            m_mw);
        return;
    }

    for (int row = 0; row < m_ui->imgTableWidget->rowCount(); ++row) {
        if (m_ui->imgTableWidget->item(row, ColImageCheckable)->checkState() == Qt::Checked) {
            InputImage img = m_state->const_image(getImageId(row));
            img.scale(scale);
            m_state->updateImage(img);
            m_ui->imgPreview->clearPixmapCache(img.id);
            {
                QSignalBlocker blocker(m_ui->imgTableWidget);
                m_ui->imgTableWidget->item(row, ColImageScale)->setText(QString::number(scale));
            }
        }
    }
    m_mw->refreshPreviews();
}

void ImageTabController::createQuickAnimation() {
    if (!hasImagesChecked()) {
        infoDialog(tr("This operation requires at least one image selected"), m_mw);
        return;
    }

    bool ok;
    QString aniName = QInputDialog::getText(m_mw, tr("New animation"), tr("Animation name:"),
                                            QLineEdit::Normal, "", &ok);
    aniName = aniName.trimmed();
    if (!ok || aniName.isEmpty() || aniName.contains(',')) {
        if (ok) {
            infoDialog(tr("Animation name cannot be empty or contain ','"), m_mw);
        }
        return;
    }

    bool addReverseFrames = yesNoDialog(tr("After finishing the animationm,"
                                           "do you want to add frames to reverse the animation?"),
                                        m_mw);

    m_state->startTransaction();
    Id aniId = m_mw->animations()->addAnimation(LvkAnimation(NullId, aniName));

    QList<Id> newFrameIds;
    FrameTabController *frames = m_mw->frames();
    AnimationTabController *anis = m_mw->animations();

    for (int row = 0; row < m_ui->imgTableWidget->rowCount(); ++row) {
        if (m_ui->imgTableWidget->item(row, ColImageCheckable)->checkState() == Qt::Checked) {
            QString frameName = aniName + "_frame_" + QString::number(row);
            Id frameId = frames->addFrameFromMouseRect(getImageId(row), frameName);
            if (frameId != NullId) {
                anis->addAframe(LvkAframe(NullId, frameId), aniId);
            }
            newFrameIds << frameId;
        }
    }

    if (addReverseFrames) {
        for (int i = newFrameIds.size() - 1; i >= 0; --i) {
            anis->addAframe(LvkAframe(NullId, newFrameIds[i]), aniId);
        }
    }

    m_state->endTransaction();
}
