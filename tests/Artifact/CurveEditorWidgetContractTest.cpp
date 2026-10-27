#include <gtest/gtest.h>

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QString>

#include <vector>

import Widget.CurveEditor;

namespace {

class CurveEditorWidgetEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "CurveEditorWidgetContractTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QApplication* application_ = nullptr;
};

testing::Environment* const curveEditorWidgetEnvironment =
    testing::AddGlobalTestEnvironment(new CurveEditorWidgetEnvironment());

void sendClick(ArtifactCore::ArtifactCurveEditorWidget& widget,
               const QPointF& position,
               Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QMouseEvent press(QEvent::MouseButtonPress, position, position, position,
                      Qt::LeftButton, Qt::LeftButton, modifiers);
    QApplication::sendEvent(&widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, position, position, position,
                        Qt::LeftButton, Qt::NoButton, modifiers);
    QApplication::sendEvent(&widget, &release);
}

void sendDrag(ArtifactCore::ArtifactCurveEditorWidget& widget,
              const QPointF& start,
              const QPointF& finish)
{
    QMouseEvent press(QEvent::MouseButtonPress, start, start, start,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);
    QMouseEvent move(QEvent::MouseMove, finish, finish, finish,
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, finish, finish, finish,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release);
}

void sendDelete(ArtifactCore::ArtifactCurveEditorWidget& widget)
{
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
}

ArtifactCore::CurveTrack makeTrack()
{
    ArtifactCore::CurveTrack track;
    track.name = QStringLiteral("Opacity");
    track.keys = {{10, 0.0f}, {90, 100.0f}};
    return track;
}

} // namespace

TEST(CurveEditorWidgetContractTest, CtrlClickInsertsAndDeleteRemovesKey)
{
    ArtifactCore::ArtifactCurveEditorWidget widget;
    widget.resize(640, 400);
    widget.setViewRange(0.0f, 100.0f, 0.0f, 100.0f);
    widget.setTracks({makeTrack()});
    widget.show();
    QApplication::processEvents();

    sendClick(widget, widget.visualPoint(0, 50.0f, 50.0f), Qt::ControlModifier);

    ASSERT_EQ(widget.tracks().size(), 1u);
    ASSERT_EQ(widget.tracks()[0].keys.size(), 3u);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 50);
    EXPECT_NEAR(widget.tracks()[0].keys[1].value, 50.0f, 0.1f);

    sendClick(widget, widget.visualPoint(0, 50.0f, 50.0f));
    sendDelete(widget);

    ASSERT_EQ(widget.tracks()[0].keys.size(), 2u);
    EXPECT_EQ(widget.tracks()[0].keys[0].frame, 10);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 90);
}

TEST(CurveEditorWidgetContractTest, ReplacingTracksClearsOldKeySelection)
{
    ArtifactCore::ArtifactCurveEditorWidget widget;
    widget.resize(640, 400);
    widget.setViewRange(0.0f, 100.0f, 0.0f, 100.0f);
    widget.setTracks({makeTrack()});
    widget.show();
    QApplication::processEvents();

    sendClick(widget, widget.visualPoint(0, 10.0f, 0.0f));
    ASSERT_TRUE(widget.visualKeySelected(0, 0));

    widget.setTracks({makeTrack()});

    EXPECT_FALSE(widget.visualKeySelected(0, 0));
    EXPECT_EQ(widget.visualSelectedTrack(), -1);
}

TEST(CurveEditorWidgetContractTest, DragMovesAllSelectedKeysByOneDelta)
{
    ArtifactCore::ArtifactCurveEditorWidget widget;
    widget.resize(640, 400);
    widget.setViewRange(0.0f, 100.0f, 0.0f, 100.0f);
    auto track = makeTrack();
    track.keys = {{10, 10.0f}, {50, 50.0f}, {90, 90.0f}};
    widget.setTracks({track});
    widget.show();
    QApplication::processEvents();

    sendClick(widget, widget.visualPoint(0, 10.0f, 10.0f));
    sendClick(widget, widget.visualPoint(0, 50.0f, 50.0f), Qt::ShiftModifier);
    ASSERT_TRUE(widget.visualKeySelected(0, 0));
    ASSERT_TRUE(widget.visualKeySelected(0, 1));

    sendDrag(widget, widget.visualPoint(0, 10.0f, 10.0f),
             widget.visualPoint(0, 20.0f, 20.0f));

    ASSERT_EQ(widget.tracks()[0].keys.size(), 3u);
    EXPECT_EQ(widget.tracks()[0].keys[0].frame, 20);
    EXPECT_NEAR(widget.tracks()[0].keys[0].value, 20.0f, 0.1f);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 60);
    EXPECT_NEAR(widget.tracks()[0].keys[1].value, 60.0f, 0.1f);
    EXPECT_EQ(widget.tracks()[0].keys[2].frame, 90);
    EXPECT_NEAR(widget.tracks()[0].keys[2].value, 90.0f, 0.1f);
    EXPECT_TRUE(widget.visualKeySelected(0, 0));
    EXPECT_TRUE(widget.visualKeySelected(0, 1));
}
