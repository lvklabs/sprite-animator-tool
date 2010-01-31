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
