package com.smitnk.projectgrease.editor

/**
 * Kotlin side of the native Grease Pencil rig (native/blender_gp/project_grease_armature.h, Blender
 * 3.6.23 armature / pose / action / Armature modifier / Parent with weights). Points and distances
 * are canvas px, angles radians counter-clockwise on screen, bones are indices in creation order.
 * Every edit returns the native result (-1: no rig / bad arguments); the caller records history
 * (HistoryController.markEdit) and redraws, like the other editors.
 */
class RigController(private val native: NativeEditorBridge) {
    companion object {
        // PG_ARM_CMD_* (project_grease_gp_bridge.h)
        const val CMD_CREATE = 0; const val CMD_REMOVE = 1; const val CMD_SET_MODE = 2
        const val CMD_BONE_ADD = 3; const val CMD_EXTRUDE = 4; const val CMD_EDIT_SELECT = 5
        const val CMD_EDIT_TRANSLATE = 6; const val CMD_EDIT_ROTATE = 7; const val CMD_EDIT_SCALE = 8
        const val CMD_BONE_SET = 9; const val CMD_PARENT_SET = 10; const val CMD_PARENT_CLEAR = 11
        const val CMD_DELETE = 12; const val CMD_RENAME = 13; const val CMD_ENVELOPE = 14
        const val CMD_POSE_SELECT = 15; const val CMD_POSE_TRANSLATE = 16; const val CMD_POSE_ROTATE = 17
        const val CMD_POSE_SCALE = 18; const val CMD_POSE_SET = 19; const val CMD_POSE_CLEAR = 20
        const val CMD_POSE_COPY = 21; const val CMD_POSE_PASTE = 22; const val CMD_KEY_INSERT = 23
        const val CMD_KEY_DELETE = 24; const val CMD_KEY_INTERPOLATION = 25; const val CMD_BIND = 26
        const val CMD_EVALUATE = 27; const val CMD_DISPLAY = 28; const val CMD_PICK = 29; const val CMD_KEY_MOVE = 30
        const val CMD_MIRROR_X = 31; const val CMD_BBONE = 32; const val CMD_POSE_BBONE = 33
        // Transform Pivot Point (V3D_AROUND_LOCAL_ORIGINS / V3D_AROUND_CENTER_MEDIAN)
        const val AROUND_INDIVIDUAL = 2; const val AROUND_MEDIAN = 3
        // PG_ARM_Q_*
        const val Q_SUMMARY = 0; const val Q_BONE = 1; const val Q_POSE = 2; const val Q_KEYS = 3
        const val Q_KEY_INTERPOLATION = 4; const val Q_ENVELOPE = 5; const val Q_SELECTED = 6
        const val Q_DISPLAY = 7; const val Q_BBONE = 8; const val Q_POSE_BBONE = 9
        // bArmature.drawtype (ARM_OCTA / ARM_LINE / ARM_B_BONE / ARM_WIRE; Envelope is not drawn)
        const val DISPLAY_OCTAHEDRAL = 0; const val DISPLAY_STICK = 1; const val DISPLAY_BBONE = 2; const val DISPLAY_WIRE = 4
        /** PG_ARM_BBONE: ease in, ease out, roll in, roll out, curve in x, z, out x, z, scale in xyz, scale out xyz */
        const val BBONE_VALUES = 14
        const val BB_EASE_IN = 0; const val BB_EASE_OUT = 1; const val BB_ROLL_IN = 2; const val BB_ROLL_OUT = 3
        const val BB_CURVE_IN_X = 4; const val BB_CURVE_IN_Z = 5; const val BB_CURVE_OUT_X = 6; const val BB_CURVE_OUT_Z = 7
        const val BB_SCALE_IN = 8; const val BB_SCALE_OUT = 11
        /** ED_view3d_select_dist_px: 75 px * U.pixelsize */
        const val SELECT_DIST_PX = 75f

        const val MODE_OBJECT = 0; const val MODE_EDIT = 1; const val MODE_POSE = 2
        // BONE_SELECTED / BONE_ROOTSEL / BONE_TIPSEL (DNA_armature_types.h)
        const val SELECT_BONE = 1; const val SELECT_HEAD = 2; const val SELECT_TAIL = 4
        const val PARENT_CONNECTED = 1; const val PARENT_KEEP_OFFSET = 2
        const val CLEAR_PARENT = 1; const val CLEAR_DISCONNECT = 2
        const val CLEAR_LOCATION = 1; const val CLEAR_ROTATION = 2; const val CLEAR_SCALE = 4
        // BEZT_IPO_*
        const val IPO_CONSTANT = 0; const val IPO_LINEAR = 1; const val IPO_BEZIER = 2
        // Parent with ... (object_relations.c PAR_ARMATURE*, GP branch)
        const val BIND_ARMATURE = 0; const val BIND_EMPTY_GROUPS = 1; const val BIND_AUTOMATIC = 2
        const val BIND_ENVELOPE = 3
        const val BONE_INFO_SIZE = 48
    }

