#!/usr/bin/env python3
"""UI/controller fixes found by the 2026-10-02 audit (idempotent, exact anchors).

 * sculpt mode gate: nothing is AVAILABLE any more (no device evidence), so the sculpt chip and
   controller must gate on "not NOT_IMPLEMENTED" instead of "== AVAILABLE";
 * Materials sheet opacity slider never reached the engine (no setMaterialColor);
 * moving the stabilizer factor slider forced stabilization on;
 * setSpacing(0) left the input filter at the previous spacing.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASE = ROOT / "android/app/src/main/java/com/smitnk/projectgrease"

EDITS = [
    (
        BASE / "editor/EditorControllers.kt",
        "FeatureRegistry.capability(FeatureId.SCULPT).state != FeatureState.NOT_IMPLEMENTED",
        [
            ("FeatureRegistry.capability(FeatureId.SCULPT).state == FeatureState.AVAILABLE",
             "FeatureRegistry.capability(FeatureId.SCULPT).state != FeatureState.NOT_IMPLEMENTED",
             "sculpt gates (controller)", 2),
            ("        if (value > 0f) legacyEuclideanThreshold=value\n",
             "        legacyEuclideanThreshold = if (value > 0f) value else 1f // 0 restores Blender's default filter\n",
             "setSpacing reset", 1),
        ],
    ),
    (
        BASE / "ui/ProjectGreaseUI.kt",
        "controller.setStabilizer(controller.stabilizerEnabled,it)",
        [
            ("GreaseMode.SCULPT->FeatureRegistry.capability(FeatureId.SCULPT).state==FeatureState.AVAILABLE",
             "GreaseMode.SCULPT->FeatureRegistry.capability(FeatureId.SCULPT).state!=FeatureState.NOT_IMPLEMENTED",
             "sculpt chip", 1),
            ("Slider(opacity,{opacity=it;controller.materials.setOpacity(it);redraw()},valueRange=0f..1f)",
             "Slider(opacity,{opacity=it;controller.materials.setOpacity(it);controller.setMaterialColor(controller.materials.colorArgb);redraw()},valueRange=0f..1f)",
             "materials opacity push", 1),
            ("{controller.setStabilizer(true,it);redraw()}",
             "{controller.setStabilizer(controller.stabilizerEnabled,it);redraw()}",
             "stabilizer factor slider", 1),
        ],
    ),
]


def main():
    pending = []
    for path, marker, edits in EDITS:
        text = path.read_text(encoding="utf-8")
        if marker in text:
            print(f"already applied: {path.relative_to(ROOT)}")
            continue
        for old, new, label, expected in edits:
            if text.count(old) != expected:
                print(f"anchor '{label}' in {path.relative_to(ROOT)} found {text.count(old)} times "
                      f"(expected {expected}); nothing changed", file=sys.stderr)
                return 1
            text = text.replace(old, new)
        pending.append((path, text))
    for path, text in pending:
        path.write_text(text, encoding="utf-8")
        print(f"patched: {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
