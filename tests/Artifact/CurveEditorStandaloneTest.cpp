// Standalone contract test + capture harness for the timeline curve editor mode.
//
// Modelled on ArtifactFxStudioRenderContractTest: the real widget is exercised
// without the application, a composition, a GPU device or a window manager, and
// every scenario is rendered to PNG so a failure can be looked at.
//
// What it covers (the public surface the timeline curve editor mode drives):
//   - sampleSpeedGraph() -> CurveTrack (frame/percent mapping, clamping, padding)
//   - CE-14 speed math: curveSpeedAt() and solveCurveKeyValueForSpeed()
//   - view range / plot rect / data<->pixel mapping (+ normalized view)
//   - fitToContent() margins and empty fallback
//   - tangent operations (Auto / Flat / Linear / Break / Unify / Constant / Bezier)
//   - selection (click, Shift+click marquee, A, Escape, focus track visibility)
//   - key insert (Ctrl+Click), delete, multi-key drag, Ctrl value snap
//   - copy/paste across tracks, Shift+D clone, reorder-safe selection rebuild
//   - inline numeric editors (frame/value) and their derived-mode suppression
//   - zero-length handle press selecting the key instead of its tangent
//   - derived (Speed) graph mode: key moves allowed, structural/tangent edits
//     and numeric value entry rejected
//   - read-only mode, buffer curve toggle, normalized-view hit testing
//   - playhead scrub, pan, cursor-anchored wheel zoom (Shift/Alt axes)
//   - per-pixel render checks over 16 scenarios and capture output for review
//
// Usage:
//   ArtifactCurveEditorStandaloneTest                     # run contracts (offscreen)
//   ArtifactCurveEditorStandaloneTest --gtest_filter=...  # subset
//   ArtifactCurveEditorStandaloneTest --capture-dir D     # override PNG output
//   ArtifactCurveEditorStandaloneTest --ui                # interactive lab window
//   ArtifactCurveEditorStandaloneTest --list-scenarios
//
// Captures default to $ARTIFACT_CURVE_EDITOR_CAPTURE_DIR, else the temp dir.
//
// The curve editor paints no caller-supplied localized strings (track names and
// the "F " spin-box prefix are fixed / caller-owned), so the ja/en/zh/zh-TW
// widget-capture matrix does not apply to this surface; there is nothing for a
// translator to change in the captured pixels.

#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QDoubleSpinBox>
#include <QSize>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <functional>
#include <string>
#include <utility>
#include <vector>

import Widget.CurveEditor;
import Artifact.Event.Types;
import Event.Bus;
import Frame.Rate;
import Time.TimeRemap;

namespace {

namespace CE = ArtifactCore;

constexpr int kWidth = 900;
constexpr int kHeight = 480;

// Plot margins baked into ArtifactCurveEditorWidget::Impl.
constexpr int kMarginLeft = 50;
constexpr int kMarginTop = 20;
constexpr int kMarginRight = 20;
constexpr int kMarginBottom = 30;

const QColor kWidgetBackground(25, 25, 25);
const QColor kPlotBackground(30, 30, 30);
const QColor kPlayheadColor(255, 80, 80);

QString g_captureDirectory;
QString g_baselineDirectory;
bool g_updateBaselines = false;

QString baselineDirectory()
{
    if (!g_baselineDirectory.isEmpty()) return g_baselineDirectory;
    const QString fromEnvironment =
        qEnvironmentVariable("ARTIFACT_CURVE_EDITOR_BASELINE_DIR");
    if (!fromEnvironment.isEmpty()) return fromEnvironment;
#ifdef ARTIFACT_CURVE_EDITOR_BASELINE_DIR
    return QString::fromUtf8(ARTIFACT_CURVE_EDITOR_BASELINE_DIR);
#else
    return QStringLiteral("baselines/curve-editor");
#endif
}

QString captureDirectory()
{
    QString directory = g_captureDirectory;
    if (directory.isEmpty()) {
        directory = qEnvironmentVariable("ARTIFACT_CURVE_EDITOR_CAPTURE_DIR");
    }
    if (directory.isEmpty()) {
        directory = QDir::tempPath() + QStringLiteral("/artifact-curve-editor");
    }
    QDir().mkpath(directory);
    return directory;
}

QString sanitize(const QString& value)
{
    QString result = value;
    for (const QChar character : QStringLiteral(".:/\\ ()")) {
        result.replace(character, QChar('_'));
    }
    return result;
}

// ---------------------------------------------------------------------------
// Synthetic key data
// ---------------------------------------------------------------------------

CE::CurveKey key(const std::int64_t frame, const float value)
{
    CE::CurveKey result;
    result.frame = frame;
    result.value = value;
    result.smooth = true;
    return result;
}

CE::CurveTrack track(const QString& name, const QColor& color,
                     std::vector<CE::CurveKey> keys)
{
    CE::CurveTrack result;
    result.name = name;
    result.color = color;
    result.visible = true;
    result.keys = std::move(keys);
    return result;
}

// 0 -> 100 -> 50 over frames 0..30..60, the reference shape used by the
// tangent and interaction contracts.
std::vector<CE::CurveTrack> primaryTracks()
{
    return {
        track(QStringLiteral("Opacity"), QColor(255, 128, 64),
              {key(0, 0.0f), key(30, 100.0f), key(60, 50.0f)}),
        track(QStringLiteral("Rotation"), QColor(96, 196, 255),
              {key(0, 80.0f), key(20, 20.0f), key(40, 70.0f)}),
    };
}

// ---------------------------------------------------------------------------
// Event injection (the widget handles input in mouse*/key*/wheel* overrides,
// so QApplication::sendEvent reaches them without needing a shown window)
// ---------------------------------------------------------------------------

void sendMouse(QWidget& widget, const QEvent::Type type, const QPointF& position,
               const Qt::MouseButton button, const Qt::MouseButtons buttons,
               const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QMouseEvent event(type, position, position, button, buttons, modifiers);
    QApplication::sendEvent(&widget, &event);
}

void pressKey(QWidget& widget, const int keyCode,
              const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(QEvent::KeyPress, keyCode, modifiers);
    QApplication::sendEvent(&widget, &event);
}

void wheel(QWidget& widget, const QPointF& position, const int angleY,
           const Qt::KeyboardModifiers modifiers)
{
    QWheelEvent event(position, position, QPoint(0, 0), QPoint(0, angleY),
                      Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &event);
}

void doubleClick(QWidget& widget, const QPointF& position,
                 const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    // Qt delivers press/release before the double-click event; the press is
    // what selects a key, so reproduce the real sequence.
    sendMouse(widget, QEvent::MouseButtonPress, position, Qt::LeftButton,
              Qt::LeftButton, modifiers);
    sendMouse(widget, QEvent::MouseButtonRelease, position, Qt::LeftButton,
              Qt::NoButton, modifiers);
    sendMouse(widget, QEvent::MouseButtonDblClick, position, Qt::LeftButton,
              Qt::LeftButton, modifiers);
    sendMouse(widget, QEvent::MouseButtonRelease, position, Qt::LeftButton,
              Qt::NoButton, modifiers);
}

void drag(CE::ArtifactCurveEditorWidget& widget, const QPointF& start,
          const QPointF& end,
          const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton, modifiers);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton,
              modifiers);
    sendMouse(widget, QEvent::MouseButtonRelease, end, Qt::LeftButton,
              Qt::NoButton, modifiers);
}

// Ctrl+Click is the insert gesture, so a Ctrl-drag has to press without the
// modifier and only hold it during the move (this is also the real gesture:
// the snap is applied while dragging).
void dragWithMoveModifiers(CE::ArtifactCurveEditorWidget& widget,
                           const QPointF& start, const QPointF& end,
                           const Qt::KeyboardModifiers modifiers)
{
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton,
              modifiers);
    sendMouse(widget, QEvent::MouseButtonRelease, end, Qt::LeftButton,
              Qt::NoButton);
}

CE::CurveKey constantKey(const std::int64_t frame, const float value)
{
    CE::CurveKey result = key(frame, value);
    result.constant = true;
    result.smooth = false;
    return result;
}

// The timeline's undo/refresh path is driven entirely by these events, so the
// harness counts them instead of relying on the widget's signal declarations.
struct CurveEditorEvents {
    int started = 0;
    int finished = 0;
    int moved = 0;
    int selected = 0;
    int deleted = 0;
    int frameChanged = 0;
    qint64 lastFrame = 0;
    int lastSelectedTrack = -1;
    int lastSelectedKey = -1;
};

void watchCurveEditorEvents(
    CurveEditorEvents& events,
    std::vector<CE::EventBus::Subscription>& subscriptions)
{
    auto& bus = CE::globalEventBus();
    subscriptions.push_back(
        bus.subscribe<Artifact::CurveEditorInteractionStartedEvent>(
            [&events](const Artifact::CurveEditorInteractionStartedEvent&) {
                ++events.started;
            }));
    subscriptions.push_back(
        bus.subscribe<Artifact::CurveEditorInteractionFinishedEvent>(
            [&events](const Artifact::CurveEditorInteractionFinishedEvent&) {
                ++events.finished;
            }));
    subscriptions.push_back(bus.subscribe<Artifact::CurveEditorKeyMovedEvent>(
        [&events](const Artifact::CurveEditorKeyMovedEvent&) { ++events.moved; }));
    subscriptions.push_back(bus.subscribe<Artifact::CurveEditorKeySelectedEvent>(
        [&events](const Artifact::CurveEditorKeySelectedEvent& event) {
            ++events.selected;
            events.lastSelectedTrack = event.trackIndex;
            events.lastSelectedKey = event.keyIndex;
        }));
    subscriptions.push_back(bus.subscribe<Artifact::CurveEditorKeyDeletedEvent>(
        [&events](const Artifact::CurveEditorKeyDeletedEvent&) {
            ++events.deleted;
        }));
    subscriptions.push_back(
        bus.subscribe<Artifact::CurveEditorCurrentFrameChangedEvent>(
            [&events](const Artifact::CurveEditorCurrentFrameChangedEvent& event) {
                ++events.frameChanged;
                events.lastFrame = event.frame;
            }));
}

QSpinBox* frameEditorOf(CE::ArtifactCurveEditorWidget& widget)
{
    const auto editors = widget.findChildren<QSpinBox*>();
    for (QSpinBox* editor : editors) {
        if (editor->prefix() == QStringLiteral("F ")) return editor;
    }
    return nullptr;
}

QDoubleSpinBox* valueEditorOf(CE::ArtifactCurveEditorWidget& widget)
{
    const auto editors = widget.findChildren<QDoubleSpinBox*>();
    return editors.isEmpty() ? nullptr : editors.front();
}

QPointF rawPoint(const CE::ArtifactCurveEditorWidget& widget, const float frame,
                 const float value)
{
    return widget.visualPoint(-1, frame, value);
}

QPointF keyPoint(const CE::ArtifactCurveEditorWidget& widget, const int trackIndex,
                 const int keyIndex)
{
    const CE::CurveKey& selectedKey = widget.tracks()[trackIndex].keys[keyIndex];
    return widget.visualPoint(trackIndex, static_cast<float>(selectedKey.frame),
                              selectedKey.value);
}

void clickKey(CE::ArtifactCurveEditorWidget& widget, const int trackIndex,
              const int keyIndex)
{
    const QPointF position = keyPoint(widget, trackIndex, keyIndex);
    sendMouse(widget, QEvent::MouseButtonPress, position, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(widget, QEvent::MouseButtonRelease, position, Qt::LeftButton,
              Qt::NoButton);
}

bool keySelected(const CE::ArtifactCurveEditorWidget& widget, const int trackIndex,
                 const int keyIndex)
{
    return widget.visualKeySelected(trackIndex, keyIndex);
}

// ---------------------------------------------------------------------------
// Offscreen rendering / pixel helpers
// ---------------------------------------------------------------------------

QImage renderWidget(CE::ArtifactCurveEditorWidget& widget)
{
    widget.resize(kWidth, kHeight);
    widget.show();
    QCoreApplication::processEvents();

    QImage image(widget.size(), QImage::Format_RGBA8888);
    image.fill(QColor(0, 0, 0));
    widget.render(&image);
    return image;
}

QColor blendOver(const QColor& foreground, const int alpha, const QColor& background)
{
    const double a = static_cast<double>(alpha) / 255.0;
    const auto mix = [a](const int fg, const int bg) {
        return static_cast<int>(std::lround(fg * a + bg * (1.0 - a)));
    };
    return QColor(mix(foreground.red(), background.red()),
                  mix(foreground.green(), background.green()),
                  mix(foreground.blue(), background.blue()));
}

bool colorNear(const QColor& actual, const QColor& expected, const int tolerance)
{
    return std::abs(actual.red() - expected.red()) <= tolerance &&
           std::abs(actual.green() - expected.green()) <= tolerance &&
           std::abs(actual.blue() - expected.blue()) <= tolerance;
}

int countNear(const QImage& image, const QRect& rect, const QColor& target,
              const int tolerance)
{
    const QRect bounded = rect.intersected(image.rect());
    int count = 0;
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (colorNear(image.pixelColor(x, y), target, tolerance)) ++count;
        }
    }
    return count;
}

QRect plotRectPixels()
{
    return QRect(kMarginLeft, kMarginTop, kWidth - kMarginLeft - kMarginRight,
                 kHeight - kMarginTop - kMarginBottom);
}

QString writePng(const QImage& image, const QString& name)
{
    const QString path = captureDirectory() + QStringLiteral("/") + name;
    EXPECT_FALSE(image.isNull()) << name.toStdString();
    EXPECT_TRUE(image.save(path, "PNG")) << path.toStdString();
    return path;
}

int countColorInBox(const QImage& image, const QPointF& center, const int radius,
                    const QColor& target, const int tolerance)
{
    const QRect box(static_cast<int>(std::lround(center.x())) - radius,
                    static_cast<int>(std::lround(center.y())) - radius,
                    radius * 2 + 1, radius * 2 + 1);
    return countNear(image, box, target, tolerance);
}

// Average y of a track-coloured stroke in one image column. Used to compare
// where the painted curve sits before and after a shape edit.
double averageStrokeRow(const QImage& image, const int x, const QColor& target,
                        const int tolerance)
{
    long sum = 0;
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        if (colorNear(image.pixelColor(x, y), target, tolerance)) {
            sum += y;
            ++count;
        }
    }
    return count > 0 ? static_cast<double>(sum) / count : -1.0;
}

QColor selectedKeyFill()
{
    return blendOver(QColor(255, 236, 128), 235, kPlotBackground);
}

QColor keyFillFor(const QColor& trackColor)
{
    return trackColor.darker(150);
}

// Defined after the image-diff helpers below.
void storeOrCompareBaseline(const QString& name, const QImage& image);

// Every interaction-visual contract writes its before/after frames next to the
// scenario captures so a failure can be inspected, not just counted.
void writeCapturePair(const QString& name, const QImage& before,
                      const QImage& after)
{
    const QString beforeName = QStringLiteral("op_%1_before.png").arg(name);
    const QString afterName = QStringLiteral("op_%1_after.png").arg(name);
    writePng(before, beforeName);
    writePng(after, afterName);
    storeOrCompareBaseline(beforeName, before);
    storeOrCompareBaseline(afterName, after);
}

// ---------------------------------------------------------------------------
// Baseline image comparison
// ---------------------------------------------------------------------------

struct ImageDiff {
    double meanAbs = 0.0;            // mean absolute channel difference
    double differingFraction = 0.0;  // fraction of pixels with any channel > kChannelTolerance
    int maxAbs = 0;
    bool sizeMismatch = false;
};

constexpr int kChannelTolerance = 12;

ImageDiff compareImages(const QImage& expected, const QImage& actual)
{
    ImageDiff diff;
    if (expected.isNull() || actual.isNull() ||
        expected.size() != actual.size()) {
        diff.sizeMismatch = true;
        diff.differingFraction = 1.0;
        diff.maxAbs = 255;
        return diff;
    }
    quint64 sum = 0;
    quint64 differing = 0;
    for (int y = 0; y < expected.height(); ++y) {
        for (int x = 0; x < expected.width(); ++x) {
            const QColor a = expected.pixelColor(x, y);
            const QColor b = actual.pixelColor(x, y);
            const int dr = std::abs(a.red() - b.red());
            const int dg = std::abs(a.green() - b.green());
            const int db = std::abs(a.blue() - b.blue());
            sum += static_cast<quint64>(dr + dg + db);
            const int delta = std::max({dr, dg, db});
            diff.maxAbs = std::max(diff.maxAbs, delta);
            if (delta > kChannelTolerance) ++differing;
        }
    }
    const double channels =
        static_cast<double>(expected.width()) * expected.height() * 3.0;
    const double pixels =
        static_cast<double>(expected.width()) * expected.height();
    diff.meanAbs = channels > 0.0 ? static_cast<double>(sum) / channels : 0.0;
    diff.differingFraction = pixels > 0.0
        ? static_cast<double>(differing) / pixels : 0.0;
    return diff;
}

