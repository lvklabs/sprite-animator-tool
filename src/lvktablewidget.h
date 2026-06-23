#ifndef LVKTABLEWIDGET_H
#define LVKTABLEWIDGET_H

#include <QKeyEvent>
#include <QSet>
#include <QTableWidget>

class LvkTableWidget : public QTableWidget {
    Q_OBJECT

public:
    LvkTableWidget(QWidget *parent = 0);

    void ignoreRow(int row);
    void ignoreColumn(int col);

    void swapRows(int row1, int row2);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    QSet<int> ignoredRows;
    QSet<int> ignoredCols;
};

#endif // LVKTABLEWIDGET_H
