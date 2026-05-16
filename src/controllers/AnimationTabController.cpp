// AnimationTabController.cpp
//
// See AnimationTabController.h. Code lifted from MainWindow.

#include "controllers/AnimationTabController.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controllers/FrameTabController.h"
#include "controllers/TransitionTabController.h"
#include "dialogs.h"
#include "lvkanimationwidget.h"
#include "lvktablewidget.h"
#include "lvkinputimagewidget.h"

#include <QInputDialog>
#include <QListIterator>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QMapIterator>
#include <QDebug>

namespace {

enum {
    ColAniId,
    ColAniName,
    ColAniFlags,
    ColAniTotal,
};

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

static QString toHexString(unsigned i) { return "0x" + QString::number(i, 16); }

}

AnimationTabController::AnimationTabController(MainWindow* mw, Ui::MainWindow* ui,
                                               SpriteState2* state, QObject* parent)
    : QObject(parent), m_mw(mw), m_ui(ui), m_state(state)
{
}

void AnimationTabController::wireSignals()
{
    connect(m_ui->actionAddAnimation,    &QAction::triggered, this, &AnimationTabController::addAnimationDialog);
    connect(m_ui->actionRemoveAnimation, &QAction::triggered, this, &AnimationTabController::removeSelAnimation);
    connect(m_ui->actionRemoveAframe,    &QAction::triggered, this, &AnimationTabController::removeSelAframe);
    connect(m_ui->actionAddAframe,       &QAction::triggered, this, &AnimationTabController::addAframeDialog);
    connect(m_ui->actionInvertAframesOrder, &QAction::triggered, this, &AnimationTabController::invertAframesOrder);
    connect(m_ui->actionRefreshAnimation,&QAction::triggered, this, &AnimationTabController::previewAnimation);

    connect(m_ui->addAniButton,       &QAbstractButton::clicked, this, &AnimationTabController::addAnimationDialog);
    connect(m_ui->removeAniButton,    &QAbstractButton::clicked, this, &AnimationTabController::removeSelAnimation);
    connect(m_ui->refreshAniButton,   &QAbstractButton::clicked, this, &AnimationTabController::previewAnimation);
    connect(m_ui->addAframeButton,    &QAbstractButton::clicked, this, &AnimationTabController::addAframeDialog);
    connect(m_ui->removeAframeButton, &QAbstractButton::clicked, this, &AnimationTabController::removeSelAframe);
    connect(m_ui->aniDecSpeedButton,  &QAbstractButton::clicked, this, [this](bool){ decAniSpeed(); });
    connect(m_ui->aniIncSpeedButton,  &QAbstractButton::clicked, this, [this](bool){ incAniSpeed(); });
    connect(m_ui->moveDownAframeButton,&QAbstractButton::clicked, this, &AnimationTabController::moveSelAframeDown);
    connect(m_ui->moveUpAframeButton, &QAbstractButton::clicked, this, &AnimationTabController::moveSelAframeUp);
    connect(m_ui->invertAframesButton,&QAbstractButton::clicked, this, &AnimationTabController::invertAframesOrder);

    connect(m_ui->landscapeCheckBox,  &QCheckBox::stateChanged, this, &AnimationTabController::switchLandscapeMode);
    // Qt6: QComboBox::activated(QString) was removed; use activated(int) + itemText().
    connect(m_ui->previewScrSizeCombo, QOverload<int>::of(&QComboBox::activated),
            this, [this](int index){ changePreviewScrSize(m_ui->previewScrSizeCombo->itemText(index)); });

    connect(m_ui->aframesTableWidget, &QTableWidget::currentCellChanged,
            this, [this](int row, int, int, int){ showSelAframe(row); });
    connect(m_ui->aniTableWidget,     &QTableWidget::currentCellChanged,
            this, [this](int row, int, int, int){ showAframes(row); });

    connect(m_ui->aframesTableWidget, &QTableWidget::cellChanged,
            this, &AnimationTabController::updateAframesTable);
    connect(m_ui->aniTableWidget,     &QTableWidget::cellChanged,
            this, &AnimationTabController::updateAniTable);
}

