package com.smitnk.projectgrease.editor;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * New-file templates, after Blender's Grease Pencil "2D Animation" and "Blank" startup files and the
 * Storyboard app template. Layer order is bottom to top; colors are ARGB. Project Grease creates the
 * layers and material slots from this; there is no 3D scene, camera or workspace content.
 */
public final class GreaseTemplates {
    public static final class Material {
        public final String name; public final int stroke; public final Integer fill;
        Material(String name, int stroke, Integer fill) { this.name = name; this.stroke = stroke; this.fill = fill; }
    }
    public static final class Template {
        public final String id, title; public final List<String> layers; public final List<Material> materials;
        public final int fps, endFrame;
        Template(String id, String title, List<String> layers, List<Material> materials, int fps, int endFrame) {
            this.id = id; this.title = title; this.layers = layers; this.materials = materials; this.fps = fps; this.endFrame = endFrame;
        }
    }

    private static final List<Material> BASIC = Arrays.asList(
        new Material("Black", 0xFF000000, null),
        new Material("White", 0xFFFFFFFF, null),
        new Material("Red", 0xFFFF0000, null),
        new Material("Green", 0xFF00FF00, null),
        new Material("Blue", 0xFF0000FF, null),
        new Material("Grey", 0xFF808080, 0xFF808080),
        new Material("Dots Stroke", 0xFF000000, null));

    public static final List<Template> ALL = Collections.unmodifiableList(Arrays.asList(
        new Template("2d_animation", "2D Animation", Arrays.asList("Background", "Fills", "Lines"), BASIC, 24, 250),
        new Template("blank", "Blank", Collections.singletonList("GP_Layer"),
            Collections.singletonList(new Material("Black", 0xFF000000, null)), 24, 250),
        new Template("storyboard", "Storyboard", Arrays.asList("Panels", "Sketch", "Notes"), BASIC, 24, 250)));

    public static Template byId(String id) {
        for (Template t : ALL) if (t.id.equals(id)) return t;
        return null;
    }
}
