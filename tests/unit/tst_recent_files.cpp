// Unit tests for the Open Recent menu (Team H5.4).
//
// MainWindow::storeRecentFile() implements MRU semantics on the
// QSettings-backed recent-files list:
//   * adding the SAME filename twice doesn't grow the list (dedup)
//   * the most-recently-stored filename ends up at index 0 (LIFO order)
//   * once MAX_RECENT_FILES (10) is reached, the oldest entry is evicted
//
// None of those properties were exercised by ctest before this file. The
// implementation is a fiddly hand-rolled bubble sort over QSettings keys
// ("RecentFiles/filename0..9"), so a regression that broke dedup or order
// would silently leak duplicate / mis-ordered entries into every user's
// settings file.
//
// storeRecentFile() is a private slot; we reach it via QMetaObject::
// invokeMethod which works for any Q_INVOKABLE / slot regardless of
// access. The menu is then verified through MainWindow's public uiPtr()
// accessor (added in Phase 3 for controller wiring) -- no friend-class
// hack required.

#include <QtTest/QtTest>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMenu>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

#include "mainwindow.h"
#include "settings.h"

#include "ui_mainwindow.h"

namespace {
// Unique QSettings scope per test executable so we don't clobber the
// running editor's recent-files list when the tests run on a developer
// machine. These are macros (not constexpr auto) so QStringLiteral can
// fold them into compile-time u16 storage; constexpr auto leaks through
// as a runtime char* and breaks QStringLiteral.
#define LVK_TEST_ORG "LvkLabsTest"
#define LVK_TEST_APP "LvkSpriteEditorTest_RecentFiles"
}

class TestRecentFiles : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();  // run once after all test slots
    void init();             // run before each test slot
    void cleanup();          // run after each test slot

    void storeOrderIsLIFO();
    void storeDeduplicates();
    void overflowEvictsOldest();
    void newMainWindowRestoresFromSettings();

private:
    /// Read the recent-files list out of QSettings, oldest-trailing.
    QStringList readRecentList() const;
};

void TestRecentFiles::initTestCase() {
    // Route QSettings to a per-test scratch dir so we do not leak
    // ~/.config/LvkLabsTest/LvkSpriteEditorTest_RecentFiles.conf into
    // the developer's home directory just because they ran `ctest`
    // locally once. Must be set BEFORE any QSettings instance is
    // constructed (including the s.setValue call below).
    QStandardPaths::setTestModeEnabled(true);

    QCoreApplication::setOrganizationName(QStringLiteral(LVK_TEST_ORG));
    QCoreApplication::setApplicationName(QStringLiteral(LVK_TEST_APP));
    QSettings s;
    // Suppress the splash About dialog -- otherwise MainWindow's
    // constructor calls exec() and blocks the test forever.
    s.setValue(QStringLiteral("ui/showAboutOnStartup"), false);
    s.sync();
}

void TestRecentFiles::cleanupTestCase() {
    // Belt-and-braces cleanup: clear() drops every key for this
    // org/app pair and unlinks the underlying QSettings backing file
    // entirely, so no residue (regardless of test-mode redirection)
    // survives the test executable.
    QSettings s;
    s.clear();
    s.sync();
    QFile::remove(s.fileName());
}

void TestRecentFiles::init() {
    // Clear any leftover recent-files entries from a prior run / slot so
    // each test starts from a known empty list.
    QSettings s;
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        s.remove(QString(QStringLiteral(KEY_RECENT_FILE)) + QString::number(i));
    }
    s.sync();
}

void TestRecentFiles::cleanup() {
    QSettings s;
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        s.remove(QString(QStringLiteral(KEY_RECENT_FILE)) + QString::number(i));
    }
    s.sync();
}

QStringList TestRecentFiles::readRecentList() const {
    QSettings s;
    QStringList out;
    for (int i = 0; i < MAX_RECENT_FILES; ++i) {
        const QString key = QString(QStringLiteral(KEY_RECENT_FILE)) + QString::number(i);
        const QString v = s.value(key).toString();
        if (!v.isEmpty()) {
            out << v;
        }
    }
    return out;
}

void TestRecentFiles::storeOrderIsLIFO() {
    MainWindow mw;
    // Store three distinct files; the most recent must end up at index 0.
    const QStringList stored = {
        QStringLiteral("/tmp/a.lvks"),
        QStringLiteral("/tmp/b.lvks"),
        QStringLiteral("/tmp/c.lvks"),
    };
    for (const QString &f : stored) {
        QVERIFY(QMetaObject::invokeMethod(&mw, "storeRecentFile",
                                          Qt::DirectConnection, Q_ARG(QString, f)));
    }

    const QStringList list = readRecentList();
    QCOMPARE(list.size(), stored.size());
    QCOMPARE(list.at(0), QStringLiteral("/tmp/c.lvks"));  // most recent
    QCOMPARE(list.at(1), QStringLiteral("/tmp/b.lvks"));
    QCOMPARE(list.at(2), QStringLiteral("/tmp/a.lvks"));  // oldest
}