    /** One bone: rest head/tail (px), roll, parent (-1), flags, length (px), pose head/tail (px). */
    data class Bone(
        val index: Int, val name: String, val headX: Float, val headY: Float, val tailX: Float, val tailY: Float,
        val roll: Float, val parent: Int, val flag: Int, val length: Float,
        val poseHeadX: Float, val poseHeadY: Float, val poseTailX: Float, val poseTailY: Float
    ) {
        val connected get() = (flag and 16) != 0 // BONE_CONNECTED
        val selected get() = (flag and SELECT_BONE) != 0
    }

    private fun cmd(c: Int, bone: Int = -1, vararg args: Float, name: String? = null) =
        native.armatureCommand(c, bone, if (args.isEmpty()) null else args, name).also { UNDO_NAMES[c]?.let(onUndoName) }
    /** The Blender operator name of the command that just ran: names the undo step the caller pushes. */
    var onUndoName: (String) -> Unit = {}
    private val UNDO_NAMES = mapOf(CMD_CREATE to "Add Armature", CMD_REMOVE to "Delete", CMD_BONE_ADD to "Add Bone",
        CMD_EXTRUDE to "Extrude", CMD_EDIT_SELECT to "Select", CMD_EDIT_TRANSLATE to "Move", CMD_EDIT_ROTATE to "Rotate",
        CMD_EDIT_SCALE to "Resize", CMD_PARENT_SET to "Make Parent", CMD_PARENT_CLEAR to "Clear Parent",
        CMD_DELETE to "Delete Selected Bone(s)", CMD_POSE_SELECT to "Select", CMD_POSE_TRANSLATE to "Move",
        CMD_POSE_ROTATE to "Rotate", CMD_POSE_SCALE to "Resize", CMD_POSE_CLEAR to "Clear User Transforms",
        CMD_POSE_PASTE to "Paste Pose", CMD_KEY_INSERT to "Insert Keyframe", CMD_KEY_DELETE to "Delete Keyframe",
        CMD_BIND to "Make Parent", CMD_MIRROR_X to "Symmetrize")

    val exists get() = boneCount >= 0
    /** -1 without a rig. */
    val boneCount get() = native.armatureQuery(Q_SUMMARY).getOrNull(0)?.toInt() ?: -1
    val activeBone get() = native.armatureQuery(Q_SUMMARY).getOrNull(1)?.toInt() ?: -1
    val mode get() = native.armatureQuery(Q_SUMMARY).getOrNull(2)?.toInt() ?: MODE_OBJECT

    fun create() = cmd(CMD_CREATE) == 1
    /** The rig as saved (project_grease_armature_io.cc): names of bones / properties live here. */
    fun json(): String = native.armatureJson()
    fun remove() = cmd(CMD_REMOVE) == 1
    /** Object / Edit / Pose (the armature skips deforming in Edit, as in Blender). */
    fun setMode(mode: Int) = cmd(CMD_SET_MODE, -1, mode.toFloat()) == 1

    fun bone(index: Int): Bone? {
        val v = native.armatureQuery(Q_BONE, index)
        if (v.size < BONE_INFO_SIZE) return null
        return Bone(index, native.armatureBoneName(index) ?: "", v[0], v[1], v[3], v[4], v[6], v[7].toInt(), v[8].toInt(),
            v[9], v[26], v[27], v[29], v[30])
    }
    fun bones(): List<Bone> = (0 until boneCount.coerceAtLeast(0)).mapNotNull { bone(it) }
    fun findBone(name: String) = bones().firstOrNull { it.name == name }?.index ?: -1

