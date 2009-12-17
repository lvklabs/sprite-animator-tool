#include <QDebug>

#include "lvktablewidget.h"

LvkTableWidget::LvkTableWidget(QWidget *parent)
        : QTableWidget(parent)
{
}

void LvkTableWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::AltModifier && event->key() == Qt::Key_Up) {
        /* nothing to do */
    } else if (event->modifiers() == Qt::AltModifier && event->key() == Qt::Key_Down) {
        /* nothing to do */
    } else {
        QTableWidget::keyPressEvent(event);
    }
}

void LvkTableWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::AltModifier && event->key() == Qt::Key_Up) {
        bool ok;
        int i = currentItem()->text().toInt(&ok);
        if (ok) {
            currentItem()->setText(QString::number(i + 1));
            emit cellChanged(currentRow(), currentColumn());
        }
    } else if (event->modifiers() == Qt::AltModifier && event->key() == Qt::Key_Down) {
        bool ok;
        int i = currentItem()->text().toInt(&ok);
        if (ok) {
            currentItem()->setText(QString::number(i - 1));
            emit cellChanged(currentRow(), currentColumn());
        }
    } else {
        QTableWidget::keyReleaseEvent(event);
    }
}
