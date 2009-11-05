#include <QtGui/QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("LavandaInk");
    QCoreApplication::setOrganizationDomain("lavandaink.com.ar");
    QCoreApplication::setApplicationName("Lvk Sprite Animation Tool");

    MainWindow w;
    w.show();
    return a.exec();
}
