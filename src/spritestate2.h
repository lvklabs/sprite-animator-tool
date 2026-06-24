#ifndef SPRITESTATE2_H
#define SPRITESTATE2_H

#include "spritestate.h"
#include "statecircularbuffer.h"

class QObject;

/// this class extends SpriteState to provide undo, redo features.
/// it also provides the method hasUnsavedChanges()
class SpriteState2 : public SpriteState {
    Q_OBJECT

public:
    SpriteState2(QObject *parent = 0);

    bool undo();
    bool redo();

    bool canUndo() const;
    bool canRedo() const;

    bool hasUnsavedChanges() const;

    void startTransaction();
    void endTransaction();

    /// J3.3: monotonically increasing count of undo()/redo() warnings
    /// emitted since construction (e.g. transaction-marker evicted by the
    /// ring buffer). The MainWindow undo/redo handlers snapshot this
    /// before the call and re-check after; a delta means the history
    /// buffer was inconsistent and the user should see an errorDialog
    /// instead of the silent stderr-only qWarning the SpriteState2 layer
    /// emits. We expose a count (rather than a bool) so multiple back-
    /// to-back undos in the same Qt event loop still each get surfaced.
    int undoRedoWarningCount() const { return _undoRedoWarnings; }

    /* inherited methods */

    void updateImage(const InputImage &img);
    void updateFrame(const LvkFrame &frame);
    void updateAnimation(const LvkAnimation &ani);
    void updateAframe(const LvkAframe &aframe, Id aniId);

    void addImage(InputImage &img);
    void addFrame(LvkFrame &frame);
    void addAnimation(LvkAnimation &ani);
    void addAframe(LvkAframe &aframe, Id aniId);

    void removeImage(Id id);
    void removeFrame(Id id);
    void removeAnimation(Id id);
    void removeAframe(Id aframeId, Id aniId);

    void setCustomHeader(const QString &header);
    QString getCustomHeader();

    void clear();
    bool save(const QString &filename, SpriteStateError *err = 0);
    bool load(const QString &filename, SpriteStateError *err = 0,
              int *rejectedCount = nullptr);

private:
    StateCircularBuffer _stBuffer;
    bool headerHasChanged;
    // Depth-count transaction begin/end so re-entrant callers don't push
    // duplicate markers into the ring buffer. With duplicates, undo() walks
    // back to the inner st_transactionStart and leaves the outer transaction
    // half-undone. Only the outermost start/end pair writes to the buffer.
    int _transactionDepth = 0;

    // J3.3: bumped by undo()/redo() whenever the eviction guard fires
    // (the matching transaction-marker rolled off the ring buffer and
    // the walk had to short-circuit to avoid spinning the UI). The
    // GUI consults undoRedoWarningCount() to surface an errorDialog so
    // a corrupt-history undo doesn't silently scramble the on-screen
    // state. Counter is intentionally monotonic across the lifetime of
    // the instance (callers diff snapshots) rather than reset by anything
    // -- simplest correct behaviour for the polling-by-delta API.
    int _undoRedoWarnings = 0;

    bool undo(StateChange &st);
    bool redo(StateChange &st);
};

#endif // SPRITESTATE2_H
