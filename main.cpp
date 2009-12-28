#include <iostream>
#include <string>
#include <QtGui/QApplication>
#include <QFileInfo>
#include <QDir>

#include "mainwindow.h"
#include "settings.h"
#include "spritestate.h"

void parseCmdLine(int argc, char *argv[], MainWindow& w);
void showHelp(const std::string& binName);
void showVersion();

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("LavandaInk");
    QCoreApplication::setOrganizationDomain("lavandaink.com.ar");
    QCoreApplication::setApplicationName("Lvk Sprite Animation Tool");

    MainWindow w;

    parseCmdLine(argc, argv, w);

    w.show();
    return a.exec();
}

void parseCmdLine(int argc, char *argv[], MainWindow& w)
{
    std::string binName(QFileInfo(argv[0]).fileName().toStdString());

    // TODO use getopt

    if (argc == 2) {
        /* Valid options:
         *
         * LvkSpriteEditor "sprite_file"
         * LvkSpriteEditor --help
         * LvkSpriteEditor --version
         */

        std::string param(argv[1]);

        if (param == "--help") {
            showHelp(binName);
            exit(0);
        } else if (param == "--version") {
            showVersion();
            exit(0);
        } else if (param[0] == '-') {
            std::cerr << binName << ": Error: Unknown option " << param << std::endl;
            exit(-1);
        }

        if (!w.openFile(QString(param.c_str()))) {
            exit(-1);
        }
    } else if (argc == 3 || argc == 5) {
        /* Valid options:
         *
         * LvkSpriteEditor --export "sprite_file"
         * LvkSpriteEditor --export "sprite_file" --output-dir "dir"
         */

        std::string param1 = argv[1];
        std::string param2 = argv[2];

        if (param1 != "--export") {
            showHelp(binName);
            exit(-1);
        }

        QString inputDir  = QFileInfo(param2.c_str()).absolutePath();
        QString inputFile = QFileInfo(param2.c_str()).fileName();
        QString outputDir;

        if (argc == 5) {
            if (std::string(argv[3]) == "--output-dir") {
                outputDir = argv[4];
                if (!QDir(outputDir).exists()) {
                    std::cerr << binName << ": Error: Output directory '" << argv[4]
                              << "' does not exist." << std::endl;
                    exit(-1);
                }
            } else {
                showHelp(binName);
                exit(-1);
            }
        }

        SpriteState sprState;
        SpriteStateError err;

        QDir::setCurrent(inputDir);

        if (!sprState.load(inputFile, &err)) {
            std::cerr << binName << ": Error: Cannot open '" << param2 << "' "
                      << SpriteState::errorMessage(err).toStdString() << std::endl;
            exit(-1);
        }
        if (!sprState.exportSprite(inputFile, outputDir, &err)) {
            std::cerr << binName << ": Error: Cannot export '" << param2 << "' "
                      << SpriteState::errorMessage(err).toStdString() << std::endl;
            exit(-1);
        } else {
            std::cerr << binName << ": Export '" << param2 << "' succesful!" << std::endl;
            exit(0);
        }
    } else if (argc > 3) {
        std::cerr << binName << ": Error: Too many arguments" << std::endl;
        showHelp(binName);
        exit(-1);
    }
}

void showHelp(const  std::string& binName)
{
    std::cout << "Usage: " << binName << " [sprite-file]" << std::endl;
    std::cout << "       " << binName << " --export sprite-file [--output-dir directory]" << std::endl;
    std::cout << "       " << binName << " --version" << std::endl;
    std::cout << "       " << binName << " --help" << std::endl;
}

void showVersion()
{
    std::cout << APP_NAME << " " << APP_VERSION << std::endl;
}
