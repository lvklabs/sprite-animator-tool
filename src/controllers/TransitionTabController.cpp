// TransitionTabController.cpp
//
// See TransitionTabController.h. Code lifted from MainWindow.

#include "controllers/TransitionTabController.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controllers/AnimationTabController.h"
#include "dialogs.h"
#include "lvkanimationwidget.h"

#include <QInputDialog>
#include <QList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QMapIterator>

namespace {

enum {
    ColTransAniId,
    ColTransAniCheckable,
    ColTransAniName,
    ColTransTotal,
};

}

TransitionTabController::TransitionTabController(MainWindow* mw, Ui::MainWindow* ui,
                                                 SpriteState2* state, QObject* parent)
    : QObject(parent), m_mw(mw), m_ui(ui), m_state(state)
{
}

void TransitionTabController::wireSignals()
{
    connect(m_ui->addAniTransButton,       &QAbstractButton::clicked, this, &TransitionTabController::addTransDialog);
    connect(m_ui->refreshTransButton,      &QAbstractButton::clicked, this, &TransitionTabController::previewTransition);
    connect(m_ui->removeAniTransButton,    &QAbstractButton::clicked, this, &TransitionTabController::removeSelTrans);
    connect(m_ui->removeAllAniTransButton, &QAbstractButton::clicked, this, &TransitionTabController::removeAllTrans);
}

Id TransitionTabController::getTransAniId(int row) const
{
    const QTableWidget* t = m_ui->transTableWidget;
    return (row >= 0 && row < t->rowCount()) ? t->item(row, ColTransAniId)->text().toInt() : NullId;
}

void TransitionTabController::addTransDialog()
{
    if (m_state->animations().isEmpty()) {
        infoDialog(tr("No animations available.\n\nGo to the \"Animations\" tab to create one animation."));
        return;
    }

    QStringList anisList;
    for (QMapIterator<Id, LvkAnimation> it(m_state->animations()); it.hasNext();) {
        it.next();
        const LvkAnimation& ani = it.value();
        anisList << tr("Id: ") + QString::number(ani.id) + tr(" Name: ") + ani.name;
    }
    anisList.sort();

    bool ok;
    QString ani_str = QInputDialog::getItem(m_mw, tr("Add animation"),
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

void TransitionTabController::addTrans(Id aniId)
{
    // TODO: m_state->addAniTrans(aniId);
    addTrans_ui(aniId);
}

void TransitionTabController::addTrans_ui(Id aniId)
{
    LvkAnimation ani = m_state->animations().value(aniId);

    QTableWidgetItem* item_aniId      = new QTableWidgetItem(QString::number(ani.id));
    QTableWidgetItem* item_aniName    = new QTableWidgetItem(ani.name);
    QTableWidgetItem* item_aniChecked = new QTableWidgetItem("");

    int rows = m_ui->transTableWidget->rowCount();
    m_ui->transTableWidget->setRowCount(rows + 1);
    m_ui->transTableWidget->setItem(rows, ColTransAniId,        item_aniId);
    m_ui->transTableWidget->setItem(rows, ColTransAniName,      item_aniName);
    m_ui->transTableWidget->setItem(rows, ColTransAniCheckable, item_aniChecked);
    m_ui->transTableWidget->setCurrentItem(item_aniId);

    previewTransition();
}

void TransitionTabController::removeSelTrans()
{
    if (m_ui->transTableWidget->currentRow() != -1) {
        m_ui->transTableWidget->removeRow(m_ui->transTableWidget->currentRow());
        previewTransition();
    }
}

void TransitionTabController::removeAllTrans()
{
    m_ui->transTableWidget->clearContents();
    m_ui->transTableWidget->setRowCount(0);
    previewTransition();
}

void TransitionTabController::previewTransition()
{
    QList<LvkAnimation> aniList;
    for (int row = 0; row < m_ui->transTableWidget->rowCount(); row++) {
        aniList << m_state->animations().value(getTransAniId(row));
    }
    m_ui->transPreview->setAnimations(aniList, m_state->fpixmaps());
    m_ui->transPreview->play();
}

void TransitionTabController::clearPreviewTransition()
{
    m_ui->transPreview->clear();
}
