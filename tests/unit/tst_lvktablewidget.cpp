// Unit test for LvkTableWidget cellChanged emission semantics (Team J3.4).
//
// Regression covered: LvkTableWidget::keyReleaseEvent used to follow each
// QTableWidgetItem::setText() with an explicit `emit cellChanged(row, col)`
// -- but setText goes through QTableWidget's underlying model, which
// already emits cellChanged via itemChanged -> setData(). So any slot
// connected to cellChanged ran twice for a single Ctrl+Up / Ctrl+Down,
// producing duplicate undo entries.
//
// The fix drops the explicit emit. This test connects a counter slot and
// simulates Ctrl+Up on a cell holding "5"; the counter must increment by
// exactly 1 (not 2 as it did pre-J3).

#include <QtTest/QtTest>
#include <QApplication>
#include <QKeyEvent>
#include <QTableWidgetItem>

#include "lvktablewidget.h"

class TestLvkTableWidget : public QObject
{
    Q_OBJECT

private slots:
    void ctrlUpEmitsCellChangedExactlyOnce();
    void ctrlDownEmitsCellChangedExactlyOnce();
};

void TestLvkTableWidget::ctrlUpEmitsCellChangedExactlyOnce()
{
    LvkTableWidget t;
    t.setRowCount(1);
    t.setColumnCount(1);
    auto *item = new QTableWidgetItem(QStringLiteral("5"));
    t.setItem(0, 0, item);
    t.setCurrentCell(0, 0);

    int signalCount = 0;
    QObject::connect(&t, &QTableWidget::cellChanged,
                     &t, [&signalCount](int, int) { ++signalCount; });

    // Drive the key handler directly so the test does not rely on the
    // event loop / focus policy quirks of a test fixture.
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Up, Qt::ControlModifier);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Up, Qt::ControlModifier);
    QApplication::sendEvent(&t, &press);
    QApplication::sendEvent(&t, &release);

    QCOMPARE(t.item(0, 0)->text(), QStringLiteral("6"));
    QCOMPARE(signalCount, 1);
}

void TestLvkTableWidget::ctrlDownEmitsCellChangedExactlyOnce()
{
    LvkTableWidget t;
    t.setRowCount(1);
    t.setColumnCount(1);
    auto *item = new QTableWidgetItem(QStringLiteral("5"));
    t.setItem(0, 0, item);
    t.setCurrentCell(0, 0);

    int signalCount = 0;
    QObject::connect(&t, &QTableWidget::cellChanged,
                     &t, [&signalCount](int, int) { ++signalCount; });

    QKeyEvent press(QEvent::KeyPress, Qt::Key_Down, Qt::ControlModifier);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Down, Qt::ControlModifier);
    QApplication::sendEvent(&t, &press);
    QApplication::sendEvent(&t, &release);

    QCOMPARE(t.item(0, 0)->text(), QStringLiteral("4"));
    QCOMPARE(signalCount, 1);
}

QTEST_MAIN(TestLvkTableWidget)
#include "tst_lvktablewidget.moc"