void TestRecentFiles::storeDeduplicates() {
    MainWindow mw;
    const QString a = QStringLiteral("/tmp/a.lvks");
    const QString b = QStringLiteral("/tmp/b.lvks");
    QVERIFY(QMetaObject::invokeMethod(&mw, "storeRecentFile",
                                      Qt::DirectConnection, Q_ARG(QString, a)));
    QVERIFY(QMetaObject::invokeMethod(&mw, "storeRecentFile",
                                      Qt::DirectConnection, Q_ARG(QString, b)));
    // Re-store `a` -- should bubble back to index 0, NOT add a duplicate.
    QVERIFY(QMetaObject::invokeMethod(&mw, "storeRecentFile",
                                      Qt::DirectConnection, Q_ARG(QString, a)));

    const QStringList list = readRecentList();
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.at(0), a);
    QCOMPARE(list.at(1), b);
}

void TestRecentFiles::overflowEvictsOldest() {
    MainWindow mw;
    // Store MAX_RECENT_FILES + 1 distinct files. The very first one stored
    // (oldest) must be evicted; the list must remain at most MAX_RECENT_FILES
    // long and contain no duplicates.
    const int overshoot = MAX_RECENT_FILES + 1;
    QStringList stored;
    stored.reserve(overshoot);
    for (int i = 0; i < overshoot; ++i) {
        const QString f = QStringLiteral("/tmp/f") + QString::number(i)
                          + QStringLiteral(".lvks");
        stored << f;
        QVERIFY(QMetaObject::invokeMethod(&mw, "storeRecentFile",
                                          Qt::DirectConnection, Q_ARG(QString, f)));
    }

    const QStringList list = readRecentList();
    QCOMPARE(list.size(), MAX_RECENT_FILES);
    // Most recent at index 0 (LIFO).
    QCOMPARE(list.at(0), stored.last());
    // The very first file stored must have fallen off the end.
    QVERIFY(!list.contains(stored.first()));
    // Last-but-one stored still survives at index 1.
    QCOMPARE(list.at(1), stored.at(stored.size() - 2));

    // No duplicates anywhere in the list.
    const QSet<QString> uniq(list.begin(), list.end());
    QCOMPARE(static_cast<int>(uniq.size()), list.size());
}

void TestRecentFiles::newMainWindowRestoresFromSettings() {
    // Stage 1: populate the recent-files list via a transient MainWindow.
    const int n = 6;  // well under capacity
    QStringList stored;
    {
        MainWindow mw1;
        for (int i = 0; i < n; ++i) {
            const QString f = QStringLiteral("/tmp/g") + QString::number(i)
                              + QStringLiteral(".lvks");
            stored << f;
            QVERIFY(QMetaObject::invokeMethod(&mw1, "storeRecentFile",
                                              Qt::DirectConnection, Q_ARG(QString, f)));
        }
    }  // mw1 destructed -- only QSettings persists

    // Stage 2: a freshly constructed MainWindow MUST rebuild its recent-
    // files menu from QSettings via initRecentFilesMenu().
    MainWindow mw2;
    QMenu *menu = mw2.uiPtr()->actionOpenRecent;
    QVERIFY(menu != nullptr);

    // The menu's actions() includes the "<no recent files>" placeholder
    // (added in initRecentFilesMenu and then hidden when entries exist).
    // Skip placeholders / hidden actions and look at the real entries.
    QList<QAction *> realActions;
    for (QAction *a : menu->actions()) {
        if (a == mw2.uiPtr()->actionNoRecentFiles) {
            continue;
        }
        realActions << a;
    }
    QCOMPARE(realActions.size(), n);

    // LIFO: the most recently stored filename is the first menu entry.
    QCOMPARE(realActions.first()->text(), stored.last());
    // And the oldest is the last entry.
    QCOMPARE(realActions.last()->text(), stored.first());

    // No duplicates among the visible action texts.
    QStringList texts;
    for (QAction *a : realActions) {
        texts << a->text();
    }
    const QSet<QString> uniq(texts.begin(), texts.end());
    QCOMPARE(static_cast<int>(uniq.size()), texts.size());
}

QTEST_MAIN(TestRecentFiles)
#include "tst_recent_files.moc"
