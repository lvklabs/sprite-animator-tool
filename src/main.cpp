// SPDX-License-Identifier: GPL-3.0-or-later
//
// Entry point for the LVK Sprite Editor.
//
// History note (Agent 10, 2026-05-16):
//   The legacy entry point parsed argv by hand (with a literal
//   `TODO use getopt` comment), and crucially constructed `MainWindow w;`
//   *before* parsing CLI flags. The MainWindow constructor pops a modal
//   About dialog (`about()` -> `QMessageBox::exec()`), so commands like
//   `--version` and `--help` would hang forever waiting for the user to
//   dismiss the modal -- they never reached the CLI parser at all.
//   See UPGRADE_NOTES.md item #9.
//
// The new layout below uses QCommandLineParser and parses the command
// line *first*. `--version` / `--help` exit before the GUI is touched.
// `--export` runs a headless export and exits. Only the interactive
// path constructs `MainWindow`.
//
//   QApplication app(argc, argv);          // Qt argc/argv plumbing only
//   parser.process(app);                   //   --version/--help exit here
//   if (cli.exportMode) return runExport();
//   MainWindow w;                          // GUI path
//   w.show();
//   return app.exec();

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <iostream>

#include "mainwindow.h"
#include "settings.h"
#include "spritestate.h"
#include "theme.h"

namespace {

struct CliOptions {
    bool exportMode = false;
    QString spriteFile;         // positional
    QString outputDir;          // -o
    QString postpScript;        // -p
    QString format = "cocos2d"; // -f (cocos2d|json|all)
};

// Run a headless export. Returns process exit code.
int runHeadlessExport(const CliOptions &cli, const QString &binName) {
    if (cli.spriteFile.isEmpty()) {
        std::cerr << binName.toStdString() << ": Error: --export requires a sprite-file argument\n";
        return -1;
    }

    QFileInfo info(cli.spriteFile);
    if (!info.exists()) {
        std::cerr << binName.toStdString() << ": Error: sprite-file '"
                  << cli.spriteFile.toStdString() << "' does not exist\n";
        return -1;
    }

    QString outputDir = cli.outputDir;
    if (outputDir.isEmpty()) {
        outputDir = info.absolutePath();
    } else if (!QDir(outputDir).exists()) {
        std::cerr << binName.toStdString() << ": Error: output directory '"
                  << outputDir.toStdString() << "' does not exist\n";
        return -1;
    } else {
        // Anchor to the invocation CWD now: QDir::setCurrent() below changes
        // the CWD to the sprite's directory, which would silently re-resolve
        // a relative -o against the wrong base (or fail the canonical-path
        // safety check in exportSprite()).
        outputDir = QFileInfo(outputDir).absoluteFilePath();
    }

    QString postpScript = cli.postpScript;
    if (!postpScript.isEmpty()) {
        if (!QFileInfo(postpScript).exists()) {
            std::cerr << binName.toStdString() << ": Error: postprocessing script '"
                      << postpScript.toStdString() << "' does not exist\n";
            return -1;
        }
        // Same CWD-change hazard as -o above.
        postpScript = QFileInfo(postpScript).absoluteFilePath();
    }

    // Set CWD to the input file's directory so relative image paths inside
    // the .lvks resolve correctly, matching the legacy behavior.
    QDir::setCurrent(info.absolutePath());
    const QString inputFile = info.fileName();

    SpriteState sprState;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    int rejectedCount = 0;

    // Progress to stderr -- the legacy Qt4 binary printed all diagnostics
    // to stderr, leaving stdout free for downstream tooling (a script can
    // redirect stdout to /dev/null and still see this line).
    std::cerr << "Loading " << inputFile.toStdString() << "..." << std::endl;
    if (!sprState.load(inputFile, &err, &rejectedCount)) {
        std::cerr << binName.toStdString() << ": Error: Cannot open '"
                  << cli.spriteFile.toStdString() << "' "
                  << SpriteState::errorMessage(err).toStdString() << "\n";
        return -1;
    }

    const SpriteState::ExportFormat format = SpriteState::parseFormat(cli.format);

    if (!sprState.exportSprite(inputFile, outputDir, postpScript, format, &err)) {
        std::cerr << binName.toStdString() << ": Error: Cannot export '"
                  << cli.spriteFile.toStdString() << "' "
                  << SpriteState::errorMessage(err).toStdString() << "\n";
        return -1;
    }

    // Team D2 (D2.3): If load() rejected any records (D2.1's format
    // whitelist enforcement now applies at load time too), surface the
    // count to stderr and return exit code 2 -- "export ran, but with
    // silent data loss." Without this, a CI pipeline running --export
    // over a tampered .lvks would observe exit 0 + missing artifacts and
    // never notice. Exit code 2 is conventional Unix shorthand for
    // "partial success" / "warnings emitted" (cf. grep(1), diff(1)).
    if (rejectedCount > 0) {
        std::cerr << binName.toStdString() << ": Warning: " << rejectedCount
                  << " image record(s) were rejected during load of '"
                  << cli.spriteFile.toStdString()
                  << "' (format whitelist). The exported artifacts are missing data.\n";
        std::cerr << binName.toStdString() << ": Export '" << cli.spriteFile.toStdString()
                  << "' completed with warnings.\n";
        return 2;
    }

    std::cerr << binName.toStdString() << ": Export '" << cli.spriteFile.toStdString()
              << "' successful!\n";
    return 0;
}

// Parse the command line. Calls QCommandLineParser::process() which will
// handle --version/--help internally (and exit). On a soft parse error
// (e.g. --format=garbage) returns false and the caller should bail.
bool parseCommandLine(QCoreApplication &app, CliOptions &cli, QString &errorMessage) {
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("LVK Sprite Animation Tool -- WYSIWYG 2D sprite "
                       "animator for Cocos2d. With no flags, opens the GUI; "
                       "with --export, runs a headless export."));
    parser.addPositionalArgument(
        QStringLiteral("sprite-file"),
        QStringLiteral("Optional .lvks file to open (or to export when --export "
                       "is set)."),
        QStringLiteral("[sprite-file]"));

    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption exportOpt(QStringList() << QStringLiteral("e") << QStringLiteral("export"),
                                 QStringLiteral("Headless mode: export <sprite-file> instead of "
                                                "launching the GUI."));
    parser.addOption(exportOpt);