void writeBaselineDiff(const QString& baselinePath, const QImage& expected,
                       const QImage& actual)
{
    const QString base = QFileInfo(baselinePath).completeBaseName();
    writePng(expected, QStringLiteral("baseline_%1_expected.png").arg(base));
    writePng(actual, QStringLiteral("baseline_%1_actual.png").arg(base));
    if (expected.size() != actual.size()) return;

    QImage overlay(actual.size(), QImage::Format_RGBA8888);
    overlay.fill(QColor(0, 0, 0));
    for (int y = 0; y < actual.height(); ++y) {
        for (int x = 0; x < actual.width(); ++x) {
            const QColor a = expected.pixelColor(x, y);
            const QColor b = actual.pixelColor(x, y);
            const int delta = std::max({std::abs(a.red() - b.red()),
                                        std::abs(a.green() - b.green()),
                                        std::abs(a.blue() - b.blue())});
            if (delta > kChannelTolerance) {
                overlay.setPixelColor(x, y, QColor(255, 0, 255));
            } else {
                const int gray = (b.red() + b.green() + b.blue()) / 9;
                overlay.setPixelColor(x, y, QColor(gray, gray, gray));
            }
        }
    }
    writePng(overlay, QStringLiteral("baseline_%1_diff.png").arg(base));
}

// Store/compare one image against the baseline tree. Used for both the
// scenario images and the operation before/after pairs, so a stray visual
// change in either path fails the loop.
void storeOrCompareBaseline(const QString& name, const QImage& image)
{
    const QString directory = baselineDirectory() + QStringLiteral("/operations");
    if (g_updateBaselines) {
        if (!QDir().mkpath(directory)) {
            ADD_FAILURE() << "cannot create " << directory.toStdString();
            return;
        }
        const QString path = directory + QStringLiteral("/") + name;
        if (!image.save(path, "PNG")) {
            ADD_FAILURE() << "cannot save " << path.toStdString();
        }
        return;
    }

    const QString path = directory + QStringLiteral("/") + name;
    if (!QFileInfo::exists(path)) {
        ADD_FAILURE() << "missing operation baseline " << path.toStdString()
                      << " (run with --update-baselines)";
        return;
    }
    const QImage expected(path);
    ASSERT_FALSE(expected.isNull()) << path.toStdString();
    const ImageDiff diff = compareImages(expected, image);
    const bool matches = !diff.sizeMismatch &&
                         diff.differingFraction <= 0.002 &&
                         diff.meanAbs <= 0.5;
    if (!matches) {
        writeBaselineDiff(path, expected, image);
    }
    EXPECT_TRUE(matches) << name.toStdString()
                         << " differingFraction=" << diff.differingFraction
                         << " meanAbs=" << diff.meanAbs
                         << " maxAbs=" << diff.maxAbs;
}

// Contact sheet: every scenario as a labelled thumbnail, written next to the
// full-size captures so the whole mode can be reviewed in one image.
QString writeContactSheet(
    const std::vector<std::pair<QString, QImage>>& captures)
{
    if (captures.empty()) return QString();

    constexpr int columns = 3;
    constexpr int cellWidth = 420;
    constexpr int cellHeight = 268;
    constexpr int thumbWidth = 400;
    constexpr int thumbHeight = 213;
    const int rows =
        (static_cast<int>(captures.size()) + columns - 1) / columns;

    QImage sheet(columns * cellWidth, rows * cellHeight, QImage::Format_RGBA8888);
    sheet.fill(QColor(18, 18, 18));
    QPainter painter(&sheet);
    painter.setRenderHint(QPainter::Antialiasing);
    // The offscreen platform has no UI font by default (labels render as
    // missing-glyph boxes), so use the same fixed font the widget paints with.
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(9);
    painter.setFont(font);

    for (int index = 0; index < static_cast<int>(captures.size()); ++index) {
        const int column = index % columns;
        const int row = index / columns;
        const QPoint origin(column * cellWidth, row * cellHeight);
        painter.setPen(QColor(210, 210, 210));
        painter.drawText(QRect(origin.x() + 8, origin.y() + 4, cellWidth - 16, 20),
                         Qt::AlignLeft | Qt::AlignVCenter, captures[index].first);
        painter.drawImage(QRect(origin.x() + 10, origin.y() + 28, thumbWidth,
                                thumbHeight),
                          captures[index].second);
        painter.setPen(QColor(60, 60, 60));
        painter.drawRect(QRect(origin.x() + 9, origin.y() + 27, thumbWidth + 1,
                               thumbHeight + 1));
    }
    painter.end();

    return writePng(sheet, QStringLiteral("curve_editor_contact_sheet.png"));
}

// ---------------------------------------------------------------------------
// Scenarios shared by the render contract and the interactive lab
// ---------------------------------------------------------------------------

struct Scenario {
    QString name;
    std::function<void(CE::ArtifactCurveEditorWidget&)> setup;
};

void setupTwoTrackView(CE::ArtifactCurveEditorWidget& widget)
{
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(30);
    widget.setKeyEditingEnabled(true);
    widget.setHandleEditingEnabled(true);
}

void setupSelectedKey(CE::ArtifactCurveEditorWidget& widget)
{
    // Give the middle key distinct handles so the capture shows the tangent
    // pins. The zero-length-handle press contract is covered separately.
    std::vector<CE::CurveTrack> tracks = primaryTracks();
    tracks[0].keys[1].inHandleFrame = -8;
    tracks[0].keys[1].inHandleValue = 6.0f;
    tracks[0].keys[1].outHandleFrame = 8;
    tracks[0].keys[1].outHandleValue = -4.0f;
    widget.setTracks(tracks);
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(30);
    widget.setKeyEditingEnabled(true);
    widget.setHandleEditingEnabled(true);
    clickKey(widget, 0, 1);
}

void setupFlatTangents(CE::ArtifactCurveEditorWidget& widget)
{
    setupSelectedKey(widget);
    (void)widget.setSelectedKeyFlatTangents();
}

void setupConstantStep(CE::ArtifactCurveEditorWidget& widget)
{
    setupSelectedKey(widget);
    (void)widget.setSelectedKeyConstant();
}

void setupMultiSelection(CE::ArtifactCurveEditorWidget& widget)
{
    widget.setTracks(primaryTracks());
    widget.setViewRange(-20.0f, 80.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(30);
    const QPointF start = rawPoint(widget, -10.0f, 110.0f);
    const QPointF end = rawPoint(widget, 70.0f, -10.0f);
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton, Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton,
              Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, end, Qt::LeftButton,
              Qt::NoButton, Qt::ShiftModifier);
}

void setupSpeedGraph(CE::ArtifactCurveEditorWidget& widget)
{
    QVector<CE::TimeRemapKeyframe> keyframes;
    keyframes.push_back(CE::TimeRemapKeyframe{0.0, 0.0});
    keyframes.push_back(CE::TimeRemapKeyframe{1.0, 2.0});
    keyframes.push_back(CE::TimeRemapKeyframe{3.0, 1.5});
    widget.setSpeedGraph(keyframes, 0, 90, CE::FrameRate(30.0f));
    widget.setCurrentFrame(45);
    // The plain sampleSpeedGraph() surface has no source property to write
    // back to, so it stays read-only. The timeline's derived Speed graph is
    // the editable one (see setupDerivedSpeedGraph).
    widget.setKeyEditingEnabled(false);
    widget.setHandleEditingEnabled(false);
}

void setupDerivedSpeedGraph(CE::ArtifactCurveEditorWidget& widget)
{
    QVector<CE::TimeRemapKeyframe> keyframes;
    keyframes.push_back(CE::TimeRemapKeyframe{0.0, 0.0});
    keyframes.push_back(CE::TimeRemapKeyframe{1.0, 2.0});
    keyframes.push_back(CE::TimeRemapKeyframe{3.0, 1.5});
    widget.setSpeedGraph(keyframes, 0, 90, CE::FrameRate(30.0f));
    widget.setCurrentFrame(45);
    widget.setDerivedGraphMode(true);
    widget.setKeyEditingEnabled(true);
    widget.setHandleEditingEnabled(false);
    clickKey(widget, 0, 1);
}

void setupNormalizedView(CE::ArtifactCurveEditorWidget& widget)
{
    widget.setTracks({
        track(QStringLiteral("Opacity"), QColor(255, 128, 64),
              {key(0, 0.0f), key(30, 100.0f), key(60, 50.0f)}),
        track(QStringLiteral("Scale"), QColor(120, 220, 140),
              {key(0, 1.0f), key(30, 0.2f), key(60, 0.9f)}),
    });
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    pressKey(widget, Qt::Key_N);
    widget.setCurrentFrame(30);
}

void setupNormalizedSelection(CE::ArtifactCurveEditorWidget& widget)
{
    setupNormalizedView(widget);
    const QPointF start = rawPoint(widget, -5.0f, 1.15f);
    const QPointF end = rawPoint(widget, 65.0f, -1.15f);
    drag(widget, start, end, Qt::ShiftModifier);
}

void setupBufferCurve(CE::ArtifactCurveEditorWidget& widget)
{
    setupTwoTrackView(widget);
    const QPointF start = keyPoint(widget, 0, 1);
    const QPointF end = widget.visualPoint(0, 40.0f, 60.0f);
    // Starting a key drag captures and shows the buffer curve automatically.
    drag(widget, start, end);
}

void setupMarqueeInProgress(CE::ArtifactCurveEditorWidget& widget)
{
    widget.setTracks(primaryTracks());
    widget.setViewRange(-20.0f, 80.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(30);
    const QPointF start = rawPoint(widget, -10.0f, 110.0f);
    const QPointF end = rawPoint(widget, 50.0f, 20.0f);
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton, Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton,
              Qt::ShiftModifier);
}

void setupLinearTangents(CE::ArtifactCurveEditorWidget& widget)
{
    setupSelectedKey(widget);
    (void)widget.setSelectedKeyLinearTangents();
}

void setupCopyPaste(CE::ArtifactCurveEditorWidget& widget)
{
    setupTwoTrackView(widget);
    widget.focusTrack(0);
    pressKey(widget, Qt::Key_A);
    pressKey(widget, Qt::Key_C);
    // focusTrack() only changes visibility; the paste target is the track of
    // the selected key, so click one key on the destination track.
    widget.focusTrack(1);
    clickKey(widget, 1, 0);
    pressKey(widget, Qt::Key_V);
}

void setupClonedSelection(CE::ArtifactCurveEditorWidget& widget)
{
    setupTwoTrackView(widget);
    widget.focusTrack(0);
    // focusTrack() refits, so widen the view after it to include the clones.
    widget.setViewRange(0.0f, 130.0f, -20.0f, 120.0f);
    pressKey(widget, Qt::Key_A);
    pressKey(widget, Qt::Key_D, Qt::ShiftModifier);
}

void setupPannedZoomed(CE::ArtifactCurveEditorWidget& widget)
{
    setupTwoTrackView(widget);
    drag(widget, rawPoint(widget, 40.0f, 100.0f), rawPoint(widget, 25.0f, 85.0f));
    wheel(widget, rawPoint(widget, 30.0f, 50.0f), 120, Qt::ShiftModifier);
}

void setupFocusedTrack(CE::ArtifactCurveEditorWidget& widget)
{
    setupTwoTrackView(widget);
    widget.focusTrack(1);
}

std::vector<Scenario> buildScenarios()
{
    return {
        {QStringLiteral("Value graph - two tracks"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupTwoTrackView(widget); }},
        {QStringLiteral("Selected key - tangent handles"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupSelectedKey(widget); }},
        {QStringLiteral("Flat tangents"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupFlatTangents(widget); }},
        {QStringLiteral("Linear tangents"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupLinearTangents(widget); }},
        {QStringLiteral("Constant (step) segment"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupConstantStep(widget); }},
        {QStringLiteral("Multi selection - marquee"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupMultiSelection(widget); }},
        {QStringLiteral("Marquee in progress"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupMarqueeInProgress(widget); }},
        {QStringLiteral("Speed graph - derived read-only"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupSpeedGraph(widget); }},
        {QStringLiteral("Speed graph - derived editing"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupDerivedSpeedGraph(widget); }},
        {QStringLiteral("Normalized view"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupNormalizedView(widget); }},
        {QStringLiteral("Normalized marquee selection"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupNormalizedSelection(widget); }},
        {QStringLiteral("Buffer curve after key drag"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupBufferCurve(widget); }},
        {QStringLiteral("Copy/paste across tracks"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupCopyPaste(widget); }},
        {QStringLiteral("Cloned selection"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupClonedSelection(widget); }},
        {QStringLiteral("Panned and zoomed view"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupPannedZoomed(widget); }},
        {QStringLiteral("Focused track"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupFocusedTrack(widget); }},
    };
}

// ---------------------------------------------------------------------------
// Interactive lab window (no new signal/slot connections: the scenario strip
// paints and hit-tests itself, and number keys are handled in a widget subclass)
// ---------------------------------------------------------------------------

class ScenarioStrip final : public QWidget {
public:
    explicit ScenarioStrip(QStringList names, QWidget* parent = nullptr)
        : QWidget(parent), names_(std::move(names))
    {
        setMinimumHeight(58);
        setMaximumHeight(58);
        setMouseTracking(true);
    }

    void setCurrent(int index)
    {
        current_ = index;
        update();
    }

    std::function<void(int)> picked;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor(32, 32, 34));
        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);
        for (int index = 0; index < names_.size(); ++index) {
            const QRect cell = cellRect(index);
            const bool active = index == current_;
            painter.setPen(Qt::NoPen);
            painter.setBrush(active ? QColor(58, 96, 150) : QColor(44, 44, 47));
            painter.drawRoundedRect(cell.adjusted(3, 3, -3, -3), 4, 4);
            painter.setPen(active ? QColor(240, 240, 240) : QColor(170, 170, 175));
            painter.drawText(cell.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                             QStringLiteral("%1  %2").arg(index + 1).arg(names_[index]));
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        for (int index = 0; index < names_.size(); ++index) {
            if (cellRect(index).contains(event->position().toPoint())) {
                setCurrent(index);
                if (picked) picked(index);
                return;
            }
        }
    }

private:
    QRect cellRect(int index) const
    {
        const int perRow = 4;
        const int columns = std::max(1, std::min(perRow, static_cast<int>(names_.size())));
        const int cellWidth = width() / columns;
        const int row = index / perRow;
        const int column = index % perRow;
        return QRect(column * cellWidth, row * 29, cellWidth, 29);
    }

    QStringList names_;
    int current_ = 0;
};

class LabCurveEditor final : public CE::ArtifactCurveEditorWidget {
public:
    using CE::ArtifactCurveEditorWidget::ArtifactCurveEditorWidget;

    std::function<void(int)> scenarioKey;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event && !event->isAutoRepeat() && scenarioKey &&
            event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
            scenarioKey(event->key() - Qt::Key_1);
            event->accept();
            return;
        }
        CE::ArtifactCurveEditorWidget::keyPressEvent(event);
    }
};

class LabWindow final : public QMainWindow {
public:
    explicit LabWindow(std::vector<Scenario> scenarios)
        : scenarios_(std::move(scenarios))
    {
        setWindowTitle(QStringLiteral(
            "Curve Editor Lab - 1-8 scenario, F fit, N normalized, B buffer, "
            "A select all, Esc deselect, Ctrl+click insert, Del delete"));
        resize(1100, 640);

        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        QStringList names;
        for (const Scenario& scenario : scenarios_) names << scenario.name;
        strip_ = new ScenarioStrip(names, central);
        strip_->picked = [this](const int index) { apply(index); };
        layout->addWidget(strip_);

        editor_ = new LabCurveEditor(central);
        editor_->scenarioKey = [this](const int index) {
            if (index >= 0 && index < static_cast<int>(scenarios_.size())) {
                strip_->setCurrent(index);
                apply(index);
            }
        };
        layout->addWidget(editor_, 1);
        setCentralWidget(central);

        if (!scenarios_.empty()) apply(0);
    }

private:
    void apply(const int index)
    {
        // focusTrack() no longer clears the selection, so use Escape (clear +
        // show all) before replacing the tracks. On an empty editor this logs a
        // harmless fitToContent fallback.
        if (!editor_->tracks().empty()) {
            pressKey(*editor_, Qt::Key_Escape);
        }
        scenarios_[index].setup(*editor_);
        setWindowTitle(QStringLiteral("Curve Editor Lab - %1")
                           .arg(scenarios_[index].name));
    }

    std::vector<Scenario> scenarios_;
    ScenarioStrip* strip_ = nullptr;
    LabCurveEditor* editor_ = nullptr;
};

int runUi(QApplication& app)
{
    LabWindow window(buildScenarios());
    window.show();
    return app.exec();
}