void AnimationTabController::refreshTables()
{
    // refresh ani
    {
        m_mw->cellChangedSignals(false);
        int row = m_ui->aniTableWidget->currentRow();
        int col = m_ui->aniTableWidget->currentColumn();
        m_ui->aniTableWidget->clearContents();
        m_ui->aniTableWidget->setRowCount(0);
        for (QMapIterator<Id, LvkAnimation> it(m_state->animations()); it.hasNext();) {
            it.next();
            const LvkAnimation& ani = it.value();
            addAnimation_ui(ani);
        }
        m_ui->aniTableWidget->setCurrentCell(row, col);
        m_mw->cellChangedSignals(true);
    }

    // refresh aframes
    {
        m_mw->cellChangedSignals(false);
        int row = m_ui->aframesTableWidget->currentRow();
        int col = m_ui->aframesTableWidget->currentColumn();
        m_ui->aframesTableWidget->clearContents();
        m_ui->aframesTableWidget->setRowCount(0);
        int ani_row = m_ui->aniTableWidget->currentRow();
        if (ani_row != -1) {
            Id aniId = getAnimationId(ani_row);
            const QList<LvkAframe> aframes = m_state->aframes(aniId);
            for (QListIterator<LvkAframe> it2(aframes); it2.hasNext();) {
                const LvkAframe& aframe = it2.next();
                addAframe_ui(aframe, aniId);
            }
        }
        m_ui->aframesTableWidget->setCurrentCell(row, col);
        m_mw->cellChangedSignals(true);
    }
}

Id AnimationTabController::getAnimationId(int row) const
{
    const QTableWidget* t = m_ui->aniTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColAniId)->text().toInt() : NullId;
}

Id AnimationTabController::getAframeId(int row) const
{
    const QTableWidget* t = m_ui->aframesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColAframeId)->text().toInt() : NullId;
}

Id AnimationTabController::getAframeFrameId(int row) const
{
    const QTableWidget* t = m_ui->aframesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColAframeFrameId)->text().toInt() : NullId;
}

Id AnimationTabController::getAframeAniId(int row) const
{
    const QTableWidget* t = m_ui->aframesTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColAframeAniId)->text().toInt() : NullId;
}

Id AnimationTabController::selectedAniId() const
{
    return getAnimationId(m_ui->aniTableWidget->currentRow());
}

Id AnimationTabController::selectedAframeId() const
{
    return getAframeId(m_ui->aframesTableWidget->currentRow());
}

Id AnimationTabController::addAnimation(const LvkAnimation& ani)
{
    LvkAnimation ani_ = ani;
    m_state->addAnimation(ani_);
    addAnimation_ui(ani_);
    return ani_.id;
}

void AnimationTabController::addAnimation_ui(const LvkAnimation& ani)
{
    int rows = m_ui->aniTableWidget->rowCount();

    QTableWidgetItem* item_id    = new QTableWidgetItem(QString::number(ani.id));
    QTableWidgetItem* item_name  = new QTableWidgetItem(ani.name);
    QTableWidgetItem* item_flags = new QTableWidgetItem(toHexString(ani.flags));

    m_mw->cellChangedSignals(false);
    m_ui->aniTableWidget->setRowCount(rows + 1);
    m_ui->aniTableWidget->setItem(rows, ColAniId,    item_id);
    m_ui->aniTableWidget->setItem(rows, ColAniName,  item_name);
    m_ui->aniTableWidget->setItem(rows, ColAniFlags, item_flags);
    m_ui->aniTableWidget->setCurrentItem(item_id);
    m_mw->cellChangedSignals(true);

    showAframes(rows);
    clearPreviewAnimation();
}

Id AnimationTabController::addAframe(const LvkAframe& aframe, Id aniId)
{
    LvkAframe aframe_ = aframe;
    m_state->addAframe(aframe_, aniId);
    addAframe_ui(aframe_, aniId);
    return aframe_.id;
}