    QCommandLineOption outputDirOpt(
        QStringList() << QStringLiteral("o") << QStringLiteral("output-dir"),
        QStringLiteral("Directory to write export artifacts into. Defaults "
                       "to the sprite-file's directory."),
        QStringLiteral("dir"));
    parser.addOption(outputDirOpt);

    QCommandLineOption postpOpt(
        QStringList() << QStringLiteral("p") << QStringLiteral("postprocessing-script"),
        QStringLiteral("Optional executable run on each exported frame image."),
        QStringLiteral("script"));
    parser.addOption(postpOpt);

    QCommandLineOption formatOpt(QStringList() << QStringLiteral("f") << QStringLiteral("format"),
                                 QStringLiteral("Export format: cocos2d (default), json, or all."),
                                 QStringLiteral("format"), QStringLiteral("cocos2d"));
    parser.addOption(formatOpt);

    // process() handles --help / --version (prints and exits) and reports
    // unknown options.
    parser.process(app);

    cli.exportMode = parser.isSet(exportOpt);
    cli.outputDir = parser.value(outputDirOpt);
    cli.postpScript = parser.value(postpOpt);
    cli.format = parser.value(formatOpt);

    const QStringList positional = parser.positionalArguments();
    if (positional.size() > 1) {
        errorMessage = QStringLiteral("At most one positional [sprite-file] "
                                      "argument is accepted, got %1.")
                           .arg(positional.size());
        return false;
    }
    if (!positional.isEmpty()) {
        cli.spriteFile = positional.first();
    }

    if (cli.exportMode) {
        if (cli.spriteFile.isEmpty()) {
            errorMessage = QStringLiteral("--export requires a [sprite-file] "
                                          "positional argument.");
            return false;
        }
        if (cli.outputDir.isEmpty()) {
            cli.outputDir = QFileInfo(cli.spriteFile).absolutePath();
        }
    }

    const QString fmt = cli.format.toLower();
    if (fmt != QStringLiteral("cocos2d") && fmt != QStringLiteral("json") &&
        fmt != QStringLiteral("all")) {
        errorMessage = QStringLiteral("--format must be one of: cocos2d, json, all "
                                      "(got '%1').")
                           .arg(cli.format);
        return false;
    }
    cli.format = fmt;
    return true;
}

} // namespace

