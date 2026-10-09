#!/usr/bin/env python3
"""Validate an openHASP skin against NanoELS's stable display contract.

This validator is intentionally offline. It does not require an openHASP build,
hardware, or a running NanoELS controller.
"""
import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAX_PAGE_ID = 12
MAX_OBJECT_ID = 254


def error(line, message):
    return f"line {line}: {message}"


def load_mapping(path):
    try:
        data = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return None, [f"mapping: {exc}"]
    if not isinstance(data, dict):
        return None, ["mapping: root must be an object"]

    contract = data.get("contract")
    if not isinstance(contract, dict):
        return None, ["mapping: missing contract object"]
    if not isinstance(contract.get("version"), int) or isinstance(contract["version"], bool):
        return None, ["mapping: contract version must be an integer"]
    required_fields = contract.get("required_fields")
    required_controls = contract.get("required_controls")
    if not isinstance(required_fields, list) or not all(isinstance(name, str) for name in required_fields):
        return None, ["mapping: required_fields must be an array of component names"]
    if not isinstance(required_controls, list) or not all(isinstance(name, str) for name in required_controls):
        return None, ["mapping: required_controls must be an array of component names"]

    components = data.get("components")
    if not isinstance(components, list):
        return None, ["mapping: components must be an array"]
    by_name = {}
    by_address = {}
    errors = []
    for index, component in enumerate(components):
        if not isinstance(component, dict):
            errors.append(f"mapping: component {index} must be an object")
            continue
        name = component.get("name")
        page = component.get("page")
        obj_id = component.get("id")
        if not isinstance(name, str) or not name:
            errors.append(f"mapping: component {index} has no valid name")
            continue
        if name in by_name:
            errors.append(f"mapping: duplicate component name {name}")
        if not valid_page(page) or not valid_object_id(obj_id):
            errors.append(f"mapping: component {name} has invalid page/object ID")
            continue
        address = (page, obj_id)
        if address in by_address:
            errors.append(f"mapping: duplicate canonical object p{page}b{obj_id}")
        by_name[name] = component
        by_address[address] = component

    for name in required_fields:
        component = by_name.get(name)
        if component is None:
            errors.append(f"mapping: required field {name} is not a component")
        elif not component.get("live_text"):
            errors.append(f"mapping: required field {name} is not live text")
    for name in required_controls:
        component = by_name.get(name)
        if component is None:
            errors.append(f"mapping: required control {name} is not a component")
        elif not component.get("action"):
            errors.append(f"mapping: required control {name} has no NanoELS action")

    if errors:
        return None, errors
    return {
        "contract": contract,
        "by_name": by_name,
        "by_address": by_address,
    }, []


def valid_page(value):
    return isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= MAX_PAGE_ID


def valid_object_id(value):
    return isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= MAX_OBJECT_ID


def parse_pages(path):
    try:
        lines = path.read_text().splitlines()
    except OSError as exc:
        return None, [f"pages: {exc}"]

    objects = {}
    errors = []
    saved_page = None
    for line_number, raw_line in enumerate(lines, 1):
        if not raw_line.strip():
            continue
        try:
            record = json.loads(raw_line)
        except json.JSONDecodeError as exc:
            errors.append(error(line_number, f"invalid JSON: {exc.msg}"))
            continue
        if not isinstance(record, dict):
            errors.append(error(line_number, "record must be an object"))
            continue
        if record.get("skip") is True:
            continue

        has_page = "page" in record
        has_id = "id" in record
        if not has_page and not has_id:
            # Comment-only records are supported by openHASP and carry no object.
            continue
        if has_page:
            if not valid_page(record["page"]):
                errors.append(error(line_number, "page must be an integer from 0 through 12"))
                continue
            saved_page = record["page"]
        if not has_id:
            errors.append(error(line_number, "object record is missing id"))
            continue
        if saved_page is None:
            errors.append(error(line_number, "object record has no page and no preceding page"))
            continue
        if not valid_object_id(record["id"]):
            errors.append(error(line_number, "id must be an integer from 0 through 254"))
            continue
        address = (saved_page, record["id"])
        if address in objects:
            errors.append(error(line_number, f"duplicate object p{saved_page}b{record['id']} (first defined on line {objects[address]['line']})"))
            continue
        objects[address] = {"line": line_number, "record": record}
    return objects, errors


def validate(pages_path, mapping_path):
    mapping, errors = load_mapping(Path(mapping_path))
    if errors:
        return errors
    objects, errors = parse_pages(Path(pages_path))
    if errors:
        return errors

    for name in mapping["contract"]["required_fields"]:
        component = mapping["by_name"][name]
        address = (component["page"], component["id"])
        if address not in objects:
            errors.append(f"required field {name} is missing at p{address[0]}b{address[1]}")

    for name in mapping["contract"]["required_controls"]:
        component = mapping["by_name"][name]
        address = (component["page"], component["id"])
        found = objects.get(address)
        if found is None:
            errors.append(f"required control {name} is missing at p{address[0]}b{address[1]}")
            continue
        line = found["line"]
        record = found["record"]
        if record.get("hidden", False):
            errors.append(error(line, f"required control {name} must not be hidden"))
        if record.get("enabled", True) is False:
            errors.append(error(line, f"required control {name} must be enabled"))
        if record.get("click", True) is False:
            errors.append(error(line, f"required control {name} must be clickable"))
        if record.get("toggle", False) is True:
            errors.append(error(line, f"required control {name} must be momentary (toggle:false)"))
        if record.get("action") not in (None, ""):
            errors.append(error(line, f"required control {name} must not define a local action"))
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, default=ROOT / "pages.jsonl", help="skin pages.jsonl")
    parser.add_argument("--mapping", type=Path, default=ROOT / "mapping.json", help="NanoELS mapping.json")
    args = parser.parse_args(argv)
    errors = validate(args.pages, args.mapping)
    if errors:
        for item in errors:
            print(f"ERROR: {item}", file=sys.stderr)
        return 1
    print(f"PASS: {args.pages} satisfies NanoELS display contract v1")
    return 0


if __name__ == "__main__":
    sys.exit(main())