// The offscreen platform on Windows ships no usable font database, so every
// painted string (axis labels, track names, contact-sheet captions) turns into
// missing-glyph boxes. Load a real system font and make it the application
// default before any widget or capture is created.
void configureCaptureFonts(const bool report)
{
    const QStringList families = QFontDatabase::families();
    if (report) {
        std::printf("[fonts] families=%lld\n",
                    static_cast<long long>(families.size()));
        for (int index = 0; index < families.size() && index < 24; ++index) {
            std::printf("  %s\n", families[index].toUtf8().constData());
        }
    }

    const QString fallbackSystem = qEnvironmentVariable("WINDIR")
        + QStringLiteral("/Fonts/");
    const QStringList preferred = {
        QStringLiteral("Segoe UI"), QStringLiteral("Arial"),
        QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans")};
    QString chosen;
    for (const QString& family : preferred) {
        if (families.contains(family)) {
            chosen = family;
            break;
        }
    }
    if (chosen.isEmpty()) {
        for (const QString& file :
             {QStringLiteral("segoeui.ttf"), QStringLiteral("arial.ttf"),
              QStringLiteral("consola.ttf")}) {
            const QString path = fallbackSystem + file;
            if (!QFileInfo::exists(path)) continue;
            const int id = QFontDatabase::addApplicationFont(path);
            if (report) {
                std::printf("[fonts] add %s -> %d\n",
                            path.toUtf8().constData(), id);
            }
            if (id < 0) continue;
            const QStringList loaded = QFontDatabase::applicationFontFamilies(id);
            if (!loaded.isEmpty()) {
                chosen = loaded.front();
                break;
            }
        }
    }

    if (report) {
        const QString fixed =
            QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
        const QString general =
            QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
        std::printf("[fonts] chosen=%s fixed=%s general=%s\n",
                    chosen.toUtf8().constData(), fixed.toUtf8().constData(),
                    general.toUtf8().constData());
    }

    if (!chosen.isEmpty()) {
        QFont font(chosen);
        font.setPointSize(9);
        QApplication::setFont(font);
    }
}

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct Options {
    bool ui = false;
    bool listScenarios = false;
    bool fontReport = false;
    bool updateBaselines = false;
    QString scenario;
};

Options parseOptions(const QStringList& arguments)
{
    Options options;
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const QString& argument = arguments[index];
        if (argument == QStringLiteral("--ui")) {
            options.ui = true;
        } else if (argument == QStringLiteral("--list-scenarios")) {
            options.listScenarios = true;
        } else if (argument == QStringLiteral("--font-report")) {
            options.fontReport = true;
        } else if (argument == QStringLiteral("--update-baselines")) {
            options.updateBaselines = true;
            g_updateBaselines = true;
        } else if (argument == QStringLiteral("--baseline-dir") &&
                   index + 1 < arguments.size()) {
            g_baselineDirectory = arguments[++index];
        } else if (argument == QStringLiteral("--capture-dir") &&
                   index + 1 < arguments.size()) {
            g_captureDirectory = arguments[++index];
        } else if (argument == QStringLiteral("--scenario") &&
                   index + 1 < arguments.size()) {
            options.scenario = arguments[++index];
        }
    }
    return options;
}

} // namespace

// ===========================================================================
// Speed graph sampling (the derived track the Speed mode displays)
// ===========================================================================

TEST(CurveEditorStandaloneTest, SpeedGraphSamplingMatchesFrameAndPercentContract)
{
    QVector<CE::TimeRemapKeyframe> keyframes;
    keyframes.push_back(CE::TimeRemapKeyframe{0.0, 0.0});
    keyframes.push_back(CE::TimeRemapKeyframe{1.0, 2.0});

    const CE::CurveTrack speed =
        CE::sampleSpeedGraph(keyframes, 0, 60, CE::FrameRate(30.0f));

    ASSERT_EQ(speed.keys.size(), 3u);
    EXPECT_EQ(speed.name, QStringLiteral("Speed (%)"));
    EXPECT_EQ(speed.keys[0].frame, 0);
    EXPECT_FLOAT_EQ(speed.keys[0].value, 100.0f);
    EXPECT_EQ(speed.keys[1].frame, 30);
    EXPECT_FLOAT_EQ(speed.keys[1].value, 200.0f);
    EXPECT_EQ(speed.keys[2].frame, 60);
    EXPECT_FLOAT_EQ(speed.keys[2].value, 200.0f);
    EXPECT_TRUE(speed.keys[1].smooth);

    // 25% bezier handle rule, in seconds -> frames at 30 fps.
    EXPECT_EQ(speed.keys[1].inHandleFrame, -8);
    EXPECT_NEAR(speed.keys[1].inHandleValue, 26.6667f, 0.01f);
    EXPECT_EQ(speed.keys[1].outHandleFrame, 8);
    EXPECT_NEAR(speed.keys[1].outHandleValue, 0.0f, 0.001f);
}

TEST(CurveEditorStandaloneTest, SpeedGraphClampsPadsAndSwapsRange)
{
    QVector<CE::TimeRemapKeyframe> fast;
    fast.push_back(CE::TimeRemapKeyframe{0.0, 0.0});
    fast.push_back(CE::TimeRemapKeyframe{1.0, 20.0});
    const CE::CurveTrack clamped =
        CE::sampleSpeedGraph(fast, 0, 45, CE::FrameRate(30.0f));
    ASSERT_EQ(clamped.keys.size(), 3u);
    EXPECT_FLOAT_EQ(clamped.keys[1].value, 1000.0f);
    EXPECT_FLOAT_EQ(clamped.keys[2].value, 1000.0f);

    QVector<CE::TimeRemapKeyframe> reverse;
    reverse.push_back(CE::TimeRemapKeyframe{0.0, 2.0});
    reverse.push_back(CE::TimeRemapKeyframe{1.0, 0.0});
    const CE::CurveTrack reversed =
        CE::sampleSpeedGraph(reverse, 0, 30, CE::FrameRate(30.0f));
    ASSERT_EQ(reversed.keys.size(), 2u);
    EXPECT_FLOAT_EQ(reversed.keys[1].value, -200.0f);

    // A reversed range is normalized instead of producing negative frames.
    const CE::CurveTrack swapped =
        CE::sampleSpeedGraph(reverse, 60, 0, CE::FrameRate(30.0f));
    ASSERT_TRUE(!swapped.keys.empty());
    EXPECT_EQ(swapped.keys.front().frame, 0);
    EXPECT_EQ(swapped.keys.back().frame, 60);
}

TEST(CurveEditorStandaloneTest, SpeedGraphEmptyInputSynthesizesFlatHundredPercent)
{
    const CE::CurveTrack track =
        CE::sampleSpeedGraph({}, 5, 95, CE::FrameRate(30.0f));
    ASSERT_EQ(track.keys.size(), 2u);
    EXPECT_EQ(track.keys[0].frame, 5);
    EXPECT_EQ(track.keys[1].frame, 95);
    EXPECT_FLOAT_EQ(track.keys[0].value, 100.0f);
    EXPECT_FLOAT_EQ(track.keys[1].value, 100.0f);
}

// ===========================================================================
// View geometry
// ===========================================================================

TEST(CurveEditorStandaloneTest, ViewMappingMatchesPlotRectContract)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(-10.0f, 50.0f, -20.0f, 120.0f);

    const QRectF plot = widget.visualPlotRect();
    EXPECT_NEAR(plot.left(), kMarginLeft, 0.01);
    EXPECT_NEAR(plot.top(), kMarginTop, 0.01);
    EXPECT_NEAR(plot.width(), kWidth - kMarginLeft - kMarginRight, 0.01);
    EXPECT_NEAR(plot.height(), kHeight - kMarginTop - kMarginBottom, 0.01);

    const QPointF topLeft = rawPoint(widget, -10.0f, 120.0f);
    EXPECT_NEAR(topLeft.x(), plot.left(), 0.01);
    EXPECT_NEAR(topLeft.y(), plot.top(), 0.01);

    const QPointF bottomRight = rawPoint(widget, 50.0f, -20.0f);
    EXPECT_NEAR(bottomRight.x(), plot.right(), 0.01);
    EXPECT_NEAR(bottomRight.y(), plot.bottom(), 0.01);

    const QPointF middle = rawPoint(widget, 20.0f, 50.0f);
    EXPECT_NEAR(middle.x(), plot.left() + plot.width() * 0.5, 0.01);
    EXPECT_NEAR(middle.y(), plot.top() + plot.height() * 0.5, 0.01);

    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), -10.0, 0.001);
    EXPECT_NEAR(view.top(), -20.0, 0.001);
    EXPECT_NEAR(view.width(), 60.0, 0.001);
    EXPECT_NEAR(view.height(), 140.0, 0.001);
}

TEST(CurveEditorStandaloneTest, FitToContentAddsMarginAndFallsBackWhenEmpty)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);

    widget.fitToContent();
    QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), 0.0, 0.001);
    EXPECT_NEAR(view.width(), 100.0, 0.001);
    EXPECT_NEAR(view.top(), -10.0, 0.001);
    EXPECT_NEAR(view.height(), 120.0, 0.001);

    widget.setTracks({track(QStringLiteral("Opacity"), QColor(255, 128, 64),
                            {key(10, 0.0f), key(50, 100.0f)})});
    widget.fitToContent();
    view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), 5.0, 0.001);
    EXPECT_NEAR(view.width(), 50.0, 0.001);
    EXPECT_NEAR(view.top(), -10.0, 0.001);
    EXPECT_NEAR(view.height(), 120.0, 0.001);
}

TEST(CurveEditorStandaloneTest, NormalizedViewMapsValuesIntoUnitRange)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks({track(QStringLiteral("Opacity"), QColor(255, 128, 64),
                            {key(0, 0.0f), key(30, 100.0f), key(60, 50.0f)})});
    widget.setViewRange(0.0f, 60.0f, 0.0f, 100.0f);

    pressKey(widget, Qt::Key_N);

    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.top(), -1.2, 0.001);
    EXPECT_NEAR(view.height(), 2.4, 0.001);
    EXPECT_NEAR(view.left(), 0.0, 0.001);
    EXPECT_NEAR(view.width(), 60.0, 0.001);

    const QPointF low = widget.visualPoint(0, 0.0f, 0.0f);
    const QPointF mid = widget.visualPoint(0, 30.0f, 50.0f);
    const QPointF high = widget.visualPoint(0, 60.0f, 100.0f);
    EXPECT_NEAR(low.y(), rawPoint(widget, 0.0f, -1.0f).y(), 0.01);
    EXPECT_NEAR(mid.y(), rawPoint(widget, 0.0f, 0.0f).y(), 0.01);
    EXPECT_NEAR(high.y(), rawPoint(widget, 0.0f, 1.0f).y(), 0.01);
    EXPECT_LT(high.y(), mid.y());
    EXPECT_LT(mid.y(), low.y());

    // N toggles back to a fitted absolute view.
    pressKey(widget, Qt::Key_N);
    const QRectF restored = widget.visualViewRange();
    EXPECT_NEAR(restored.top(), -10.0, 0.01);
    EXPECT_NEAR(restored.height(), 120.0, 0.01);
}

TEST(CurveEditorStandaloneTest, FocusTrackIsolatesAndRestoresVisibility)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());

    widget.focusTrack(1);
    EXPECT_FALSE(widget.tracks()[0].visible);
    EXPECT_TRUE(widget.tracks()[1].visible);
    // focusTrack owns visibility and the view fit, not the selection.
    EXPECT_EQ(widget.visualSelectedTrack(), -1);

    widget.focusTrack(-1);
    EXPECT_TRUE(widget.tracks()[0].visible);
    EXPECT_TRUE(widget.tracks()[1].visible);
    EXPECT_EQ(widget.visualSelectedTrack(), -1);
}

TEST(CurveEditorStandaloneTest, FocusTrackDoesNotClobberTheSelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_EQ(widget.visualSelectedKey(), 1);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 1);

    // The timeline calls focusTrack() on every refresh; it must keep the key
    // the user selected, otherwise tangent edits hit a different key than the
    // inline frame/value editors show.
    widget.focusTrack(0);
    EXPECT_EQ(widget.visualSelectedKey(), 1);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 1);
    EXPECT_TRUE(widget.setSelectedKeyFlatTangents());
    EXPECT_NEAR(widget.tracks()[0].keys[1].inHandleValue, 0.0f, 0.001f);
    EXPECT_EQ(widget.tracks()[0].keys[0].inHandleFrame, 0);
}

TEST(CurveEditorStandaloneTest, SetTracksPrunesStaleSelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());

    clickKey(widget, 0, 2);
    ASSERT_EQ(widget.visualSelectedKey(), 2);

    // Replacing the track data with fewer keys must not leave a dangling
    // selection that later edit operations would index into.
    widget.setTracks({track(QStringLiteral("Only"), QColor(255, 128, 64),
                            {key(0, 1.0f)})});
    EXPECT_EQ(widget.visualSelectedTrack(), -1);
    EXPECT_EQ(widget.visualSelectedKey(), -1);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 0);
    EXPECT_FALSE(widget.setSelectedKeyFlatTangents());
    (void)renderWidget(widget);
}

// ===========================================================================
// Tangent operations
// ===========================================================================

TEST(CurveEditorStandaloneTest, TangentOperationsRewriteHandlesAndFlags)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    EXPECT_TRUE(widget.setSelectedKeyFlatTangents());
    {
        const CE::CurveKey& flat = widget.tracks()[0].keys[1];
        EXPECT_EQ(flat.inHandleFrame, -10);
        EXPECT_EQ(flat.outHandleFrame, 10);
        EXPECT_NEAR(flat.inHandleValue, 0.0f, 0.001f);
        EXPECT_NEAR(flat.outHandleValue, 0.0f, 0.001f);
        EXPECT_TRUE(flat.smooth);
        EXPECT_FALSE(flat.brokenTangents);
    }

    EXPECT_TRUE(widget.setSelectedKeyAutoTangents());
    {
        const CE::CurveKey& automatic = widget.tracks()[0].keys[1];
        EXPECT_NEAR(automatic.inHandleValue, -8.33333f, 0.01f);
        EXPECT_NEAR(automatic.outHandleValue, 8.33333f, 0.01f);
        EXPECT_TRUE(automatic.smooth);
        EXPECT_FALSE(automatic.brokenTangents);
    }

    EXPECT_TRUE(widget.setSelectedKeyLinearTangents());
    {
        const CE::CurveKey& linear = widget.tracks()[0].keys[1];
        EXPECT_NEAR(linear.inHandleValue, -33.3333f, 0.01f);
        EXPECT_NEAR(linear.outHandleValue, -16.6667f, 0.01f);
        EXPECT_FALSE(linear.smooth);
    }

    EXPECT_TRUE(widget.setSelectedKeyBrokenTangents());
    {
        const CE::CurveKey& broken = widget.tracks()[0].keys[1];
        EXPECT_TRUE(broken.brokenTangents);
        EXPECT_FALSE(broken.smooth);
    }

    EXPECT_TRUE(widget.setSelectedKeyUnifiedTangents());
    {
        const CE::CurveKey& unified = widget.tracks()[0].keys[1];
        EXPECT_FALSE(unified.brokenTangents);
        EXPECT_TRUE(unified.smooth);
        EXPECT_NEAR(unified.inHandleValue / -10.0f, unified.outHandleValue / 10.0f,
                    0.001f);
        EXPECT_NEAR(unified.inTangent, -2.5f, 0.001f);
        EXPECT_NEAR(unified.outTangent, -2.5f, 0.001f);
    }

    EXPECT_TRUE(widget.setSelectedKeyConstant());
    {
        const CE::CurveKey& constant = widget.tracks()[0].keys[1];
        EXPECT_TRUE(constant.constant);
        EXPECT_FALSE(constant.smooth);
    }

    EXPECT_TRUE(widget.setSelectedKeyBezier());
    {
        const CE::CurveKey& bezier = widget.tracks()[0].keys[1];
        EXPECT_FALSE(bezier.constant);
        EXPECT_TRUE(bezier.smooth);
    }
}

TEST(CurveEditorStandaloneTest, TangentOperationsRequireASelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());

    pressKey(widget, Qt::Key_Escape);
    ASSERT_EQ(widget.visualSelectedTrack(), -1);

    EXPECT_FALSE(widget.setSelectedKeyAutoTangents());
    EXPECT_FALSE(widget.setSelectedKeyFlatTangents());
    EXPECT_FALSE(widget.setSelectedKeyLinearTangents());
    EXPECT_FALSE(widget.setSelectedKeyBrokenTangents());
    EXPECT_FALSE(widget.setSelectedKeyUnifiedTangents());
    EXPECT_FALSE(widget.setSelectedKeyConstant());
    EXPECT_FALSE(widget.setSelectedKeyBezier());
}

// ===========================================================================
// Editing interactions
// ===========================================================================

