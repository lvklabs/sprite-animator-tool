// FrameTabController.cpp
//
// See FrameTabController.h. Code lifted from MainWindow.

#include "controllers/FrameTabController.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controllers/ImageTabController.h"
#include "dialogs.h"
#include "lvkinputimagewidget.h"
#include "lvkframedefwidget.h"

#include <QInputDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QPixmap>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QMapIterator>
#include <QVector>
#include <QDebug>

namespace {

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

enum {
    BlendNone,
    BlendFrameRect,
    BlendExistentFrame,
    BlendFrameId,
    BlendModeTotal
};

static bool isValidFrameName(const QString& name, bool showErrorDialog)
{
    if (name.isEmpty()) {
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

}

FrameTabController::FrameTabController(MainWindow* mw, Ui::MainWindow* ui,
                                       SpriteState2* state, QObject* parent)
    : QObject(parent), m_mw(mw), m_ui(ui), m_state(state), m_blendFrameId(NullId)
{
}

void FrameTabController::wireSignals()
{
    connect(m_ui->actionAddFrame,    &QAction::triggered, this,
            [this](bool){ addFrameDialog(); });
    connect(m_ui->actionRemoveFrame, &QAction::triggered, this, &FrameTabController::removeSelFrame);
    connect(m_ui->actionRemoveAllUnusedFrames, &QAction::triggered, this,
            &FrameTabController::removeAllUnusedFrames);

    connect(m_ui->addFrameButton,    &QAbstractButton::clicked, this,
            [this](bool){ addFrameDialog(); });
    connect(m_ui->removeFrameButton, &QAbstractButton::clicked, this, &FrameTabController::removeSelFrame);
    connect(m_ui->removeAllUnusedFramesButton, &QAbstractButton::clicked, this,
            &FrameTabController::removeAllUnusedFrames);
    connect(m_ui->hideFramePreviewButton, &QAbstractButton::clicked, this,
            &FrameTabController::hideShowFramePreview);

    connect(m_ui->imgPreview, &LvkFrameDefWidget::mouseRectChangeFinished,
            this, [this](const QRect&){ blendFrameRect(); });
    connect(m_ui->imgPreview, &LvkFrameDefWidget::frameRectChanging,
            this, &FrameTabController::updateCurrentFrame_ui);
    connect(m_ui->imgPreview, &LvkFrameDefWidget::frameRectChangeFinished,
            this, &FrameTabController::updateCurrentFrame);

    connect(m_ui->framesTableWidget, &QTableWidget::currentCellChanged,
            this, [this](int row, int, int, int){ showSelFrame(row); });
    connect(m_ui->framesTableWidget, &QTableWidget::cellChanged,
            this, &FrameTabController::updateFramesTable);

    blendComboBoxSignals(true);
}

void FrameTabController::refreshTable()
{
    m_mw->cellChangedSignals(false);

    int row = m_ui->framesTableWidget->currentRow();
    int col = m_ui->framesTableWidget->currentColumn();

    m_ui->framesTableWidget->clearContents();
    m_ui->framesTableWidget->setRowCount(0);

    for (QMapIterator<Id, LvkFrame> it(m_state->frames()); it.hasNext();) {
        it.next();
        const LvkFrame& frame = it.value();
        addFrame_ui(frame);
    }

    m_ui->framesTableWidget->setCurrentCell(row, col);

    m_mw->cellChangedSignals(true);
}

Id FrameTabController::getFrameId(int row) const
{
    const QTableWidget* t = m_ui->framesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColFrameId)->text().toInt() : NullId;
}

Id FrameTabController::getFrameImgId(int row) const
{
    const QTableWidget* t = m_ui->framesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColFrameImgId)->text().toInt() : NullId;
}

Id FrameTabController::selectedFrameId() const
{
    return getFrameId(m_ui->framesTableWidget->currentRow());
}

