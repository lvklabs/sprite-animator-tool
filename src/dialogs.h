#ifndef DIALOGS_H
#define DIALOGS_H

class QString;
class QWidget;

typedef enum {
    YesButton = 0,
    NoButton = 1,
    CancelButton = 2,
} DialogButton;

// Team H3: the modal helpers now take an optional parent widget so the
// resulting QMessageBox is centred over the owning window (MainWindow or
// the tab controller's parent) and inherits its style/palette. Each
// helper also sets a window title and a matching QMessageBox icon -- the
// previous "no title, no icon, no parent" variants surfaced as a tiny
// untitled bubble floating in the screen centre, which on some WMs was
// hard to even spot.
void infoDialog(const QString &str, QWidget *parent = nullptr);

bool yesNoDialog(const QString &str, QWidget *parent = nullptr);

DialogButton yesNoCancelDialog(const QString &str, QWidget *parent = nullptr);

// Team H3: dedicated "something went wrong" helper. Uses the Critical
// icon + an "Error" title so users can tell error alerts apart from the
// informational ones at a glance.
void errorDialog(const QString &str, QWidget *parent = nullptr);

#endif // DIALOGS_H