TEST(CurveEditorStandaloneTest, CtrlClickInsertsKeyAndDeleteRemovesIt)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    // One track only: insertKeyAt() targets the selected track, else the curve
    // nearest the click. With the two-track fixture the Rotation track wins.
    widget.setTracks({primaryTracks()[0]});
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    const QPointF insertPoint = rawPoint(widget, 45.0f, 25.0f);
    sendMouse(widget, QEvent::MouseButtonPress, insertPoint, Qt::LeftButton,
              Qt::LeftButton, Qt::ControlModifier);

    ASSERT_EQ(widget.tracks()[0].keys.size(), 4u);
    EXPECT_EQ(widget.tracks()[0].keys[2].frame, 45);
    EXPECT_NEAR(widget.tracks()[0].keys[2].value, 25.0f, 0.01f);
    EXPECT_TRUE(keySelected(widget, 0, 2));

    pressKey(widget, Qt::Key_Delete);
    ASSERT_EQ(widget.tracks()[0].keys.size(), 3u);
    EXPECT_EQ(widget.tracks()[0].keys[0].frame, 0);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 30);
    EXPECT_EQ(widget.tracks()[0].keys[2].frame, 60);
}

TEST(CurveEditorStandaloneTest, SelectAllAndEscapeToggleSelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    pressKey(widget, Qt::Key_A);
    for (int trackIndex = 0; trackIndex < 2; ++trackIndex) {
        for (int keyIndex = 0;
             keyIndex < static_cast<int>(widget.tracks()[trackIndex].keys.size());
             ++keyIndex) {
            EXPECT_TRUE(keySelected(widget, trackIndex, keyIndex));
        }
    }

    pressKey(widget, Qt::Key_A);
    EXPECT_FALSE(keySelected(widget, 0, 0));
    EXPECT_EQ(widget.visualSelectedTrack(), -1);

    pressKey(widget, Qt::Key_A);
    EXPECT_TRUE(keySelected(widget, 0, 0));
    pressKey(widget, Qt::Key_Escape);
    EXPECT_FALSE(keySelected(widget, 0, 0));
    EXPECT_EQ(widget.visualSelectedTrack(), -1);
}

TEST(CurveEditorStandaloneTest, ShiftMarqueeAddsToTheExistingSelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 0);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 1);

    // A box around the frame-60 key only must keep the earlier frame-0 key.
    drag(widget, rawPoint(widget, 55.0f, 60.0f),
         rawPoint(widget, 65.0f, 40.0f), Qt::ShiftModifier);

    EXPECT_EQ(widget.visualSelectedKeyCount(), 2);
    EXPECT_TRUE(keySelected(widget, 0, 0));
    EXPECT_TRUE(keySelected(widget, 0, 2));
}

TEST(CurveEditorStandaloneTest, EscapeThenMarqueeReplacesTheSelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 0);
    pressKey(widget, Qt::Key_Escape);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 0);

    drag(widget, rawPoint(widget, 55.0f, 60.0f),
         rawPoint(widget, 65.0f, 40.0f), Qt::ShiftModifier);

    EXPECT_EQ(widget.visualSelectedKeyCount(), 1);
    EXPECT_FALSE(keySelected(widget, 0, 0));
    EXPECT_TRUE(keySelected(widget, 0, 2));
}

TEST(CurveEditorStandaloneTest, MarqueeSelectsKeysInsideRect)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(-20.0f, 80.0f, -20.0f, 120.0f);

    const QPointF start = rawPoint(widget, -10.0f, 110.0f);
    const QPointF end = rawPoint(widget, 70.0f, -10.0f);
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton, Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton,
              Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, end, Qt::LeftButton,
              Qt::NoButton, Qt::ShiftModifier);

    for (int trackIndex = 0; trackIndex < 2; ++trackIndex) {
        for (int keyIndex = 0;
             keyIndex < static_cast<int>(widget.tracks()[trackIndex].keys.size());
             ++keyIndex) {
            EXPECT_TRUE(keySelected(widget, trackIndex, keyIndex));
        }
    }

    // A tighter rectangle selects only the middle key of the first track.
    pressKey(widget, Qt::Key_Escape);
    const QPointF near = rawPoint(widget, 25.0f, 90.0f);
    const QPointF far = rawPoint(widget, 35.0f, 110.0f);
    sendMouse(widget, QEvent::MouseButtonPress, near, Qt::LeftButton,
              Qt::LeftButton, Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseMove, far, Qt::NoButton, Qt::LeftButton,
              Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, far, Qt::LeftButton,
              Qt::NoButton, Qt::ShiftModifier);
    EXPECT_EQ(widget.visualSelectedTrack(), 0);
    EXPECT_TRUE(keySelected(widget, 0, 1));
    EXPECT_EQ(widget.tracks()[0].keys.size(), 3u);
}

TEST(CurveEditorStandaloneTest, KeyDragMovesFrameAndValueAndKeepsOrder)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);

    // primaryTracks() keys carry zero-length handles, i.e. the handle pins sit
    // on the key. The press must still move the key (regression: it used to be
    // captured by MoveHandleIn).
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    const QPointF start = keyPoint(widget, 0, 1);
    const QPointF end = widget.visualPoint(0, 40.0f, 80.0f);
    drag(widget, start, end);

    ASSERT_EQ(widget.tracks()[0].keys.size(), 3u);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 40);
    EXPECT_NEAR(widget.tracks()[0].keys[1].value, 80.0f, 0.5f);
    EXPECT_TRUE(keySelected(widget, 0, 1));

    const auto& keys = widget.tracks()[0].keys;
    EXPECT_LT(keys[0].frame, keys[1].frame);
    EXPECT_LT(keys[1].frame, keys[2].frame);
}

TEST(CurveEditorStandaloneTest, PlayheadScrubUpdatesCurrentFrame)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    const int scrubY = plotRectPixels().bottom() + 10;
    const qreal x25 = rawPoint(widget, 25.0f, 0.0f).x();
    sendMouse(widget, QEvent::MouseButtonPress, QPointF(x25, scrubY),
              Qt::LeftButton, Qt::LeftButton);
    EXPECT_EQ(widget.currentFrame(), 25);

    const qreal x35 = rawPoint(widget, 35.0f, 0.0f).x();
    sendMouse(widget, QEvent::MouseMove, QPointF(x35, scrubY), Qt::NoButton,
              Qt::LeftButton);
    EXPECT_EQ(widget.currentFrame(), 35);

    sendMouse(widget, QEvent::MouseButtonRelease, QPointF(x35, scrubY),
              Qt::LeftButton, Qt::NoButton);
}

TEST(CurveEditorStandaloneTest, ShiftWheelZoomsXWithoutMovingTheAnchor)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    const QPointF anchor = rawPoint(widget, 30.0f, 50.0f);
    wheel(widget, anchor, 120, Qt::ShiftModifier);

    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.width(), 54.0, 0.01);
    EXPECT_NEAR(view.left() + view.width() * 0.5, 30.0, 0.01);
    EXPECT_NEAR(view.top(), -20.0, 0.01);
    EXPECT_NEAR(view.height(), 140.0, 0.01);
}

TEST(CurveEditorStandaloneTest, ReadOnlyModeRejectsSpeedGraphEdits)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    setupSpeedGraph(widget);
    ASSERT_FALSE(widget.tracks().empty());

    const std::vector<CE::CurveKey> before = widget.tracks()[0].keys;

    const QPointF insertPoint = rawPoint(widget, 45.0f, 55.0f);
    sendMouse(widget, QEvent::MouseButtonPress, insertPoint, Qt::LeftButton,
              Qt::LeftButton, Qt::ControlModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, insertPoint, Qt::LeftButton,
              Qt::NoButton);
    EXPECT_EQ(widget.tracks()[0].keys.size(), before.size());

    pressKey(widget, Qt::Key_Delete);
    pressKey(widget, Qt::Key_Backspace);
    EXPECT_EQ(widget.tracks()[0].keys.size(), before.size());

    // A drag must pan the view instead of moving keys.
    const QRectF viewBefore = widget.visualViewRange();
    const QPointF start = keyPoint(widget, 0, 0);
    const QPointF end = start + QPointF(40.0, 25.0);
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
    sendMouse(widget, QEvent::MouseButtonRelease, end, Qt::LeftButton,
              Qt::NoButton);

    ASSERT_EQ(widget.tracks()[0].keys.size(), before.size());
    for (std::size_t index = 0; index < before.size(); ++index) {
        EXPECT_EQ(widget.tracks()[0].keys[index].frame, before[index].frame);
        EXPECT_FLOAT_EQ(widget.tracks()[0].keys[index].value, before[index].value);
    }
    EXPECT_NE(widget.visualViewRange().left(), viewBefore.left());
}

// ===========================================================================
// CE-14 speed math (the timeline's derived Speed graph uses these directly)
// ===========================================================================

namespace {

std::vector<CE::CurveKey> speedFixture()
{
    return {key(0, 0.0f), key(30, 100.0f), key(60, 50.0f)};
}

} // namespace

TEST(CurveEditorStandaloneTest, CurveSpeedMatchesAveragedSegmentFormula)
{
    const std::vector<CE::CurveKey> keys = speedFixture();

    EXPECT_NEAR(CE::curveSpeedAt(keys, 0, 30.0), 100.0f, 0.001f);
    EXPECT_NEAR(CE::curveSpeedAt(keys, 1, 30.0), 75.0f, 0.001f);
    EXPECT_NEAR(CE::curveSpeedAt(keys, 2, 30.0), 50.0f, 0.001f);

    // Out-of-range indices and a non-positive frame rate are inert.
    EXPECT_FLOAT_EQ(CE::curveSpeedAt(keys, -1, 30.0), 0.0f);
    EXPECT_FLOAT_EQ(CE::curveSpeedAt(keys, 3, 30.0), 0.0f);
    EXPECT_FLOAT_EQ(CE::curveSpeedAt(keys, 1, 0.0), 0.0f);
}

TEST(CurveEditorStandaloneTest, CurveSpeedIgnoresConstantSegments)
{
    std::vector<CE::CurveKey> keys = speedFixture();
    keys[0] = constantKey(0, 0.0f);

    // key 0: the only outgoing segment is constant -> no samples.
    EXPECT_FLOAT_EQ(CE::curveSpeedAt(keys, 0, 30.0), 0.0f);
    // key 1: left segment is constant, right segment stays active.
    EXPECT_NEAR(CE::curveSpeedAt(keys, 1, 30.0), 50.0f, 0.001f);
    // key 2: left segment (1 -> 2) is active.
    EXPECT_NEAR(CE::curveSpeedAt(keys, 2, 30.0), 50.0f, 0.001f);

    keys[1].constant = true;
    EXPECT_FLOAT_EQ(CE::curveSpeedAt(keys, 1, 30.0), 0.0f);
    EXPECT_FLOAT_EQ(CE::curveSpeedAt(keys, 2, 30.0), 0.0f);
}

TEST(CurveEditorStandaloneTest, SolveSpeedOnEndpointKeysIsExact)
{
    std::vector<CE::CurveKey> keys = speedFixture();

    float solved = 0.0f;
    ASSERT_TRUE(CE::solveCurveKeyValueForSpeed(keys, 0, 200.0f, 30.0, solved));
    EXPECT_NEAR(solved, -100.0f, 0.001f);
    keys[0].value = solved;
    EXPECT_NEAR(CE::curveSpeedAt(keys, 0, 30.0), 200.0f, 0.01f);

    keys = speedFixture();
    ASSERT_TRUE(CE::solveCurveKeyValueForSpeed(keys, 2, 80.0f, 30.0, solved));
    EXPECT_NEAR(solved, 20.0f, 0.001f);
    keys[2].value = solved;
    EXPECT_NEAR(CE::curveSpeedAt(keys, 2, 30.0), 80.0f, 0.01f);
}

TEST(CurveEditorStandaloneTest, SolveSpeedOnInteriorKeyRoundTrips)
{
    std::vector<CE::CurveKey> keys = {key(0, 0.0f), key(20, 100.0f),
                                      key(60, 50.0f)};

    float solved = 0.0f;
    ASSERT_TRUE(CE::solveCurveKeyValueForSpeed(keys, 1, 60.0f, 30.0, solved));
    keys[1].value = solved;
    EXPECT_NEAR(CE::curveSpeedAt(keys, 1, 30.0), 60.0f, 0.05f);
    // Two valid branches exist here (-36.67 and 70); the solver keeps the one
    // closest to the current value, so a small drag does not jump branches.
    EXPECT_NEAR(solved, 70.0f, 0.5f);
}

TEST(CurveEditorStandaloneTest, SolveSpeedRejectsInvalidOrUnreachableTargets)
{
    const std::vector<CE::CurveKey> equalSpans = {key(0, 0.0f), key(30, 100.0f),
                                                  key(60, 200.0f)};
    float solved = 0.0f;
    // Below the minimum average the neighbours allow (target 50 < 100 at the
    // key), and equal spans give no middle branch to move along.
    EXPECT_FALSE(CE::solveCurveKeyValueForSpeed(equalSpans, 1, 50.0f, 30.0, solved));
    EXPECT_FALSE(CE::solveCurveKeyValueForSpeed(equalSpans, 1, -10.0f, 30.0, solved));
    EXPECT_FALSE(CE::solveCurveKeyValueForSpeed(equalSpans, -1, 100.0f, 30.0, solved));
    EXPECT_FALSE(CE::solveCurveKeyValueForSpeed(equalSpans, 3, 100.0f, 30.0, solved));
    EXPECT_FALSE(CE::solveCurveKeyValueForSpeed(equalSpans, 1, 100.0f, 0.0, solved));

    // Re-solving for the value the key already has is idempotent.
    EXPECT_TRUE(CE::solveCurveKeyValueForSpeed(equalSpans, 1, 100.0f, 30.0, solved));
    EXPECT_NEAR(solved, 100.0f, 0.001f);

    // A key between two constant segments has no speed to solve for.
    std::vector<CE::CurveKey> allConstant = {constantKey(0, 0.0f),
                                             constantKey(30, 100.0f),
                                             constantKey(60, 50.0f)};
    EXPECT_FALSE(CE::solveCurveKeyValueForSpeed(allConstant, 0, 100.0f, 30.0, solved));
}

TEST(CurveEditorStandaloneTest, SolveSpeedPreservesDirectionAndConstantNeighbours)
{
    std::vector<CE::CurveKey> keys = speedFixture();
    float solved = 0.0f;
    ASSERT_TRUE(CE::solveCurveKeyValueForSpeed(keys, 2, 200.0f, 30.0, solved));
    EXPECT_LT(solved, keys[1].value);

    // Left neighbour constant: only the right segment contributes.
    std::vector<CE::CurveKey> constantLeft = {constantKey(0, 0.0f),
                                              key(30, 100.0f), key(60, 50.0f)};
    ASSERT_TRUE(CE::solveCurveKeyValueForSpeed(constantLeft, 1, 40.0f, 30.0, solved));
    const std::vector<CE::CurveKey> applied = {constantLeft[0], key(30, solved),
                                               constantLeft[2]};
    EXPECT_NEAR(CE::curveSpeedAt(applied, 1, 30.0), 40.0f, 0.01f);
}

TEST(CurveEditorStandaloneTest, SolveSpeedHandlesDecimalFrameRates)
{
    std::vector<CE::CurveKey> keys = speedFixture();
    float solved = 0.0f;
    ASSERT_TRUE(CE::solveCurveKeyValueForSpeed(keys, 1, 80.0f, 23.976, solved));
    keys[1].value = solved;
    EXPECT_NEAR(CE::curveSpeedAt(keys, 1, 23.976), 80.0f, 0.05f);
    EXPECT_NEAR(solved, 125.1f, 0.2f);
}

TEST(CurveEditorStandaloneTest, SolveSpeedHandlesDecreasingNeighbours)
{
    std::vector<CE::CurveKey> keys = {key(0, 200.0f), key(30, 100.0f),
                                      key(60, 0.0f)};
    ASSERT_NEAR(CE::curveSpeedAt(keys, 1, 30.0), 100.0f, 0.001f);

    float solved = 0.0f;
    ASSERT_TRUE(CE::solveCurveKeyValueForSpeed(keys, 1, 200.0f, 30.0, solved));
    keys[1].value = solved;
    EXPECT_NEAR(CE::curveSpeedAt(keys, 1, 30.0), 200.0f, 0.05f);
    // The two branches (-100 and 300) are equidistant from the current 100;
    // the solver takes the first valid candidate.
    EXPECT_NEAR(solved, -100.0f, 0.5f);
}

TEST(CurveEditorStandaloneTest, SolveSpeedIsIdempotentForUnequalSpans)
{
    std::vector<CE::CurveKey> keys = {key(0, 0.0f), key(20, 100.0f),
                                      key(60, 50.0f)};
    const float currentSpeed = CE::curveSpeedAt(keys, 1, 30.0);
    ASSERT_NEAR(currentSpeed, 93.75f, 0.001f);

    float solved = 0.0f;
    ASSERT_TRUE(CE::solveCurveKeyValueForSpeed(keys, 1, currentSpeed, 30.0,
                                               solved));
    EXPECT_NEAR(solved, 100.0f, 0.001f);
}

