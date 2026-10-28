#pragma once

// Adapters between Video.Transitions.* and ArtifactTestTransitionRenderer.
//
// REQUIRED INCLUDE ORDER: this header needs the imported Video/NLE types, and
// the ArtifactCore module rules forbid pulling an STL header in after the
// `import` declarations, so include it last:
//
//   #include <Qt and STL headers>            (including <variant>)
//   #include "ArtifactTestTransitionRenderer.h"
//   import NLE.Core;
//   import Video.VideoFrame;
//   import Video.AbstractTransition;
//   import Video.TransitionFactory;
//   import Video.Transitions.*;            (one per implementation)
//   #include "ArtifactTestTransitionBridge.h"      <-- here
//
// Every import of Video.Transitions.* is load-bearing: it forces the module's
// static registrar to be linked, which is how TransitionFactory learns the
// implementation exists (see ArtifactNLETransitionBridge.cpp for the same
// pattern in the application).
//
// kindName() is an explicit switch rather than a static_cast of the enum value
// so that inserting a TransitionKind in the middle fails the registry contract
// instead of silently renaming two transitions.

#include "ArtifactTestTransitionRenderer.h"

namespace ArtifactTestTransitionBridge {

using ArtifactTestTransitionRenderer::Image;

// Concrete kinds, in TransitionKind declaration order, excluding Cut. Cut has no
// implementation on purpose: the NLE bridge treats a missing creator as a
// passthrough of the left frame.
inline std::vector<ArtifactCore::NLE::TransitionKind> concreteKinds()
{
    using ArtifactCore::NLE::TransitionKind;
    return {TransitionKind::Crossfade,     TransitionKind::Dissolve,
            TransitionKind::Wipe,          TransitionKind::Slide,
            TransitionKind::Zoom,          TransitionKind::GlitchDisplace,
            TransitionKind::Spin,          TransitionKind::LinearWipe,
            TransitionKind::RadialWipe,    TransitionKind::Flip,
            TransitionKind::Cube,          TransitionKind::Doors,
            TransitionKind::LightLeak,     TransitionKind::GradientWipe,
            TransitionKind::IrisWipe,      TransitionKind::BlockDissolve};
}

inline const char* kindName(const ArtifactCore::NLE::TransitionKind kind)
{
    using ArtifactCore::NLE::TransitionKind;
    switch (kind) {
    case TransitionKind::Cut: return "Cut";
    case TransitionKind::Crossfade: return "Crossfade";
    case TransitionKind::Dissolve: return "Dissolve";
    case TransitionKind::Wipe: return "Wipe";
    case TransitionKind::Slide: return "Slide";
    case TransitionKind::Zoom: return "Zoom";
    case TransitionKind::GlitchDisplace: return "GlitchDisplace";
    case TransitionKind::Spin: return "Spin";
    case TransitionKind::LinearWipe: return "LinearWipe";
    case TransitionKind::RadialWipe: return "RadialWipe";
    case TransitionKind::Flip: return "Flip";
    case TransitionKind::Cube: return "Cube";
    case TransitionKind::Doors: return "Doors";
    case TransitionKind::LightLeak: return "LightLeak";
    case TransitionKind::GradientWipe: return "GradientWipe";
    case TransitionKind::IrisWipe: return "IrisWipe";
    case TransitionKind::BlockDissolve: return "BlockDissolve";
    }
    return "Unknown";
}

inline int kindValue(const ArtifactCore::NLE::TransitionKind kind)
{
    return static_cast<int>(kind);
}

// Registers every concrete transition through its exported hook. The static
// registrar inside each implementation unit does not run for a static-library
// consumer (link-time initializer sections are not pulled by an import alone),
// so the lab asks explicitly. Idempotent: registerCreator replaces the creator
// for the same kind.
inline void ensureRegistered()
{
    static const bool registered = []() {
        ArtifactCore::registerBlockDissolveTransition();
        ArtifactCore::registerCrossfadeTransition();
        ArtifactCore::registerCubeTransition();
        ArtifactCore::registerDissolveTransition();
        ArtifactCore::registerDoorsTransition();
        ArtifactCore::registerFlipTransition();
        ArtifactCore::registerGlitchDisplaceTransition();
        ArtifactCore::registerGradientWipeTransition();
        ArtifactCore::registerIrisWipeTransition();
        ArtifactCore::registerLightLeakTransition();
        ArtifactCore::registerLinearWipeTransition();
        ArtifactCore::registerRadialWipeTransition();
        ArtifactCore::registerSlideTransition();
        ArtifactCore::registerSpinTransition();
        ArtifactCore::registerWipeTransition();
        ArtifactCore::registerZoomTransition();
        return true;
    }();
    (void)registered;
}

inline bool equalsIgnoreCase(const char* a, const char* b)
{
    if (!a || !b) return false;
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

// Accepts the observable name, plus the shorthands the OTIO exporter accepts.
inline bool kindFromName(const char* name, ArtifactCore::NLE::TransitionKind& out)
{
    using ArtifactCore::NLE::TransitionKind;
    for (const TransitionKind kind : concreteKinds()) {
        if (equalsIgnoreCase(name, kindName(kind))) {
            out = kind;
            return true;
        }
    }
    if (equalsIgnoreCase(name, "Cut")) {
        out = TransitionKind::Cut;
        return true;
    }
    if (equalsIgnoreCase(name, "Glitch") || equalsIgnoreCase(name, "glitchdisplace")) {
        out = TransitionKind::GlitchDisplace;
        return true;
    }
    if (equalsIgnoreCase(name, "Iris")) {
        out = TransitionKind::IrisWipe;
        return true;
    }
    return false;
}

struct RenderOptions {
    double progress = 0.0;
    int frameIndex = 0;
    double fps = 30.0;
    // Zero means "use the input frame size".
    int width = 0;
    int height = 0;
};

struct RenderResult {
    ArtifactCore::NLE::TransitionKind kind = ArtifactCore::NLE::TransitionKind::Cut;
    // False for Cut (or any kind without a creator): the output is the left
    // frame unchanged, exactly like the NLE bridge passthrough.
    bool hasCreator = false;
    bool initializeAccepted = false;
    bool validOutput = false;
    const char* name = "Cut";
    Image image;
    // The right input as seen after process(), so a test can prove the
    // transition only mutates the left frame.
    Image rightAfter;
};

inline ArtifactCore::CpuVideoFrame toCpuFrame(const Image& image)
{
    ArtifactCore::CpuVideoFrame frame;
    frame.meta.width = image.width;
    frame.meta.height = image.height;
    frame.meta.pixelFormat = ArtifactCore::VideoFramePixelFormat::RGBA8;
    frame.strideBytes = image.width * 4;
    frame.bytes = image.pixels;
    return frame;
}

inline Image fromCpuFrame(const ArtifactCore::CpuVideoFrame& frame)
{
    Image image;
    if (!frame.isValid()) return image;
    image.width = frame.meta.width;
    image.height = frame.meta.height;
    image.pixels = frame.bytes;
    return image;
}

inline RenderResult render(const ArtifactCore::NLE::TransitionKind kind,
                           const Image& left, const Image& right,
                           const RenderOptions& options)
{
    ensureRegistered();

    RenderResult result;
    result.kind = kind;
    result.name = kindName(kind);
    result.rightAfter = right;

    ArtifactCore::AbstractTransition* transition =
        ArtifactCore::TransitionFactory::instance().create(kind);
    if (!transition) {
        result.image = left;
        result.validOutput = !left.empty();
        return result;
    }

    result.hasCreator = true;
    result.name = transition->name();

    ArtifactCore::DecodedVideoFrame leftFrame = toCpuFrame(left);
    ArtifactCore::DecodedVideoFrame rightFrame = toCpuFrame(right);

    ArtifactCore::TransitionContext context;
    context.progress = options.progress;
    context.frameIndex = options.frameIndex;
    context.fps = options.fps;
    context.width = options.width > 0 ? options.width : left.width;
    context.height = options.height > 0 ? options.height : left.height;
    // No implementation reads ctx.transition today, and the NLE bridge's
    // callers do not set it either, so the lab leaves it null. That keeps this
    // header from needing Transition/FrameRange/QVariantMap completeness.
    context.transition = nullptr;

    result.initializeAccepted = transition->initialize(context);
    if (result.initializeAccepted) {
        transition->process(leftFrame, rightFrame, context);
    }

    if (std::holds_alternative<ArtifactCore::CpuVideoFrame>(leftFrame)) {
        result.image = fromCpuFrame(std::get<ArtifactCore::CpuVideoFrame>(leftFrame));
        result.validOutput = !result.image.empty();
    }
    if (std::holds_alternative<ArtifactCore::CpuVideoFrame>(rightFrame)) {
        result.rightAfter = fromCpuFrame(std::get<ArtifactCore::CpuVideoFrame>(rightFrame));
    }

    delete transition;
    return result;
}

inline RenderResult render(const ArtifactCore::NLE::TransitionKind kind,
                           const RenderOptions& options)
{
    const int width = options.width > 0 ? options.width : 96;
    const int height = options.height > 0 ? options.height : 64;
    return render(kind, ArtifactTestTransitionRenderer::makeDiagnosticImage(width, height, false),
                  ArtifactTestTransitionRenderer::makeDiagnosticImage(width, height, true),
                  options);
}

} // namespace ArtifactTestTransitionBridge
