// AnimationTabController.h
//
// Agent 8: owns the "Animations" tab, including the animation table,
// the aframes sub-list, and the play/speed controls. Pulled out of the
// MainWindow god class in Phase 3 of the upgrade plan.

#ifndef LVK_CONTROLLERS_ANIMATION_TAB_CONTROLLER_H
#define LVK_CONTROLLERS_ANIMATION_TAB_CONTROLLER_H

#include <QObject>
#include <QString>

#include "types.h"

class MainWindow;
class SpriteState2;
namespace Ui {
class MainWindow;
}
class LvkAnimation;
class LvkAframe;

class AnimationTabController : public QObject {
    Q_OBJECT
public:
    AnimationTabController(MainWindow *mw, Ui::MainWindow *ui, SpriteState2 *state,
                           QObject *parent = nullptr);
    ~AnimationTabController() override = default;

    void wireSignals();
    void refreshTables(); // refreshes ani + aframe tables

    Id getAnimationId(int row) const;
    Id getAframeId(int row) const;
    Id getAframeFrameId(int row) const;
    Id getAframeAniId(int row) const;
    Id selectedAniId() const;
    Id selectedAframeId() const;

    Id addAnimation(const LvkAnimation &ani);
    void addAnimation_ui(const LvkAnimation &ani);
    Id addAframe(const LvkAframe &aframe, Id aniId);
    void addAframe_ui(const LvkAframe &aframe, Id aniId);

    void showSelAframe(int row);
    void showAframe(Id frameId);

    void removeAnimation(int row);
    void removeAframe(int row);

public slots:
    void addAnimationDialog();
    void addAframeDialog();
    void removeSelAnimation();
    void removeSelAframe();

    void moveSelAframeUp();
    void moveSelAframeDown();
    void moveSelAframe(int offset);
    void invertAframesOrder();

    void showAframes(int row);
    void previewAnimation();
    void clearPreviewAnimation();
    void incAniSpeed(int ms = 10);
    void decAniSpeed(int ms = 10);
    void switchLandscapeMode();
    void changePreviewScrSize(const QString &text);

    void updateAframesTable(int row, int col);
    void updateAniTable(int row, int col);

private:
    MainWindow *m_mw = nullptr;
    Ui::MainWindow *m_ui = nullptr;
    SpriteState2 *m_state = nullptr;
};

#endif // LVK_CONTROLLERS_ANIMATION_TAB_CONTROLLER_H