    // ---- edit mode ----
    /** Adds a bone (selected, active); returns its index. Name "" is "Bone" (made unique). */
    fun addBone(headX: Float, headY: Float, tailX: Float, tailY: Float, name: String = "") =
        cmd(CMD_BONE_ADD, -1, headX, headY, tailX, tailY, name = name)
    /** Extrude from the selected tips, new tips moved by (dx, dy); returns the number of new bones. */
    fun extrude(dx: Float, dy: Float) = cmd(CMD_EXTRUDE, -1, dx, dy)
    fun editSelect(bone: Int, flags: Int = SELECT_BONE, extend: Boolean = false) =
        cmd(CMD_EDIT_SELECT, bone, flags.toFloat(), if (extend) 1f else 0f)
    fun editTranslate(dx: Float, dy: Float) = cmd(CMD_EDIT_TRANSLATE, -1, dx, dy)
    fun editRotate(angle: Float, pivotX: Float, pivotY: Float) = cmd(CMD_EDIT_ROTATE, -1, angle, pivotX, pivotY)
    fun editScale(factor: Float, pivotX: Float, pivotY: Float) = cmd(CMD_EDIT_SCALE, -1, factor, pivotX, pivotY)
    fun setBone(bone: Int, headX: Float, headY: Float, tailX: Float, tailY: Float, roll: Float) =
        cmd(CMD_BONE_SET, bone, headX, headY, tailX, tailY, roll)
    /** Selected bones get the active bone as parent. */
    fun parentSet(mode: Int) = cmd(CMD_PARENT_SET, -1, mode.toFloat())
    fun parentClear(mode: Int) = cmd(CMD_PARENT_CLEAR, -1, mode.toFloat())
    fun deleteSelected() = cmd(CMD_DELETE)
    /** Blender's unique-name rule; the vertex group of the old name follows. */
    fun rename(bone: Int, name: String) = cmd(CMD_RENAME, bone, name = name) == 1
    /** dist / radii in px, weight, deform, multiply vertex groups by envelope. */
    fun setEnvelope(bone: Int, dist: Float, weight: Float, radHead: Float, radTail: Float, deform: Boolean, multiplyVg: Boolean) =
        cmd(CMD_ENVELOPE, bone, dist, weight, radHead, radTail, if (deform) 1f else 0f, if (multiplyVg) 1f else 0f)
    fun envelope(bone: Int) = native.armatureQuery(Q_ENVELOPE, bone)

    /** Bendy Bones of the bone (rna_armature.c): segments 1..32, Display Size X / Z (Blender units). */
    data class BBone(val segments: Int, val displayX: Float, val displayZ: Float, val values: FloatArray)
    fun bbone(bone: Int): BBone? = native.armatureQuery(Q_BBONE, bone).takeIf { it.size >= 3 + BBONE_VALUES }?.let {
        BBone(it[0].toInt(), it[1], it[2], it.copyOfRange(3, 3 + BBONE_VALUES)) }
    fun setBBone(bone: Int, segments: Int, displayX: Float, displayZ: Float, values: FloatArray) =
        cmd(CMD_BBONE, bone, segments.toFloat(), displayX, displayZ, *values) == 1
    /** The pose channel's B-Bone offsets (rna_pose.c), the same PG_ARM_BBONE order. */
    fun poseBBone(bone: Int): FloatArray? = native.armatureQuery(Q_POSE_BBONE, bone).takeIf { it.size >= BBONE_VALUES }
    fun setPoseBBone(bone: Int, values: FloatArray) = cmd(CMD_POSE_BBONE, bone, *values) == 1

