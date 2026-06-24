#include <QDebug>

#include "lvktablewidget.h"

LvkTableWidget::LvkTableWidget(QWidget *parent) : QTableWidget(parent) {}

void LvkTableWidget::ignoreRow(int row) {
    ignoredRows.insert(row);
}

void LvkTableWidget::ignoreColumn(int col) {
    ignoredCols.insert(col);
}

void LvkTableWidget::swapRows(int row1, int row2) {
    QTableWidgetItem *tmp1;
    QTableWidgetItem *tmp2;

    for (int col = 0; col < columnCount(); ++col) {
        tmp1 = takeItem(row1, col);
        tmp2 = takeItem(row2, col);
        setItem(row1, col, tmp2);
        setItem(row2, col, tmp1);
    }
}

void LvkTableWidget::keyPressEvent(QKeyEvent *event) {
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Up) {
        /* nothing to do */
    } else if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Down) {
        /* nothing to do */
    } else {
        QTableWidget::keyPressEvent(event);
    }
}

void LvkTableWidget::keyReleaseEvent(QKeyEvent *event) {
    // Team J1 (J1.2): currentItem() returns nullptr when the focused cell
    // has no QTableWidgetItem set on it -- the common case after
    // setRowCount(n) without a matching setItem(). Dereferencing it
    // (->text()) on Ctrl+Up / Ctrl+Down would segfault the GUI. Guard
    // up front and delegate to the base class so the keystroke still
    // gets standard handling (focus navigation etc.).
    QTableWidgetItem *item = currentItem();
    if (!item) {
        QTableWidget::keyReleaseEvent(event);
        return;
    }

    bool ignore = ignoredRows.contains(currentRow()) || ignoredCols.contains(currentColumn());

    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Up) {
        if (!ignore) {
            bool ok;
            int i = item->text().toInt(&ok);
            if (ok) {
                // J1+J3: use the cached `item` pointer (J1 already
                // null-guarded above) and drop the redundant `emit
                // cellChanged(...)`. QTableWidgetItem::setText writes
                // through the QTableWidgetModel which already emits
                // cellChanged via itemChanged->setData. The earlier
                // explicit emit fired the signal twice, producing
                // spurious undo entries on Ctrl+Up.
                item->setText(QString::number(i + 1));
            }
        }
    } else if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Down) {
        if (!ignore) {
            bool ok;
            int i = item->text().toInt(&ok);
            if (ok) {
                // J1+J3: see Ctrl+Up branch — cached `item`, no emit.
                item->setText(QString::number(i - 1));
            }
        }
    } else {
        QTableWidget::keyReleaseEvent(event);
    }
}
