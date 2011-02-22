#ifndef LVKTABLEWIDGET_H
#define LVKTABLEWIDGET_H

#include <QTableWidget>
#include <QKeyEvent>
#include <QSet>

class LvkTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    LvkTableWidget(QWidget *parent = 0);

    void ignoreRow(int row);
    void ignoreColumn(int col);

    void swapRows(int row1, int row2);

protected:
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);

private:
    QSet<int> ignoredRows;
    QSet<int> ignoredCols;
};

#endif // LVKTABLEWIDGET_H