    // ---- pose mode ----
    fun poseSelect(bone: Int, extend: Boolean = false) = cmd(CMD_POSE_SELECT, bone, if (extend) 1f else 0f)
    fun isSelected(bone: Int) = native.armatureQuery(Q_SELECTED, bone).getOrNull(0) == 1f
    fun poseTranslate(dx: Float, dy: Float) = cmd(CMD_POSE_TRANSLATE, -1, dx, dy)
    /** Pose rotate / scale pivot: Median Point (Blender's default) or Individual Origins. */
    var pivot = AROUND_MEDIAN
    fun poseRotate(angle: Float) = cmd(CMD_POSE_ROTATE, -1, angle, pivot.toFloat())
    fun poseScale(factor: Float) = cmd(CMD_POSE_SCALE, -1, factor, pivot.toFloat())
    /** loc[3], quat[4] (w first), scale[3] in bone space. */
    fun pose(bone: Int) = native.armatureQuery(Q_POSE, bone)
    fun setPose(bone: Int, values: FloatArray) = cmd(CMD_POSE_SET, bone, *values)
    fun poseClear(what: Int) = cmd(CMD_POSE_CLEAR, -1, what.toFloat())
    fun poseCopy() = cmd(CMD_POSE_COPY)
    fun posePaste(flipped: Boolean, selectedOnly: Boolean = false) =
        cmd(CMD_POSE_PASTE, -1, if (flipped) 1f else 0f, if (selectedOnly) 1f else 0f)

    // ---- action (dope sheet: AnimationController.boneKeys) ----
    /** LocRotScale keys on the selected bones at [frame]. */
    fun insertKey(frame: Int) = cmd(CMD_KEY_INSERT, -1, frame.toFloat())
    fun deleteKey(bone: Int, frame: Int) = cmd(CMD_KEY_DELETE, bone, frame.toFloat())
    fun setInterpolation(bone: Int, frame: Int, ipo: Int) = cmd(CMD_KEY_INTERPOLATION, bone, frame.toFloat(), ipo.toFloat())
    fun interpolation(bone: Int, frame: Int) = native.armatureQuery(Q_KEY_INTERPOLATION, bone, frame.toFloat()).getOrNull(0)?.toInt() ?: -1
    /** Dope sheet Grab: the bone's keys at [frame] go to [to] (a key already there is replaced). */
    fun moveKey(bone: Int, frame: Int, to: Int) = cmd(CMD_KEY_MOVE, bone, frame.toFloat(), to.toFloat())
    fun keys(bone: Int): FloatArray = native.armatureQuery(Q_KEYS, bone)
    /** Evaluates the action at [frame] (the backend also does it whenever the scene frame changes). */
    fun evaluate(frame: Float) = cmd(CMD_EVALUATE, -1, frame)

    // ---- viewport display (saved with the rig) ----
    /** Octahedral / Stick, and In Front (bones over the strokes). */
    fun setDisplay(type: Int, inFront: Boolean) = cmd(CMD_DISPLAY, -1, type.toFloat(), if (inFront) 1f else 0f) == 1
    val displayType get() = native.armatureQuery(Q_DISPLAY).getOrNull(0)?.toInt() ?: DISPLAY_OCTAHEDRAL
    val showInFront get() = native.armatureQuery(Q_DISPLAY).getOrNull(1) == 1f
    /** X-Axis Mirror (Armature.use_mirror_x): edits also change the bone of the flipped name (saved with the rig). */
    fun setMirrorX(on: Boolean) = cmd(CMD_MIRROR_X, -1, if (on) 1f else 0f) == 1
    val mirrorX get() = native.armatureQuery(Q_DISPLAY).getOrNull(2) == 1f
    /** The Extrude tool is armed: the next canvas drag extrudes from the selected tips (RigTouch). */
    var extrudeNext = false
    /**
     * Tap select in Edit / Pose mode: the bone nearest to (x, y) within [dist] (Blender's
     * ED_view3d_select_dist_px); in Edit mode a head / tail joint selects that end only. A tap on
     * nothing deselects all unless [extend]. Camera view: x, y, dist in canvas px. 3D view: x, y,
     * dist in view px and [canvasToWindow] = View3DNative.nativeCanvasMatrix(). Returns the bone or -1.
     */
    fun pick(x: Float, y: Float, dist: Float, extend: Boolean = false, canvasToWindow: FloatArray? = null): Int {
        val head = floatArrayOf(x, y, dist, if (extend) 1f else 0f)
        return cmd(CMD_PICK, -1, *(if (canvasToWindow != null && canvasToWindow.size == 16) head + canvasToWindow else head))
    }