TEST(CurveEditorStandaloneTest, SpeedSolverRoundTripsOnRandomCurves)
{
    std::mt19937 rng(0x5EEDu);
    std::uniform_int_distribution<int> countDist(2, 5);
    std::uniform_int_distribution<int> frameStep(1, 40);
    std::uniform_int_distribution<int> valueDist(-200, 200);
    std::uniform_int_distribution<int> constantChance(0, 3);
    std::uniform_real_distribution<double> fpsDist(12.0, 60.0);
    std::uniform_real_distribution<float> targetDist(0.0f, 400.0f);

    for (int caseIndex = 0; caseIndex < 300; ++caseIndex) {
        const int count = countDist(rng);
        std::vector<CE::CurveKey> keys;
        std::int64_t frame = 0;
        for (int index = 0; index < count; ++index) {
            frame += frameStep(rng);
            CE::CurveKey generated = key(frame, static_cast<float>(valueDist(rng)));
            generated.constant = constantChance(rng) == 0;
            keys.push_back(generated);
        }
        const double fps = fpsDist(rng);
        const int index = std::uniform_int_distribution<int>(0, count - 1)(rng);
        const float target = targetDist(rng);

        float solved = 0.0f;
        if (CE::solveCurveKeyValueForSpeed(keys, index, target, fps, solved)) {
            EXPECT_TRUE(std::isfinite(solved)) << "case " << caseIndex;
            std::vector<CE::CurveKey> applied = keys;
            applied[static_cast<size_t>(index)].value = solved;
            const float achieved = CE::curveSpeedAt(applied, index, fps);
            const float tolerance = std::max(0.5f, target * 0.01f);
            EXPECT_NEAR(achieved, target, tolerance) << "case " << caseIndex;
        }

        const float speed = CE::curveSpeedAt(keys, index, fps);
        EXPECT_TRUE(std::isfinite(speed)) << "case " << caseIndex;
        EXPECT_GE(speed, 0.0f) << "case " << caseIndex;
    }
}

// ===========================================================================
// Hit testing and view mapping
// ===========================================================================

TEST(CurveEditorStandaloneTest, ZeroLengthHandlePressMovesTheKeyNotTheTangent)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    // primaryTracks() keys have zero-length handles, i.e. the in/out handle
    // pins sit exactly on the key. Pressing an already-selected key must move
    // it rather than start MoveHandleIn.
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    drag(widget, keyPoint(widget, 0, 1), widget.visualPoint(0, 40.0f, 80.0f));

    ASSERT_EQ(widget.tracks()[0].keys.size(), 3u);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 40);
    EXPECT_NEAR(widget.tracks()[0].keys[1].value, 80.0f, 0.5f);
    EXPECT_EQ(widget.tracks()[0].keys[1].inHandleFrame, 0);
    EXPECT_EQ(widget.tracks()[0].keys[1].outHandleFrame, 0);
    EXPECT_NEAR(widget.tracks()[0].keys[1].inHandleValue, 0.0f, 0.001f);
    EXPECT_FALSE(widget.tracks()[0].keys[1].brokenTangents);
}

TEST(CurveEditorStandaloneTest, DistinctHandlePressStillEditsTheTangent)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    std::vector<CE::CurveTrack> tracks = primaryTracks();
    tracks[0].keys[1].inHandleFrame = -8;
    tracks[0].keys[1].outHandleFrame = 8;
    widget.setTracks(tracks);
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    const QPointF handle = widget.visualPoint(0, 22.0f, 100.0f);
    drag(widget, handle, widget.visualPoint(0, 18.0f, 90.0f));

    const CE::CurveKey& edited = widget.tracks()[0].keys[1];
    EXPECT_EQ(edited.frame, 30);
    EXPECT_NEAR(edited.value, 100.0f, 0.01f);
    EXPECT_TRUE(edited.brokenTangents);
    EXPECT_LE(edited.inHandleValue, -0.01f);
}

TEST(CurveEditorStandaloneTest, NormalizedViewKeepsKeysClickableAndSelectable)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setNormalizedView(true);
    ASSERT_TRUE(widget.visualNormalizedView());

    clickKey(widget, 0, 1);
    EXPECT_TRUE(keySelected(widget, 0, 1));
    EXPECT_EQ(widget.visualSelectedTrack(), 0);
    EXPECT_EQ(widget.visualSelectedKey(), 1);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 1);

    pressKey(widget, Qt::Key_Escape);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 0);

    // Marquee across the normalized range selects both tracks' keys.
    drag(widget, rawPoint(widget, -5.0f, 1.15f), rawPoint(widget, 65.0f, -1.15f),
         Qt::ShiftModifier);
    for (int trackIndex = 0; trackIndex < 2; ++trackIndex) {
        for (int keyIndex = 0;
             keyIndex < static_cast<int>(widget.tracks()[trackIndex].keys.size());
             ++keyIndex) {
            EXPECT_TRUE(keySelected(widget, trackIndex, keyIndex))
                << "track " << trackIndex << " key " << keyIndex;
        }
    }
}

TEST(CurveEditorStandaloneTest, NormalizedDragMapsDisplayUnitsBackToValues)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks({track(QStringLiteral("Opacity"), QColor(255, 128, 64),
                            {key(0, 0.0f), key(30, 100.0f), key(60, 50.0f)})});
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setNormalizedView(true);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    // Display 1.0 is the track maximum (100); dragging to display 0.5 must
    // land halfway, at 75.
    drag(widget, keyPoint(widget, 0, 1), rawPoint(widget, 30.0f, 0.5f));

    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 30);
    EXPECT_NEAR(widget.tracks()[0].keys[1].value, 75.0f, 1.0f);
}

// ===========================================================================
// Derived (Speed) graph editing rules — CE-14
// ===========================================================================

TEST(CurveEditorStandaloneTest, DerivedGraphModeAllowsKeyMovesOnly)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setDerivedGraphMode(true);
    widget.setKeyEditingEnabled(true);
    widget.setHandleEditingEnabled(true);
    ASSERT_TRUE(widget.derivedGraphMode());
    ASSERT_TRUE(widget.visualKeyEditingEnabled());

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));
    const std::size_t originalCount = widget.tracks()[0].keys.size();

    // Copy / paste / clone / delete are all refused.
    pressKey(widget, Qt::Key_C);
    pressKey(widget, Qt::Key_V);
    EXPECT_EQ(widget.tracks()[0].keys.size(), originalCount);
    pressKey(widget, Qt::Key_D, Qt::ShiftModifier);
    EXPECT_EQ(widget.tracks()[0].keys.size(), originalCount);
    pressKey(widget, Qt::Key_Delete);
    EXPECT_EQ(widget.tracks()[0].keys.size(), originalCount);
    pressKey(widget, Qt::Key_Backspace);
    EXPECT_EQ(widget.tracks()[0].keys.size(), originalCount);
    EXPECT_TRUE(keySelected(widget, 0, 1));

    // Tangent edits and numeric value entry are refused too.
    EXPECT_FALSE(widget.setSelectedKeyAutoTangents());
    EXPECT_FALSE(widget.setSelectedKeyFlatTangents());
    EXPECT_FALSE(widget.setSelectedKeyLinearTangents());
    EXPECT_FALSE(widget.setSelectedKeyBrokenTangents());
    EXPECT_FALSE(widget.setSelectedKeyUnifiedTangents());
    EXPECT_FALSE(widget.setSelectedKeyConstant());
    EXPECT_FALSE(widget.setSelectedKeyBezier());
    EXPECT_FALSE(widget.promptSetSelectedKeyValue());

    // Ctrl+Click cannot insert an independent key into a derived graph.
    sendMouse(widget, QEvent::MouseButtonPress, rawPoint(widget, 45.0f, 25.0f),
              Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, rawPoint(widget, 45.0f, 25.0f),
              Qt::LeftButton, Qt::NoButton, Qt::ControlModifier);
    EXPECT_EQ(widget.tracks()[0].keys.size(), originalCount);

    // A key drag still works: that is the CE-14 retime/retarget path.
    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));
    drag(widget, keyPoint(widget, 0, 1), widget.visualPoint(0, 40.0f, 75.0f));
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 40);
    EXPECT_NEAR(widget.tracks()[0].keys[1].value, 75.0f, 0.75f);
    EXPECT_FALSE(widget.tracks()[0].keys[1].brokenTangents);
}

TEST(CurveEditorStandaloneTest, DerivedGraphModeClampsSpeedToZero)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    setupDerivedSpeedGraph(widget);
    ASSERT_TRUE(keySelected(widget, 0, 1));
    ASSERT_NEAR(widget.tracks()[0].keys[1].value, 200.0f, 0.01f);

    drag(widget, keyPoint(widget, 0, 1), widget.visualPoint(0, 30.0f, -1000.0f));

    // Speed has no negative range; the drag clamps instead of producing a
    // value the write-back solver would reject.
    EXPECT_FLOAT_EQ(widget.tracks()[0].keys[1].value, 0.0f);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 30);
}

TEST(CurveEditorStandaloneTest, DerivedGraphModeCanBeTurnedOff)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setDerivedGraphMode(true);
    widget.setKeyEditingEnabled(true);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));
    EXPECT_FALSE(widget.setSelectedKeyFlatTangents());
    EXPECT_EQ(widget.tracks()[0].keys.size(), 3u);

    widget.setDerivedGraphMode(false);
    EXPECT_FALSE(widget.derivedGraphMode());
    EXPECT_TRUE(widget.setSelectedKeyFlatTangents());

    const QPointF insert = rawPoint(widget, 45.0f, 25.0f);
    sendMouse(widget, QEvent::MouseButtonPress, insert, Qt::LeftButton,
              Qt::LeftButton, Qt::ControlModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, insert, Qt::LeftButton,
              Qt::NoButton, Qt::ControlModifier);
    EXPECT_EQ(widget.tracks()[0].keys.size(), 4u);
}

TEST(CurveEditorStandaloneTest, ReadOnlyModeSuppressesEveryTangentApi)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    widget.setKeyEditingEnabled(false);
    EXPECT_FALSE(widget.visualKeyEditingEnabled());
    EXPECT_FALSE(widget.setSelectedKeyAutoTangents());
    EXPECT_FALSE(widget.setSelectedKeyFlatTangents());
    EXPECT_FALSE(widget.setSelectedKeyLinearTangents());
    EXPECT_FALSE(widget.setSelectedKeyBrokenTangents());
    EXPECT_FALSE(widget.setSelectedKeyUnifiedTangents());
    EXPECT_FALSE(widget.setSelectedKeyConstant());
    EXPECT_FALSE(widget.setSelectedKeyBezier());
    EXPECT_FALSE(widget.promptSetSelectedKeyValue());

    widget.setKeyEditingEnabled(true);
    EXPECT_TRUE(widget.setSelectedKeyFlatTangents());
}

// ===========================================================================
// Selection editing: copy/paste, clone, delete, snap, reorder
// ===========================================================================

TEST(CurveEditorStandaloneTest, CopyPasteAcrossTracksAnchorsOnTheSelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    widget.focusTrack(0);
    pressKey(widget, Qt::Key_A);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 3);
    pressKey(widget, Qt::Key_C);

    widget.focusTrack(1);
    clickKey(widget, 1, 0);
    ASSERT_EQ(widget.visualSelectedTrack(), 1);
    ASSERT_EQ(widget.visualSelectedKey(), 0);
    pressKey(widget, Qt::Key_V);

    const auto& keys = widget.tracks()[1].keys;
    ASSERT_EQ(keys.size(), 5u);
    EXPECT_EQ(keys[0].frame, 0);
    EXPECT_EQ(keys[1].frame, 20);
    EXPECT_EQ(keys[2].frame, 30);
    EXPECT_EQ(keys[3].frame, 40);
    EXPECT_EQ(keys[4].frame, 60);
    EXPECT_NEAR(keys[2].value, 100.0f, 0.001f);
    EXPECT_NEAR(keys[4].value, 50.0f, 0.001f);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 3);

    // With no selection, paste falls back to the source track and anchors on
    // the playhead (45 here): 45/75/105 are added to the source track.
    pressKey(widget, Qt::Key_Escape);
    widget.setCurrentFrame(45);
    pressKey(widget, Qt::Key_V);
    ASSERT_EQ(widget.tracks()[1].keys.size(), 5u);
    const auto& sourceKeys = widget.tracks()[0].keys;
    ASSERT_EQ(sourceKeys.size(), 6u);
    EXPECT_EQ(sourceKeys[2].frame, 45);
    EXPECT_EQ(sourceKeys[4].frame, 75);
    EXPECT_EQ(sourceKeys[5].frame, 105);
    EXPECT_EQ(widget.visualSelectedTrack(), 0);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 3);
}

TEST(CurveEditorStandaloneTest, PasteWithoutSelectionAnchorsOnThePlayhead)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);  // frame 30, value 100
    pressKey(widget, Qt::Key_C);
    pressKey(widget, Qt::Key_Escape);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 0);

    widget.setCurrentFrame(45);
    pressKey(widget, Qt::Key_V);

    const auto& keys = widget.tracks()[0].keys;
    ASSERT_EQ(keys.size(), 4u);
    EXPECT_EQ(keys[0].frame, 0);
    EXPECT_EQ(keys[1].frame, 30);
    EXPECT_EQ(keys[2].frame, 45);
    EXPECT_EQ(keys[3].frame, 60);
    EXPECT_NEAR(keys[2].value, 100.0f, 0.001f);
    EXPECT_EQ(widget.visualSelectedTrack(), 0);
    EXPECT_EQ(widget.visualSelectedKey(), 2);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 1);
}

TEST(CurveEditorStandaloneTest, PasteOnTheSameTrackAnchorsOnTheSelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    // Copy key 1 (frame 30, value 100), then paste with key 0 (frame 0)
    // selected: the source start lands on the anchor and replaces that key.
    clickKey(widget, 0, 1);
    ASSERT_EQ(widget.visualSelectedKey(), 1);
    pressKey(widget, Qt::Key_C);
    clickKey(widget, 0, 0);
    ASSERT_EQ(widget.visualSelectedKey(), 0);
    pressKey(widget, Qt::Key_V);

    const auto& keys = widget.tracks()[0].keys;
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0].frame, 0);
    EXPECT_NEAR(keys[0].value, 100.0f, 0.001f);
    EXPECT_EQ(widget.visualSelectedKey(), 0);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 1);
}

TEST(CurveEditorStandaloneTest, CloneSelectionOffsetsByTheSelectedSpan)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.focusTrack(0);
    widget.setViewRange(0.0f, 130.0f, -20.0f, 120.0f);

    pressKey(widget, Qt::Key_A);
    pressKey(widget, Qt::Key_D, Qt::ShiftModifier);

    const auto& keys = widget.tracks()[0].keys;
    ASSERT_EQ(keys.size(), 6u);
    EXPECT_EQ(keys[3].frame, 61);
    EXPECT_EQ(keys[4].frame, 91);
    EXPECT_EQ(keys[5].frame, 121);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 3);
    EXPECT_TRUE(keySelected(widget, 0, 3));
}

TEST(CurveEditorStandaloneTest, BackspaceDeletesEveryMarqueeSelectedKey)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(-20.0f, 80.0f, -20.0f, 120.0f);

    drag(widget, rawPoint(widget, -10.0f, 110.0f),
         rawPoint(widget, 70.0f, -10.0f), Qt::ShiftModifier);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 6);

    pressKey(widget, Qt::Key_Backspace);
    EXPECT_TRUE(widget.tracks()[0].keys.empty());
    EXPECT_TRUE(widget.tracks()[1].keys.empty());
    EXPECT_EQ(widget.visualSelectedTrack(), -1);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 0);
}

TEST(CurveEditorStandaloneTest, XKeyDeletesEverySelectedKey)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(-20.0f, 80.0f, -20.0f, 120.0f);

    drag(widget, rawPoint(widget, -10.0f, 110.0f),
         rawPoint(widget, 70.0f, -10.0f), Qt::ShiftModifier);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 6);

    pressKey(widget, Qt::Key_X);
    EXPECT_TRUE(widget.tracks()[0].keys.empty());
    EXPECT_TRUE(widget.tracks()[1].keys.empty());
    EXPECT_EQ(widget.visualSelectedKeyCount(), 0);
}

TEST(CurveEditorStandaloneTest, CtrlDragSnapsTheValueToWholeUnits)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));
    dragWithMoveModifiers(widget, keyPoint(widget, 0, 1),
                          widget.visualPoint(0, 39.0f, 76.6f),
                          Qt::ControlModifier);

    const CE::CurveKey& moved = widget.tracks()[0].keys[1];
    EXPECT_EQ(moved.frame, 39);
    // Ctrl snaps the value delta to whole units. The press point is stored as
    // an integer pixel, so the snapped result can be one unit away from the
    // exact pointer value (76.6 -> 76 or 77).
    EXPECT_NEAR(moved.value, std::round(moved.value), 0.001f);
    EXPECT_NEAR(moved.value, 76.6f, 1.5f);
}

