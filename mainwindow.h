#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include <QImage>
#include <QPixmap>
#include <QHash>
#include <QSettings>

#include "types.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"
#include "spritestate.h"
#include "lvkframegraphicsgroup.h"


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

private:

    Ui::MainWindow *ui;

    /// current file open
    QString                 _filename;

    /// app settings
    QSettings settings;

    /// sets current file and updates windows title
    void setCurrentFile(const QString& filename);

    /// current sprite state
    SpriteState             _sprState;

    /// Counter. Next image id
    Id _imgId;

    /// Counter. Next frame id
    Id _frameId;

    /// Counter. Next animation id
    Id _aniId;

    /// Counter. Next animation frame id
    Id _aframeId;

    /// Current animation for fixing animation issue

    LvkFrameGraphicsGroup* currentAnimation;

    /// Add new input image
    void addImage(const InputImage& image);

    /// Add new frame
    void addFrame(const LvkFrame& frame);

    /// Add new animation
    void addAnimation(const LvkAnimation& ani);

    /// Add new animation frame to the animation @param aniId
    void addAframe(const LvkAframe& aframe, Id aniId);
    void addAframe_(const LvkAframe& aframe, Id aniId);

private slots:
    void saveFile();
    void saveAsFile();
    void openFileDialog();
    bool openFile(const QString& filename);
    void closeFile();
    void exit();

    void initRecentFilesMenu();
    void addRecentFileMenu(const QString& filename);
    void storeRecentFile(const QString& filename);

    void about();

    void addImageDialog();
    void showSelImage(int row);
    void imgZoomIn();
    void imgZoomOut();
    void removeSelImage();
    void removeImage(int row);

    bool addFrameFromImgRegion();
    void showSelFrame(int row);
    void removeSelFrame();
    void removeFrame(int row);

    void addAnimationDialog();
    void showSelAnimation(int row);
    void showSelAnimation_(int row);
    void previewAnimation();
    void removeSelAnimation();
    void removeAnimation(int row);

    void addAframeDialog();
    void showSelAframe(int row);
    void removeSelAframe();
    void removeAframe(int row);
};

#endif // MAINWINDOW_H