void AnimationTabController::addAframe_ui(const LvkAframe& aframe, Id aniId)
{
    QTableWidgetItem* item_id     = new QTableWidgetItem(QString::number(aframe.id));
    QTableWidgetItem* item_fid    = new QTableWidgetItem(QString::number(aframe.frameId));
    QTableWidgetItem* item_delay  = new QTableWidgetItem(QString::number(aframe.delay));
    QTableWidgetItem* item_sticky = new QTableWidgetItem(QString::number(aframe.sticky));
    QTableWidgetItem* item_ox     = new QTableWidgetItem(QString::number(aframe.ox));
    QTableWidgetItem* item_oy     = new QTableWidgetItem(QString::number(aframe.oy));
    QTableWidgetItem* item_aniId  = new QTableWidgetItem(QString::number(aniId));

    int rows = m_ui->aframesTableWidget->rowCount();

    m_mw->cellChangedSignals(false);
    m_ui->aframesTableWidget->setRowCount(rows + 1);
    m_ui->aframesTableWidget->setItem(rows, ColAframeId,      item_id);
    m_ui->aframesTableWidget->setItem(rows, ColAframeFrameId, item_fid);
    m_ui->aframesTableWidget->setItem(rows, ColAframeDelay,   item_delay);
    m_ui->aframesTableWidget->setItem(rows, ColAframeSticky,  item_sticky);
    m_ui->aframesTableWidget->setItem(rows, ColAframeOx,      item_ox);
    m_ui->aframesTableWidget->setItem(rows, ColAframeOy,      item_oy);
    m_ui->aframesTableWidget->setItem(rows, ColAframeAniId,   item_aniId);
    m_ui->aframesTableWidget->setCurrentItem(item_id);
    m_mw->cellChangedSignals(true);

    showAframe(aframe.id);
    previewAnimation();

    m_ui->aframesTableWidget->setFocus();
    m_ui->aframesTableWidget->setCurrentCell(rows, ColAframeFrameId);
}

void AnimationTabController::addAnimationDialog()
{
    m_mw->showAnimationsTab();
    bool ok;
    QString name = QInputDialog::getText(m_mw, tr("New animation"),
                                         tr("Animation name:"),
                                         QLineEdit::Normal, "", &ok);
    if (ok) {
        name = name.trimmed();
        if (name.isEmpty()) {
            infoDialog(tr("Cannot add an animation without name"));
            return;
        }
        addAnimation(LvkAnimation(NullId, name));
    }
}

void AnimationTabController::addAframeDialog()
{
    if (m_ui->aniTableWidget->currentRow() == -1) {
        infoDialog(tr("No animation selected"));
        return;
    }
    Id frameId = m_mw->frames()->getFrameDialog(tr("Add animation frame"));
    if (frameId != NullId) {
        addAframe(LvkAframe(NullId, frameId), selectedAniId());
    }
}

void AnimationTabController::removeSelAnimation()
{
    m_mw->showAnimationsTab();
    int currentRow = m_ui->aniTableWidget->currentRow();
    if (currentRow == -1) {
        infoDialog(tr("No animation selected"));
        return;
    }
    QString aniName = m_ui->aniTableWidget->item(currentRow, ColAniName)->text();
    if (!yesNoDialog(tr("Are you sure you want to remove the animation '") + aniName + tr("'?"))) {
        return;
    }
    removeAnimation(currentRow);
    showAframes(m_ui->aniTableWidget->currentRow());
}

void AnimationTabController::removeAnimation(int row)
{
    Id aniId = getAnimationId(row);
    m_mw->cellChangedSignals(false);
    m_ui->aniTableWidget->removeRow(row);
    m_mw->cellChangedSignals(true);
    clearPreviewAnimation();
    m_state->removeAnimation(aniId);
}

void AnimationTabController::removeSelAframe()
{
    m_mw->showAnimationsTab();
    int currentRow = m_ui->aframesTableWidget->currentRow();
    if (currentRow == -1) {
        infoDialog(tr("No frame selected"));
        return;
    }
    if (!yesNoDialog(tr("Are you sure you want to remove the selected frame?"))) {
        return;
    }
    removeAframe(currentRow);
}

void AnimationTabController::removeAframe(int row)
{
    Id aframeId = getAframeId(row);
    Id aniId = selectedAniId();
    m_state->removeAframe(aframeId, aniId);

    m_mw->cellChangedSignals(false);
    m_ui->aframesTableWidget->removeRow(row);
    m_mw->cellChangedSignals(true);

    m_ui->aframePreview->setPixmap(QPixmap());

    previewAnimation();
}

void AnimationTabController::moveSelAframeUp()   { moveSelAframe(1); }
void AnimationTabController::moveSelAframeDown() { moveSelAframe(-1); }

void AnimationTabController::moveSelAframe(int offset)
{
    LvkTableWidget* table = m_ui->aframesTableWidget;
    if (table->currentRow() == -1) {
        infoDialog(tr("No frame selected"));
        return;
    } else if (m_ui->aniTableWidget->currentRow() == -1) {
        infoDialog(tr("No animation selected"));
        return;
    }
    int currentRow = table->currentRow();
    int targetRow = currentRow - offset;
    if (targetRow < 0 || targetRow >= table->rowCount()) {
        return;
    }
    LvkAnimation ani = m_state->const_animation(selectedAniId());
    ani.swapAframes(getAframeId(currentRow), getAframeId(targetRow));
    m_state->updateAnimation(ani);

    m_mw->cellChangedSignals(false);
    table->swapRows(currentRow, targetRow);
    m_mw->cellChangedSignals(true);

    previewAnimation();
    table->setCurrentCell(targetRow, table->currentColumn());
}

