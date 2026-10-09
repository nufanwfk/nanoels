#!/usr/bin/env python3
"""Unit tests for the offline NanoELS openHASP skin validator."""
import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PACKAGE_ROOT / "tools"))
import validate_skin  # noqa: E402


MAPPING = PACKAGE_ROOT / "mapping.json"
PAGES = PACKAGE_ROOT / "pages.jsonl"


def bundled_records():
    return [json.loads(line) for line in PAGES.read_text().splitlines() if line.strip()]


def record(records, page, obj_id):
    return next(item for item in records if item.get("page") == page and item.get("id") == obj_id)


class SkinValidatorTests(unittest.TestCase):
    def validate_records(self, records):
        with tempfile.TemporaryDirectory() as directory:
            pages = Path(directory) / "pages.jsonl"
            pages.write_text("\n".join(json.dumps(item) for item in records) + "\n")
            return validate_skin.validate(pages, MAPPING)

    def test_bundled_skin_passes(self):
        self.assertEqual(validate_skin.validate(PAGES, MAPPING), [])

    def test_missing_required_stop_fails(self):
        records = [item for item in bundled_records() if not (item.get("page") == 1 and item.get("id") == 23)]
        self.assertIn("required control mStop is missing at p1b23", self.validate_records(records))

    def test_optional_field_and_control_may_be_missing(self):
        records = [
            item for item in bundled_records()
            if (item.get("page"), item.get("id")) not in {(1, 17), (1, 36)}
        ]
        self.assertEqual(self.validate_records(records), [])

    def test_duplicate_object_fails(self):
        records = bundled_records()
        records.append(copy.deepcopy(record(records, 1, 23)))
        errors = self.validate_records(records)
        self.assertTrue(any("duplicate object p1b23" in item for item in errors), errors)

    def test_required_stop_must_be_visible_enabled_clickable_and_momentary(self):
        invalid_properties = {
            "hidden": True,
            "enabled": False,
            "click": False,
            "toggle": True,
        }
        for key, value in invalid_properties.items():
            with self.subTest(key=key):
                records = bundled_records()
                record(records, 1, 23)[key] = value
                errors = self.validate_records(records)
                self.assertTrue(any("required control mStop" in item for item in errors), errors)

    def test_required_stop_uses_openhasp_property_defaults_when_omitted(self):
        records = bundled_records()
        stop = record(records, 1, 23)
        for key in ("hidden", "enabled", "click", "toggle"):
            stop.pop(key, None)
        self.assertEqual(self.validate_records(records), [])

    def test_required_stop_cannot_have_local_action(self):
        records = bundled_records()
        record(records, 1, 23)["action"] = "page 2"
        errors = self.validate_records(records)
        self.assertTrue(any("must not define a local action" in item for item in errors), errors)

    def test_unknown_future_object_is_allowed(self):
        records = bundled_records()
        records.append({"page": 1, "id": 254, "obj": "label", "text": "future feature"})
        self.assertEqual(self.validate_records(records), [])

    def test_malformed_json_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            pages = Path(directory) / "pages.jsonl"
            pages.write_text('{"page":1,"id":23\n')
            errors = validate_skin.validate(pages, MAPPING)
        self.assertTrue(any("invalid JSON" in item for item in errors), errors)


if __name__ == "__main__":
    unittest.main()