    /** Parent with Armature / Empty Groups / Automatic Weights / Envelope Weights; adds the Armature modifier. */
    fun bind(mode: Int) = cmd(CMD_BIND, -1, mode.toFloat())

    // ---- constraints / custom properties / drivers (project_grease_constraint.h, project_grease_driver.h) ----
    // Numbers are Blender's own (DNA_constraint_types.h / DNA_anim_types.h); see RigNames for names.
    object RigIds {
        // PG_ARM_CMD_* 40..59 / PG_ARM_Q_* 20..25 (project_grease_gp_bridge.h)
        const val CMD_CON_ADD = 40; const val CMD_CON_REMOVE = 41; const val CMD_CON_MOVE = 42
        const val CMD_CON_TARGET = 43; const val CMD_CON_SET = 44; const val CMD_CON_CHILDOF_INVERSE = 45
        const val CMD_PROP_ADD = 46; const val CMD_PROP_REMOVE = 47; const val CMD_PROP_SET = 48
        const val CMD_DRIVER_ADD = 49; const val CMD_DRIVER_REMOVE = 50; const val CMD_DRIVER_TYPE = 51
        const val CMD_DRIVER_EXPRESSION = 52; const val CMD_DRIVER_VAR_ADD = 53; const val CMD_DRIVER_VAR_REMOVE = 54
        const val CMD_DRIVER_VAR_RENAME = 55; const val CMD_DRIVER_VAR_PATH = 56; const val CMD_DRIVER_VAR_TRANSFORM = 57
        const val CMD_DRIVER_KEYS = 58; const val CMD_DRIVER_GENERATOR = 59
        const val Q_CONSTRAINTS = 20; const val Q_CONSTRAINT = 21; const val Q_PROPS = 22; const val Q_PROP = 23
        const val Q_DRIVERS = 24; const val Q_DRIVER = 25
        // CONSTRAINT_TYPE_*
        const val CON_CHILD_OF = 1; const val CON_IK = 3; const val CON_LIMIT_ROTATION = 5
        const val CON_COPY_ROTATION = 8; const val CON_COPY_LOCATION = 9; const val CON_DAMPED_TRACK = 21
        // CONSTRAINT_SPACE_*
        const val SPACE_WORLD = 0; const val SPACE_LOCAL = 1; const val SPACE_POSE = 2; const val SPACE_LOCAL_WITH_PARENT = 3
        const val SPACE_OWNER_LOCAL = 6
        // bKinematicConstraint.flag
        const val IK_USE_TAIL = 1; const val IK_ROTATION = 2; const val IK_STRETCH = 16; const val IK_POSITION = 32
        // ROTLIKE_* / LOCLIKE_* (X 1, Y 2, Z 4, X invert 16, Y invert 32, Z invert 64; LOCLIKE_OFFSET 8)
        const val AXIS_X = 1; const val AXIS_Y = 2; const val AXIS_Z = 4; const val LOC_OFFSET = 8
        const val INVERT_X = 16; const val INVERT_Y = 32; const val INVERT_Z = 64
        // ROTLIKE_MIX_*
        const val MIX_REPLACE = 0; const val MIX_OFFSET = 1; const val MIX_ADD = 2; const val MIX_BEFORE = 3; const val MIX_AFTER = 4
        // TRACK_* (Damped Track)
        const val TRACK_X = 0; const val TRACK_Y = 1; const val TRACK_Z = 2; const val TRACK_NX = 3; const val TRACK_NY = 4; const val TRACK_NZ = 5
        // LIMIT_XROT / YROT / ZROT, LIMIT_TRANSFORM (flag2)
        const val LIMIT_X = 1; const val LIMIT_Y = 2; const val LIMIT_Z = 4; const val LIMIT_AFFECT_TRANSFORM = 2
        // CHILDOF_* (LOCX 1 .. SIZEZ 256 = CHILDOF_ALL 511, SET_INVERSE 512)
        const val CHILDOF_ALL = 511; const val CHILDOF_LOCATION = 7; const val CHILDOF_ROTATION = 56; const val CHILDOF_SCALE = 448
        // DRIVER_TYPE_*
        const val DRIVER_AVERAGE = 0; const val DRIVER_SCRIPTED = 1; const val DRIVER_SUM = 2; const val DRIVER_MIN = 3; const val DRIVER_MAX = 4
        // DVAR_TYPE_* / DTAR_TRANSCHAN_* / DTAR_FLAG_*
        const val VAR_SINGLE_PROPERTY = 0; const val VAR_TRANSFORM_CHANNEL = 3
        const val CHAN_LOC_X = 0; const val CHAN_LOC_Y = 1; const val CHAN_LOC_Z = 2; const val CHAN_ROT_X = 3
        const val CHAN_ROT_Y = 4; const val CHAN_ROT_Z = 5; const val CHAN_SCALE_X = 6; const val CHAN_SCALE_Y = 7
        const val CHAN_SCALE_Z = 8; const val CHAN_ROT_W = 9; const val CHAN_SCALE_AVG = 10
        const val TRANSFORM_SPACE = 4; const val LOCAL_SPACE = 4 or 8 // DTAR_FLAG_LOCALSPACE (| LOCAL_CONSTS)
        const val GENERATOR_EXPANDED = 0; const val GENERATOR_FACTORISED = 1
    }