void AnimationTabController::invertAframesOrder()
{
    LvkTableWidget* table = m_ui->aframesTableWidget;
    LvkAnimation ani = m_state->const_animation(selectedAniId());
    int rowCount = table->rowCount();

    m_mw->cellChangedSignals(false);
    for (int r = 0; r < rowCount / 2; r++) {
        int r2 = rowCount - r - 1;
        ani.swapAframes(getAframeId(r), getAframeId(r2));
        table->swapRows(r, r2);
    }
    m_mw->cellChangedSignals(true);

    m_state->updateAnimation(ani);
    previewAnimation();
}

void AnimationTabController::showAframes(int row)
{
    m_ui->aniPreview->stop();
    m_mw->cellChangedSignals(false);

    m_ui->aframePreview->setEnabled(false);
    m_ui->aframesTableWidget->setEnabled(false);
    m_ui->aframesTableWidget->clearContents();
    m_ui->aframesTableWidget->setRowCount(0);

    if (row == -1) {
        m_mw->cellChangedSignals(true);
        return;
    }

    int animationId = getAnimationId(row);
    LvkAnimation ani = m_state->animations().value(animationId);
    for (QListIterator<LvkAframe> it(ani._aframes); it.hasNext();) {
        LvkAframe aFrame = it.next();
        addAframe_ui(aFrame, animationId);
    }

    if (m_ui->aframesTableWidget->rowCount() > 0) {
        m_ui->aframesTableWidget->selectRow(0);
        showSelAframe(0);
    }

    m_ui->aframePreview->setEnabled(true);
    m_ui->aframesTableWidget->setEnabled(true);

    m_mw->cellChangedSignals(true);

    previewAnimation();
}

void AnimationTabController::showSelAframe(int row)
{
    Id frameId = (row == -1) ? NullId : getAframeFrameId(row);
    showAframe(frameId);
}

void AnimationTabController::showAframe(Id frameId)
{
    const QPixmap& selPixmap = m_state->fpixmap(frameId);
    int w = selPixmap.width();
    int h = selPixmap.height();
    m_ui->aframePreview->setPixmap(selPixmap);
    m_ui->aframePreview->setGeometry(0, 0, w, h);
    m_ui->aframePreview->updateGeometry();
}

void AnimationTabController::previewAnimation()
{
    if (m_ui->aniTableWidget->currentRow() == -1) {
        return;
    }
    LvkAnimation selectedAni = m_state->animations().value(selectedAniId());
    m_ui->aniPreview->setAnimation(selectedAni, m_state->fpixmaps());
    m_ui->aniPreview->play();
}

void AnimationTabController::clearPreviewAnimation()
{
    m_ui->aniPreview->clear();
}

void AnimationTabController::incAniSpeed(int ms)
{
    if (m_ui->aniTableWidget->currentRow() == -1) {
        infoDialog(tr("No animation selected"));
        return;
    }
    Id aniId = selectedAniId();
    for (int r = 0; r < m_ui->aframesTableWidget->rowCount(); ++r) {
        LvkAframe aframe = m_state->const_aframe(aniId, getAframeId(r));
        aframe.delay -= ms;
        if (aframe.delay < 0) aframe.delay = 0;
        m_state->updateAframe(aframe, aniId);
        m_ui->aframesTableWidget->item(r, ColAframeDelay)->setText(QString::number(aframe.delay));
    }
    previewAnimation();
}

void AnimationTabController::decAniSpeed(int ms)
{
    incAniSpeed(-ms);
}

void AnimationTabController::switchLandscapeMode()
{
    changePreviewScrSize(m_ui->previewScrSizeCombo->currentText());
}

