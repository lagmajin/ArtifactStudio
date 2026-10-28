// Standalone contract test + capture harness for the timeline curve editor mode.
//
// Modelled on ArtifactFxStudioRenderContractTest: the real widget is exercised
// without the application, a composition, a GPU device or a window manager, and
// every scenario is rendered to PNG so a failure can be looked at.
//
// What it covers (the public surface the timeline curve editor mode drives):
//   - sampleSpeedGraph() -> CurveTrack (frame/percent mapping, clamping, padding)
//   - view range / plot rect / data<->pixel mapping (+ normalized view)
//   - fitToContent() margins and empty fallback
//   - tangent operations (Auto / Flat / Linear / Break / Unify / Constant / Bezier)
//   - selection (click, Shift+click marquee, A, Escape, focus track visibility)
//   - key insert (Ctrl+Click), delete, multi-key drag
//   - playhead scrub, cursor-anchored wheel zoom
//   - read-only mode contract used by the Speed graph (CE-14)
//   - per-pixel render checks and capture output for visual inspection
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
#include <QSize>
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
#include <functional>
#include <string>
#include <utility>
#include <vector>

import Widget.CurveEditor;
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
    setupTwoTrackView(widget);
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
    // The timeline keeps the Speed graph read-only until CE-14 defines the
    // write-back contract; the harness reproduces that mode here.
    widget.setKeyEditingEnabled(false);
    widget.setHandleEditingEnabled(false);
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

void setupBufferCurve(CE::ArtifactCurveEditorWidget& widget)
{
    setupTwoTrackView(widget);
    const QPointF start = keyPoint(widget, 0, 1);
    const QPointF end = widget.visualPoint(0, 40.0f, 60.0f);
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
    sendMouse(widget, QEvent::MouseButtonRelease, end, Qt::LeftButton,
              Qt::NoButton);
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
        {QStringLiteral("Constant (step) segment"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupConstantStep(widget); }},
        {QStringLiteral("Multi selection - marquee"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupMultiSelection(widget); }},
        {QStringLiteral("Speed graph - read-only"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupSpeedGraph(widget); }},
        {QStringLiteral("Normalized view"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupNormalizedView(widget); }},
        {QStringLiteral("Buffer curve after key drag"),
         [](CE::ArtifactCurveEditorWidget& widget) { setupBufferCurve(widget); }},
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
        // Drop the previous scenario's selection before replacing the tracks.
        // focusTrack() on an empty editor logs a harmless fitToContent fallback.
        if (!editor_->tracks().empty()) {
            editor_->focusTrack(-1);
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
    EXPECT_EQ(widget.visualSelectedTrack(), 1);

    widget.focusTrack(-1);
    EXPECT_TRUE(widget.tracks()[0].visible);
    EXPECT_TRUE(widget.tracks()[1].visible);
    EXPECT_EQ(widget.visualSelectedTrack(), -1);
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

    // Non-zero handles: a selected key with zero-length handles sits exactly on
    // its own handle pins, so the next press would grab a tangent instead.
    std::vector<CE::CurveTrack> tracks = primaryTracks();
    tracks[0].keys[1].inHandleFrame = -8;
    tracks[0].keys[1].inHandleValue = 0.0f;
    tracks[0].keys[1].outHandleFrame = 8;
    tracks[0].keys[1].outHandleValue = 0.0f;
    widget.setTracks(tracks);
    widget.setViewRange(0.0f, 60.0f, -20.0f, 120.0f);

    clickKey(widget, 0, 1);
    ASSERT_TRUE(keySelected(widget, 0, 1));

    const QPointF start = keyPoint(widget, 0, 1);
    const QPointF end = widget.visualPoint(0, 40.0f, 80.0f);
    sendMouse(widget, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(widget, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
    sendMouse(widget, QEvent::MouseButtonRelease, end, Qt::LeftButton,
              Qt::NoButton);

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
// Render contract + captures
// ===========================================================================

TEST(CurveEditorStandaloneTest, RenderCapturesEveryScenarioAndMatchesPixelContracts)
{
    const std::vector<Scenario> scenarios = buildScenarios();
    ASSERT_EQ(scenarios.size(), 8u);

    std::vector<std::pair<QString, QImage>> captures;
    QImage valueGraph;
    QImage selectedKeyGraph;

    for (int index = 0; index < static_cast<int>(scenarios.size()); ++index) {
        CE::ArtifactCurveEditorWidget widget;
        widget.setAttribute(Qt::WA_DontShowOnScreen, true);
        widget.resize(kWidth, kHeight);
        scenarios[index].setup(widget);

        const QImage image = renderWidget(widget);
        ASSERT_FALSE(image.isNull()) << scenarios[index].name.toStdString();
        EXPECT_EQ(image.size(), QSize(kWidth, kHeight));
        if (index == 0) valueGraph = image;
        if (index == 1) selectedKeyGraph = image;

        const QString fileName =
            QStringLiteral("curve_%1_%2.png")
                .arg(index + 1, 2, 10, QChar('0'))
                .arg(sanitize(scenarios[index].name));
        const QString path = writePng(image, fileName);
        EXPECT_TRUE(QFileInfo::exists(path));
        captures.emplace_back(scenarios[index].name, image);
    }

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

    // The speed graph is a single derived track with its own colour.
    {
        CE::ArtifactCurveEditorWidget widget;
        widget.setAttribute(Qt::WA_DontShowOnScreen, true);
        widget.resize(kWidth, kHeight);
        setupSpeedGraph(widget);
        const QImage image = renderWidget(widget);
        EXPECT_GT(countNear(image, plotRectPixels(), QColor(96, 196, 255), 45), 100);
    }

    const QString sheet = writeContactSheet(captures);
    EXPECT_FALSE(sheet.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(sheet));
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