TEST(CurveEditorStandaloneTest, SelectionIsRebuiltWhenADragReordersKeys)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 100.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));
    drag(widget, keyPoint(widget, 0, 1), widget.visualPoint(0, 90.0f, 100.0f));

    const auto& keys = widget.tracks()[0].keys;
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0].frame, 0);
    EXPECT_EQ(keys[1].frame, 60);
    EXPECT_EQ(keys[2].frame, 90);
    EXPECT_EQ(widget.visualSelectedKey(), 2);
    EXPECT_TRUE(keySelected(widget, 0, 2));
    EXPECT_EQ(widget.visualSelectedKeyCount(), 1);
}

TEST(CurveEditorStandaloneTest, NumericEditorsMoveTheSelectedKey)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    setupSelectedKey(widget);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    // The inline editors are positioned and shown from paintEvent().
    (void)renderWidget(widget);
    QSpinBox* frameEditor = frameEditorOf(widget);
    QDoubleSpinBox* valueEditor = valueEditorOf(widget);
    ASSERT_NE(frameEditor, nullptr);
    ASSERT_NE(valueEditor, nullptr);

    frameEditor->setValue(45);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 45);
    EXPECT_EQ(widget.visualSelectedKey(), 1);

    valueEditor->setValue(80.0);
    EXPECT_NEAR(widget.tracks()[0].keys[1].value, 80.0f, 0.001f);
    EXPECT_EQ(widget.tracks()[0].keys[1].frame, 45);

    // A derived graph hides the numeric editors: its values are computed.
    CE::ArtifactCurveEditorWidget derived;
    derived.setAttribute(Qt::WA_DontShowOnScreen, true);
    derived.resize(kWidth, kHeight);
    setupDerivedSpeedGraph(derived);
    (void)renderWidget(derived);
    if (QSpinBox* derivedFrameEditor = frameEditorOf(derived)) {
        EXPECT_TRUE(derivedFrameEditor->isHidden());
    }
    if (QDoubleSpinBox* derivedValueEditor = valueEditorOf(derived)) {
        EXPECT_TRUE(derivedValueEditor->isHidden());
    }
}

// ===========================================================================
// View navigation and display toggles
// ===========================================================================

TEST(CurveEditorStandaloneTest, DoubleClickFocusesATrackAndBackgroundResets)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    doubleClick(widget, keyPoint(widget, 0, 1));
    EXPECT_EQ(widget.visualSelectedTrack(), 0);
    EXPECT_TRUE(widget.tracks()[0].visible);
    EXPECT_FALSE(widget.tracks()[1].visible);

    doubleClick(widget, rawPoint(widget, 55.0f, 5.0f));
    EXPECT_EQ(widget.visualSelectedTrack(), -1);
    EXPECT_TRUE(widget.tracks()[0].visible);
    EXPECT_TRUE(widget.tracks()[1].visible);
}

TEST(CurveEditorStandaloneTest, WheelZoomAxesAreIndependent)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    const QPointF anchor = rawPoint(widget, 30.0f, 50.0f);
    wheel(widget, anchor, 120, Qt::AltModifier);
    QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.width(), 60.0, 0.01);
    EXPECT_NEAR(view.height(), 126.0, 0.01);

    wheel(widget, anchor, 120, Qt::NoModifier);
    view = widget.visualViewRange();
    EXPECT_NEAR(view.width(), 54.0, 0.01);
    EXPECT_NEAR(view.height(), 113.4, 0.01);
}

TEST(CurveEditorStandaloneTest, PanDragChangesTheViewAndLeavesKeysAlone)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    const std::vector<CE::CurveKey> before = widget.tracks()[0].keys;
    drag(widget, rawPoint(widget, 40.0f, 100.0f), rawPoint(widget, 25.0f, 85.0f));

    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), 15.0, 0.5);
    EXPECT_NEAR(view.top(), -5.0, 0.5);
    ASSERT_EQ(widget.tracks()[0].keys.size(), before.size());
    for (std::size_t index = 0; index < before.size(); ++index) {
        EXPECT_EQ(widget.tracks()[0].keys[index].frame, before[index].frame);
        EXPECT_FLOAT_EQ(widget.tracks()[0].keys[index].value, before[index].value);
    }
}

TEST(CurveEditorStandaloneTest, BufferCurveToggleCapturesThePreEditShape)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    setupTwoTrackView(widget);
    ASSERT_FALSE(widget.visualBufferCurveVisible());

    // The key-drag press captures the pre-edit shape and shows the buffer.
    drag(widget, keyPoint(widget, 0, 1), widget.visualPoint(0, 40.0f, 60.0f));
    EXPECT_TRUE(widget.visualBufferCurveVisible());
    const QImage withBuffer = renderWidget(widget);

    pressKey(widget, Qt::Key_B);
    EXPECT_FALSE(widget.visualBufferCurveVisible());
    const QImage withoutBuffer = renderWidget(widget);
    EXPECT_NE(withoutBuffer, withBuffer);

    int differingPixels = 0;
    const QRect plot = plotRectPixels();
    for (int y = plot.top(); y <= plot.bottom(); ++y) {
        for (int x = plot.left(); x <= plot.right(); ++x) {
            if (withBuffer.pixel(x, y) != withoutBuffer.pixel(x, y)) {
                ++differingPixels;
            }
        }
    }
    EXPECT_GT(differingPixels, 2000);

    pressKey(widget, Qt::Key_B);
    EXPECT_TRUE(widget.visualBufferCurveVisible());
}

TEST(CurveEditorStandaloneTest, NormalizedApiMatchesTheKeyboardToggle)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    widget.setNormalizedView(true);
    EXPECT_TRUE(widget.visualNormalizedView());
    EXPECT_NEAR(widget.visualViewRange().top(), -1.2, 0.001);
    EXPECT_NEAR(widget.visualViewRange().height(), 2.4, 0.001);

    widget.setNormalizedView(false);
    EXPECT_FALSE(widget.visualNormalizedView());
    EXPECT_NEAR(widget.visualViewRange().top(), -10.0, 0.01);
    EXPECT_NEAR(widget.visualViewRange().height(), 120.0, 0.01);
}

// ===========================================================================
// Event contract (the timeline's undo and refresh path)
// ===========================================================================

TEST(CurveEditorStandaloneTest, KeyDragPublishesInteractionAndMoveEvents)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    CurveEditorEvents events;
    std::vector<CE::EventBus::Subscription> subscriptions;
    watchCurveEditorEvents(events, subscriptions);

    drag(widget, keyPoint(widget, 0, 1), widget.visualPoint(0, 40.0f, 80.0f));

    EXPECT_EQ(events.started, 1);
    EXPECT_EQ(events.finished, 1);
    EXPECT_EQ(events.moved, 1);
    EXPECT_EQ(events.selected, 1);
    EXPECT_EQ(events.lastSelectedTrack, 0);
    EXPECT_EQ(events.lastSelectedKey, 1);
    EXPECT_EQ(events.deleted, 0);
}

TEST(CurveEditorStandaloneTest, TangentEditPublishesOneInteraction)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    CurveEditorEvents events;
    std::vector<CE::EventBus::Subscription> subscriptions;
    watchCurveEditorEvents(events, subscriptions);

    EXPECT_TRUE(widget.setSelectedKeyFlatTangents());
    EXPECT_EQ(events.started, 1);
    EXPECT_EQ(events.finished, 1);
    EXPECT_EQ(events.moved, 0);
}

TEST(CurveEditorStandaloneTest, RejectedEditsPublishNoInteractionEvents)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setDerivedGraphMode(true);
    widget.setKeyEditingEnabled(true);
    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    CurveEditorEvents events;
    std::vector<CE::EventBus::Subscription> subscriptions;
    watchCurveEditorEvents(events, subscriptions);

    pressKey(widget, Qt::Key_Delete);
    pressKey(widget, Qt::Key_Backspace);
    pressKey(widget, Qt::Key_X);
    EXPECT_FALSE(widget.setSelectedKeyFlatTangents());
    EXPECT_FALSE(widget.setSelectedKeyAutoTangents());
    EXPECT_FALSE(widget.promptSetSelectedKeyValue());

    EXPECT_EQ(events.started, 0);
    EXPECT_EQ(events.finished, 0);
    EXPECT_EQ(events.moved, 0);
    EXPECT_EQ(events.deleted, 0);
}

TEST(CurveEditorStandaloneTest, SelectAllOnlySelectsVisibleTracks)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    widget.focusTrack(0);  // hides the Rotation track
    pressKey(widget, Qt::Key_A);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 3);
    EXPECT_TRUE(keySelected(widget, 0, 2));
    EXPECT_FALSE(keySelected(widget, 1, 0));
}

TEST(CurveEditorStandaloneTest, FitToContentIgnoresHiddenTracks)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    widget.focusTrack(1);  // Rotation: frames 0..40, values 20..80
    widget.fitToContent();
    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), -5.0, 0.01);   // 0 - max(4, 5)
    EXPECT_NEAR(view.right(), 45.0, 0.01);  // 40 + 5
    EXPECT_LT(view.top(), 20.0);
    EXPECT_GT(view.bottom(), 80.0);
}

TEST(CurveEditorStandaloneTest, MarqueeSkipsHiddenTracks)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(-20.0f, 80.0f, -20.0f, 120.0f);

    widget.focusTrack(0);
    drag(widget, rawPoint(widget, -10.0f, 110.0f),
         rawPoint(widget, 70.0f, -10.0f), Qt::ShiftModifier);

    EXPECT_EQ(widget.visualSelectedKeyCount(), 3);
    EXPECT_FALSE(keySelected(widget, 1, 0));
}

TEST(CurveEditorStandaloneTest, InfinityExtensionStaysFlatForConstantEdges)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);

    // Key 0 is constant, so the pre-extension left of frame 0 must be a flat
    // dashed line at value 0 instead of the (50-0)/30 slope of the jump.
    std::vector<CE::CurveTrack> tracks = {
        track(QStringLiteral("Step"), QColor(255, 128, 64),
              {constantKey(0, 0.0f), key(30, 50.0f), key(60, 100.0f)})};
    widget.setTracks(tracks);
    widget.setViewRange(-40.0f, 80.0f, -20.0f, 120.0f);

    const QImage image = renderWidget(widget);
    const QRect plot = plotRectPixels();
    const int zeroY = static_cast<int>(
        kMarginTop + (1.0f - (0.0f + 20.0f) / 140.0f) *
                         (kHeight - kMarginTop - kMarginBottom));
    // The infinity preview is the track colour at alpha 115 over the plot
    // background (30,30,30).
    const QColor infinityStroke =
        blendOver(QColor(255, 128, 64), 115, QColor(30, 30, 30));
    const QRect preExtension(plot.left() + 2, zeroY - 2,
                             static_cast<int>(plot.width() * 0.25f), 5);
    EXPECT_GT(countNear(image, preExtension, infinityStroke, 25), 20);

    // And nothing of the sloped variant (which would sit well below zero).
    const QRect slopedRegion(plot.left() + 2, zeroY + 12,
                             static_cast<int>(plot.width() * 0.25f), 10);
    EXPECT_LT(countNear(image, slopedRegion, infinityStroke, 25), 5);
}

TEST(CurveEditorStandaloneTest, PlayheadScrubPublishesFrameChanges)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    CurveEditorEvents events;
    std::vector<CE::EventBus::Subscription> subscriptions;
    watchCurveEditorEvents(events, subscriptions);

    const int scrubY = plotRectPixels().bottom() + 10;
    const qreal x25 = rawPoint(widget, 25.0f, 0.0f).x();
    sendMouse(widget, QEvent::MouseButtonPress, QPointF(x25, scrubY),
              Qt::LeftButton, Qt::LeftButton);
    EXPECT_EQ(events.started, 1);
    EXPECT_EQ(events.frameChanged, 1);
    EXPECT_EQ(events.lastFrame, 25);

    const qreal x35 = rawPoint(widget, 35.0f, 0.0f).x();
    sendMouse(widget, QEvent::MouseMove, QPointF(x35, scrubY), Qt::NoButton,
              Qt::LeftButton);
    EXPECT_EQ(events.frameChanged, 2);
    EXPECT_EQ(events.lastFrame, 35);

    sendMouse(widget, QEvent::MouseButtonRelease, QPointF(x35, scrubY),
              Qt::LeftButton, Qt::NoButton);
    EXPECT_EQ(events.finished, 1);
}

// ===========================================================================
// Interaction -> capture -> pixel result
// ===========================================================================

TEST(CurveEditorStandaloneTest, ClickPaintsTheSelectionHighlightOnTheClickedKey)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    const QPointF middle = keyPoint(widget, 0, 1);
    const QPointF first = keyPoint(widget, 0, 0);
    const QImage before = renderWidget(widget);
    EXPECT_LE(countColorInBox(before, middle, 8, selectedKeyFill(), 30), 2);

    clickKey(widget, 0, 1);
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("click_select"), before, after);

    EXPECT_GT(countColorInBox(after, middle, 8, selectedKeyFill(), 30), 10);
    EXPECT_LE(countColorInBox(after, first, 8, selectedKeyFill(), 30), 2);
}

TEST(CurveEditorStandaloneTest, KeyDragRepaintsTheDiamondAtTheNewPosition)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    const QPointF oldPoint = keyPoint(widget, 0, 1);
    const QImage before = renderWidget(widget);

    drag(widget, oldPoint, widget.visualPoint(0, 40.0f, 80.0f));
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("drag_key"), before, after);

    // The diamond must be gone from the old frame/value and present at the new
    // one. The pre-edit buffer curve is visible there, but its blended colour
    // is not the selected-key fill.
    EXPECT_LE(countColorInBox(after, oldPoint, 6, selectedKeyFill(), 20), 2);
    const QPointF newPoint = widget.visualPoint(0, 40.0f, 80.0f);
    EXPECT_GT(countColorInBox(after, newPoint, 8, selectedKeyFill(), 30), 10);
}

TEST(CurveEditorStandaloneTest, DeleteRemovesThePaintedKey)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 2);
    const QPointF point = keyPoint(widget, 0, 2);
    const QImage before = renderWidget(widget);
    EXPECT_GT(countColorInBox(before, point, 8, selectedKeyFill(), 30), 5);

    pressKey(widget, Qt::Key_Delete);
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("delete_key"), before, after);

    ASSERT_EQ(widget.tracks()[0].keys.size(), 2u);
    EXPECT_LE(countColorInBox(after, point, 8, selectedKeyFill(), 30), 2);
    EXPECT_LE(countColorInBox(after, point, 8,
                              keyFillFor(QColor(255, 128, 64)), 15), 2);
}

TEST(CurveEditorStandaloneTest, InsertPaintsANewDiamondAtTheClickPoint)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks({primaryTracks()[0]});
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    const QPointF insert = rawPoint(widget, 45.0f, 25.0f);
    const QImage before = renderWidget(widget);

    sendMouse(widget, QEvent::MouseButtonPress, insert, Qt::LeftButton,
              Qt::LeftButton, Qt::ControlModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, insert, Qt::LeftButton,
              Qt::NoButton, Qt::ControlModifier);
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("insert_key"), before, after);

    ASSERT_EQ(widget.tracks()[0].keys.size(), 4u);
    const QPointF inserted = keyPoint(widget, 0, 2);
    EXPECT_GT(countColorInBox(after, inserted, 8, selectedKeyFill(), 30), 10);
    EXPECT_LE(countColorInBox(before, inserted, 8, selectedKeyFill(), 30), 2);
}

TEST(CurveEditorStandaloneTest, PlayheadScrubRepaintsAtTheNewFrame)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    setupTwoTrackView(widget);
    const QImage before = renderWidget(widget);

    const int scrubY = plotRectPixels().bottom() + 10;
    const qreal x12 = rawPoint(widget, 12.0f, 0.0f).x();
    const qreal x30 = rawPoint(widget, 30.0f, 0.0f).x();
    sendMouse(widget, QEvent::MouseButtonPress, QPointF(x12, scrubY),
              Qt::LeftButton, Qt::LeftButton);
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("scrub_playhead"), before, after);

    const QRect plot = plotRectPixels();
    const QRect newColumn(static_cast<int>(x12) - 3, plot.top(), 7, plot.height());
    const QRect oldColumn(static_cast<int>(x30) - 3, plot.top(), 7, plot.height());
    // Tight tolerance: the orange track colour is within 60 of the playhead
    // red but not within 20.
    EXPECT_GT(countNear(after, newColumn, kPlayheadColor, 20), 20);
    EXPECT_LE(countNear(after, oldColumn, kPlayheadColor, 20), 5);

    sendMouse(widget, QEvent::MouseButtonRelease, QPointF(x12, scrubY),
              Qt::LeftButton, Qt::NoButton);
}

