// Unit tests for the SpriteState CRUD primitives:
//   - addImage / addFrame / addAnimation / addAframe auto-id assignment
//   - removeFrame cascading to the pixmap cache
//   - isFrameUnused correctness across animations
//
// SpriteState holds QPixmap members, so this test needs a GUI application
// (QPixmap requires a QGuiApplication). We use QTEST_MAIN to get one.

#include <QtTest/QtTest>
#include <QApplication>

#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"
#include "types.h"

class TestSpriteStateCrud : public QObject
{
    Q_OBJECT

private slots:
    void testAddImageAutoIdSequential();
    void testAddImageHonoursExplicitIdAndBumpsCounter();
    void testAddFrameAutoIdSequential();
    void testAddAnimationAutoIdSequential();
    void testAddAframeAutoIdSequential();
    void testRemoveFrameClearsPixmapCacheEntry();
    void testRemoveAnimationDropsFromCollection();
    void testIsFrameUnusedTrueWhenNoAnimationReferencesIt();
    void testIsFrameUnusedFalseWhenAframeReferencesIt();
    void testClearResetsCountersAndCollections();
};

void TestSpriteStateCrud::testAddImageAutoIdSequential()
{
    SpriteState s;
    InputImage a, b, c;
    s.addImage(a);
    s.addImage(b);
    s.addImage(c);
    QCOMPARE(a.id, 0);
    QCOMPARE(b.id, 1);
    QCOMPARE(c.id, 2);
    QCOMPARE(s.images().size(), 3);
}

void TestSpriteStateCrud::testAddImageHonoursExplicitIdAndBumpsCounter()
{
    SpriteState s;
    InputImage explicit_(42, "/some/path.png", 1.0);
    s.addImage(explicit_);
    QCOMPARE(explicit_.id, 42);

    // next auto-assigned id should jump past the explicit one
    InputImage next;
    s.addImage(next);
    QCOMPARE(next.id, 43);
}

void TestSpriteStateCrud::testAddFrameAutoIdSequential()
{
    SpriteState s;
    LvkFrame f0, f1, f2;
    s.addFrame(f0);
    s.addFrame(f1);
    s.addFrame(f2);
    QCOMPARE(f0.id, 0);
    QCOMPARE(f1.id, 1);
    QCOMPARE(f2.id, 2);
    QCOMPARE(s.frames().size(), 3);
}

void TestSpriteStateCrud::testAddAnimationAutoIdSequential()
{
    SpriteState s;
    LvkAnimation a, b;
    s.addAnimation(a);
    s.addAnimation(b);
    QCOMPARE(a.id, 0);
    QCOMPARE(b.id, 1);
    QCOMPARE(s.animations().size(), 2);
}

void TestSpriteStateCrud::testAddAframeAutoIdSequential()
{
    SpriteState s;
    LvkAnimation ani;
    s.addAnimation(ani);
    const Id aniId = ani.id;

    LvkAframe af0, af1, af2;
    // delay > 0 so the underlying QList::insert(index, val) lands at
    // increasing positions (id 0,1,2).
    af0.delay = 100;
    af1.delay = 100;
    af2.delay = 100;

    s.addAframe(af0, aniId);
    s.addAframe(af1, aniId);
    s.addAframe(af2, aniId);
    QCOMPARE(af0.id, 0);
    QCOMPARE(af1.id, 1);
    QCOMPARE(af2.id, 2);
    // NOTE: avoid SpriteState::aframes(Id) -- in Qt6 it returns a
    // reference into a temporary QMap entry copy (see UPGRADE_NOTES.md
    // entry "Agent 1, issue 1"). Read via animations() instead, which
    // returns a const reference to the map itself.
    QCOMPARE(s.animations().value(aniId)._aframes.size(), 3);
}

void TestSpriteStateCrud::testRemoveFrameClearsPixmapCacheEntry()
{
    SpriteState s;
    LvkFrame f;
    s.addFrame(f);
    QVERIFY(s.frames().contains(f.id));
    // even with a null source image, addFrame inserts an entry into the
    // pixmap cache because reloadFramePixmap() unconditionally inserts.
    QVERIFY(s.fpixmaps().contains(f.id));

    s.removeFrame(f.id);
    QVERIFY(!s.frames().contains(f.id));
    QVERIFY(!s.fpixmaps().contains(f.id));
}

void TestSpriteStateCrud::testRemoveAnimationDropsFromCollection()
{
    SpriteState s;
    LvkAnimation a;
    s.addAnimation(a);
    QVERIFY(s.animations().contains(a.id));
    s.removeAnimation(a.id);
    QVERIFY(!s.animations().contains(a.id));
}

void TestSpriteStateCrud::testIsFrameUnusedTrueWhenNoAnimationReferencesIt()
{
    SpriteState s;
    LvkFrame f;
    s.addFrame(f);
    QVERIFY(s.isFrameUnused(f.id));
}

void TestSpriteStateCrud::testIsFrameUnusedFalseWhenAframeReferencesIt()
{
    SpriteState s;
    LvkFrame f;
    s.addFrame(f);

    LvkAnimation ani;
    s.addAnimation(ani);

    LvkAframe af;
    af.frameId = f.id;
    af.delay = 200;
    s.addAframe(af, ani.id);

    QVERIFY(!s.isFrameUnused(f.id));
}

void TestSpriteStateCrud::testClearResetsCountersAndCollections()
{
    SpriteState s;
    InputImage img;
    LvkFrame f;
    LvkAnimation a;
    s.addImage(img);
    s.addFrame(f);
    s.addAnimation(a);
    s.clear();

    QCOMPARE(s.images().size(), 0);
    QCOMPARE(s.frames().size(), 0);
    QCOMPARE(s.animations().size(), 0);
    QCOMPARE(s.fpixmaps().size(), 0);

    // counter is reset: next added entity should get id 0 again.
    InputImage img2;
    s.addImage(img2);
    QCOMPARE(img2.id, 0);
}

QTEST_MAIN(TestSpriteStateCrud)
#include "tst_spritestate_crud.moc"