    /** Escaped like Blender's RNA paths: pose.bones["name"]. */
    private fun quoted(name: String) = "[\"" + name.replace("\\", "\\\\").replace("\"", "\\\"") + "\"]"
    fun bonePath(bone: String, channel: String) = "pose.bones" + quoted(bone) + "." + channel
    fun bonePropertyPath(bone: String, property: String) = "pose.bones" + quoted(bone) + quoted(property)
    fun objectPropertyPath(property: String) = quoted(property)
    fun layerPath(layer: String, property: String) = "layers" + quoted(layer) + "." + property
    /** Time Offset modifier [modifier] of [layer]: "offset" or "frame_scale". */
    fun timeOffsetPath(layer: String, modifier: Int, property: String = "offset") =
        "layers" + quoted(layer) + ".modifiers[" + modifier + "]." + property

    /** Adds a constraint of [type] to [bone] (no target yet); returns its index or -1. */
    fun addConstraint(bone: Int, type: Int, name: String = "") = cmd(RigIds.CMD_CON_ADD, bone, type.toFloat(), name = name)
    fun removeConstraint(bone: Int, index: Int) = cmd(RigIds.CMD_CON_REMOVE, bone, index.toFloat()) == 1
    fun moveConstraint(bone: Int, index: Int, to: Int) = cmd(RigIds.CMD_CON_MOVE, bone, index.toFloat(), to.toFloat()) == 1
    /** Target bone ("" = the armature object, null clears); [pole] for the IK pole target. */
    fun setConstraintTarget(bone: Int, index: Int, target: String?, pole: Boolean = false) =
        cmd(RigIds.CMD_CON_TARGET, bone, index.toFloat(), if (pole) 1f else 0f, if (target == null) 1f else 0f, name = target ?: "") == 1
    /**
     * Settings: influence, owner space, target space, head/tail, mute, then per type (see
     * project_grease_constraint.h): IK flag, chain count, iterations, pole angle, weight, orient
     * weight; Copy Rotation flag, euler order, mix; Copy Location flag; Damped Track axis; Limit
     * Rotation flag, flag2, xmin, xmax, ymin, ymax, zmin, zmax, euler order; Child Of flag.
     */
    fun setConstraint(bone: Int, index: Int, values: FloatArray) = cmd(RigIds.CMD_CON_SET, bone, index.toFloat(), *values) == 1
    /** type, then the settings above (Child Of also its inverse matrix). */
    fun constraint(bone: Int, index: Int): FloatArray = native.armatureQuery(RigIds.Q_CONSTRAINT, bone, index.toFloat())
    fun constraintCount(bone: Int) = native.armatureQuery(RigIds.Q_CONSTRAINTS, bone).getOrNull(0)?.toInt() ?: 0
    fun childOfSetInverse(bone: Int, index: Int) = cmd(RigIds.CMD_CON_CHILDOF_INVERSE, bone, index.toFloat(), 1f) == 1
    fun childOfClearInverse(bone: Int, index: Int) = cmd(RigIds.CMD_CON_CHILDOF_INVERSE, bone, index.toFloat(), 0f) == 1

