import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path


class ArtifactRendererCliContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.executable = sys.argv[1]
        cls.valid_job = Path(sys.argv[2])
        del sys.argv[1:3]

    def run_cli(self, *arguments):
        return subprocess.run(
            [self.executable, *arguments],
            capture_output=True,
            text=True,
            check=False,
            timeout=10,
        )

    def load_valid_job(self):
        with self.valid_job.open(encoding="utf-8") as job_file:
            return json.load(job_file)

    @staticmethod
    def write_job(directory, name, job):
        path = Path(directory) / name
        path.write_text(json.dumps(job), encoding="utf-8")
        return path

    @staticmethod
    def read_rgba_png(path):
        data = path.read_bytes()
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            raise AssertionError("render output is not a PNG")

        offset = 8
        compressed = bytearray()
        while offset < len(data):
            length = struct.unpack_from(">I", data, offset)[0]
            chunk_type = data[offset + 4:offset + 8]
            chunk_data = data[offset + 8:offset + 8 + length]
            offset += 12 + length
            if chunk_type == b"IHDR":
                width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                    ">IIBBBBB", chunk_data
                )
                if (bit_depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
                    raise AssertionError("expected a non-interlaced 8-bit RGBA PNG")
            elif chunk_type == b"IDAT":
                compressed.extend(chunk_data)
            elif chunk_type == b"IEND":
                break

        raw = zlib.decompress(compressed)
        stride = width * 4
        rows = []
        previous = bytearray(stride)
        cursor = 0
        for _ in range(height):
            filter_type = raw[cursor]
            cursor += 1
            scanline = bytearray(raw[cursor:cursor + stride])
            cursor += stride
            for index in range(stride):
                left = scanline[index - 4] if index >= 4 else 0
                above = previous[index]
                upper_left = previous[index - 4] if index >= 4 else 0
                if filter_type == 1:
                    scanline[index] = (scanline[index] + left) & 0xFF
                elif filter_type == 2:
                    scanline[index] = (scanline[index] + above) & 0xFF
                elif filter_type == 3:
                    scanline[index] = (scanline[index] + ((left + above) // 2)) & 0xFF
                elif filter_type == 4:
                    estimate = left + above - upper_left
                    distances = (abs(estimate - left), abs(estimate - above), abs(estimate - upper_left))
                    predictor = left if distances[0] <= distances[1] and distances[0] <= distances[2] else (
                        above if distances[1] <= distances[2] else upper_left
                    )
                    scanline[index] = (scanline[index] + predictor) & 0xFF
                elif filter_type != 0:
                    raise AssertionError(f"unsupported PNG filter: {filter_type}")
            rows.append(scanline)
            previous = scanline

        pixels = [tuple(row[index:index + 4]) for row in rows for index in range(0, stride, 4)]
        return width, height, pixels

    @staticmethod
    def add_valid_component_bake(job, frame_count=1):
        job["snapshot"]["composition"]["layerComponentSimulationBake"] = {
            "version": 1,
            "compositionId": job["composition"]["id"],
            "descriptorHash": "a" * 64,
            "frameRate": job["composition"]["fps"],
            "frames": [
                {"frame": str(frame), "layers": []}
                for frame in range(frame_count)
            ],
            "currentFrame": "0",
        }

    def test_validate_only_accepts_valid_job_without_rendering(self):
        with tempfile.TemporaryDirectory() as directory:
            output_path = Path(directory) / "must-not-render"
            with self.valid_job.open(encoding="utf-8") as job_file:
                job = json.load(job_file)
            job["output"]["path"] = str(output_path)
            path = Path(directory) / "valid-job.json"
            path.write_text(json.dumps(job), encoding="utf-8")
            result = self.run_cli("--job", str(path), "--validate-only")

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "ok")
        self.assertFalse(output_path.exists())

    def test_validate_only_opens_job_and_preserves_unicode_paths_with_spaces(
        self,
    ):
        with tempfile.TemporaryDirectory(prefix="renderer job 日本語 ") as directory:
            job = self.load_valid_job()
            output_path = Path(directory) / "renders 日本語" / "frame.png"
            job["output"]["path"] = str(output_path)
            path = self.write_job(directory, "job with spaces 日本語.json", job)

            result = self.run_cli("--job", str(path), "--validate-only")

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "ok")
        self.assertFalse(output_path.exists())

    def test_minimum_positive_schema_boundaries_are_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.valid_job.open(encoding="utf-8") as job_file:
                job = json.load(job_file)
            job["version"] = 1
            job["composition"]["frameStart"] = 0
            job["composition"]["frameEnd"] = 1
            job["output"]["width"] = 1
            job["output"]["height"] = 1
            job["output"]["path"] = str(Path(directory) / "boundary-output")
            path = Path(directory) / "boundary-valid-job.json"
            path.write_text(json.dumps(job), encoding="utf-8")
            result = self.run_cli("--job", str(path), "--validate-only")

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "ok")

    def test_schema_rejects_values_just_outside_required_boundaries(self):
        mutations = {
            "zero_version": lambda job: job.update(version=0),
            "string_version": lambda job: job.update(version="1"),
            "blank_job_id": lambda job: job.update(jobId="  "),
            "blank_output_path": lambda job: job["output"].update(path="  "),
            "zero_width": lambda job: job["output"].update(width=0),
            "string_width": lambda job: job["output"].update(width="1920"),
            "zero_height": lambda job: job["output"].update(height=0),
            "fractional_height": lambda job: job["output"].update(height=1.5),
            "string_frame_end": lambda job: job["composition"].update(
                frameEnd="1"
            ),
            "empty_frame_range": lambda job: job["composition"].update(
                frameEnd=job["composition"]["frameStart"]
            ),
        }

        with tempfile.TemporaryDirectory() as directory:
            for label, mutate in mutations.items():
                with self.subTest(boundary=label):
                    with self.valid_job.open(encoding="utf-8") as job_file:
                        job = json.load(job_file)
                    mutate(job)
                    path = Path(directory) / f"{label}.json"
                    path.write_text(json.dumps(job), encoding="utf-8")
                    result = self.run_cli(
                        "--job", str(path), "--validate-only"
                    )

                    self.assertEqual(result.returncode, 5, result.stderr)
                    self.assertIn(
                        "Invalid external render job schema", result.stderr
                    )

    def test_dump_summary_reports_job_and_snapshot_counts(self):
        result = self.run_cli("--job", str(self.valid_job), "--dump-summary")

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        with self.valid_job.open(encoding="utf-8") as job_file:
            job = json.load(job_file)

        self.assertEqual(summary["jobId"], job["jobId"])
        self.assertEqual(summary["outputPath"], job["output"]["path"])
        self.assertEqual(
            summary["transportedLayerCount"], len(job["snapshot"]["layers"])
        )
        self.assertEqual(summary["version"], job["version"])
        self.assertEqual(summary["mode"], job["mode"])
        self.assertEqual(summary["compositionId"], job["composition"]["id"])
        self.assertEqual(summary["compositionName"], job["composition"]["name"])
        self.assertEqual(
            summary["frameStart"], job["composition"]["frameStart"]
        )
        self.assertEqual(summary["frameEnd"], job["composition"]["frameEnd"])
        self.assertEqual(summary["fps"], job["composition"]["fps"])
        self.assertEqual(summary["outputFormat"], job["output"]["format"])
        self.assertEqual(summary["width"], job["output"]["width"])
        self.assertEqual(summary["height"], job["output"]["height"])
        self.assertEqual(summary["backend"], job["quality"]["backend"])
        self.assertEqual(summary["componentSimulationBakePresent"], False)
        self.assertEqual(summary["componentSimulationBakeValid"], True)
        self.assertEqual(
            summary["componentSimulationBakeUsableForStart"], False
        )
        self.assertEqual(summary["componentSimulationBakeFrameCount"], 0)

    def test_component_bake_accepts_frame_count_limit_and_reports_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            self.add_valid_component_bake(job, frame_count=120)
            path = self.write_job(directory, "maximum-bake.json", job)
            result = self.run_cli("--job", str(path), "--dump-summary")

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertTrue(summary["componentSimulationBakePresent"])
        self.assertTrue(summary["componentSimulationBakeValid"])
        self.assertTrue(summary["componentSimulationBakeUsableForStart"])
        self.assertEqual(summary["componentSimulationBakeFrameCount"], 120)

    def test_component_bake_previous_frame_can_seed_render_start(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            self.add_valid_component_bake(job)
            bake = job["snapshot"]["composition"][
                "layerComponentSimulationBake"
            ]
            bake["frames"][0]["frame"] = "-1"
            bake["currentFrame"] = "-1"
            path = self.write_job(directory, "previous-frame-bake.json", job)
            result = self.run_cli("--job", str(path), "--dump-summary")

        self.assertEqual(result.returncode, 0, result.stderr)
        summary = json.loads(result.stdout)
        self.assertTrue(summary["componentSimulationBakeValid"])
        self.assertTrue(summary["componentSimulationBakeUsableForStart"])
        self.assertEqual(summary["componentSimulationBakeFrameCount"], 1)

    def test_component_bake_rejects_frame_count_above_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            self.add_valid_component_bake(job, frame_count=121)
            path = self.write_job(directory, "oversized-bake.json", job)
            result = self.run_cli("--job", str(path), "--validate-only")

        self.assertEqual(result.returncode, 5)
        self.assertIn("Invalid layer component simulation bake header", result.stderr)

    def test_component_bake_frame_rate_tolerance_boundary(self):
        cases = (
            (0.0000005, 0, None),
            (0.000002, 5, "Invalid layer component simulation bake header"),
        )
        with tempfile.TemporaryDirectory() as directory:
            for delta, expected_code, expected_error in cases:
                with self.subTest(frame_rate_delta=delta):
                    job = self.load_valid_job()
                    self.add_valid_component_bake(job)
                    bake = job["snapshot"]["composition"][
                        "layerComponentSimulationBake"
                    ]
                    bake["frameRate"] += delta
                    path = self.write_job(
                        directory, f"frame-rate-{delta}.json", job
                    )
                    result = self.run_cli(
                        "--job", str(path), "--validate-only"
                    )

                    self.assertEqual(
                        result.returncode, expected_code, result.stderr
                    )
                    if expected_error:
                        self.assertIn(expected_error, result.stderr)
                    else:
                        self.assertEqual(result.stdout.strip(), "ok")

    def test_component_bake_rejects_invalid_header_and_frame_payloads(self):
        def invalid_hash(bake):
            bake["descriptorHash"] = "a" * 63

        def wrong_rate(bake):
            bake["frameRate"] += 1.0

        def duplicate_frame(bake):
            bake["frames"].append({"frame": "0", "layers": []})

        def non_string_frame(bake):
            bake["frames"][0]["frame"] = 0

        def non_array_layers(bake):
            bake["frames"][0]["layers"] = {}

        def missing_current_frame(bake):
            bake["currentFrame"] = "1"

        def malformed_frame_object(bake):
            bake["frames"][0] = "not-an-object"

        def overflowing_frame_number(bake):
            bake["frames"][0]["frame"] = "9223372036854775808"

        def malformed_current_frame(bake):
            bake["currentFrame"] = "not-a-number"

        mutations = {
            "invalid_descriptor_hash": (
                invalid_hash,
                "Invalid layer component simulation bake header",
            ),
            "mismatched_frame_rate": (
                wrong_rate,
                "Invalid layer component simulation bake header",
            ),
            "duplicate_frame_number": (
                duplicate_frame,
                "Invalid layer component simulation bake frame payload",
            ),
            "non_string_frame_number": (
                non_string_frame,
                "Invalid layer component simulation bake frame payload",
            ),
            "layers_not_array": (
                non_array_layers,
                "Invalid layer component simulation bake frame payload",
            ),
            "current_frame_missing": (
                missing_current_frame,
                "Invalid layer component simulation bake current frame",
            ),
            "frame_not_object": (
                malformed_frame_object,
                "Invalid layer component simulation bake frame",
            ),
            "frame_number_overflow": (
                overflowing_frame_number,
                "Invalid layer component simulation bake frame payload",
            ),
            "current_frame_not_integer": (
                malformed_current_frame,
                "Invalid layer component simulation bake current frame",
            ),
        }

        with tempfile.TemporaryDirectory() as directory:
            for label, (mutate, message) in mutations.items():
                with self.subTest(bake_contract=label):
                    job = self.load_valid_job()
                    self.add_valid_component_bake(job)
                    bake = job["snapshot"]["composition"][
                        "layerComponentSimulationBake"
                    ]
                    mutate(bake)
                    path = self.write_job(directory, f"{label}.json", job)
                    result = self.run_cli(
                        "--job", str(path), "--validate-only"
                    )

                    self.assertEqual(result.returncode, 5, result.stderr)
                    self.assertIn(message, result.stderr)

    def test_component_bake_must_be_an_object(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            job["snapshot"]["composition"][
                "layerComponentSimulationBake"
            ] = []
            path = self.write_job(directory, "non-object-bake.json", job)
            result = self.run_cli("--job", str(path), "--validate-only")

        self.assertEqual(result.returncode, 5, result.stderr)
        self.assertIn("Invalid external render job schema", result.stderr)

    def test_diagnostic_render_writes_png_sequence_and_completion_events(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            output_directory = Path(directory) / "frames"
            job["output"].update(
                path=str(output_directory), format="png", width=16, height=16
            )
            job["composition"]["frameStart"] = 7
            job["composition"]["frameEnd"] = 9
            summary_path = Path(directory) / "summary.json"
            event_log_path = Path(directory) / "events.jsonl"
            job["diagnostics"].update(
                summaryFile=str(summary_path), eventLogFile=str(event_log_path)
            )
            path = self.write_job(directory, "render-job.json", job)

            result = self.run_cli("--job", str(path))

            self.assertEqual(result.returncode, 0, result.stderr)
            png_paths = [
                output_directory / "Example_Composition_0007.png",
                output_directory / "Example_Composition_0008.png",
            ]
            self.assertEqual(
                sorted(path.name for path in output_directory.glob("*.png")),
                [path.name for path in png_paths],
            )
            for png_path in png_paths:
                with self.subTest(frame=png_path.name):
                    data = png_path.read_bytes()
                    self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
                    self.assertEqual(data[12:16], b"IHDR")
                    self.assertEqual(struct.unpack(">II", data[16:24]), (16, 16))

            # The diagnostic backend must rasterize its frame card, not just
            # create a correctly named PNG container.
            width, height, pixels = self.read_rgba_png(png_paths[0])
            self.assertEqual((width, height), (16, 16))
            self.assertIn((36, 42, 54, 255), pixels)
            self.assertGreater(
                sum(1 for red, green, blue, alpha in pixels if blue > red + 35 and blue > green + 35 and alpha == 255),
                20,
            )

            events = [
                json.loads(line)
                for line in event_log_path.read_text(encoding="utf-8").splitlines()
            ]
            self.assertEqual(
                [event["event"] for event in events],
                ["renderStarted", "renderProgress", "renderProgress", "renderCompleted"],
            )
            self.assertEqual(
                [event["progress"] for event in events], [0, 0, 50, 100]
            )
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(summary["jobId"], job["jobId"])
            self.assertEqual(summary["frameStart"], 7)
            self.assertEqual(summary["frameEnd"], 9)

    def test_sequence_filename_normalizes_name_and_keeps_large_frame_number(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            output_directory = Path(directory) / "normalized-frames"
            job["composition"]["name"] = "  My   Test   Composition  "
            job["composition"]["frameStart"] = 10000
            job["composition"]["frameEnd"] = 10001
            job["output"].update(
                path=str(output_directory), format="png", width=16, height=16
            )
            path = self.write_job(directory, "normalized-name-job.json", job)

            result = self.run_cli("--job", str(path))

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            sorted(item.name for item in output_directory.glob("*.png")),
            ["My_Test_Composition_10000.png"],
        )

    def test_sequence_filename_uses_fallback_for_blank_composition_name(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            output_directory = Path(directory) / "fallback-frames"
            job["composition"]["name"] = "   "
            job["output"].update(
                path=str(output_directory), format="png", width=16, height=16
            )
            path = self.write_job(directory, "blank-name-job.json", job)

            result = self.run_cli("--job", str(path))

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            sorted(item.name for item in output_directory.glob("*.png")),
            ["render_0000.png"],
        )

    def test_pre_cancelled_render_returns_failure_without_pngs(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            output_directory = Path(directory) / "cancelled-frames"
            job["output"].update(
                path=str(output_directory), format="png", width=16, height=16
            )
            cancel_path = Path(directory) / "cancel.request"
            cancel_path.write_text("cancel", encoding="utf-8")
            event_log_path = Path(directory) / "cancelled-events.jsonl"
            summary_path = Path(directory) / "cancelled-summary.json"
            job["diagnostics"].update(
                cancelFile=str(cancel_path),
                eventLogFile=str(event_log_path),
                summaryFile=str(summary_path),
            )
            path = self.write_job(directory, "cancelled-job.json", job)

            result = self.run_cli("--job", str(path))

            self.assertEqual(result.returncode, 6)
            self.assertIn("Cancelled", result.stderr)
            self.assertEqual(list(output_directory.glob("*.png")), [])
            events = [
                json.loads(line)
                for line in event_log_path.read_text(encoding="utf-8").splitlines()
            ]
            self.assertEqual([event["event"] for event in events], ["renderStarted"])

    def test_unwritable_output_returns_failure_without_completion_event(self):
        with tempfile.TemporaryDirectory() as directory:
            job = self.load_valid_job()
            blocker = Path(directory) / "file-blocker"
            blocker.write_text("not a directory", encoding="utf-8")
            output_directory = blocker / "frames"
            job["output"].update(
                path=str(output_directory), format="png", width=16, height=16
            )
            event_log_path = Path(directory) / "failed-events.jsonl"
            summary_path = Path(directory) / "failed-summary.json"
            job["diagnostics"].update(
                eventLogFile=str(event_log_path), summaryFile=str(summary_path)
            )
            path = self.write_job(directory, "unwritable-job.json", job)

            result = self.run_cli("--job", str(path))

            self.assertEqual(result.returncode, 6)
            self.assertIn("Failed to write PNG", result.stderr)
            self.assertFalse(output_directory.exists())
            events = [
                json.loads(line)
                for line in event_log_path.read_text(encoding="utf-8").splitlines()
            ]
            self.assertEqual(
                [event["event"] for event in events],
                ["renderStarted", "renderProgress"],
            )

    def test_invalid_json_returns_parse_error_exit_code(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "invalid.json"
            path.write_text("{invalid", encoding="utf-8")
            result = self.run_cli("--job", str(path), "--validate-only")

        self.assertEqual(result.returncode, 4)
        self.assertIn("invalid job json", result.stderr)

    def test_invalid_schema_returns_validation_exit_code(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "invalid-schema.json"
            path.write_text("{}", encoding="utf-8")
            result = self.run_cli("--job", str(path), "--validate-only")

        self.assertEqual(result.returncode, 5)
        self.assertIn("Invalid external render job schema", result.stderr)

    def test_missing_job_file_returns_open_error_exit_code(self):
        with tempfile.TemporaryDirectory() as directory:
            missing_path = Path(directory) / "missing.json"
            result = self.run_cli("--job", str(missing_path), "--validate-only")

        self.assertEqual(result.returncode, 3)
        self.assertIn("failed to open job file", result.stderr)

    def test_missing_job_arguments_print_usage(self):
        result = self.run_cli("--validate-only")

        self.assertEqual(result.returncode, 2)
        self.assertIn("usage: artifact-renderer --job <job.json>", result.stderr)


if __name__ == "__main__":
    unittest.main()
