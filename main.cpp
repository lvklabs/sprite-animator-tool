#include <iostream>
#include <QtGui/QApplication>
#include <string>

#include "mainwindow.h"
#include "settings.h"

void showHelp(const std::string& appName);
void showVersion();

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("LavandaInk");
    QCoreApplication::setOrganizationDomain("lavandaink.com.ar");
    QCoreApplication::setApplicationName("Lvk Sprite Animation Tool");

    std::string appName(argv[0]);

    MainWindow w;

    if (argc == 2) {
        std::string param(argv[1]);

        if (param == "--help") {
            showHelp(appName);
            return 0;
        } else if (param == "--version") {
            showVersion();
            return 0;
        }
        
        if (!w.openFile(QString(param.c_str()))) {
            std::cerr << appName << ": Error: Cannot open " << param << std::endl;
            showHelp(appName);
            return -1;
        }
    } else if (argc > 2) {
        std::cerr << appName << ": Error: Too many arguments" << std::endl;
        showHelp(appName);
        return -1;
    }

    w.show();
    return a.exec();
}


void showHelp(const  std::string& appName)
{
    std::cout << "Usage: " << appName << " [sprite_file]"        << std::endl;
    std::cout << "       " << appName << " [--help | --version]" << std::endl;
}

void showVersion()
{
    std::cout << APP_NAME << " " << APP_VERSION << std::endl;
}
