// Team J1 (J1.2): regression test for LvkTableWidget::keyReleaseEvent
// null-deref. Pre-J1 the slot called currentItem()->text() without
// guarding for nullptr; pressing Ctrl+Up or Ctrl+Down on a cell that
// had no QTableWidgetItem set (the typical state after setRowCount(n)
// without setItem) would segfault the GUI.
//
// The fix early-returns to QTableWidget::keyReleaseEvent when
// currentItem() is null. This test exercises that path: build the
// widget, fix focus on row 0 col 0, do NOT call setItem, then send
// Ctrl+Up and Ctrl+Down via QTest::keyClick. The test "passes" by
// simply not crashing -- but we also assert no spurious cellChanged
// signal fires (the empty-cell case must be a true no-op).

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "lvktablewidget.h"

class TestLvkTableWidget : public QObject
{
    Q_OBJECT

private slots:
    void ctrlUpOnEmptyCellDoesNotCrash();
    void ctrlDownOnEmptyCellDoesNotCrash();
    void ctrlUpOnSetCellStillIncrements();
};

void TestLvkTableWidget::ctrlUpOnEmptyCellDoesNotCrash()
{
    LvkTableWidget widget;
    widget.setRowCount(1);
    widget.setColumnCount(1);
    // Deliberately DO NOT call setItem -- currentItem() will return
    // nullptr while currentRow()/currentColumn() still report 0.
    widget.setCurrentCell(0, 0);

    QSignalSpy spy(&widget, &QTableWidget::cellChanged);

    // Pre-J1: this dereferenced nullptr and crashed.
    QTest::keyClick(&widget, Qt::Key_Up, Qt::ControlModifier);

    // No cellChanged should fire on an empty cell.
    QCOMPARE(spy.count(), 0);
}

void TestLvkTableWidget::ctrlDownOnEmptyCellDoesNotCrash()
{
    LvkTableWidget widget;
    widget.setRowCount(1);
    widget.setColumnCount(1);
    widget.setCurrentCell(0, 0);

    QSignalSpy spy(&widget, &QTableWidget::cellChanged);

    QTest::keyClick(&widget, Qt::Key_Down, Qt::ControlModifier);

    QCOMPARE(spy.count(), 0);
}

void TestLvkTableWidget::ctrlUpOnSetCellStillIncrements()
{
    // Pin that the null-guard did NOT regress the normal happy path:
    // on a populated cell with an integer value, Ctrl+Up still
    // increments and still emits cellChanged.
    LvkTableWidget widget;
    widget.setRowCount(1);
    widget.setColumnCount(1);
    QTableWidgetItem *item = new QTableWidgetItem(QStringLiteral("7"));
    widget.setItem(0, 0, item);
    widget.setCurrentCell(0, 0);

    QSignalSpy spy(&widget, &QTableWidget::cellChanged);

    QTest::keyClick(&widget, Qt::Key_Up, Qt::ControlModifier);

    QCOMPARE(widget.item(0, 0)->text(), QStringLiteral("8"));
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestLvkTableWidget)
#include "tst_lvk_table_widget.moc"
