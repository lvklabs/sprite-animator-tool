// TransitionTabController.h
//
// Agent 8: tiny controller for the "Test Transitions" tab. Pulled out of
// MainWindow alongside the other tab controllers.

#ifndef LVK_CONTROLLERS_TRANSITION_TAB_CONTROLLER_H
#define LVK_CONTROLLERS_TRANSITION_TAB_CONTROLLER_H

#include <QObject>

#include "types.h"

class MainWindow;
class SpriteState2;
namespace Ui {
class MainWindow;
}

class TransitionTabController : public QObject {
    Q_OBJECT
public:
    TransitionTabController(MainWindow *mw, Ui::MainWindow *ui, SpriteState2 *state,
                            QObject *parent = nullptr);
    ~TransitionTabController() override = default;

    void wireSignals();

    Id getTransAniId(int row) const;

    void addTrans_ui(Id aniId);

public slots:
    void addTransDialog();
    void addTrans(Id aniId);
    void removeSelTrans();
    void removeAllTrans();
    void previewTransition();
    void clearPreviewTransition();

private:
    MainWindow *m_mw = nullptr;
    Ui::MainWindow *m_ui = nullptr;
    SpriteState2 *m_state = nullptr;
};

#endif // LVK_CONTROLLERS_TRANSITION_TAB_CONTROLLER_H
