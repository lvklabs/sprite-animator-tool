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

    QApplication::setWindowIcon(QIcon(":/icons/app-icon-128x128"));

    QCoreApplication::setOrganizationName(LVK_NAME);
    QCoreApplication::setOrganizationDomain(LVK_DOMAIN);
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(APP_VERSION);

    MainWindow w;

    parseCmdLine(argc, argv, w);

    w.show();
    return a.exec();
}

void parseCmdLine(int argc, char *argv[], MainWindow& w)
{
    std::string binName(QFileInfo(argv[0]).fileName().toStdString());

    // TODO use getopt !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    if (argc == 1) {
        //nothing to do
    } else if (argc == 2) {
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
        } else if (param == "--export") {
            showHelp(binName);
            exit(-1);
        } else if (param[0] == '-') {
            std::cerr << binName << ": Error: Unknown option " << param << std::endl;
            exit(-1);
        }

        if (!w.openFile(QString(param.c_str()))) {
            exit(-1);
        }
    } else if (argc == 3 || argc == 5 || argc == 7) {
        /* Valid options:
         *
         * LvkSpriteEditor --export "sprite_file" [ --output-dir "dir" [ --postprocessing-script script_file ]  ]
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
        QString postpScript;

        if (argc >= 5) {
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

        if (argc >= 7) {
            if (std::string(argv[5]) == "--postprocessing-script") {
                postpScript = argv[6];
                if (!QFile(postpScript).exists()) {
                    std::cerr << binName << ": Error: postprocessing script '" << argv[6]
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

        std::cout << "Loading " << inputFile.toStdString() << "..." << std::endl;
        if (!sprState.load(inputFile, &err)) {
            std::cerr << binName << ": Error: Cannot open '" << param2 << "' "
                      << SpriteState::errorMessage(err).toStdString() << std::endl;
            exit(-1);
        }
        if (!sprState.exportSprite(inputFile, outputDir, postpScript, &err)) {
            std::cerr << binName << ": Error: Cannot export '" << param2 << "' "
                      << SpriteState::errorMessage(err).toStdString() << std::endl;
            exit(-1);
        } else {
            std::cerr << binName << ": Export '" << param2 << "' succesful!" << std::endl;
            exit(0);
        }
    } else {
        std::cerr << binName << ": Error: bad arguments" << std::endl;
        showHelp(binName);
        exit(-1);
    }
}

void showHelp(const  std::string& binName)
{
    std::cout << "Usage: " << binName << " [sprite-file]" << std::endl;
    std::cout << "       " << binName << " --export sprite-file [ --output-dir dir [ --postprocessing-script script_file ]  ]" << std::endl;
    std::cout << "       " << binName << " --version" << std::endl;
    std::cout << "       " << binName << " --help" << std::endl;
}

void showVersion()
{
    std::cout << APP_ABOUT << std::endl;
}