TEST(CurveEditorStandaloneTest, NormalizedToggleRepaintsKeyPositions)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    const QImage absolute = renderWidget(widget);
    const QPointF absolutePoint = keyPoint(widget, 0, 1);
    EXPECT_GT(countColorInBox(absolute, absolutePoint, 8,
                              keyFillFor(QColor(255, 128, 64)), 15), 5);

    widget.setNormalizedView(true);
    const QImage normalized = renderWidget(widget);
    writeCapturePair(QStringLiteral("normalized_toggle"), absolute, normalized);

    const QPointF normalizedPoint = keyPoint(widget, 0, 1);
    EXPECT_GT(std::abs(normalizedPoint.y() - absolutePoint.y()), 10.0);
    EXPECT_GT(countColorInBox(normalized, normalizedPoint, 8,
                              keyFillFor(QColor(255, 128, 64)), 15), 5);
    EXPECT_LE(countColorInBox(normalized, absolutePoint, 8,
                              keyFillFor(QColor(255, 128, 64)), 15), 2);
}

TEST(CurveEditorStandaloneTest, DerivedModeStopsPaintingTangentHandles)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    setupSelectedKey(widget);
    ASSERT_TRUE(keySelected(widget, 0, 1));
    // Move the playhead off the key: it is painted over the diamond and would
    // hide the fill pixels this contract looks for.
    widget.setCurrentFrame(0);

    const QPointF handle = widget.visualPoint(0, 22.0f, 106.0f);
    const QImage valueGraph = renderWidget(widget);
    EXPECT_GT(countColorInBox(valueGraph, handle, 6, QColor(255, 230, 120), 25), 3);

    widget.setDerivedGraphMode(true);
    const QImage derivedGraph = renderWidget(widget);
    writeCapturePair(QStringLiteral("derived_handles"), valueGraph, derivedGraph);

    EXPECT_LE(countColorInBox(derivedGraph, handle, 6, QColor(255, 230, 120), 25), 1);
    // The key itself stays visible and selected.
    EXPECT_GT(countColorInBox(derivedGraph, keyPoint(widget, 0, 1), 8,
                              selectedKeyFill(), 30), 10);
}

TEST(CurveEditorStandaloneTest, BufferCurveIsPaintedAtThePreEditPosition)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    setupTwoTrackView(widget);
    // Sample the pre-edit curve at frame 20 (value 66.7 on the straight
    // 0 -> 100 segment). That point is above every main-curve area fill, so
    // the buffer stroke is composited over the bare plot; the old key position
    // itself sits under the playhead at frame 30.
    const QPointF bufferPoint = widget.visualPoint(0, 20.0f, 200.0f / 3.0f);
    const QPointF oldPoint = keyPoint(widget, 0, 1);

    // A key drag captures the buffer and shows it.
    drag(widget, oldPoint, widget.visualPoint(0, 40.0f, 80.0f));
    const QImage withBuffer = renderWidget(widget);
    pressKey(widget, Qt::Key_B);
    const QImage withoutBuffer = renderWidget(widget);
    writeCapturePair(QStringLiteral("buffer_toggle"), withoutBuffer, withBuffer);

    const QColor bufferStroke =
        blendOver(QColor(255, 128, 64).lighter(130), 90, kPlotBackground);
    EXPECT_GT(countColorInBox(withBuffer, bufferPoint, 3, bufferStroke, 20), 5);
    EXPECT_LE(countColorInBox(withoutBuffer, bufferPoint, 3, bufferStroke, 20), 1);
}

TEST(CurveEditorStandaloneTest, MarqueeIsPaintedOnlyWhileDragging)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(-20.0f, 80.0f, -20.0f, 120.0f);

    const QPointF start = rawPoint(widget, -10.0f, 110.0f);
    const QPointF end = rawPoint(widget, 50.0f, 20.0f);
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton, Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton,
              Qt::ShiftModifier);
    const QImage during = renderWidget(widget);

    sendMouse(widget, QEvent::MouseButtonRelease, end, Qt::LeftButton,
              Qt::NoButton, Qt::ShiftModifier);
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("marquee_lifetime"), during, after);

    // Use the marquee fill (alpha 40 over the plot background), not the dashed
    // border: the border colour is close to the anti-aliased blue curve, while
    // the fill covers a large unique area.
    const QColor marqueeFill = blendOver(QColor(120, 170, 255), 40, kPlotBackground);
    EXPECT_GT(countNear(during, plotRectPixels(), marqueeFill, 2), 5000);
    EXPECT_LE(countNear(after, plotRectPixels(), marqueeFill, 2), 10);
}

TEST(CurveEditorStandaloneTest, HandleDragMovesThePaintedHandlePin)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    setupSelectedKey(widget);
    widget.setCurrentFrame(0);

    const QPointF oldHandle = widget.visualPoint(0, 22.0f, 106.0f);
    const QPointF keyCenter = keyPoint(widget, 0, 1);
    const QImage before = renderWidget(widget);
    EXPECT_GT(countColorInBox(before, oldHandle, 6, QColor(255, 230, 120), 25), 3);

    drag(widget, oldHandle, widget.visualPoint(0, 18.0f, 90.0f));
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("handle_drag"), before, after);

    const CE::CurveKey& edited = widget.tracks()[0].keys[1];
    EXPECT_EQ(edited.frame, 30);
    EXPECT_NEAR(edited.value, 100.0f, 0.01f);
    // The pin is gone from the old point and painted at the new handle point.
    EXPECT_LE(countColorInBox(after, oldHandle, 6, QColor(255, 230, 120), 25), 1);
    EXPECT_GT(countColorInBox(after, widget.visualPoint(0, 18.0f, 90.0f), 6,
                              QColor(255, 230, 120), 25), 3);
    // The key itself stays selected and in place.
    EXPECT_GT(countColorInBox(after, keyCenter, 8, selectedKeyFill(), 30), 4);
}

TEST(CurveEditorStandaloneTest, DeleteMiddleKeyRecomputesThePaintedCurve)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(0);

    clickKey(widget, 0, 1);
    const QColor focusedStroke = QColor(255, 128, 64).lighter(130);
    const int column =
        static_cast<int>(std::lround(rawPoint(widget, 25.0f, 0.0f).x()));
    const QImage before = renderWidget(widget);
    const double rowBefore = averageStrokeRow(before, column, focusedStroke, 15);
    ASSERT_GT(rowBefore, 0.0);

    pressKey(widget, Qt::Key_Delete);
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("delete_middle_curve"), before, after);

    // Only 0 -> 60 remains, so frame 25 drops from ~83 to ~21 and the curve is
    // no longer the focused (brightened) colour.
    const QColor baseStroke(255, 128, 64);
    const double rowAfter = averageStrokeRow(after, column, baseStroke, 15);
    ASSERT_GT(rowAfter, 0.0);
    EXPECT_GT(rowAfter, rowBefore + 100.0);
}

TEST(CurveEditorStandaloneTest, InsertKeyRecomputesThePaintedCurve)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks({primaryTracks()[0]});
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(0);

    // Frame 42 sits in the new 30 -> 25 dip, away from the inserted key's
    // handle line and diamond.
    const int column =
        static_cast<int>(std::lround(rawPoint(widget, 42.0f, 0.0f).x()));
    const QColor baseStroke(255, 128, 64);
    const QImage before = renderWidget(widget);
    const double rowBefore = averageStrokeRow(before, column, baseStroke, 15);
    ASSERT_GT(rowBefore, 0.0);

    const QPointF insert = rawPoint(widget, 45.0f, 25.0f);
    sendMouse(widget, QEvent::MouseButtonPress, insert, Qt::LeftButton,
              Qt::LeftButton, Qt::ControlModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, insert, Qt::LeftButton,
              Qt::NoButton, Qt::ControlModifier);
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("insert_curve"), before, after);

    // The inserted 45/25 key becomes selected, so the curve is brightened and
    // frame 48 now sits around value 30 instead of 70.
    const QColor focusedStroke = QColor(255, 128, 64).lighter(130);
    const double rowAfter = averageStrokeRow(after, column, focusedStroke, 15);
    ASSERT_GT(rowAfter, 0.0);
    EXPECT_GT(rowAfter, rowBefore + 60.0);
}

TEST(CurveEditorStandaloneTest, FocusTrackHidesAndRestoresTheOtherCurve)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(0);

    const QColor rotationColor(96, 196, 255);
    const QRect plot = plotRectPixels();
    const QImage both = renderWidget(widget);
    EXPECT_GT(countNear(both, plot, rotationColor, 30), 100);

    widget.focusTrack(0);
    const QImage focused = renderWidget(widget);
    writeCapturePair(QStringLiteral("focus_track"), both, focused);
    EXPECT_LE(countNear(focused, plot, rotationColor, 30), 5);
    EXPECT_GT(countNear(focused, plot, QColor(255, 128, 64), 30), 50);

    widget.focusTrack(-1);
    const QImage restored = renderWidget(widget);
    EXPECT_GT(countNear(restored, plot, rotationColor, 30), 100);
}

TEST(CurveEditorStandaloneTest, MultiKeyDragRepaintsEverySelectedKey)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 80.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(5);

    clickKey(widget, 0, 1);
    const QPointF second = keyPoint(widget, 0, 2);
    sendMouse(widget, QEvent::MouseButtonPress, second, Qt::LeftButton,
              Qt::LeftButton, Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, second, Qt::LeftButton,
              Qt::NoButton, Qt::ShiftModifier);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 2);

    const QPointF old1 = keyPoint(widget, 0, 1);
    const QPointF old2 = keyPoint(widget, 0, 2);
    const QImage before = renderWidget(widget);
    EXPECT_GT(countColorInBox(before, old1, 8, selectedKeyFill(), 30), 5);
    EXPECT_GT(countColorInBox(before, old2, 8, selectedKeyFill(), 30), 5);

    // Both selected keys move by the same delta (frame +10, value -20).
    drag(widget, old1, widget.visualPoint(0, 40.0f, 80.0f));
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("multi_drag"), before, after);

    const QPointF new1 = widget.visualPoint(0, 40.0f, 80.0f);
    const QPointF new2 = widget.visualPoint(0, 70.0f, 30.0f);
    EXPECT_GT(countColorInBox(after, new1, 8, selectedKeyFill(), 30), 5);
    EXPECT_GT(countColorInBox(after, new2, 8, selectedKeyFill(), 30), 5);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 2);
}

TEST(CurveEditorStandaloneTest, SelectAllHighlightsEveryKeyAndEscapeClearsIt)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(5);

    const QImage before = renderWidget(widget);
    EXPECT_LE(countColorInBox(before, keyPoint(widget, 0, 1), 8,
                              selectedKeyFill(), 30), 2);

    pressKey(widget, Qt::Key_A);
    const QImage highlighted = renderWidget(widget);
    for (int trackIndex = 0; trackIndex < 2; ++trackIndex) {
        for (int keyIndex = 0;
             keyIndex < static_cast<int>(widget.tracks()[trackIndex].keys.size());
             ++keyIndex) {
            EXPECT_GT(countColorInBox(highlighted,
                                      keyPoint(widget, trackIndex, keyIndex), 8,
                                      selectedKeyFill(), 30), 4)
                << "track " << trackIndex << " key " << keyIndex;
        }
    }

    pressKey(widget, Qt::Key_Escape);
    const QImage cleared = renderWidget(widget);
    writeCapturePair(QStringLiteral("select_all"), before, highlighted);
    writeCapturePair(QStringLiteral("escape_clear"), highlighted, cleared);

    // Escape also refits the view, so re-read the key positions.
    for (int trackIndex = 0; trackIndex < 2; ++trackIndex) {
        for (int keyIndex = 0;
             keyIndex < static_cast<int>(widget.tracks()[trackIndex].keys.size());
             ++keyIndex) {
            EXPECT_LE(countColorInBox(cleared,
                                      keyPoint(widget, trackIndex, keyIndex), 8,
                                      selectedKeyFill(), 30), 2)
                << "track " << trackIndex << " key " << keyIndex;
        }
    }
}

TEST(CurveEditorStandaloneTest, PastePaintsSelectedKeysAtTheDestination)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    widget.focusTrack(0);
    pressKey(widget, Qt::Key_A);
    pressKey(widget, Qt::Key_C);
    widget.focusTrack(1);
    // focusTrack() fits only the existing 0..40 keys; widen the view so the
    // pasted frame-60 key is inside the plot, and keep the playhead away.
    widget.setViewRange(-5.0f, 70.0f, 0.0f, 110.0f);
    widget.setCurrentFrame(5);
    clickKey(widget, 1, 0);

    const QPointF pasted30 = widget.visualPoint(1, 30.0f, 100.0f);
    const QPointF pasted60 = widget.visualPoint(1, 60.0f, 50.0f);
    const QImage before = renderWidget(widget);
    EXPECT_LE(countColorInBox(before, pasted30, 8, selectedKeyFill(), 30), 2);
    EXPECT_LE(countColorInBox(before, pasted60, 8, selectedKeyFill(), 30), 2);

    pressKey(widget, Qt::Key_V);
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("paste_keys"), before, after);

    ASSERT_EQ(widget.visualSelectedKeyCount(), 3);
    EXPECT_GT(countColorInBox(after, pasted30, 8, selectedKeyFill(), 30), 5);
    EXPECT_GT(countColorInBox(after, pasted60, 8, selectedKeyFill(), 30), 5);
}

TEST(CurveEditorStandaloneTest, RandomizedEditSequenceKeepsInvariants)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setKeyEditingEnabled(true);
    widget.setHandleEditingEnabled(true);

    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> opDist(0, 15);
    std::uniform_int_distribution<int> frameDist(-20, 100);
    std::uniform_int_distribution<int> valueDist(-40, 140);
    std::uniform_int_distribution<int> deltaFrameDist(-30, 30);
    std::uniform_int_distribution<int> deltaValueDist(-60, 60);

    for (int step = 0; step < 300; ++step) {
        const int op = opDist(rng);
        int trackIndex = -1;
        int keyIndex = -1;
        const bool hasKey = [&]() {
            if (widget.tracks().empty()) return false;
            std::uniform_int_distribution<int> trackDist(
                0, static_cast<int>(widget.tracks().size()) - 1);
            trackIndex = trackDist(rng);
            if (widget.tracks()[trackIndex].keys.empty()) return false;
            std::uniform_int_distribution<int> keyDist(
                0, static_cast<int>(widget.tracks()[trackIndex].keys.size()) - 1);
            keyIndex = keyDist(rng);
            return true;
        }();

        switch (op) {
        case 0:
            if (hasKey) clickKey(widget, trackIndex, keyIndex);
            break;
        case 1:
            if (hasKey) {
                const QPointF point = keyPoint(widget, trackIndex, keyIndex);
                sendMouse(widget, QEvent::MouseButtonPress, point, Qt::LeftButton,
                          Qt::LeftButton, Qt::ShiftModifier);
                sendMouse(widget, QEvent::MouseButtonRelease, point,
                          Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
            }
            break;
        case 2:
            if (hasKey) {
                const CE::CurveKey& selectedKey =
                    widget.tracks()[trackIndex].keys[keyIndex];
                const float targetFrame = static_cast<float>(
                    selectedKey.frame + deltaFrameDist(rng));
                const float targetValue =
                    selectedKey.value + static_cast<float>(deltaValueDist(rng));
                drag(widget, keyPoint(widget, trackIndex, keyIndex),
                     widget.visualPoint(trackIndex, targetFrame, targetValue));
            }
            break;
        case 3: {
            const QPointF insert = rawPoint(
                widget, static_cast<float>(frameDist(rng)),
                static_cast<float>(valueDist(rng)));
            sendMouse(widget, QEvent::MouseButtonPress, insert, Qt::LeftButton,
                      Qt::LeftButton, Qt::ControlModifier);
            sendMouse(widget, QEvent::MouseButtonRelease, insert, Qt::LeftButton,
                      Qt::NoButton, Qt::ControlModifier);
            break;
        }
        case 4:
            pressKey(widget, Qt::Key_Delete);
            break;
        case 5:
            pressKey(widget, Qt::Key_Backspace);
            break;
        case 6:
            pressKey(widget, Qt::Key_X);
            break;
        case 7:
            pressKey(widget, Qt::Key_C);
            break;
        case 8:
            pressKey(widget, Qt::Key_V);
            break;
        case 9:
            pressKey(widget, Qt::Key_D, Qt::ShiftModifier);
            break;
        case 10:
            pressKey(widget, Qt::Key_A);
            break;
        case 11:
            pressKey(widget, Qt::Key_Escape);
            break;
        case 12:
            pressKey(widget, Qt::Key_N);
            break;
        case 13:
            pressKey(widget, Qt::Key_B);
            break;
        case 14:
            widget.setDerivedGraphMode(!widget.derivedGraphMode());
            break;
        case 15:
            wheel(widget, rawPoint(widget, 30.0f, 50.0f), 120,
                  Qt::ShiftModifier);
            break;
        default:
            break;
        }

        int totalKeys = 0;
        for (const auto& track : widget.tracks()) {
            totalKeys += static_cast<int>(track.keys.size());
            for (std::size_t index = 1; index < track.keys.size(); ++index) {
                EXPECT_LE(track.keys[index - 1].frame, track.keys[index].frame)
                    << "step " << step;
            }
        }
        EXPECT_LE(widget.visualSelectedKeyCount(), totalKeys) << "step " << step;

        const int primaryTrack = widget.visualSelectedTrack();
        const int primaryKey = widget.visualSelectedKey();
        if (primaryTrack >= 0) {
            ASSERT_LT(primaryTrack, static_cast<int>(widget.tracks().size()))
                << "step " << step;
            ASSERT_GE(primaryKey, 0) << "step " << step;
            ASSERT_LT(primaryKey,
                      static_cast<int>(widget.tracks()[primaryTrack].keys.size()))
                << "step " << step;
            EXPECT_TRUE(widget.visualKeySelected(primaryTrack, primaryKey))
                << "step " << step;
        } else {
            EXPECT_EQ(primaryKey, -1) << "step " << step;
        }
    }

    pressKey(widget, Qt::Key_A);
    pressKey(widget, Qt::Key_Escape);
    EXPECT_EQ(widget.visualSelectedKeyCount(), 0);
    EXPECT_EQ(widget.visualSelectedTrack(), -1);
}

