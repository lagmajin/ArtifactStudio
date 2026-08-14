"""Regression tests for the design model and Intent validation contract."""

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("audit_design.py")
SPEC = importlib.util.spec_from_file_location("audit_design", MODULE_PATH)
MODEL = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODEL)


class DesignModelTests(unittest.TestCase):
    def setUp(self):
        self.fixture = next(f for f in MODEL.load_fixtures() if f["id"] == "text_sample1")
        self.valid_intent = {
            "action": "create_text_animation",
            "target": "text-layer-1",
            "selection": {"unit": "word", "order": "natural"},
            "operators": [{"type": "opacity", "mode": "replace", "from": 0, "to": 1}],
            "timing": {"duration": 0.8, "stagger": 0.08},
        }

    def test_valid_intent_has_no_diagnostics(self):
        self.assertEqual(MODEL.validate_intent(self.valid_intent, self.fixture), [])

    def test_intent_schema_enums_cover_model_supported_values(self):
        schema = json.loads((MODEL.ROOT / "intent_schema.json").read_text(encoding="utf-8"))
        selection = schema["properties"]["selection"]["properties"]
        operators = schema["properties"]["operators"]["items"]["properties"]
        timing = schema["properties"]["timing"]["properties"]
        self.assertTrue({"character", "grapheme", "word", "line", "paragraph",
                         "tag", "regex"}.issubset(set(selection["unit"]["enum"])))
        self.assertTrue({"position", "scale", "rotation", "opacity", "skew",
                         "tracking", "color", "stroke", "blur", "noise"}.issubset(
                         set(operators["type"]["enum"])))
        self.assertTrue({"linear", "smooth", "sharp", "spring", "custom"}.issubset(
                         set(timing["easing"]["enum"])))

    def test_intent_schema_limits_match_model_contract(self):
        schema = json.loads((MODEL.ROOT / "intent_schema.json").read_text(encoding="utf-8"))
        root = schema["properties"]
        operator_schema = root["operators"]
        operator_item = operator_schema["items"]
        self.assertEqual(operator_schema["maxItems"], 32)
        self.assertEqual(set(operator_item["required"]), {"type", "from", "to"})
        self.assertEqual(root["selection"]["properties"]["pattern"]["maxLength"], 4096)
        self.assertEqual(root["timing"]["properties"]["duration"]["maximum"], 86400)
        self.assertEqual(root["timing"]["properties"]["stagger"]["maximum"], 86400)

    def test_artifactcore_text_animator_public_contract_is_present(self):
        core_header = MODEL.ROOT.parent.parent / "ArtifactCore" / "include" / "Text" / "TextAnimator.ixx"
        self.assertTrue(core_header.exists(), core_header)
        source = core_header.read_text(encoding="utf-8")
        for shape in ("Square", "RampUp", "RampDown", "Triangle", "Round", "Smooth"):
            self.assertIn(shape, source)
        for order in ("Natural", "Reverse", "RandomStable", "CenterOut", "EdgeIn"):
            self.assertIn(f"{order},", source)
        for field in ("position", "scale", "rotation", "opacity", "skew",
                      "tracking", "z", "fillColor", "strokeColor", "strokeWidth", "blur"):
            self.assertIn(field, source)
        self.assertIn("applyAnimatorStack", source)

    def test_artifactcore_text_animator_implementation_keeps_safety_contracts(self):
        core_source = MODEL.ROOT.parent.parent / "ArtifactCore" / "src" / "Text" / "TextAnimator.cppm"
        self.assertTrue(core_source.exists(), core_source)
        source = core_source.read_text(encoding="utf-8")
        required_fragments = (
            "calculateWigglyWeight(animationIndex, time",
            "selectedClusters",
            "std::clamp(extraWeights",
            "if (!std::isfinite(totalWeight))",
            "accumulateColorOverride",
            "cumulativeTracking",
        )
        for fragment in required_fragments:
            self.assertIn(fragment, source)

    def test_color_glyph_contract_is_explicit_end_to_end(self):
        core_contract = MODEL.ROOT.parent.parent / "ArtifactCore" / "include" / "Text" / "GlyphAtlas.ixx"
        core_source = MODEL.ROOT.parent.parent / "ArtifactCore" / "src" / "Text" / "GlyphAtlas.cppm"
        gpu_shader = MODEL.ROOT.parent.parent / "Artifact" / "src" / "Render" / "ShaderManager.cppm"
        renderer = MODEL.ROOT.parent.parent / "Artifact" / "src" / "Render" / "DiligentImmediateSubmitter.cppm"
        layout_contract = MODEL.ROOT.parent.parent / "ArtifactCore" / "include" / "Text" / "TextLayoutContract.ixx"
        self.assertIn("ColorBitmap", layout_contract.read_text(encoding="utf-8"))
        self.assertIn("colorPreserved", core_contract.read_text(encoding="utf-8"))
        self.assertIn("rasterizeColorGlyphWithDirectWrite", core_source.read_text(encoding="utf-8"))
        self.assertIn("input.Color.a < 0.0f", gpu_shader.read_text(encoding="utf-8"))
        self.assertIn("glyph.rect.colorPreserved", renderer.read_text(encoding="utf-8"))

    def test_gpu_audit_does_not_pass_stale_color_samples(self):
        audit = (MODEL.ROOT / "audit_gpu_matrix.ps1").read_text(encoding="utf-8")
        self.assertIn("needs-color-atlas", audit)
        self.assertIn("stale-library-or-binary", audit)
        self.assertIn("ArtifactRender.lib", audit)
        self.assertIn("passed =", audit)

    def test_text_gpu_runtime_boundary_does_not_link_full_renderer(self):
        cmake = (MODEL.ROOT.parent.parent / "Artifact" / "CMakeLists.txt").read_text(encoding="utf-8")
        start = cmake.index("add_library(ArtifactRenderTextRuntime STATIC)")
        end = cmake.index("if(DILIGENT_COMPILE_DEFINITIONS)", start)
        runtime = cmake[cmake.index("set(ARTIFACT_RENDER_TEXT_MODULES"):end]
        self.assertIn("ArtifactCoreTextRuntime", runtime)
        self.assertIn("PrimitiveRenderer2D.cppm", runtime)
        self.assertIn("DiligentImmediateSubmitter.cppm", runtime)
        self.assertNotIn("target_link_libraries(ArtifactRenderTextRuntime PUBLIC ArtifactRender", runtime)
        self.assertIn("ArtifactRenderTextRuntime Qt6::Core Qt6::Gui", cmake)
        facade = (MODEL.ROOT.parent.parent / "Artifact" / "include" / "Render" / "ArtifactTextRenderer.ixx")
        source = facade.read_text(encoding="utf-8")
        for method in ("initializeHeadless", "drawGlyphs", "flushAndWait", "readbackToImage", "destroy"):
            self.assertIn(method, source)
        self.assertIn("Minimal GPU contract for text animation validation", source)
        target = (MODEL.ROOT.parent.parent / "Artifact" / "include" / "Render" / "ArtifactTextRenderTarget.ixx")
        target_source = target.read_text(encoding="utf-8")
        for method in ("create", "renderTargetView", "clear", "readback", "destroy"):
            self.assertIn(method, target_source)
        self.assertIn("independent of swap chains, layers, and composition", target_source)
        self.assertIn("ArtifactTextRenderTarget.ixx", cmake)
        self.assertIn("ArtifactTextGlyphSubmitter.ixx", cmake)
        self.assertIn("ArtifactTextGlyphPipelineAdapter.ixx", cmake)
        self.assertIn("ArtifactTextGlyphShaderSources.ixx", cmake)
        self.assertIn("ArtifactTextRenderer.ixx", cmake)
        self.assertIn("Artifact.Render.TextRenderTarget", cmake)
        self.assertIn("add_library(ArtifactTextRenderTargetRuntime STATIC", cmake)
        self.assertIn("add_library(ArtifactTextGlyphSubmitterRuntime STATIC", cmake)
        self.assertIn("Qt6::Core Qt6::Gui ${DILIGENT_LIBS}", cmake)

        glyph_contract = (MODEL.ROOT.parent.parent / "Artifact" / "include" / "Render" / "ArtifactTextGlyphSubmitter.ixx").read_text(encoding="utf-8")
        self.assertNotIn("import Artifact.Render.ShaderManager", glyph_contract)
        adapter = (MODEL.ROOT.parent.parent / "Artifact" / "include" / "Render" / "ArtifactTextGlyphPipelineAdapter.ixx").read_text(encoding="utf-8")
        self.assertIn("import Artifact.Render.ShaderManager", adapter)

    def test_artifactcore_glyph_contract_exposes_selector_metadata(self):
        layout_header = MODEL.ROOT.parent.parent / "ArtifactCore" / "include" / "Text" / "TextLayoutContract.ixx"
        self.assertTrue(layout_header.exists(), layout_header)
        source = layout_header.read_text(encoding="utf-8")
        for field in ("clusterId", "selectorTag", "stableTokenId", "clusterIndex", "lineIndex"):
            self.assertIn(field, source)
        for field in ("offsetPosition", "offsetRotation", "offsetScale", "offsetOpacity",
                      "offsetSkew", "offsetTracking", "offsetZ", "offsetBlur"):
            self.assertIn(field, source)

    def test_every_declared_fixture_produces_a_valid_audit_result(self):
        fixtures = MODEL.load_fixtures()
        self.assertGreaterEqual(len(fixtures), 17)
        for fixture in fixtures:
            with self.subTest(fixture=fixture["id"]):
                result = MODEL.audit_fixture(fixture)
                self.assertIn(result["tier"], {"smoke", "contract", "stress"})
                self.assertIn(result["status"], {"pass", "warning", "error"})
                self.assertEqual(result["issues"], [])
                self.assertEqual(result["stateCount"], result["virtualGlyphCount"])
                self.assertTrue(result["stateFinite"])
                self.assertLessEqual(len(result["stateSample"]), 4)

    def test_unknown_action_is_rejected(self):
        intent = {**self.valid_intent, "action": "delete_everything"}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("unsupportedAction", codes)

    def test_target_must_be_a_non_empty_string(self):
        for target in (None, 12, ""):
            intent = {**self.valid_intent, "target": target}
            codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
            self.assertIn("targetNotFound", codes)

    def test_unknown_fields_are_rejected_at_each_contract_level(self):
        cases = [
            ({**self.valid_intent, "typo": True}, "unknownIntentField"),
            ({**self.valid_intent,
              "selection": {"unit": "word", "order": "natural", "typo": True}},
             "unknownSelectionField"),
            ({**self.valid_intent,
              "operators": [{"type": "opacity", "from": 0, "to": 1, "typo": True}]},
             "unknownOperatorField"),
            ({**self.valid_intent,
              "timing": {"duration": 1, "stagger": 0, "typo": True}},
             "unknownTimingField"),
            ({**self.valid_intent, "options": {"typo": True}}, "unknownOptionsField"),
        ]
        for intent, expected_code in cases:
            with self.subTest(expected_code=expected_code):
                codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
                self.assertIn(expected_code, codes)

    def test_options_must_be_an_object_when_present(self):
        intent = {**self.valid_intent, "options": None}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("invalidOptionsType", codes)

    def test_regex_requires_pattern(self):
        intent = {**self.valid_intent, "selection": {"unit": "regex", "order": "natural"}}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("invalidRegex", codes)

    def test_regex_type_and_length_are_enforced(self):
        for pattern in (12, "x" * 4097):
            intent = {**self.valid_intent,
                      "selection": {"unit": "regex", "order": "natural",
                                    "pattern": pattern}}
            codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
            self.assertIn("invalidRegex", codes)

    def test_negative_timing_is_rejected(self):
        intent = {**self.valid_intent,
                  "timing": {"duration": -0.1, "stagger": 0.0}}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("timelineOutOfRange", codes)

    def test_timing_schema_upper_bound_is_enforced(self):
        intent = {**self.valid_intent,
                  "timing": {"duration": 86400.1, "stagger": 0.0}}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("timelineOutOfRange", codes)

    def test_easing_and_operator_axis_seed_are_validated(self):
        intent = {**self.valid_intent,
                  "timing": {"duration": 1, "stagger": 0, "easing": "elastic"},
                  "operators": [{"type": "position", "from": [0, 0], "to": [1, 1],
                                 "axis": "q", "seed": "random"}]}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("unsupportedEasing", codes)
        self.assertIn("unsupportedOperatorAxis", codes)
        self.assertIn("invalidOperatorSeed", codes)

    def test_json_boolean_is_not_accepted_as_integer_line_or_seed(self):
        line = {**self.valid_intent,
                "selection": {"unit": "line", "order": "natural", "value": True}}
        seed = {**self.valid_intent,
                "operators": [{"type": "noise", "from": 0.0, "to": 1.0,
                                "seed": False}]}
        self.assertIn("missingSelectionValue",
                      {d["code"] for d in MODEL.validate_intent(line, self.fixture)})
        self.assertIn("invalidOperatorSeed",
                      {d["code"] for d in MODEL.validate_intent(seed, self.fixture)})

    def test_json_boolean_is_not_accepted_as_timing_or_operator_number(self):
        timing = {**self.valid_intent,
                  "timing": {"duration": True, "stagger": 0.0}}
        opacity = {**self.valid_intent,
                   "operators": [{"type": "opacity", "from": False, "to": 1.0}]}
        self.assertIn("timelineOutOfRange",
                      {d["code"] for d in MODEL.validate_intent(timing, self.fixture)})
        self.assertIn("invalidOperatorRange",
                      {d["code"] for d in MODEL.validate_intent(opacity, self.fixture)})

    def test_empty_operator_stack_is_rejected(self):
        intent = {**self.valid_intent, "operators": []}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("operatorConflict", codes)

    def test_unknown_operator_and_order_are_rejected(self):
        intent = {**self.valid_intent,
                  "selection": {"unit": "word", "order": "sideways"},
                  "operators": [{"type": "teleport"}]}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("unsupportedSelectionOrder", codes)
        self.assertIn("unsupportedOperator", codes)

    def test_operator_range_and_mode_are_validated(self):
        intent = {**self.valid_intent, "operators": [{
            "type": "opacity", "mode": "teleport", "from": -0.2, "to": 2.0
        }]}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("unsupportedOperatorMode", codes)
        self.assertIn("operatorValueOutOfRange", codes)

    def test_operator_missing_range_is_rejected(self):
        intent = {**self.valid_intent,
                  "operators": [{"type": "position", "mode": "add", "from": [0, 0]}]}
        codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
        self.assertIn("missingOperatorRange", codes)

    def test_operator_shapes_and_colors_are_validated(self):
        position = {**self.valid_intent,
                    "operators": [{"type": "position", "from": [0, "x"], "to": [1, 2]}]}
        color = {**self.valid_intent,
                 "operators": [{"type": "color", "from": "red", "to": "#12345"}]}
        self.assertIn("invalidOperatorRange",
                      {d["code"] for d in MODEL.validate_intent(position, self.fixture)})
        self.assertIn("invalidOperatorColor",
                      {d["code"] for d in MODEL.validate_intent(color, self.fixture)})

    def test_malformed_intent_returns_diagnostics_without_crashing(self):
        cases = [
            (None, "invalidIntentType"),
            ({"selection": []}, "invalidSelectionType"),
            ({**self.valid_intent, "timing": []}, "invalidTimingType"),
            ({**self.valid_intent, "operators": "opacity"}, "invalidOperatorList"),
            ({**self.valid_intent, "operators": [None]}, "invalidOperatorType"),
        ]
        for intent, expected_code in cases:
            codes = {d["code"] for d in MODEL.validate_intent(intent, self.fixture)}
            self.assertIn(expected_code, codes)

    def test_reversed_range_remains_finite(self):
        glyphs = MODEL.virtual_glyphs("Text1")
        states = MODEL.evaluate(glyphs, [{
            "selector": {"start": 80.0, "end": 20.0, "shape": "Smooth"},
            "properties": {"position": [1.0, 0.0], "scale": 0.9,
                           "rotation": 10.0, "opacity": 0.5},
        }])
        self.assertEqual(len(states), len(glyphs))
        self.assertTrue(all(0.0 <= state["opacity"] <= 1.0 for state in states))

    def test_core_selector_shapes_triangle_and_round_are_supported(self):
        self.assertEqual(MODEL.weight(0, 3, 0.0, 100.0, "Triangle"), 0.0)
        self.assertEqual(MODEL.weight(1, 3, 0.0, 100.0, "Triangle"), 1.0)
        self.assertAlmostEqual(MODEL.weight(1, 3, 0.0, 100.0, "Round"), 1.0)

    def test_selector_offset_and_easing_are_applied_before_shape(self):
        self.assertEqual(MODEL.weight(0, 3, 0.0, 100.0, "Square", offset=50.0), 0.0)
        eased = MODEL.weight(1, 3, 0.0, 100.0, "RampUp", ease_high=10.0)
        self.assertAlmostEqual(eased, 0.25)

    def test_selector_orders_match_core_deterministic_domains(self):
        self.assertEqual([MODEL.ordered_rank(i, 5, "reverse") for i in range(5)],
                         [4, 3, 2, 1, 0])
        self.assertEqual([MODEL.ordered_rank(i, 5, "center_out") for i in range(5)],
                         [3, 1, 0, 2, 4])
        self.assertEqual([MODEL.ordered_rank(i, 5, "edge_in") for i in range(5)],
                         [0, 2, 4, 3, 1])

    def test_random_stable_order_is_seeded_and_is_a_permutation(self):
        first = [MODEL.ordered_rank(i, 16, "random_stable", 42) for i in range(16)]
        second = [MODEL.ordered_rank(i, 16, "random_stable", 42) for i in range(16)]
        other = [MODEL.ordered_rank(i, 16, "random_stable", 43) for i in range(16)]
        self.assertEqual(first, second)
        self.assertNotEqual(first, other)
        self.assertEqual(sorted(first), list(range(16)))

    def test_wiggly_weight_is_deterministic_and_bounded(self):
        first = MODEL.wiggly_weight(3, 0.25, True, seed=99)
        second = MODEL.wiggly_weight(3, 0.25, True, seed=99)
        self.assertEqual(first, second)
        self.assertGreaterEqual(first, 0.0)
        self.assertLessEqual(first, 1.0)
        self.assertEqual(MODEL.wiggly_weight(3, 0.25, False), 1.0)

    def test_wiggly_non_finite_inputs_are_safe(self):
        value = MODEL.wiggly_weight(0, float("nan"), True,
                                     rate=float("inf"), correlation=float("nan"),
                                     phase=float("nan"))
        self.assertTrue(0.0 <= value <= 1.0)

    def test_wiggly_modulates_selector_weight_not_raw_transform(self):
        glyphs = MODEL.virtual_glyphs("A")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "Square", "time": 0.25,
                                   "wiggly": {"enabled": True, "seed": 7}},
                    "properties": {"position": [10.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertGreaterEqual(states[0]["position"][0], 0.0)
        self.assertLessEqual(states[0]["position"][0], 10.0)

    def test_wiggly_uses_cluster_index_for_combining_sequence(self):
        glyphs = MODEL.virtual_glyphs("e\u0301A")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "Square", "time": 0.25,
                                   "wiggly": {"enabled": True, "seed": 7}},
                    "properties": {"position": [10.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual(states[0]["position"], states[1]["position"])

    def test_extra_weights_are_clamped_and_applied_per_glyph(self):
        glyphs = MODEL.virtual_glyphs("ABC")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "Square",
                                   "extraWeights": [0.0, 0.5, 2.0]},
                    "properties": {"position": [10.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual([state["position"][0] for state in states], [0.0, 5.0, 10.0])

    def test_extra_weight_non_finite_value_disables_only_that_glyph(self):
        glyphs = MODEL.virtual_glyphs("AB")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "Square",
                                   "extraWeights": [float("nan")]},
                    "properties": {"position": [10.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual(states[0]["position"][0], 0.0)
        self.assertEqual(states[1]["position"][0], 10.0)

    def test_animator_stack_uses_additive_position_multiplicative_scale_and_opacity(self):
        glyphs = MODEL.virtual_glyphs("A")
        animator = lambda position, scale, opacity: {
            "selector": {"start": 0.0, "end": 100.0, "shape": "Square"},
            "properties": {"position": position, "scale": scale,
                           "rotation": 0.0, "opacity": opacity},
        }
        states = MODEL.evaluate(glyphs, [
            animator([10.0, 2.0], 0.5, 0.8),
            animator([-3.0, 1.0], 2.0, 0.5),
        ])
        self.assertEqual(states[0]["position"], [7.0, 3.0])
        self.assertEqual(states[0]["scale"], 1.0)
        self.assertAlmostEqual(states[0]["opacity"], 0.4)

    def test_animator_stack_order_is_deterministic(self):
        glyphs = MODEL.virtual_glyphs("AB")
        stack = [{"selector": {"start": 0.0, "end": 50.0, "shape": "RampUp"},
                  "properties": {"position": [4.0, 0.0], "scale": 1.2,
                                 "rotation": 10.0, "opacity": 0.7}},
                 {"selector": {"start": 50.0, "end": 100.0, "shape": "RampDown"},
                  "properties": {"position": [0.0, 3.0], "scale": 0.8,
                                 "rotation": -5.0, "opacity": 0.5}}]
        first = MODEL.evaluate(glyphs, stack)
        second = MODEL.evaluate(glyphs, stack)
        self.assertEqual(first, second)
        self.assertTrue(all(0.0 <= state["opacity"] <= 1.0 for state in first))

    def test_animator_evaluation_applies_selector_order(self):
        glyphs = MODEL.virtual_glyphs("ABC")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "RampUp", "order": "reverse"},
                    "properties": {"position": [10.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual([state["position"][0] for state in states], [10.0, 5.0, 0.0])

    def test_animator_cluster_unit_gives_combining_codepoints_one_weight(self):
        glyphs = MODEL.virtual_glyphs("e\u0301A")
        animator = {"selector": {"unit": "cluster", "start": 0.0,
                                   "end": 100.0, "shape": "RampUp"},
                    "properties": {"position": [10.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual(states[0]["position"], states[1]["position"])
        self.assertNotEqual(states[1]["position"], states[2]["position"])

    def test_animator_default_percentage_domain_is_cluster_based(self):
        glyphs = MODEL.virtual_glyphs("e\u0301A")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "RampUp"},
                    "properties": {"position": [10.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual(states[0]["position"], states[1]["position"])

    def test_animator_numeric_properties_reach_final_state(self):
        glyphs = MODEL.virtual_glyphs("AB")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "Square"},
                    "properties": {"position": [0.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0,
                                   "skew": 12.0, "tracking": 4.0,
                                   "z": 3.0, "blur": 2.0,
                                   "strokeWidth": 1.5}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual(states[0]["skew"], 12.0)
        self.assertEqual(states[0]["tracking"], 4.0)
        self.assertEqual(states[0]["z"], 3.0)
        self.assertEqual(states[0]["blur"], 2.0)
        self.assertEqual(states[0]["strokeWidth"], 1.5)
        self.assertEqual(states[1]["position"][0], 4.0)

    def test_animator_fill_and_stroke_color_overrides_are_weighted(self):
        glyphs = MODEL.virtual_glyphs("AB")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "Square"},
                    "properties": {"position": [0.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0,
                                   "colorEnabled": True,
                                   "fillColor": [1.0, 0.0, 0.0, 1.0],
                                   "strokeEnabled": True,
                                   "strokeColor": [0.0, 0.0, 1.0, 1.0],
                                   "strokeWidth": 2.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual(states[0]["fillColor"], [1.0, 0.0, 0.0, 1.0])
        self.assertEqual(states[0]["strokeColor"], [0.0, 0.0, 1.0, 1.0])
        self.assertTrue(states[0]["hasColorOverride"])
        self.assertTrue(states[0]["hasStrokeOverride"])

    def test_partial_color_override_is_interpolated(self):
        glyphs = MODEL.virtual_glyphs("AB")
        animator = {"selector": {"start": 0.0, "end": 100.0,
                                   "shape": "RampUp"},
                    "properties": {"position": [0.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0,
                                   "colorEnabled": True,
                                   "fillColor": [0.0, 0.0, 0.0, 1.0]}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual(states[0]["fillColor"], [1.0, 1.0, 1.0, 1.0])
        self.assertEqual(states[1]["fillColor"], [0.0, 0.0, 0.0, 1.0])

    def test_invalid_enabled_color_is_rejected_before_state_generation(self):
        glyphs = MODEL.virtual_glyphs("A")
        for key, value in (("fillColor", [1.0, 0.0, 0.0]),
                           ("fillColor", [float("nan"), 0.0, 0.0, 1.0]),
                           ("strokeColor", ["blue"]),):
            properties = {"position": [0.0, 0.0], "scale": 1.0,
                          "rotation": 0.0, "opacity": 1.0,
                          "colorEnabled": key == "fillColor",
                          "strokeEnabled": key == "strokeColor",
                          key: value}
            animator = {"selector": {"start": 0.0, "end": 100.0,
                                       "shape": "Square"},
                        "properties": properties}
            with self.subTest(key=key, value=value):
                with self.assertRaises(ValueError):
                    MODEL.evaluate(glyphs, [animator])

    def test_animator_line_unit_does_not_use_source_codepoint_index(self):
        glyphs = MODEL.virtual_glyphs("A\nB")
        animator = {"selector": {"unit": "line", "start": 0.0,
                                   "end": 100.0, "shape": "RampUp"},
                    "properties": {"position": [10.0, 0.0], "scale": 1.0,
                                   "rotation": 0.0, "opacity": 1.0}}
        states = MODEL.evaluate(glyphs, [animator])
        self.assertEqual(states[0]["position"][0], 0.0)
        self.assertEqual(states[1]["position"][0], 10.0)

    def test_combining_mark_stays_in_one_cluster(self):
        glyphs = MODEL.virtual_glyphs("e\u0301")
        self.assertEqual(len(glyphs), 2)
        self.assertEqual({glyph["cluster"] for glyph in glyphs}, {0})

    def test_zwj_family_stays_in_one_cluster(self):
        glyphs = MODEL.virtual_glyphs("👨‍👩‍👧‍👦")
        self.assertEqual({glyph["cluster"] for glyph in glyphs}, {0})

    def test_regional_indicator_pair_stays_in_one_cluster(self):
        glyphs = MODEL.virtual_glyphs("🇯🇵")
        self.assertEqual({glyph["cluster"] for glyph in glyphs}, {0})

    def test_newline_is_not_a_glyph_and_creates_a_line(self):
        glyphs = MODEL.virtual_glyphs("A\nB")
        self.assertEqual(len(glyphs), 2)
        self.assertEqual({glyph["line"] for glyph in glyphs}, {0, 1})

    def test_audit_reports_cluster_and_line_counts(self):
        result = MODEL.audit_fixture({"id": "fixture", "tier": "contract", "text": "e\u0301\n🇯🇵"})
        self.assertEqual(result["virtualClusterCount"], 2)
        self.assertEqual(result["virtualLineCount"], 2)

    def test_word_identity_tracks_existing_and_added_word(self):
        diff = MODEL.word_identity_diff("Text Sample1", "Text Sample2")
        self.assertEqual(diff["matches"], [{"word": "Text", "beforeIndex": 0, "afterIndex": 0}])
        self.assertEqual(diff["added"], ["Sample2"])
        self.assertEqual(diff["removed"], ["Sample1"])
        self.assertEqual(diff["status"], "pass")

    def test_duplicate_word_match_is_explicitly_ambiguous(self):
        diff = MODEL.word_identity_diff("go go now", "go now go")
        self.assertEqual(diff["status"], "warning")
        self.assertEqual(diff["ambiguous"], ["go"])

    def test_layout_constraint_corrects_overlap_and_reports_it(self):
        result = MODEL.preserve_layout([0.0, 8.0, 30.0], [10.0, 10.0, 10.0], 50.0, 2.0)
        self.assertEqual(result["status"], "warning")
        self.assertEqual(result["positions"], [0.0, 12.0, 30.0])
        self.assertTrue(result["corrections"])
        self.assertFalse(result["overflow"])

    def test_layout_constraint_keeps_glyphs_inside_box(self):
        result = MODEL.preserve_layout([-5.0, 48.0], [10.0, 10.0], 50.0)
        self.assertEqual(result["positions"], [0.0, 40.0])
        self.assertFalse(result["overflow"])

    def test_layout_constraint_rejects_invalid_width(self):
        result = MODEL.preserve_layout([0.0], [-1.0], 50.0)
        self.assertEqual(result, {"status": "error", "code": "invalidGlyphWidth"})

    def test_layout_constraint_rejects_non_finite_position_and_box(self):
        self.assertEqual(
            MODEL.preserve_layout([float("nan")], [10.0], 50.0),
            {"status": "error", "code": "invalidGlyphPosition"})
        self.assertEqual(
            MODEL.preserve_layout([0.0], [10.0], float("inf")),
            {"status": "error", "code": "invalidLayoutConstraint"})
        self.assertEqual(
            MODEL.preserve_layout([0.0], [10.0], 50.0, float("nan")),
            {"status": "error", "code": "invalidLayoutConstraint"})

    def test_core_snapshot_matching_structure_passes(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "A\nB"}
        snapshot = {"GlyphCount": 2, "ClusterCount": 2, "LineCount": 2,
                    "states": [{"index": 0, "scale": 1.0, "rotation": 0.0, "opacity": 1.0}]}
        self.assertEqual(MODEL.compare_core_snapshot(fixture, snapshot)["status"], "pass")

    def test_complete_core_snapshot_contract_passes(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "AB"}
        state = {"index": 0, "position": [1.0, -2.0], "scale": 0.9,
                 "rotation": 12.0, "opacity": 0.8, "skew": 2.0,
                 "tracking": 1.0, "z": 3.0, "blur": 0.5,
                 "strokeWidth": 1.0, "fillColor": [1.0, 0.0, 0.0, 1.0],
                 "strokeColor": [0.0, 0.0, 1.0, 1.0]}
        snapshot = {"GlyphCount": 2, "ClusterCount": 2, "LineCount": 1,
                    "InstanceCount": 2, "StateCount": 1, "states": [state]}
        self.assertEqual(MODEL.compare_core_snapshot(fixture, snapshot)["status"], "pass")

    def test_core_snapshot_structure_mismatch_is_reported(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "e\u0301"}
        snapshot = {"GlyphCount": 2, "ClusterCount": 2, "LineCount": 1, "states": []}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        self.assertEqual(result["status"], "error")
        self.assertTrue(any(d["code"] == "structureMismatch" for d in result["differences"]))

    def test_core_snapshot_non_finite_state_is_reported(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "A"}
        snapshot = {"GlyphCount": 1, "ClusterCount": 1, "LineCount": 1,
                    "states": [{"index": 0, "scale": float("nan"), "rotation": 0.0, "opacity": 1.0}]}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        self.assertEqual(result["status"], "error")
        self.assertEqual(result["differences"][0]["code"], "coreNonFinite")

    def test_core_snapshot_rejects_duplicate_and_out_of_range_indices(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "AB"}
        snapshot = {"GlyphCount": 2, "ClusterCount": 2, "LineCount": 1,
                    "states": [{"index": 0, "scale": 1.0},
                               {"index": 0, "scale": 1.0},
                               {"index": 9, "scale": 1.0}]}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        codes = {difference["code"] for difference in result["differences"]}
        self.assertIn("duplicateCoreIndex", codes)
        self.assertIn("coreIndexOutOfRange", codes)

    def test_core_snapshot_rejects_invalid_position_and_property_ranges(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "A"}
        snapshot = {"GlyphCount": 1, "ClusterCount": 1, "LineCount": 1,
                    "states": [{"index": 0, "position": ["NaN"],
                                 "scale": -1.0, "opacity": 2.0}]}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        codes = {difference["code"] for difference in result["differences"]}
        self.assertIn("coreNonFinite", codes)
        self.assertIn("coreScaleOutOfRange", codes)
        self.assertIn("coreOpacityOutOfRange", codes)

    def test_core_snapshot_rejects_invalid_color_state(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "A"}
        snapshot = {"GlyphCount": 1, "ClusterCount": 1, "LineCount": 1,
                    "states": [{"index": 0, "fillColor": [1.0, 0.0, 2.0, 1.0],
                                 "strokeColor": ["x", 0.0, 0.0, 1.0]}]}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        colors = [d for d in result["differences"] if d["code"] == "coreColorOutOfRange"]
        self.assertEqual({d["field"] for d in colors}, {"fillColor", "strokeColor"})

    def test_core_snapshot_rejects_non_finite_extended_animator_state(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "A"}
        snapshot = {"GlyphCount": 1, "ClusterCount": 1, "LineCount": 1,
                    "states": [{"index": 0, "skew": float("nan"),
                                 "tracking": float("inf"), "z": "bad",
                                 "blur": 0.0, "strokeWidth": 1.0}]}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        fields = {d["field"] for d in result["differences"] if d["code"] == "coreNonFinite"}
        self.assertEqual(fields, {"skew", "tracking", "z"})

    def test_core_snapshot_rejects_non_list_states(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "A"}
        snapshot = {"GlyphCount": 1, "ClusterCount": 1, "LineCount": 1,
                    "states": {"index": 0}}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        self.assertEqual(result["status"], "error")
        self.assertIn("invalidCoreStates", {d["code"] for d in result["differences"]})

    def test_core_snapshot_checks_gpu_instance_and_state_counts(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "AB"}
        snapshot = {"GlyphCount": 2, "ClusterCount": 2, "LineCount": 1,
                    "InstanceCount": 1, "StateCount": 3,
                    "states": [{"index": 0, "scale": 1.0}]}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        codes = {difference["code"] for difference in result["differences"]}
        self.assertIn("instanceCountMismatch", codes)
        self.assertIn("stateCountMismatch", codes)

    def test_core_snapshot_rejects_non_integer_gpu_counts(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "A"}
        snapshot = {"GlyphCount": 1, "ClusterCount": 1, "LineCount": 1,
                    "InstanceCount": True, "StateCount": 1, "states": []}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        codes = {difference["code"] for difference in result["differences"]}
        self.assertIn("invalidInstanceCount", codes)

    def test_core_snapshot_does_not_treat_boolean_as_index_or_position_number(self):
        fixture = {"id": "fixture", "tier": "contract", "text": "A"}
        snapshot = {"GlyphCount": 1, "ClusterCount": 1, "LineCount": 1,
                    "states": [{"index": True, "position": [False, 0.0],
                                 "scale": 1.0, "opacity": 1.0}]}
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        codes = {d["code"] for d in result["differences"]}
        self.assertIn("coreIndexOutOfRange", codes)
        self.assertIn("coreNonFinite", codes)

    def test_deterministic_boundary_matrix_preserves_invariants(self):
        texts = ["", "A", "e\u0301", "😀", "👨‍👩‍👧‍👦", "🇯🇵", "A\nB", "日本語 😀"]
        for text in texts:
            for repeat in range(1, 8):
                glyphs = MODEL.virtual_glyphs(text * repeat)
                states = MODEL.evaluate(glyphs, [{
                    "selector": {"start": 100.0 - repeat * 13.0,
                                  "end": repeat * 17.0, "shape": "Smooth"},
                    "properties": {"position": [repeat, -repeat],
                                   "scale": 0.5 + repeat * 0.05,
                                   "rotation": repeat * 11.0,
                                   "opacity": 1.0 / (repeat + 1)},
                }])
                self.assertEqual(len(states), len(glyphs), text)
                for state in states:
                    self.assertTrue(all(__import__("math").isfinite(value)
                                        for value in state["position"] +
                                        [state["scale"], state["rotation"], state["opacity"]]))
                    self.assertGreaterEqual(state["opacity"], 0.0)
                    self.assertLessEqual(state["opacity"], 1.0)

    def test_stress_5000_glyphs_preserves_state_count_and_finiteness(self):
        text = ("Text 😀 e\u0301 日本語\n" * 625)[:5000]
        glyphs = MODEL.virtual_glyphs(text)
        states = MODEL.evaluate(glyphs, [{
            "selector": {"start": 15.0, "end": 85.0, "shape": "Smooth"},
            "properties": {"position": [12.0, -4.0], "scale": 0.8,
                           "rotation": 15.0, "opacity": 0.5},
        }])
        self.assertGreaterEqual(len(glyphs), 4000)
        self.assertEqual(len(states), len(glyphs))
        self.assertTrue(all(
            all(__import__("math").isfinite(value)
                for value in state["position"] +
                [state["scale"], state["rotation"], state["opacity"]])
            for state in states))

    def test_timeline_stagger_delays_later_glyphs(self):
        progress = MODEL.timeline_progress(3, 1.0, 0.25, 0.5, "linear")
        self.assertEqual(progress, [0.5, 0.25, 0.0])

    def test_timeline_before_and_after_range_is_clamped(self):
        self.assertEqual(MODEL.timeline_progress(2, 1.0, 0.1, -1.0), [0.0, 0.0])
        self.assertEqual(MODEL.timeline_progress(2, 1.0, 0.1, 10.0), [1.0, 1.0])

    def test_timeline_rejects_invalid_values_and_easing(self):
        with self.assertRaises(ValueError):
            MODEL.timeline_progress(1, -1.0, 0.0, 0.0)
        with self.assertRaises(ValueError):
            MODEL.timeline_progress(1, 1.0, 0.0, 0.0, "unknown")

    def test_spring_is_deterministic_and_can_overshoot(self):
        first = MODEL.timeline_progress(1, 1.0, 0.0, 0.25, "spring")
        second = MODEL.timeline_progress(1, 1.0, 0.0, 0.25, "spring")
        self.assertEqual(first, second)
        self.assertGreater(first[0], 1.0)
        self.assertTrue(__import__("math").isfinite(first[0]))

    def test_spring_overshoot_is_clamped_for_opacity_and_scale(self):
        self.assertEqual(MODEL.apply_timed_property(0.0, 1.0, 1.2, "opacity"), 1.0)
        self.assertEqual(MODEL.apply_timed_property(-0.2, 1.0, 0.0, "scale"), 0.0)

    def test_spring_overshoot_is_preserved_for_motion(self):
        self.assertEqual(MODEL.apply_timed_property(0.0, 10.0, 1.2, "position"), 12.0)

    def test_unknown_timed_property_is_rejected(self):
        with self.assertRaises(ValueError):
            MODEL.apply_timed_property(0.0, 1.0, 0.5, "unknown")

    def test_non_finite_timed_property_is_rejected_before_clamping(self):
        for values in ((float("nan"), 1.0, 0.5),
                       (0.0, float("inf"), 0.5),
                       (0.0, 1.0, float("nan"))):
            with self.subTest(values=values):
                with self.assertRaises(ValueError):
                    MODEL.apply_timed_property(*values, "opacity")

    def test_timeline_cli_exposes_selected_easing(self):
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--timeline", "1", "1", "0", "0.25",
             "--easing", "spring"], capture_output=True, text=True, check=True)
        result = json.loads(completed.stdout)
        self.assertGreater(result["progress"][0], 1.0)

    def test_intent_example_array_can_be_previewed_by_index(self):
        examples = json.loads((MODEL.ROOT / "intent_examples.json").read_text())
        self.assertEqual(len(examples), 2)
        emoji_fixture = next(f for f in MODEL.load_fixtures() if f["id"] == "emoji_sentence")
        result = MODEL.preview_intent(examples[1]["intent"], emoji_fixture)
        self.assertEqual(result["status"], "pass")

    def test_intent_and_unicode_fixture_json_round_trip_preserves_preview(self):
        fixture = next(f for f in MODEL.load_fixtures() if f["id"] == "emoji_sentence")
        intent = json.loads(json.dumps(self.valid_intent, ensure_ascii=False))
        intent["selection"] = {"unit": "regex", "order": "natural", "pattern": "😀"}
        before = MODEL.preview_intent(intent, fixture)
        restored_fixture = json.loads(json.dumps(fixture, ensure_ascii=False))
        restored_intent = json.loads(json.dumps(intent, ensure_ascii=False))
        after = MODEL.preview_intent(restored_intent, restored_fixture)
        self.assertEqual(before["selectedGlyphCount"], after["selectedGlyphCount"])
        self.assertEqual(before["operators"], after["operators"])
        self.assertEqual(before["diagnostics"], after["diagnostics"])

    def test_all_intent_examples_round_trip_without_diagnostic_drift(self):
        examples = json.loads((MODEL.ROOT / "intent_examples.json").read_text(encoding="utf-8"))
        fixtures = {fixture["id"]: fixture for fixture in MODEL.load_fixtures()}
        targets = [fixtures["text_sample1"], fixtures["emoji_sentence"]]
        for example, fixture in zip(examples, targets):
            intent = json.loads(json.dumps(example["intent"], ensure_ascii=False))
            original = MODEL.preview_intent(intent, fixture)
            restored = MODEL.preview_intent(
                json.loads(json.dumps(intent, ensure_ascii=False)),
                json.loads(json.dumps(fixture, ensure_ascii=False)))
            self.assertEqual(original["status"], restored["status"], example["name"])
            self.assertEqual(original["diagnostics"], restored["diagnostics"], example["name"])

    def test_intent_example_array_cli_integration(self):
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--intent",
             str(MODEL.ROOT / "intent_examples.json"), "--fixture", "emoji_sentence",
             "--intent-index", "1"], capture_output=True, text=True, check=True)
        result = json.loads(completed.stdout)
        self.assertEqual(result["result"]["status"], "pass")
        self.assertEqual(result["result"]["operators"], ["scale", "rotation"])

    def test_intent_cli_json_out_writes_machine_readable_report(self):
        output = MODEL.ROOT / "reports" / "test_cli" / "preview.json"
        output.parent.mkdir(parents=True, exist_ok=True)
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--intent",
             str(MODEL.ROOT / "intent_examples.json"), "--fixture", "text_sample1",
             "--json-out", str(output)], capture_output=True, text=True, check=True)
        self.assertEqual(completed.stdout, "")
        report = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(report["model"], "design-reference-v1")
        self.assertEqual(report["result"]["status"], "pass")
        output.unlink(missing_ok=True)

    def test_tier_cli_json_out_writes_fixture_audit_report(self):
        output = MODEL.ROOT / "reports" / "test_cli" / "contract.json"
        output.unlink(missing_ok=True)
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--tier", "contract",
             "--json-out", str(output)], capture_output=True, text=True, check=True)
        self.assertEqual(completed.stdout, "")
        report = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(report["model"], "design-reference-v1")
        self.assertEqual(report["status"], "pass")
        self.assertEqual(report["fixtureCount"], 7)
        output.unlink(missing_ok=True)

    def test_tier_cli_report_contains_evaluated_state_summary(self):
        output = MODEL.ROOT / "reports" / "test_cli" / "smoke_states.json"
        output.unlink(missing_ok=True)
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--tier", "smoke",
             "--json-out", str(output)], capture_output=True, text=True, check=True)
        report = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(completed.stdout, "")
        for result in report["results"]:
            self.assertEqual(result["stateCount"], result["virtualGlyphCount"])
            self.assertTrue(result["stateFinite"])
            self.assertLessEqual(len(result["stateSample"]), 4)
        output.unlink(missing_ok=True)

    def test_full_audit_report_round_trips_as_unicode_json(self):
        fixtures = MODEL.load_fixtures()
        report = {"model": "design-reference-v1", "fixtureCount": len(fixtures),
                  "results": [MODEL.audit_fixture(fixture) for fixture in fixtures]}
        restored = json.loads(json.dumps(report, ensure_ascii=False))
        self.assertEqual(restored["fixtureCount"], len(fixtures))
        self.assertEqual(restored["results"][-1]["text"], fixtures[-1]["text"])
        self.assertEqual(restored["results"][-1]["stateCount"],
                         restored["results"][-1]["virtualGlyphCount"])

    def test_intent_cli_error_returns_nonzero_and_machine_readable_output(self):
        invalid = MODEL.ROOT / "reports" / "invalid_intent.json"
        invalid.parent.mkdir(parents=True, exist_ok=True)
        invalid.write_text(json.dumps({**self.valid_intent,
                                       "selection": {"unit": "regex",
                                                     "order": "natural",
                                                     "pattern": "["}}), encoding="utf-8")
        try:
            completed = subprocess.run(
                [sys.executable, str(MODULE_PATH), "--intent", str(invalid)],
                capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            report = json.loads(completed.stdout)
            self.assertEqual(report["result"]["status"], "error")
            self.assertEqual(completed.stderr, "")
        finally:
            invalid.unlink(missing_ok=True)

    def test_preview_handles_non_object_intent_without_traceback(self):
        result = MODEL.preview_intent("not an object", self.fixture)
        self.assertEqual(result["status"], "error")
        self.assertIn("invalidIntentType", {d["code"] for d in result["diagnostics"]})

    def test_malformed_operator_list_preview_is_machine_readable(self):
        intent = {**self.valid_intent, "operators": "opacity"}
        result = MODEL.preview_intent(intent, self.fixture)
        self.assertEqual(result["status"], "error")
        self.assertIn("invalidOperatorList", {d["code"] for d in result["diagnostics"]})
        self.assertEqual(result["operatorCount"], 0)

    def test_malformed_operator_list_cli_has_no_traceback(self):
        invalid = MODEL.ROOT / "reports" / "invalid_operator_intent.json"
        invalid.parent.mkdir(parents=True, exist_ok=True)
        invalid.write_text(json.dumps({**self.valid_intent, "operators": "opacity"}),
                           encoding="utf-8")
        try:
            completed = subprocess.run(
                [sys.executable, str(MODULE_PATH), "--intent", str(invalid)],
                capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            report = json.loads(completed.stdout)
            self.assertEqual(report["result"]["status"], "error")
            self.assertIn("invalidOperatorList",
                          {d["code"] for d in report["result"]["diagnostics"]})
            self.assertEqual(completed.stderr, "")
        finally:
            invalid.unlink(missing_ok=True)

    def test_invalid_intent_json_out_persists_error_report(self):
        intent_file = MODEL.ROOT / "reports" / "invalid_intent_for_output.json"
        output = MODEL.ROOT / "reports" / "invalid_intent_report.json"
        intent_file.parent.mkdir(parents=True, exist_ok=True)
        intent_file.write_text(json.dumps({**self.valid_intent,
                                           "selection": {"unit": "regex",
                                                         "order": "natural",
                                                         "pattern": "["}}),
                               encoding="utf-8")
        output.unlink(missing_ok=True)
        try:
            completed = subprocess.run(
                [sys.executable, str(MODULE_PATH), "--intent", str(intent_file),
                 "--json-out", str(output)], capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, "")
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(report["result"]["status"], "error")
            self.assertIn("invalidRegex",
                          {d["code"] for d in report["result"]["diagnostics"]})
        finally:
            intent_file.unlink(missing_ok=True)
            output.unlink(missing_ok=True)

    def test_layout_cli_malformed_json_is_machine_readable(self):
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--layout-check", "[", "[10]"],
            capture_output=True, text=True)
        self.assertNotEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertEqual(result["status"], "error")
        self.assertEqual(completed.stderr, "")

    def test_layout_cli_rejects_non_array_json(self):
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--layout-check", "1", "[10]"],
            capture_output=True, text=True)
        self.assertNotEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertEqual(result["code"], "layout arrays required")

    def test_word_diff_cli_returns_identity_report(self):
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--word-diff", "Text Sample1", "Text Sample2"],
            capture_output=True, text=True, check=True)
        result = json.loads(completed.stdout)
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["added"], ["Sample2"])
        self.assertEqual(result["removed"], ["Sample1"])

    def test_layout_cli_valid_input_returns_corrections(self):
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--layout-check", "[0,8,30]", "[10,10,10]",
             "--box-width", "50"], capture_output=True, text=True, check=True)
        result = json.loads(completed.stdout)
        self.assertEqual(result["status"], "warning")
        self.assertEqual(result["positions"], [0, 10.0, 30])

    def test_timeline_cli_invalid_numeric_input_is_machine_readable(self):
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--timeline", "1", "nan", "0", "0"],
            capture_output=True, text=True)
        self.assertNotEqual(completed.returncode, 0)
        result = json.loads(completed.stdout)
        self.assertEqual(result["status"], "error")
        self.assertEqual(completed.stderr, "")

    def test_regex_preview_reports_empty_selection(self):
        intent = {**self.valid_intent,
                  "selection": {"unit": "regex", "order": "natural", "pattern": "😀"}}
        result = MODEL.preview_intent(intent, self.fixture)
        self.assertEqual(result["selectedGlyphCount"], 0)
        self.assertIn({"code": "selectionEmpty", "severity": "warning"}, result["diagnostics"])

    def test_regex_preview_counts_matching_glyphs(self):
        fixture = {"id": "emoji", "tier": "smoke", "text": "A 😀 B"}
        intent = {**self.valid_intent,
                  "selection": {"unit": "regex", "order": "natural", "pattern": "😀"}}
        result = MODEL.preview_intent(intent, fixture)
        self.assertEqual(result["selectedGlyphCount"], 1)

    def test_regex_selection_expands_to_entire_unicode_cluster(self):
        fixture = {"id": "emoji", "tier": "smoke", "text": "😀👍🏽"}
        intent = {**self.valid_intent,
                  "selection": {"unit": "regex", "order": "natural", "pattern": "👍🏽"}}
        result = MODEL.preview_intent(intent, fixture)
        selected = MODEL.select_glyphs(MODEL.virtual_glyphs(fixture["text"]),
                                       intent["selection"], fixture)
        self.assertEqual(result["selectedGlyphCount"], 2)
        self.assertEqual({glyph["cluster"] for glyph in selected}, {1})

    def test_regex_selection_expands_combining_mark_to_base_cluster(self):
        fixture = {"id": "combining", "tier": "smoke", "text": "e\u0301A"}
        intent = {**self.valid_intent,
                  "selection": {"unit": "regex", "order": "natural", "pattern": "\u0301"}}
        result = MODEL.preview_intent(intent, fixture)
        self.assertEqual(result["selectedGlyphCount"], 2)

    def test_zwj_sequence_is_one_animation_cluster(self):
        fixture = {"id": "zwj", "tier": "smoke", "text": "A 👩‍💻 B"}
        glyphs = MODEL.virtual_glyphs(fixture["text"])
        emoji_clusters = {
            glyph["cluster"] for glyph in glyphs
            if glyph["char"] in "👩‍💻"
        }
        self.assertEqual(len(emoji_clusters), 1)
        selected = MODEL.select_glyphs(
            glyphs,
            {"unit": "character", "order": "natural", "amount": 100},
            fixture,
        )
        self.assertEqual(
            {glyph["cluster"] for glyph in selected
             if glyph["char"] in "👩‍💻"},
            emoji_clusters,
        )

    def test_empty_selection_makes_preview_warning_not_pass(self):
        intent = {**self.valid_intent,
                  "selection": {"unit": "regex", "order": "natural", "pattern": "😀"}}
        result = MODEL.preview_intent(intent, self.fixture)
        self.assertEqual(result["status"], "warning")

    def test_invalid_regex_is_reported_as_error(self):
        intent = {**self.valid_intent,
                  "selection": {"unit": "regex", "order": "natural", "pattern": "["}}
        diagnostics = MODEL.validate_intent(intent, self.fixture)
        self.assertIn("invalidRegex", {d["code"] for d in diagnostics})

    def test_tag_selection_requires_value(self):
        intent = {**self.valid_intent,
                  "selection": {"unit": "tag", "order": "natural"}}
        diagnostics = MODEL.validate_intent(intent, self.fixture)
        self.assertIn("missingSelectionValue", {d["code"] for d in diagnostics})

    def test_line_selection_requires_integer_and_counts_line(self):
        intent = {**self.valid_intent,
                  "selection": {"unit": "line", "order": "natural", "value": 1}}
        fixture = {"id": "lines", "tier": "contract", "text": "A\nBC"}
        result = MODEL.preview_intent(intent, fixture)
        self.assertEqual(result["selectedGlyphCount"], 2)

        invalid = {**intent, "selection": {"unit": "line", "order": "natural", "value": 9}}
        empty = MODEL.preview_intent(invalid, fixture)
        self.assertEqual(empty["selectedGlyphCount"], 0)
        self.assertIn("selectionEmpty", {d["code"] for d in empty["diagnostics"]})

    def test_line_and_tag_selection_values_have_valid_types(self):
        line = {**self.valid_intent,
                "selection": {"unit": "line", "order": "natural", "value": -1}}
        tag = {**self.valid_intent,
               "selection": {"unit": "tag", "order": "natural", "value": 2}}
        line_codes = {d["code"] for d in MODEL.validate_intent(line, self.fixture)}
        tag_codes = {d["code"] for d in MODEL.validate_intent(tag, self.fixture)}
        self.assertIn("invalidSelectionValue", line_codes)
        self.assertIn("invalidSelectionValue", tag_codes)

    def test_grapheme_selection_does_not_confuse_codepoints_with_units(self):
        fixture = {"id": "combining", "tier": "contract", "text": "e\u0301"}
        intent = {**self.valid_intent,
                  "selection": {"unit": "grapheme", "order": "natural"}}
        result = MODEL.preview_intent(intent, fixture)
        self.assertEqual(result["selectedGlyphCount"], 2)
        self.assertEqual(result["selectedUnitCount"], 1)

    def test_tag_selection_requires_explicit_metadata(self):
        intent = {**self.valid_intent,
                  "selection": {"unit": "tag", "order": "natural", "value": "headline"}}
        result = MODEL.preview_intent(intent, self.fixture)
        self.assertEqual(result["status"], "warning")
        self.assertIn("tagMetadataUnavailable", {d["code"] for d in result["diagnostics"]})

    def test_tag_selection_uses_fixture_indices_when_metadata_exists(self):
        fixture = {"id": "tagged", "tier": "contract", "text": "Text1",
                   "tags": {"headline": [0, 1, 2]}}
        intent = {**self.valid_intent,
                  "selection": {"unit": "tag", "order": "natural", "value": "headline"}}
        result = MODEL.preview_intent(intent, fixture)
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["selectedGlyphCount"], 3)

    def test_empty_paragraph_selection_is_warning(self):
        fixture = {"id": "empty", "tier": "smoke", "text": ""}
        intent = {**self.valid_intent,
                  "selection": {"unit": "paragraph", "order": "natural"}}
        result = MODEL.preview_intent(intent, fixture)
        self.assertEqual(result["status"], "warning")
        self.assertEqual(result["selectedUnitCount"], 0)

    def test_intent_index_out_of_range_is_machine_readable_error(self):
        completed = subprocess.run(
            [sys.executable, str(MODULE_PATH), "--intent",
             str(MODEL.ROOT / "intent_examples.json"), "--intent-index", "99"],
            capture_output=True, text=True)
        self.assertNotEqual(completed.returncode, 0)
        self.assertEqual(json.loads(completed.stdout)["code"], "intentIndexOutOfRange")

    def test_empty_intent_array_is_machine_readable_error(self):
        empty = MODEL.ROOT / "empty_intents.json"
        empty.write_text("[]", encoding="utf-8")
        try:
            completed = subprocess.run(
                [sys.executable, str(MODULE_PATH), "--intent", str(empty)],
                capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            self.assertEqual(json.loads(completed.stdout)["code"], "emptyIntentList")
        finally:
            empty.unlink(missing_ok=True)

    def test_unknown_fixture_is_machine_readable_error(self):
        intent_file = MODEL.ROOT / "intent_word_reveal.json"
        intent_file.write_text(json.dumps(self.valid_intent), encoding="utf-8")
        try:
            completed = subprocess.run(
                [sys.executable, str(MODULE_PATH), "--intent", str(intent_file),
                 "--fixture", "does-not-exist"], capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            self.assertEqual(json.loads(completed.stdout)["code"], "fixtureNotFound")
        finally:
            intent_file.unlink(missing_ok=True)

    def test_malformed_intent_json_is_machine_readable_error(self):
        malformed = MODEL.ROOT / "malformed_intent.json"
        malformed.write_text('{"action":', encoding="utf-8")
        try:
            completed = subprocess.run(
                [sys.executable, str(MODULE_PATH), "--intent", str(malformed)],
                capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            self.assertEqual(json.loads(completed.stdout)["code"], "invalidJson")
            self.assertEqual(completed.stderr, "")
        finally:
            malformed.unlink(missing_ok=True)

    def test_malformed_core_snapshot_is_machine_readable_error(self):
        malformed = MODEL.ROOT / "malformed_snapshot.json"
        malformed.write_text('{"GlyphCount":', encoding="utf-8")
        try:
            completed = subprocess.run(
                [sys.executable, str(MODULE_PATH), "--core-snapshot", str(malformed),
                 "--fixture", "text_sample1"], capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            self.assertEqual(json.loads(completed.stdout)["code"], "invalidJson")
            self.assertEqual(completed.stderr, "")
        finally:
            malformed.unlink(missing_ok=True)

    def test_non_object_core_snapshot_is_machine_readable_error(self):
        malformed = MODEL.ROOT / "reports" / "array_snapshot.json"
        malformed.parent.mkdir(parents=True, exist_ok=True)
        malformed.write_text("[]", encoding="utf-8")
        try:
            completed = subprocess.run(
                [sys.executable, str(MODULE_PATH), "--core-snapshot", str(malformed),
                 "--fixture", "text_sample1"], capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            result = json.loads(completed.stdout)
            self.assertIn("invalidCoreSnapshot",
                          {d["code"] for d in result["differences"]})
            self.assertEqual(completed.stderr, "")
        finally:
            malformed.unlink(missing_ok=True)

    def test_core_like_snapshot_fixture_passes(self):
        snapshot = json.loads((MODEL.ROOT / "snapshots" / "text_sample1_core_like.json").read_text())
        fixture = next(f for f in MODEL.load_fixtures() if f["id"] == "text_sample1")
        self.assertEqual(MODEL.compare_core_snapshot(fixture, snapshot)["status"], "pass")

    def test_bad_structure_fixture_fails(self):
        snapshot = json.loads((MODEL.ROOT / "snapshots" / "text_sample1_bad_structure.json").read_text())
        fixture = next(f for f in MODEL.load_fixtures() if f["id"] == "text_sample1")
        result = MODEL.compare_core_snapshot(fixture, snapshot)
        self.assertEqual(result["status"], "error")
        self.assertIn("structureMismatch", {d["code"] for d in result["differences"]})

    def test_preview_is_non_mutating_and_explainable(self):
        before = json.dumps(self.valid_intent, sort_keys=True)
        preview = MODEL.preview_intent(self.valid_intent, self.fixture)
        after = json.dumps(self.valid_intent, sort_keys=True)
        self.assertEqual(before, after)
        self.assertEqual(preview["status"], "pass")
        self.assertEqual(preview["selectedGlyphCount"], 11)
        self.assertEqual(preview["operators"], ["opacity"])

    def test_evaluate_is_non_mutating_for_glyphs_and_animators(self):
        glyphs = MODEL.virtual_glyphs("e\u0301 😀 AB")
        animators = [{
            "selector": {"start": 0.0, "end": 100.0, "shape": "Smooth",
                         "order": "center_out"},
            "properties": {"position": [4.0, -2.0], "scale": 0.9,
                           "rotation": 12.0, "opacity": 0.8, "tracking": 1.5},
        }]
        before_glyphs = json.dumps(glyphs, ensure_ascii=False, sort_keys=True)
        before_animators = json.dumps(animators, ensure_ascii=False, sort_keys=True)
        MODEL.evaluate(glyphs, animators)
        self.assertEqual(before_glyphs, json.dumps(glyphs, ensure_ascii=False, sort_keys=True))
        self.assertEqual(before_animators, json.dumps(animators, ensure_ascii=False, sort_keys=True))

    def test_long_multi_animator_evaluation_remains_finite(self):
        glyphs = MODEL.virtual_glyphs("Text 😀 e\u0301\n" * 600)
        animators = []
        for index in range(16):
            animators.append({
                "selector": {"start": index * 3.0, "end": 100.0 - index * 2.0,
                             "shape": "Smooth", "order": "random_stable",
                             "seed": index, "wiggly": {"enabled": True,
                                                         "seed": index}},
                "properties": {"position": [index * 0.5, -index * 0.25],
                               "scale": 1.0 - index * 0.01,
                               "rotation": index * 2.0, "opacity": 1.0 - index * 0.02,
                               "skew": index * 0.1, "tracking": 0.2,
                               "z": index * 0.3, "blur": index * 0.05},
            })
        states = MODEL.evaluate(glyphs, animators)
        self.assertEqual(len(states), len(glyphs))
        for state in states:
            values = state["position"] + [state[key] for key in
                      ("scale", "rotation", "opacity", "skew", "tracking", "z", "blur")]
            self.assertTrue(all(__import__("math").isfinite(value) for value in values))
            self.assertGreaterEqual(state["opacity"], 0.0)


class ShapedGlyphContractTests(unittest.TestCase):
    ROOT = Path(__file__).parents[2]

    def test_core_keeps_shaped_glyph_identity(self):
        contract = (self.ROOT / "ArtifactCore/include/Text/TextLayoutContract.ixx").read_text(encoding="utf-8")
        atlas = (self.ROOT / "ArtifactCore/include/Text/GlyphAtlas.ixx").read_text(encoding="utf-8")
        self.assertIn("shapedGlyphIndex", contract)
        self.assertIn("shapedGlyphIndices", contract)
        self.assertIn("shapedGlyphIndex", atlas)

    def test_unicode_edge_cases_have_explicit_boundaries(self):
        shaping = (self.ROOT / "ArtifactCore/src/Text/TextShapingBackend.cppm").read_text(encoding="utf-8")
        submitter = (self.ROOT / "Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm").read_text(encoding="utf-8")
        self.assertIn("0x1F000", shaping)
        self.assertIn("stringMappingValid", shaping)
        self.assertIn("glyph.shapedGlyphIndex == 0", submitter)

    def test_gpu_smoke_validates_executable_freshness_and_qpa(self):
        script = (Path(__file__).with_name("run_gpu_smoke.ps1")).read_text(encoding="utf-8")
        self.assertIn("QT_QPA_PLATFORM_PLUGIN_PATH", script)
        self.assertIn("GPU smoke executable is stale", script)


if __name__ == "__main__":
    unittest.main()
