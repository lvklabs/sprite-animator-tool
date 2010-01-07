#ifndef DIALOGS_H
#define DIALOGS_H

class QString;

typedef enum {
    YesButton     = 0,
    NoButton      = 1,
    CancelButton  = 2,
} DialogButton;

void infoDialog(const QString& str);

bool yesNoDialog(const QString& str);

DialogButton yesNoCancelDialog(const QString& str);

#endif // DIALOGS_H
