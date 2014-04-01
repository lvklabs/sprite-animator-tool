#include <QDebug>

#include "lvktablewidget.h"

LvkTableWidget::LvkTableWidget(QWidget *parent)
        : QTableWidget(parent)
{
}

void LvkTableWidget::ignoreRow(int row)
{
    ignoredRows.insert(row);
}

void LvkTableWidget::ignoreColumn(int col)
{
    ignoredCols.insert(col);
}

void LvkTableWidget::swapRows(int row1, int row2)
{
    QTableWidgetItem* tmp1;
    QTableWidgetItem* tmp2;

    for (int col = 0; col < columnCount(); ++col) {
        tmp1 = takeItem(row1, col);
        tmp2 = takeItem(row2, col);
        setItem(row1, col, tmp2);
        setItem(row2, col, tmp1);
    }
}

void LvkTableWidget::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Up) {
        /* nothing to do */
    } else if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Down) {
        /* nothing to do */
    } else {
        QTableWidget::keyPressEvent(event);
    }
}

void LvkTableWidget::keyReleaseEvent(QKeyEvent *event)
{
    bool ignore = ignoredRows.contains(currentRow()) || ignoredCols.contains(currentColumn());

    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Up) {
        if (!ignore) {
            bool ok;
            int i = currentItem()->text().toInt(&ok);
            if (ok) {
                currentItem()->setText(QString::number(i + 1));
                emit cellChanged(currentRow(), currentColumn());
            }
        }
    } else if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Down) {
        if (!ignore) {
            bool ok;
            int i = currentItem()->text().toInt(&ok);
            if (ok) {
                currentItem()->setText(QString::number(i - 1));
                emit cellChanged(currentRow(), currentColumn());
            }
        }
    } else {
        QTableWidget::keyReleaseEvent(event);
    }
}