void AnimationTabController::changePreviewScrSize(const QString& text)
{
    QString res = text.mid(0, text.indexOf(' '));
    bool ok = false;
    bool custom = false;

    if (res == tr("Custom...")) {
        custom = true;
        res = QInputDialog::getText(m_mw, tr("Insert custom screen resolution"),
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
    if (m_ui->landscapeCheckBox->isChecked()) {
        m_ui->aniPreview->setScreenSize(h, w);
        m_ui->transPreview->setScreenSize(h, w);
    } else {
        m_ui->aniPreview->setScreenSize(w, h);
        m_ui->transPreview->setScreenSize(w, h);
    }
    if (custom) {
        bool found = false;
        for (int i = 0; i < m_ui->previewScrSizeCombo->count(); ++i) {
            if (res == m_ui->previewScrSizeCombo->itemText(i)) {
                found = true;
                break;
            }
        }
        if (!found) m_ui->previewScrSizeCombo->addItem(res);
    }
}

void AnimationTabController::updateAframesTable(int row, int col)
{
    QTableWidget* table = m_ui->aframesTableWidget;
    QString newValue = table->item(row, col)->text();
    Id aniId    = table->item(row, ColAframeAniId)->text().toInt();
    Id aframeId = table->item(row, ColAframeId)->text().toInt();
    LvkAframe aframe = m_state->const_aframe(aniId, aframeId);

    auto setCellInt = [&](int c, int v){
        m_mw->cellChangedSignals(false);
        table->item(row, c)->setText(QString::number(v));
        m_mw->cellChangedSignals(true);
    };

    bool ok = true;
    int i = newValue.toInt(&ok);

    switch (col) {
    case ColAframeId:
        ok = false;
        infoDialog(tr("Column \"Id\" is not editable"));
        break;
    case ColAframeAniId:
        ok = false;
        infoDialog(tr("Column \"Animation Id\" is not editable."));
        setCellInt(col, aniId);
        break;
    case ColAframeFrameId:
        if (ok && m_state->frames().contains(i)) {
            aframe.frameId = i;
        } else {
            infoDialog(tr("Invalid frame id."));
            setCellInt(col, aframe.frameId);
        }
        break;
    case ColAframeOx:
        if (ok) { aframe.ox = i; } else {
            infoDialog(tr("Invalid frame offset."));
            setCellInt(col, aframe.ox);
        }
        break;
    case ColAframeOy:
        if (ok) { aframe.oy = i; } else {
            infoDialog(tr("Invalid frame offset."));
            setCellInt(col, aframe.oy);
        }
        break;
    case ColAframeSticky:
        if (ok && (i == 0 || i == 1)) {
            aframe.sticky = i;
        } else {
            infoDialog(tr("Invalid sticky value. Use: 1 = On, 0 = Off"));
            setCellInt(col, aframe.sticky);
        }
        break;
    case ColAframeDelay:
        if (ok && i >= 0) {
            aframe.delay = i;
        } else {
            infoDialog(tr("Invalid frame delay."));
            setCellInt(col, aframe.delay);
        }
        break;
    }

    if (ok) {
        m_state->updateAframe(aframe, aniId);
        if (m_ui->aniPreview->isPlaying()) {
            previewAnimation();
        }
    }
}

void AnimationTabController::updateAniTable(int row, int col)
{
    QTableWidget* table = m_ui->aniTableWidget;
    QString newValue = table->item(row, col)->text();
    Id aniId = table->item(row, ColAniId)->text().toInt();
    LvkAnimation ani = m_state->const_animation(aniId);

    auto setCellStr = [&](int c, const QString& v){
        m_mw->cellChangedSignals(false);
        table->item(row, c)->setText(v);
        m_mw->cellChangedSignals(true);
    };

    switch (col) {
    case ColAniId:
        infoDialog(tr("Column \"Id\" is not editable"));
        break;
    case ColAniName:
        if (newValue.isEmpty()) {
            infoDialog(tr("Animation name cannot be empty"));
            setCellStr(col, ani.name);
        } else if (newValue.contains(',')) {
            infoDialog(tr("Animation name cannot contain the character ','"));
            setCellStr(col, ani.name);
        } else {
            ani.name = newValue;
            m_state->updateAnimation(ani);
        }
        break;
    case ColAniFlags:
        if (newValue.isEmpty()) {
            ani.flags = 0;
            m_state->updateAnimation(ani);
            setCellStr(col, toHexString(ani.flags));
        } else {
            bool ok = false;
            unsigned flags = newValue.toInt(&ok, 16);
            if (ok) {
                ani.flags = flags;
                m_state->updateAnimation(ani);
            } else {
                infoDialog(tr("Animation flags must be a 32 bits hex number"));
            }
            setCellStr(col, toHexString(ani.flags));
        }
        break;
    }
}