    /** Custom float property of the object ([bone] -1) or a bone; an existing name is edited. Index or -1. */
    fun addProperty(name: String, bone: Int = -1, value: Float = 1f, min: Float = 0f, max: Float = 1f, default: Float = 1f) =
        cmd(RigIds.CMD_PROP_ADD, bone, value, min, max, default, name = name)
    fun removeProperty(index: Int) = cmd(RigIds.CMD_PROP_REMOVE, -1, index.toFloat()) == 1
    /** Clamped to the property's range; drivers re-evaluate. */
    fun setProperty(index: Int, value: Float) = cmd(RigIds.CMD_PROP_SET, -1, index.toFloat(), value) == 1
    val propertyCount get() = native.armatureQuery(RigIds.Q_PROPS).getOrNull(0)?.toInt() ?: 0
    /** bone (-1 object), value, min, max, default */
    fun property(index: Int): FloatArray = native.armatureQuery(RigIds.Q_PROP, -1, index.toFloat())

    /** Driver on [path] / [arrayIndex]; an existing one is returned. Keys (0,0)(1,1) or a Generator. */
    fun addDriver(path: String, arrayIndex: Int = 0, type: Int = RigIds.DRIVER_SCRIPTED, generator: Boolean = false) =
        cmd(RigIds.CMD_DRIVER_ADD, -1, arrayIndex.toFloat(), type.toFloat(), if (generator) 1f else 0f, name = path)
    fun removeDriver(driver: Int) = cmd(RigIds.CMD_DRIVER_REMOVE, -1, driver.toFloat()) == 1
    fun setDriverType(driver: Int, type: Int) = cmd(RigIds.CMD_DRIVER_TYPE, -1, driver.toFloat(), type.toFloat()) == 1
    /** Scripted expression (Blender's simple expressions: var, var*k+c, -var, functions ...). */
    fun setDriverExpression(driver: Int, expression: String) =
        cmd(RigIds.CMD_DRIVER_EXPRESSION, -1, driver.toFloat(), name = expression) == 1
    fun addDriverVariable(driver: Int, name: String = "var", type: Int = RigIds.VAR_SINGLE_PROPERTY) =
        cmd(RigIds.CMD_DRIVER_VAR_ADD, -1, driver.toFloat(), type.toFloat(), name = name)
    fun removeDriverVariable(driver: Int, variable: Int) =
        cmd(RigIds.CMD_DRIVER_VAR_REMOVE, -1, driver.toFloat(), variable.toFloat()) == 1
    fun renameDriverVariable(driver: Int, variable: Int, name: String) =
        cmd(RigIds.CMD_DRIVER_VAR_RENAME, -1, driver.toFloat(), variable.toFloat(), name = name) == 1
    /** Single Property: an RNA path with the array index in brackets (pose.bones["B"].location[1]). */
    fun setDriverVariablePath(driver: Int, variable: Int, path: String) =
        cmd(RigIds.CMD_DRIVER_VAR_PATH, -1, driver.toFloat(), variable.toFloat(), name = path) == 1
    /** Transform Channel of [bone] ("" the object): CHAN_*, DTAR_ROTMODE_* (0 auto), space flags. */
    fun setDriverVariableTransform(driver: Int, variable: Int, bone: String, channel: Int, rotationMode: Int = 0, space: Int = 0) =
        cmd(RigIds.CMD_DRIVER_VAR_TRANSFORM, -1, driver.toFloat(), variable.toFloat(), channel.toFloat(),
            rotationMode.toFloat(), space.toFloat(), name = bone) == 1
    /** Driver F-Curve keys as (x, y, ipo) triples; extrapolate linearly when [extend]. */
    fun setDriverKeys(driver: Int, keys: FloatArray, extend: Boolean = true) =
        cmd(RigIds.CMD_DRIVER_KEYS, -1, driver.toFloat(), if (extend) 1f else 0f, *keys) == 1
    /** Generator modifier (expanded c0 + c1 x + ... or factorised); empty removes it. */
    fun setDriverGenerator(driver: Int, mode: Int, additive: Boolean, coefficients: FloatArray) =
        cmd(RigIds.CMD_DRIVER_GENERATOR, -1, driver.toFloat(), mode.toFloat(), if (additive) 1f else 0f, *coefficients) == 1
    val driverCount get() = native.armatureQuery(RigIds.Q_DRIVERS).getOrNull(0)?.toInt() ?: 0
    /** array index, type, flag (1 invalid), driver value, F-Curve value, variable count, keys, generator, then per variable type, value, flag. */
    fun driver(index: Int): FloatArray = native.armatureQuery(RigIds.Q_DRIVER, -1, index.toFloat())

