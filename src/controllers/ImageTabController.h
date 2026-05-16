// ImageTabController.h
//
// Agent 8 (Phase 3 MainWindow refactor): owns the "Images" tab logic
// that used to live as ~600 lines of slots in MainWindow. The controller
// is a thin QObject; the production data lives in SpriteState2 (owned by
// MainWindow), the widgets live in Ui::MainWindow (also owned by
// MainWindow).
//
// Cross-cutting concerns we explicitly do NOT own:
//   - file open / save (MainWindow)
//   - undo / redo (MainWindow drives, SpriteState2 records)
//   - export pipeline (ExportController)
//
// MainWindow constructs us, hands us non-owning pointers to ui + state,
// and connects our signals back into its own slots where cross-tab side
// effects are required (e.g. removing an image triggers frame removals
// in FrameTabController).

#ifndef LVK_CONTROLLERS_IMAGE_TAB_CONTROLLER_H
#define LVK_CONTROLLERS_IMAGE_TAB_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QRect>

#include "types.h"

class MainWindow;
class SpriteState2;
namespace Ui { class MainWindow; }
class InputImage;

class ImageTabController : public QObject
{
    Q_OBJECT
public:
    ImageTabController(MainWindow* mw, Ui::MainWindow* ui, SpriteState2* state,
                       QObject* parent = nullptr);
    ~ImageTabController() override = default;

    /// Wire UI signals (buttons + table) into our slots. Called by
    /// MainWindow once after construction.
    void wireSignals();

    /// Repopulate the imgTableWidget from the SpriteState. Called by
    /// MainWindow::refresh_ui() after loads/undo/redo.
    void refreshTable();

    // ---- helpers used by other controllers / MainWindow ----
    Id   getImageId(int row) const;
    Id   selectedImgId() const;
    Id   addImage(const InputImage& image);
    void addImage_ui(const InputImage& image);
    void showSelImage(int row);
    void showImage(Id imgId, bool clearPixmapCache = false);
    void showSelImageWithFrameRect(int row, const QRect& rect);
    void reloadImage(Id imgId);
    void removeImage(int row);

    QString toRelativePath(const QString& filePath) const;

signals:
    void imageRemoved(Id imgId);
    void cellChangeRequest(bool connected);

public slots:
    void addImageDialog();
    void removeSelImage();
    void reloadSelImage();

    void updateImgTable(int row, int col);

    // "Quick mode" / multi-image operations
    void switchQuickMode();
    void checkAllImages();
    void invertCheckedImages();
    void scaleCheckedImages();
    void createQuickAnimation();

private:
    bool hasImagesChecked() const;

    MainWindow*     m_mw    = nullptr;
    Ui::MainWindow* m_ui    = nullptr;
    SpriteState2*   m_state = nullptr;
};

#endif // LVK_CONTROLLERS_IMAGE_TAB_CONTROLLER_H