Id FrameTabController::getFrameDialog(const QString& title)
{
    if (m_state->frames().isEmpty()) {
        infoDialog(tr("No frames available.\n\nGo to the \"Frames\" tab to create one frame."));
        return NullId;
    }
    QStringList framesList;
    for (QMapIterator<Id, LvkFrame> it(m_state->frames()); it.hasNext();) {
        it.next();
        const LvkFrame& frame = it.value();
        framesList << tr("Id: ") + QString::number(frame.id) + tr(" Name: ") + frame.name;
    }
    framesList.sort();

    Id frameId = NullId;
    bool ok;
    QString frame_str = QInputDialog::getItem(m_mw, title, tr("Choose a frame:"),
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

bool FrameTabController::addFrameDialog(const QString& defaultName, bool promptName)
{
    m_mw->showFramesTab();

    QString name = defaultName;
    if (promptName) {
        bool ok;
        name = QInputDialog::getText(m_mw, tr("New frame"), tr("Frame name:"),
                                     QLineEdit::Normal, defaultName, &ok);
        if (!ok) {
            return false;
        }
    }
    name = name.trimmed();
    if (!isValidFrameName(name, true)) {
        return false;
    }
    Id imgId = m_mw->images()->selectedImgId();
    return addFrameFromMouseRect(imgId, name) != NullId;
}

Id FrameTabController::addFrameFromMouseRect(Id imgId, const QString& name)
{
    QRect frameRect = m_ui->imgPreview->mouseFrameRect();
    bool validRect = true;
    int ox = 0, oy = 0, w = 0, h = 0;

    if (frameRect.isNull()) {
        ox = 0; oy = 0;
        w  = m_state->ipixmap(imgId).width();
        h  = m_state->ipixmap(imgId).height();
    } else if (frameRect.width() == 0) {
        infoDialog(tr("Cannot add a frame with null width"));
        validRect = false;
    } else if (frameRect.height() == 0) {
        infoDialog(tr("Cannot add a frame with null height"));
        validRect = false;
    } else {
        ox = frameRect.x();
        oy = frameRect.y();
        w  = frameRect.width();
        h  = frameRect.height();
    }
    return validRect ? addFrame(LvkFrame(NullId, imgId, ox, oy, w, h, name)) : NullId;
}

Id FrameTabController::addFrame(const LvkFrame& frame)
{
    LvkFrame frame_ = frame;
    m_state->addFrame(frame_);
    addFrame_ui(frame_);
    return frame_.id;
}

void FrameTabController::addFrame_ui(const LvkFrame& frame)
{
    QTableWidgetItem* item_id   = new QTableWidgetItem(QString::number(frame.id));
    QTableWidgetItem* item_vid  = new QTableWidgetItem(QString::number(frame.id));
    QTableWidgetItem* item_ox   = new QTableWidgetItem(QString::number(frame.ox));
    QTableWidgetItem* item_oy   = new QTableWidgetItem(QString::number(frame.oy));
    QTableWidgetItem* item_w    = new QTableWidgetItem(QString::number(frame.w));
    QTableWidgetItem* item_h    = new QTableWidgetItem(QString::number(frame.h));
    QTableWidgetItem* item_iid  = new QTableWidgetItem(QString::number(frame.imgId));
    QTableWidgetItem* item_name = new QTableWidgetItem(frame.name);

    int rows = m_ui->framesTableWidget->rowCount();

    m_mw->cellChangedSignals(false);
    m_ui->framesTableWidget->setRowCount(rows + 1);
    m_ui->framesTableWidget->setItem(rows, ColFrameId,        item_id);
    m_ui->framesTableWidget->setItem(rows, ColFrameVisibleId, item_vid);
    m_ui->framesTableWidget->setItem(rows, ColFrameOx,        item_ox);
    m_ui->framesTableWidget->setItem(rows, ColFrameOy,        item_oy);
    m_ui->framesTableWidget->setItem(rows, ColFrameW,         item_w);
    m_ui->framesTableWidget->setItem(rows, ColFrameH,         item_h);
    m_ui->framesTableWidget->setItem(rows, ColFrameImgId,     item_iid);
    m_ui->framesTableWidget->setItem(rows, ColFrameName,      item_name);
    m_mw->cellChangedSignals(true);

    showFrame(frame.id);

    m_ui->framesTableWidget->setFocus();
    m_ui->framesTableWidget->setCurrentCell(rows, ColFrameOx);
}

void FrameTabController::showSelFrame(int row)
{
    Id frameId = (row == -1) ? NullId : getFrameId(row);
    showFrame(frameId);
}

void FrameTabController::showFrame(Id frameId)
{
    const QPixmap& selPixmap = m_state->fpixmap(frameId);
    m_ui->framePreview->setPixmap(selPixmap);

    if (frameId == NullId) {
        m_mw->images()->showSelImageWithFrameRect(-1, QRect());
    } else {
        const LvkFrame frame = m_state->const_frame(frameId);
        for (int r = 0; r < m_ui->imgTableWidget->rowCount(); r++) {
            if (frame.imgId == m_mw->images()->getImageId(r)) {
                m_ui->imgTableWidget->selectRow(r);
                m_mw->images()->showSelImageWithFrameRect(r, frame.rect());
                break;
            }
        }
        m_ui->imgPreview->scrollToFrame(frame);
    }
}

void FrameTabController::removeSelFrame()
{
    m_mw->showFramesTab();
    int currentRow = m_ui->framesTableWidget->currentRow();
    if (currentRow == -1) {
        infoDialog(tr("No frame selected"));
        return;
    }
    QString frameName = m_ui->framesTableWidget->item(currentRow, ColFrameName)->text();
    if (!yesNoDialog(tr("Are you sure you want to remove the frame '") + frameName + tr("'?"))) {
        return;
    }
    removeFrame(currentRow);
}

void FrameTabController::removeFrame(int row)
{
    Id frameId = getFrameId(row);

    m_mw->cellChangedSignals(false);
    m_ui->framesTableWidget->removeRow(row);
    m_mw->cellChangedSignals(true);

    m_ui->framePreview->setPixmap(QPixmap());

    m_state->removeFrame(frameId);
}

void FrameTabController::updateCurrentFrame(const QRect& rect)
{
    if (m_ui->framesTableWidget->currentRow() == -1) {
        return;
    }
    LvkFrame frame = m_state->const_frame(selectedFrameId());
    frame.setRect(rect);
    m_state->updateFrame(frame);
    updateCurrentFrame_ui(rect);
    showFrame(frame.id);
}

void FrameTabController::updateCurrentFrame_ui(const QRect& rect)
{
    int currentRow = m_ui->framesTableWidget->currentRow();
    if (currentRow == -1) {
        return;
    }
    m_mw->cellChangedSignals(false);
    m_ui->framesTableWidget->item(currentRow, ColFrameOx)->setText(QString::number(rect.x()));
    m_ui->framesTableWidget->item(currentRow, ColFrameOy)->setText(QString::number(rect.y()));
    m_ui->framesTableWidget->item(currentRow, ColFrameW)->setText(QString::number(rect.width()));
    m_ui->framesTableWidget->item(currentRow, ColFrameH)->setText(QString::number(rect.height()));
    m_mw->cellChangedSignals(true);
}

void FrameTabController::removeAllUnusedFrames()
{
    if (!yesNoDialog(tr("Are you sure you want to remove all unused animations?"))) {
        return;
    }
    QVector<int> unusedRows;
    for (int row = 0; row < m_ui->framesTableWidget->rowCount(); ++row) {
        Id frameId = getFrameId(row);
        if (m_state->isFrameUnused(frameId)) {
            unusedRows.append(row);
        }
    }
    m_state->startTransaction();
    for (int i = unusedRows.size() - 1; i >= 0; --i) {
        removeFrame(unusedRows[i]);
    }
    m_state->endTransaction();
    infoDialog(QString::number(unusedRows.size()) + tr(" unused frame(s) were removed."));
}

void FrameTabController::hideFramePreview()
{
    m_ui->tabLayout->setColumnStretch(12, 0);
    m_ui->hSpacerFramePreview->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_ui->frameZoomInButton->hide();
    m_ui->frameZoomOutButton->hide();
    m_ui->framePreviewScroll->hide();
    m_ui->blendModeComboBox->hide();
    m_ui->blendModeLabel->hide();
    m_ui->hideFramePreviewButton->setIcon(QIcon(":/buttons/button-show"));
    m_ui->hideFramePreviewButton->setToolTip(tr("Show frames preview"));
    m_ui->framePreviewLayout->update();
}

void FrameTabController::showFramePreview()
{
    m_ui->tabLayout->setColumnStretch(12, 2);
    m_ui->hSpacerFramePreview->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_ui->frameZoomInButton->show();
    m_ui->frameZoomOutButton->show();
    m_ui->framePreviewScroll->show();
    m_ui->blendModeComboBox->show();
    m_ui->blendModeLabel->show();
    m_ui->hideFramePreviewButton->setIcon(QIcon(":/buttons/button-hide"));
    m_ui->hideFramePreviewButton->setToolTip(tr("Hide frames preview"));
    m_ui->framePreviewLayout->update();
}

void FrameTabController::hideShowFramePreview()
{
    static bool visible = false;
    if (visible) {
        hideFramePreview();
    } else {
        showFramePreview();
    }
    visible = !visible;
}

void FrameTabController::updateFramesTable(int row, int col)
{
    QTableWidget* table = m_ui->framesTableWidget;
    QString newValue = table->item(row, col)->text();
    Id frameId = table->item(row, ColFrameId)->text().toInt();
    LvkFrame frame = m_state->const_frame(frameId);

    auto setCellInt = [&](int c, int v){
        m_mw->cellChangedSignals(false);
        table->item(row, c)->setText(QString::number(v));
        m_mw->cellChangedSignals(true);
    };
    auto setCellStr = [&](int c, const QString& v){
        m_mw->cellChangedSignals(false);
        table->item(row, c)->setText(v);
        m_mw->cellChangedSignals(true);
    };

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
        setCellInt(col, frame.id);
        break;
    case ColFrameImgId:
        if (ok && m_state->images().contains(i)) {
            frame.imgId = i;
        } else {
            infoDialog(tr("Invalid image Id"));
            setCellInt(col, frame.imgId);
        }
        break;
    case ColFrameOx:
        if (ok) { frame.ox = i; } else {
            infoDialog(tr("Invalid frame offset."));
            setCellInt(col, frame.ox);
        }
        break;
    case ColFrameOy:
        if (ok) { frame.oy = i; } else {
            infoDialog(tr("Invalid frame offset."));
            setCellInt(col, frame.oy);
        }
        break;
    case ColFrameW:
        if (ok) { frame.w = i; } else {
            infoDialog(tr("Invalid frame width."));
            setCellInt(col, frame.w);
        }
        break;
    case ColFrameH:
        if (ok) { frame.h = i; } else {
            infoDialog(tr("Invalid frame height."));
            setCellInt(col, frame.h);
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
            setCellStr(col, frame.name);
        }
        break;
    }

    if (ok) {
        switch (col) {
        case ColFrameOx:
        case ColFrameOy:
        case ColFrameW:
        case ColFrameH:
            m_ui->imgPreview->setFrameRect(frame.rect());
            // fallthrough
        case ColFrameImgId:
        case ColFrameName:
            m_state->updateFrame(frame);
            showFrame(frame.id);
            break;
        default:
            break;
        }
    }
}

// ---- Blend mode pane -----------------------------------------------------

void FrameTabController::setBlendPixmap()
{
    blendComboBoxSignals(false);

    switch (m_ui->blendModeComboBox->currentIndex()) {
    case BlendNone:           blendNone(); break;
    case BlendExistentFrame:  blendExistentFrame(); break;
    case BlendFrameRect:      blendFrameRect(); break;
    case BlendFrameId:        blendFrameId(); break;
    default:                  blendNone(); break;
    }

    blendComboBoxSignals(true);
    m_ui->framePreview->repaint();
}

void FrameTabController::blendNone()
{
    m_ui->framePreview->setBlendPixmap(QPixmap());
}

void FrameTabController::blendExistentFrame()
{
    Id frameId = getFrameDialog("Blend pixmap");
    if (frameId != NullId) {
        m_blendFrameId = frameId;
        m_ui->framePreview->setBlendPixmap(m_state->fpixmap(frameId));

        if (m_ui->blendModeComboBox->count() == BlendModeTotal - 1) {
            m_ui->blendModeComboBox->addItem("");
        }
        if (m_ui->blendModeComboBox->count() == BlendModeTotal) {
            m_ui->blendModeComboBox->setItemText(BlendFrameId, tr("Selected frame with frame id ") + QString::number(frameId));
            m_ui->blendModeComboBox->setCurrentIndex(BlendFrameId);
        }
    } else {
        blendNone();
        m_ui->blendModeComboBox->setCurrentIndex(BlendNone);
    }
}

void FrameTabController::blendFrameRect()
{
    QRect frameRect = m_ui->imgPreview->mouseFrameRect();
    if (!frameRect.isNull() && frameRect.width() > 0 && frameRect.height() > 0) {
        Id imgId = m_mw->images()->selectedImgId();
        if (imgId != NullId) {
            const QPixmap& imgPixmap = m_state->images().value(imgId).pixmap;
            QPixmap mouseRectPixmap(imgPixmap.copy(frameRect));

            if (m_ui->blendModeComboBox->currentIndex() == BlendFrameRect) {
                m_ui->framePreview->setBlendPixmap(mouseRectPixmap);
            } else if (m_ui->blendModeComboBox->currentIndex() == BlendNone) {
                m_ui->framePreview->setPixmap(mouseRectPixmap);
            }
            m_ui->framePreview->repaint();
        }
    }
}

void FrameTabController::blendFrameId()
{
    if (m_blendFrameId != NullId && m_state->fpixmaps().contains(m_blendFrameId)) {
        m_ui->framePreview->setBlendPixmap(m_state->fpixmap(m_blendFrameId));
    } else {
        infoDialog(tr("Frame id ") + QString::number(m_blendFrameId) + tr(" is no longer available"));
        m_blendFrameId = NullId;
    }
    if (m_blendFrameId == NullId) {
        m_ui->blendModeComboBox->removeItem(BlendFrameId);
        m_ui->blendModeComboBox->setCurrentIndex(BlendNone);
    }
}

void FrameTabController::blendComboBoxSignals(bool connected)
{
    if (connected) {
        connect(m_ui->blendModeComboBox, &QComboBox::currentIndexChanged,
                this, &FrameTabController::setBlendPixmap);
    } else {
        disconnect(m_ui->blendModeComboBox, &QComboBox::currentIndexChanged,
                   this, &FrameTabController::setBlendPixmap);
    }
}