TEST(CurveEditorStandaloneTest, FrameSelectionFitsTheSelectedKey)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 1);
    widget.setViewRange(0.0f, 500.0f, -1000.0f, 1000.0f);
    widget.frameSelection();

    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), 25.0, 0.01);   // frame 30 - 5
    EXPECT_NEAR(view.right(), 35.0, 0.01);  // frame 30 + 5
    EXPECT_NEAR(view.top(), 95.0, 0.01);    // value 100 - 5
    EXPECT_NEAR(view.bottom(), 105.0, 0.01);
}

TEST(CurveEditorStandaloneTest, FrameSelectionUsesEverySelectedKey)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 0);
    const QPointF last = keyPoint(widget, 0, 2);
    sendMouse(widget, QEvent::MouseButtonPress, last, Qt::LeftButton,
              Qt::LeftButton, Qt::ShiftModifier);
    sendMouse(widget, QEvent::MouseButtonRelease, last, Qt::LeftButton,
              Qt::NoButton, Qt::ShiftModifier);
    ASSERT_EQ(widget.visualSelectedKeyCount(), 2);

    widget.setViewRange(0.0f, 500.0f, -1000.0f, 1000.0f);
    widget.frameSelection();

    // Frames 0..60 (margin 6) and values 0..50 (margin 5).
    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), -6.0, 0.01);
    EXPECT_NEAR(view.right(), 66.0, 0.01);
    EXPECT_NEAR(view.top(), -5.0, 0.01);
    EXPECT_NEAR(view.bottom(), 55.0, 0.01);
}

TEST(CurveEditorStandaloneTest, FrameSelectionFallsBackToVisibleKeys)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());

    widget.setViewRange(0.0f, 500.0f, -1000.0f, 1000.0f);
    widget.frameSelection();

    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), -6.0, 0.01);   // frames 0..60
    EXPECT_NEAR(view.right(), 66.0, 0.01);
    EXPECT_NEAR(view.top(), -10.0, 0.01);   // values 0..100
    EXPECT_NEAR(view.bottom(), 110.0, 0.01);
}

TEST(CurveEditorStandaloneTest, FKeyFramesTheSelection)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    widget.setViewRange(0.0f, 500.0f, -1000.0f, 1000.0f);
    pressKey(widget, Qt::Key_F);

    const QRectF view = widget.visualViewRange();
    EXPECT_NEAR(view.left(), 25.0, 0.01);
    EXPECT_NEAR(view.top(), 95.0, 0.01);
}

TEST(CurveEditorStandaloneTest, FlatTangentsRepaintTheCurveShape)
{
    CE::ArtifactCurveEditorWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(kWidth, kHeight);
    widget.setTracks(primaryTracks());
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);
    widget.setCurrentFrame(0);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));
    const int column =
        static_cast<int>(std::lround(rawPoint(widget, 15.0f, 0.0f).x()));
    // The selected track is drawn with QColor::lighter(130).
    const QColor stroke = QColor(255, 128, 64).lighter(130);

    const QImage before = renderWidget(widget);
    const double rowBefore = averageStrokeRow(before, column, stroke, 15);
    ASSERT_GT(rowBefore, 0.0);

    EXPECT_TRUE(widget.setSelectedKeyFlatTangents());
    const QImage after = renderWidget(widget);
    writeCapturePair(QStringLiteral("flat_tangent"), before, after);

    const double rowAfter = averageStrokeRow(after, column, stroke, 15);
    ASSERT_GT(rowAfter, 0.0);
    // A flat tangent at the 100-valued key lifts the frame-15 part of the
    // segment well above the straight 0 -> 100 line.
    EXPECT_LT(rowAfter, rowBefore - 20.0);
}

TEST(CurveEditorStandaloneTest, ImageDiffSeparatesNoiseFromRealChanges)
{
    QImage reference(120, 80, QImage::Format_RGBA8888);
    reference.fill(QColor(30, 30, 30));

    const ImageDiff identical = compareImages(reference, reference);
    EXPECT_FALSE(identical.sizeMismatch);
    EXPECT_EQ(identical.maxAbs, 0);
    EXPECT_DOUBLE_EQ(identical.differingFraction, 0.0);
    EXPECT_DOUBLE_EQ(identical.meanAbs, 0.0);

    // A uniform 4-level shift is noise: it never exceeds the channel
    // tolerance, so it must not count as a differing pixel.
    QImage noise = reference;
    for (int y = 0; y < noise.height(); ++y) {
        for (int x = 0; x < noise.width(); ++x) {
            noise.setPixelColor(x, y, QColor(34, 34, 34));
        }
    }
    const ImageDiff small = compareImages(reference, noise);
    EXPECT_EQ(small.maxAbs, 4);
    EXPECT_DOUBLE_EQ(small.differingFraction, 0.0);
    EXPECT_NEAR(small.meanAbs, 4.0, 0.001);

    // A 20x20 block is a real change.
    QImage changed = reference;
    for (int y = 10; y < 30; ++y) {
        for (int x = 10; x < 30; ++x) {
            changed.setPixelColor(x, y, QColor(255, 0, 0));
        }
    }
    const ImageDiff block = compareImages(reference, changed);
    EXPECT_FALSE(block.sizeMismatch);
    EXPECT_NEAR(block.differingFraction, 400.0 / (120.0 * 80.0), 1e-9);
    EXPECT_EQ(block.maxAbs, 225);

    QImage wrongSize(10, 10, QImage::Format_RGBA8888);
    wrongSize.fill(Qt::black);
    EXPECT_TRUE(compareImages(reference, wrongSize).sizeMismatch);
}

// ===========================================================================
// Render contract + captures
// ===========================================================================

TEST(CurveEditorStandaloneTest, RenderCapturesEveryScenarioAndMatchesPixelContracts)
{
    const std::vector<Scenario> scenarios = buildScenarios();
    ASSERT_EQ(scenarios.size(), 16u);

    std::vector<std::pair<QString, QImage>> captures;

    for (int index = 0; index < static_cast<int>(scenarios.size()); ++index) {
        CE::ArtifactCurveEditorWidget widget;
        widget.setAttribute(Qt::WA_DontShowOnScreen, true);
        widget.resize(kWidth, kHeight);
        scenarios[index].setup(widget);

        const QImage image = renderWidget(widget);
        ASSERT_FALSE(image.isNull()) << scenarios[index].name.toStdString();
        EXPECT_EQ(image.size(), QSize(kWidth, kHeight));

        const QString fileName =
            QStringLiteral("curve_%1_%2.png")
                .arg(index + 1, 2, 10, QChar('0'))
                .arg(sanitize(scenarios[index].name));
        const QString path = writePng(image, fileName);
        EXPECT_TRUE(QFileInfo::exists(path));
        captures.emplace_back(scenarios[index].name, image);
    }

    const auto imageFor = [&captures](const QString& name) -> QImage {
        for (const auto& entry : captures) {
            if (entry.first == name) return entry.second;
        }
        return QImage();
    };

    const QImage valueGraph = imageFor(QStringLiteral("Value graph - two tracks"));
    const QImage selectedKeyGraph =
        imageFor(QStringLiteral("Selected key - tangent handles"));
    ASSERT_FALSE(valueGraph.isNull());
    ASSERT_FALSE(selectedKeyGraph.isNull());
    const QRect plot = plotRectPixels();

    // Widget background outside the plot.
    EXPECT_TRUE(colorNear(valueGraph.pixelColor(4, 4), kWidgetBackground, 2));

    // Plot backdrop: one known-empty point (frame 32, value 115 is above both
    // curves and off the 5-frame / 20-value grid) plus a share of the area.
    const QPoint emptyPoint(static_cast<int>(kMarginLeft + (32.0f / 60.0f) *
                                             (kWidth - kMarginLeft - kMarginRight)),
                            static_cast<int>(kMarginTop +
                                             (1.0f - (115.0f + 20.0f) / 140.0f) *
                                                 (kHeight - kMarginTop - kMarginBottom)));
    EXPECT_TRUE(plot.contains(emptyPoint));
    EXPECT_TRUE(colorNear(valueGraph.pixelColor(emptyPoint), kPlotBackground, 6));

    const int plotArea = plot.width() * plot.height();
    const int backdropPixels =
        countNear(valueGraph, plot, kPlotBackground, 3);
    EXPECT_GT(backdropPixels, plotArea / 12);

    // Track curve stroke.
    EXPECT_GT(countNear(valueGraph, plot, QColor(255, 128, 64), 45), 100);

    // Playhead at frame 30 for the 0..60 view.
    {
        const int playheadX =
            static_cast<int>((kMarginLeft +
                              (30.0f / 60.0f) * (kWidth - kMarginLeft - kMarginRight)));
        const QRect playheadColumn(playheadX - 3, plot.top(), 7, plot.height());
        EXPECT_GT(countNear(valueGraph, playheadColumn, kPlayheadColor, 60), 100);
    }

    // Selected key highlight is present in the handles scenario.
    EXPECT_GT(countNear(selectedKeyGraph, plot, QColor(255, 236, 128), 60), 20);

    // Constant interpolation paints a horizontal step from frame 30 to 60 at
    // value 100 instead of the curved segment.
    {
        const QImage constant = imageFor(QStringLiteral("Constant (step) segment"));
        ASSERT_FALSE(constant.isNull());
        const int y = static_cast<int>(
            kMarginTop + (1.0f - (100.0f + 20.0f) / 140.0f) *
                             (kHeight - kMarginTop - kMarginBottom));
        const QRect stepLine(static_cast<int>(kMarginLeft + (32.0f / 60.0f) * plot.width()),
                              y - 2,
                              static_cast<int>(plot.width() * 0.4f), 5);
        // The selected track is drawn with QColor::lighter(130), which also
        // lowers the saturation (Qt lightens toward white).
        EXPECT_GT(countNear(constant, stepLine,
                            QColor(255, 128, 64).lighter(130), 10), 100);
    }

    // The derived Speed graph keeps the derived track colour and still shows
    // the selection highlight (CE-14 editing path).
    {
        const QImage derived =
            imageFor(QStringLiteral("Speed graph - derived editing"));
        ASSERT_FALSE(derived.isNull());
        // The selected track is drawn with QColor::lighter(130).
        EXPECT_GT(countNear(derived, plot, QColor(96, 196, 255).lighter(130), 10),
                  100);
        EXPECT_GT(countNear(derived, plot, QColor(255, 236, 128), 60), 20);
    }

    // The normalized marquee scenario selects keys from both tracks, which
    // exercises displayValue() during box selection.
    {
        const QImage normalizedSelection =
            imageFor(QStringLiteral("Normalized marquee selection"));
        ASSERT_FALSE(normalizedSelection.isNull());
        EXPECT_GT(countNear(normalizedSelection, plot, QColor(255, 236, 128), 60), 20);
    }

    // The in-progress marquee paints its dashed rectangle.
    {
        const QImage marquee = imageFor(QStringLiteral("Marquee in progress"));
        ASSERT_FALSE(marquee.isNull());
        EXPECT_GT(countNear(marquee, plot, QColor(120, 170, 255), 70), 40);
    }

    // The buffer curve adds the pre-edit shape, so the capture must differ
    // from the same post-drag state rendered without the buffer.
    {
        const QImage buffer = imageFor(QStringLiteral("Buffer curve after key drag"));
        ASSERT_FALSE(buffer.isNull());

        CE::ArtifactCurveEditorWidget widget;
        widget.setAttribute(Qt::WA_DontShowOnScreen, true);
        widget.resize(kWidth, kHeight);
        setupTwoTrackView(widget);
        drag(widget, keyPoint(widget, 0, 1), widget.visualPoint(0, 40.0f, 60.0f));
        // The drag itself shows the buffer; hide it for the baseline capture.
        pressKey(widget, Qt::Key_B);
        const QImage plain = renderWidget(widget);

        int differingPixels = 0;
        for (int y = plot.top(); y <= plot.bottom(); ++y) {
            for (int x = plot.left(); x <= plot.right(); ++x) {
                if (buffer.pixel(x, y) != plain.pixel(x, y)) ++differingPixels;
            }
        }
        EXPECT_GT(differingPixels, 2000);
    }

    const QString sheet = writeContactSheet(captures);
    EXPECT_FALSE(sheet.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(sheet));
}

TEST(CurveEditorStandaloneTest, RenderScenariosMatchStoredBaselines)
{
    const std::vector<Scenario> scenarios = buildScenarios();
    const QString directory = baselineDirectory();
    ASSERT_FALSE(directory.isEmpty());
    ASSERT_TRUE(QDir().mkpath(directory)) << directory.toStdString();

    int updated = 0;
    int compared = 0;
    for (int index = 0; index < static_cast<int>(scenarios.size()); ++index) {
        CE::ArtifactCurveEditorWidget widget;
        widget.setAttribute(Qt::WA_DontShowOnScreen, true);
        widget.resize(kWidth, kHeight);
        scenarios[index].setup(widget);

        const QImage image = renderWidget(widget);
        ASSERT_FALSE(image.isNull()) << scenarios[index].name.toStdString();

        const QString fileName =
            QStringLiteral("curve_%1_%2.png")
                .arg(index + 1, 2, 10, QChar('0'))
                .arg(sanitize(scenarios[index].name));
        const QString path = directory + QStringLiteral("/") + fileName;

        if (g_updateBaselines) {
            ASSERT_TRUE(image.save(path, "PNG")) << path.toStdString();
            ++updated;
            continue;
        }

        if (!QFileInfo::exists(path)) {
            ADD_FAILURE() << "missing baseline " << path.toStdString()
                          << " (run with --update-baselines)";
            continue;
        }

        const QImage expected(path);
        ASSERT_FALSE(expected.isNull()) << path.toStdString();
        const ImageDiff diff = compareImages(expected, image);
        const bool matches = !diff.sizeMismatch &&
                             diff.differingFraction <= 0.002 &&
                             diff.meanAbs <= 0.5;
        if (!matches) {
            writeBaselineDiff(path, expected, image);
        }
        EXPECT_TRUE(matches) << fileName.toStdString()
                             << " differingFraction=" << diff.differingFraction
                             << " meanAbs=" << diff.meanAbs
                             << " maxAbs=" << diff.maxAbs;
        ++compared;
    }

    if (g_updateBaselines) {
        EXPECT_EQ(updated, static_cast<int>(scenarios.size()));
        std::printf("[baselines] wrote %d scenarios to %s\n", updated,
                    directory.toUtf8().constData());
    } else {
        EXPECT_EQ(compared, static_cast<int>(scenarios.size()));
    }
}

// ===========================================================================
// Entry point: GTest contracts by default, live lab on --ui
// ===========================================================================

int main(int argc, char** argv)
{
    // Build the argument list from argc/argv directly: QCoreApplication's
    // argument store is not valid before the application object exists.
    QStringList arguments;
    arguments.reserve(argc);
    for (int index = 0; index < argc; ++index) {
        arguments << QString::fromLocal8Bit(argv[index]);
    }
    const Options options = parseOptions(arguments);

    if (options.listScenarios) {
        const std::vector<Scenario> scenarios = buildScenarios();
        for (int index = 0; index < static_cast<int>(scenarios.size()); ++index) {
            std::printf("%d: %s\n", index + 1,
                        scenarios[index].name.toUtf8().constData());
        }
        return 0;
    }

    if (!options.ui && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication application(argc, argv);

    if (qEnvironmentVariable("QT_QPA_PLATFORM") == QStringLiteral("offscreen")) {
        configureCaptureFonts(options.fontReport);
    }

    if (options.ui) {
        return runUi(application);
    }

    ::testing::InitGoogleTest(&argc, argv);
    if (!options.scenario.isEmpty()) {
        ::testing::GTEST_FLAG(filter) = options.scenario.toStdString();
    }
    return RUN_ALL_TESTS();
}