int main(int argc, char *argv[]) {
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
    // Headless invocations (--export/--version/--help) must not die with
    // SIGABRT on a display-less machine (CI runner, ssh session): the xcb
    // platform plugin aborts inside the QApplication constructor, before
    // QCommandLineParser ever runs. Pre-scan argv for those flags and fall
    // back to the offscreen platform when no display is reachable. An
    // explicit QT_QPA_PLATFORM from the user always wins.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") &&
        qEnvironmentVariableIsEmpty("DISPLAY") &&
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        for (int i = 1; i < argc; ++i) {
            const QByteArray arg(argv[i]);
            if (arg == "-e" || arg == "--export" || arg == "-v" || arg == "--version" ||
                arg == "-h" || arg == "--help" || arg == "--help-all") {
                qputenv("QT_QPA_PLATFORM", "offscreen");
                break;
            }
        }
    }
#endif

    QApplication app(argc, argv);

    // Phase 6b (Item 27): Install a QTranslator BEFORE constructing
    // MainWindow so any tr() calls in the GUI pick up translated strings
    // for the system locale.  The .ts/.qm assets ship under :/i18n via
    // qt_add_lrelease + qt_add_resources (see CMakeLists.txt).  We keep
    // the translator object alive for the lifetime of the process (static
    // duration) -- installTranslator only borrows the pointer.
    //
    // load() returns false when no .qm matches the locale (e.g. when the
    // user runs under LANG=C or an unsupported locale); that is the
    // expected steady state and not an error.  installTranslator returns
    // false when the .qm is empty (skeleton .ts with no translations
    // yet) -- also a non-fatal "no translations available" state.  In
    // both cases tr() simply returns the source string verbatim.
    static QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("lvkspriteeditor"), QStringLiteral("_"),
                        QStringLiteral(":/i18n"))) {
        if (app.installTranslator(&translator)) {
            qDebug() << "main: installed translator for locale" << QLocale().name();
        } else {
            qDebug() << "main: translator loaded but empty for locale" << QLocale().name()
                     << "-- using source strings";
        }
    } else {
        qDebug() << "main: no translation found for locale" << QLocale().name()
                 << "-- falling back to source strings";
    }

    // SECURITY (Phase 4): Cap on per-image decoder allocation. Defeats
    // decompression-bomb PNG/TIFF inputs that would otherwise expand into
    // huge raw pixel buffers when QPixmap/QImage loads them. Qt's own
    // default is 256 MB (Qt6) and unlimited pre-6.0.
    // 256 MB -- large enough for 8K RGBA atlases, small enough to block
    // decompression bombs. (A 4096x4096 RGBA atlas is 64 MB exactly,
    // which would butt right up against a 64 MB cap once Qt's per-decode
    // overhead is counted; legit large-character atlases exceed that.)
    QImageReader::setAllocationLimit(256);

    QCoreApplication::setOrganizationName(QStringLiteral(LVK_NAME));
    QCoreApplication::setOrganizationDomain(QStringLiteral(LVK_DOMAIN));
    QCoreApplication::setApplicationName(QStringLiteral(APP_NAME));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    const QString binName = QFileInfo(QString::fromLocal8Bit(argv[0])).fileName();

    // Parse the CLI BEFORE constructing MainWindow. This fixes
    // UPGRADE_NOTES.md #9: previously `MainWindow w;` ran first, which
    // popped a modal About dialog from its constructor, so --version
    // and --help hung instead of printing and exiting.
    CliOptions cli;
    QString cliError;
    if (!parseCommandLine(app, cli, cliError)) {
        std::cerr << binName.toStdString() << ": Error: " << cliError.toStdString() << "\n";
        return -1;
    }

    if (cli.exportMode) {
        return runHeadlessExport(cli, binName);
    }

    // Interactive GUI path. Only now is it safe to set up the icon and
    // construct MainWindow.
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon-128x128")));

    // Agent 9: apply the persisted (or auto-detected) theme palette.
    // QSettings requires org/app names set above, so this must follow them.
    Theme::apply(&app, Theme::loadFromSettings());

    MainWindow w;
    if (!cli.spriteFile.isEmpty()) {
        if (!w.openFile(cli.spriteFile)) {
            return -1;
        }
    }
    w.show();
    return QApplication::exec();
}
