#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include <QImage>
#include <QPixmap>
#include <QHash>
#include <QSettings>
#include <QTableWidget>
#include <QLabel>
#include <QCloseEvent>

#include "types.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"
#include "spritestate2.h"
#include "dialogs.h"


namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    /// Opens an sprite file
    /// @returns true if success, false otherwise
    bool openFile(const QString& filename);

protected:
    virtual void closeEvent(QCloseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);

private:

    Ui::MainWindow *ui;

    /// current file open
    QString  _filename;

    /// current export file open
    QString _exportFileName;

    /// app settings
    QSettings settings;

    /// sets current file and updates the main window title.
    /// The filename will be stored in the recent files section in the config file
    void setCurrentFile(const QString& filename);

    /// sets current export file
    void setCurrentExportFile(const QString& exportFileName);

    /// current sprite state
    SpriteState2 _sprState;

    /// Labels to show information in the status bar
    QLabel* statusBarMousePos;
    QLabel* statusBarRectSize;

    /// initialize recent files menu
    void initRecentFilesMenu();

    /// initialize signals
    void initSignals();

    /// initialize tables
    void initTables();

    /// connect or disconnect cellChanged() signals
    void cellChangedSignals(bool connected);

    /// Add new input image
    void addImage(const InputImage& image);

    /// Add new input image but just updates ui (i.e sprite state is unchanged)
    void addImage_ui(const InputImage& image);

    /// Add new frame
    void addFrame(const LvkFrame& frame);

    /// Add new frame but just updates ui (i.e sprite state is unchanged)
    void addFrame_ui(const LvkFrame& frame);

    /// Add new animation
    void addAnimation(const LvkAnimation& ani);

    /// Add new animation but just updates ui (i.e sprite state is unchanged)
    void addAnimation_ui(const LvkAnimation& ani);

    /// Add new animation frame to the animation @param aniId
    void addAframe(const LvkAframe& aframe, Id aniId);

    /// Add new aframe but just updates ui (i.e sprite state is unchanged)
    void addAframe_ui(const LvkAframe& aframe, Id aniId);

    /// shorthand to handle tables
    inline QString getItem(const QTableWidget* table, int row, int col)
    { return table->item(row, col)->text(); }

    /// shorthand to handle tables
    inline Id getIdItem(const QTableWidget* table, int row, int col)
    { return (Id)(getItem(table, row, col).toInt()); }

    /// shorthand to handle tables
    inline void setItem(const QTableWidget* table, int row, int col, const QString& value)
    { cellChangedSignals(false); table->item(row, col)->setText(value); cellChangedSignals(true); }

    /// shorthand to handle tables
    inline void setItem(const QTableWidget* table, int row, int col, int value)
    { setItem(table, row, col, QString::number(value)); }

    /// refresh whole user interface
    void refresh_ui();

    /// refresh images table ui
    void refresh_imgTable();

    /// refresh frames table ui
    void refresh_frameTable();

    /// refresh animations table ui
    void refresh_aniTable();

    /// refresh aframes table ui
    void refresh_aframeTable();

    /// opens an sprite file, returns the error in @param err if not a null pointer
    bool openFile_(const QString& filename, SpriteState::SpriteStateError* err = 0);

private slots:
    bool saveFile();
    bool saveAsFile();
    void openFileDialog();
    void closeFile();
    void exportFile();
    void exportAsFile();
    void exit();

    DialogButton saveChangesDialog();
    bool openFile_checkUnsaved(const QString& filename);
    void closeFile_checkUnsaved();

    void undo();
    void redo();

    void showFramesTab();
    void showAnimationsTab();

    void about();

    void addRecentFileMenu(const QString& filename);
    void storeRecentFile(const QString& filename);

    void addImageDialog();
    void showSelImage(int row);
    void showImage(Id imgId);
    void showSelImageWithFrameRect(int row, const QRect& rect);
    void removeSelImage();
    void removeImage(int row);

    bool addFrameDialog();
    void showSelFrame(int row);
    void showFrame(Id frameId);
    void scrollImgPreview(Id frameId);
    void removeSelFrame();
    void removeFrame(int row);

    void hideFramePreview();
    void showFramePreview();
    void hideShowFramePreview();

    void addAnimationDialog();
    void showAframes(int row);
    void removeSelAnimation();
    void removeAnimation(int row);

    void previewAnimation();
    void clearPreviewAnimation();
    void incAniSpeed(int ms = 10);
    void decAniSpeed(int ms = 10);
    void changePreviewScrSize(const QString& text);

    void addAframeDialog();
    void showSelAframe(int row);
    void showAframe(Id aframeId);
    void removeSelAframe();
    void removeAframe(int row);

    void updateImgTable(int row, int col);
    void updateFramesTable(int row, int col);
    void updateAframesTable(int row, int col);
    void updateAniTable(int row, int col);

    void showMousePosition(int x, int y);
    void showMouseRect(const QRect& rect);
};

#endif // MAINWINDOW_H