    // ---- drawing swap / master slider (built only from Blender features) ----
    /**
     * Swap: the layer's Time Offset modifier in Fixed Frame mode shows drawing number "offset"; a driver
     * sets offset = round(swap) where swap is Loc X of the control bone "Swap.<layer>" (1 unit = one
     * drawing). Keys on that bone key the swap; [linkSlider] adds custom properties to the same driver.
     * Returns the driver index or -1. The caller adds the TIME modifier ([modifier] is its index) and sets
     * mode Fixed Frame (param 0 = 2).
     */
    fun addSwap(layer: String, modifier: Int, headX: Float, headY: Float): Int {
        val bone = "Swap." + layer
        if (findBone(bone) < 0) {
            val m = mode
            if (m != MODE_EDIT) setMode(MODE_EDIT)
            if (addBone(headX, headY, headX, headY - 60f, bone) < 0) return -1
            setMode(m)
        }
        val d = addDriver(timeOffsetPath(layer, modifier))
        if (d < 0) return -1
        if (driver(d).getOrNull(5)?.toInt() == 0) {
            val v = addDriverVariable(d, "swap", RigIds.VAR_TRANSFORM_CHANNEL)
            setDriverVariableTransform(d, v, bone, RigIds.CHAN_LOC_X, 0, RigIds.LOCAL_SPACE)
            setDriverExpression(d, "round(swap)")
        }
        return d
    }

    /** Swap bone Loc X for drawing [index] (pose units; 1 unit per drawing). */
    fun setSwap(layer: String, index: Int): Boolean {
        val b = findBone("Swap." + layer)
        if (b < 0) return false
        val p = pose(b).takeIf { it.size >= 10 } ?: return false
        p[0] = index.toFloat()
        return setPose(b, p) >= 0
    }

    /**
     * Links the object property [prop] (master slider) to [path]/[index] with value = prop * [factor]
     * (added to an existing driver's expression, so a swap keeps its own key). Rotation of a 2D bone is a
     * pair of drivers on rotation_quaternion w / z (bone Z is the screen normal): pass [rotation] true and
     * [factor] in radians at prop 1. Returns the number of drivers written.
     */
    fun linkSlider(prop: String, path: String, index: Int, factor: Float, rotation: Boolean = false): Int {
        val v = prop.replace(Regex("[^A-Za-z0-9_]"), "_").let { if (it.firstOrNull()?.isDigit() != false) "p_$it" else it }
        fun one(p: String, i: Int, term: (String) -> String): Boolean {
            val d = addDriver(p, i)
            if (d < 0) return false
            val info = driver(d)
            val vars = info.getOrNull(5)?.toInt() ?: 0
            val k = addDriverVariable(d, v)
            if (k < 0 || !setDriverVariablePath(d, k, objectPropertyPath(prop))) return false
            // the existing expression is kept as a term (a swap's round(swap)); a fresh driver is the term alone
            val old = if (vars == 0) null else runCatching {
                org.json.JSONObject(json()).getJSONArray("drivers").getJSONObject(d).getString("expression") }.getOrNull()
            val e = if (old == null) term(v) else old.replace("round(swap)", "round(swap + ${term(v)})").takeIf { it != old } ?: "$old + ${term(v)}"
            return setDriverExpression(d, e)
        }
        if (!rotation) return if (one(path, index) { "$it*$factor" }) 1 else 0
        var n = 0
        if (one(path, 0) { "cos($it*${factor / 2f})" }) n++
        if (one(path, 3) { "sin($it*${factor / 2f})" }) n++
        return n
    }
}
