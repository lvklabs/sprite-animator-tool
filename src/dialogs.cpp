// dialogs.cpp -- Team H3: dialog helpers grew a parent / title / icon.
//
// The legacy helpers shipped a stock "QMessageBox msg;" with no parent
// window, no window title, and no QMessageBox::Icon. On most desktops
// that surfaces as a bare unlabelled bubble floating in the centre of
// the screen rather than over the application -- easy to miss, and on
// some window managers it doesn't even get a taskbar entry.
//
// The fix is to accept an optional parent QWidget* so callers can pass
// their owning window (the controllers have m_mw; MainWindow methods
// can pass `this`). With a parent set, QMessageBox::exec() centres the
// dialog over that widget, inherits its style/palette, and gets the
// usual modal-child decorations from the window system. We also set a
// per-helper window title and a matching QMessageBox::Icon so users can
// distinguish info / question / warning / error at a glance.
//
// We deliberately keep the simple free-function API: there are dozens of
// call sites and there is no value in turning each into a class. The
// default parent is nullptr so legacy call sites still compile.

#include <QCoreApplication>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "dialogs.h"

void infoDialog(const QString &str, QWidget *parent) {
    QMessageBox msg(parent);
    msg.setWindowTitle(QObject::tr("Information"));
    msg.setIcon(QMessageBox::Information);
    msg.setText(str);
    msg.exec();
}

bool yesNoDialog(const QString &str, QWidget *parent) {
    QMessageBox msg(parent);
    msg.setWindowTitle(QObject::tr("Question"));
    msg.setIcon(QMessageBox::Question);

    QPushButton *yes = msg.addButton(QMessageBox::Yes);
    QPushButton *no = msg.addButton(QMessageBox::No);

    msg.setText(str);
    msg.exec();

    if (msg.clickedButton() == yes) {
        return true;
    }
    if (msg.clickedButton() == no) {
        return false;
    }
    return false;
}

DialogButton yesNoCancelDialog(const QString &str, QWidget *parent) {
    QMessageBox msg(parent);
    msg.setWindowTitle(QObject::tr("Question"));
    msg.setIcon(QMessageBox::Question);

    QPushButton *yes = msg.addButton(QMessageBox::Yes);
    QPushButton *no = msg.addButton(QMessageBox::No);
    QPushButton *cancel = msg.addButton(QMessageBox::Cancel);

    msg.setText(str);
    msg.exec();

    if (msg.clickedButton() == yes) {
        return YesButton;
    }
    if (msg.clickedButton() == no) {
        return NoButton;
    }
    if (msg.clickedButton() == cancel) {
        return CancelButton;
    }
    return CancelButton;
}

void errorDialog(const QString &str, QWidget *parent) {
    QMessageBox msg(parent);
    msg.setWindowTitle(QObject::tr("Error"));
    msg.setIcon(QMessageBox::Critical);
    msg.setText(str);
    msg.exec();
}
