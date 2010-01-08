#include <QString>
#include <QMessageBox>
#include <QPushButton>

#include "dialogs.h"

void infoDialog(const QString& str)
{
    QMessageBox msg;

    msg.setText(str);
    msg.exec();
}

bool yesNoDialog(const QString& str)
{
    QMessageBox msg;

    QPushButton *yes    = msg.addButton(QMessageBox::Yes);
    QPushButton *no     = msg.addButton(QMessageBox::No);

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

DialogButton yesNoCancelDialog(const QString& str)
{
    QMessageBox msg;

    QPushButton *yes    = msg.addButton(QMessageBox::Yes);
    QPushButton *no     = msg.addButton(QMessageBox::No);
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
