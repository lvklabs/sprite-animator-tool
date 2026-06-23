// FrameTabController.h
//
// Agent 8: owns the "Frames" tab. Split out of MainWindow alongside
// ImageTab / AnimationTab / TransitionTab / ExportControllers (Phase 3).
//
// Cross-controller hand-offs:
//   - createQuickAnimation in ImageTabController calls addFrameFromMouseRect
//     here, then addAframe in AnimationTabController. We expose the
//     necessary helpers as public methods (not slots) so the calling
//     controller can sequence them inside a startTransaction()/endTransaction()
//     undo group.

#ifndef LVK_CONTROLLERS_FRAME_TAB_CONTROLLER_H
#define LVK_CONTROLLERS_FRAME_TAB_CONTROLLER_H

#include <QObject>
#include <QRect>
#include <QString>

#include "types.h"

class MainWindow;
class SpriteState2;
namespace Ui {
class MainWindow;
}
class LvkFrame;

class FrameTabController : public QObject {
    Q_OBJECT
public:
    FrameTabController(MainWindow *mw, Ui::MainWindow *ui, SpriteState2 *state,
                       QObject *parent = nullptr);
    ~FrameTabController() override = default;

    void wireSignals();
    void refreshTable();

    // ---- helpers ----
    Id getFrameId(int row) const;
    Id getFrameImgId(int row) const;
    Id selectedFrameId() const;
    bool addFrameDialog(const QString &defaultName = QString(), bool promptName = true);
    Id addFrameFromMouseRect(Id imgId, const QString &name);
    Id addFrame(const LvkFrame &frame);
    void addFrame_ui(const LvkFrame &frame);
    void showSelFrame(int row);
    void showFrame(Id frameId);
    void removeFrame(int row);

    /// Used by AnimationTabController to pick a frame interactively.
    Id getFrameDialog(const QString &title);

public slots:
    void removeSelFrame();
    void removeAllUnusedFrames();
    void updateCurrentFrame(const QRect &rect);
    void updateCurrentFrame_ui(const QRect &rect);

    void hideFramePreview();
    void showFramePreview();
    void hideShowFramePreview();

    void updateFramesTable(int row, int col);

    // Blend pane (logically lives in the Frames tab).
    void setBlendPixmap();
    void blendNone();
    void blendExistentFrame();
    void blendFrameRect();
    void blendFrameId();

    void blendComboBoxSignals(bool connected);

private:
    MainWindow *m_mw = nullptr;
    Ui::MainWindow *m_ui = nullptr;
    SpriteState2 *m_state = nullptr;
    Id m_blendFrameId; // current blend selection
};

#endif // LVK_CONTROLLERS_FRAME_TAB_CONTROLLER_H
