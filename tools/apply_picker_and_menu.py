#!/usr/bin/env python3
"""Color picker in the Materials sheet; More-menu duplicate/delete act on the selection (idempotent)."""
import pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
UI = ROOT / "android/app/src/main/java/com/smitnk/projectgrease/ui/ProjectGreaseUI.kt"
EDITS = [
    ('            Text("Thickness "+thickness.toInt(),Modifier.padding(horizontal=20.dp))\n',
     '            Text("Color picker",Modifier.padding(horizontal=20.dp))\n'
     '            BlenderColorPicker(controller.materials.colorArgb,{controller.setMaterialColor(it);redraw()})\n'
     '            Spacer(Modifier.height(12.dp))\n'
     '            Text("Thickness "+thickness.toInt(),Modifier.padding(horizontal=20.dp))\n',
     "picker"),
    ('ListItem(headlineContent={Text("Delete selected stroke")},modifier=Modifier.clickable{controller.deleteSelectedStroke();redraw();onDismiss()})',
     'ListItem(headlineContent={Text("Delete selection")},modifier=Modifier.clickable{if(!controller.deleteSelectedStrokes())controller.deleteSelectedStroke();redraw();onDismiss()})',
     "delete item"),
    ('ListItem(headlineContent={Text("Duplicate selected stroke")},modifier=Modifier.clickable{controller.duplicateSelectedStroke();redraw();onDismiss()})',
     'ListItem(headlineContent={Text("Duplicate selection")},modifier=Modifier.clickable{if(!controller.duplicateSelection())controller.duplicateSelectedStroke();redraw();onDismiss()})',
     "duplicate item"),
]
text = UI.read_text(encoding="utf-8")
if "BlenderColorPicker(" in text:
    print("already applied"); sys.exit(0)
for old, new, label in EDITS:
    if text.count(old) != 1:
        sys.exit(f"anchor '{label}' found {text.count(old)} times; nothing changed")
    text = text.replace(old, new)
UI.write_text(text, encoding="utf-8")
print("patched:", UI.relative_to(ROOT))
