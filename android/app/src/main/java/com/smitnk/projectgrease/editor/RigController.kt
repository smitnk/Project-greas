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
        const val CMD_EVALUATE = 27
        // PG_ARM_Q_*
        const val Q_SUMMARY = 0; const val Q_BONE = 1; const val Q_POSE = 2; const val Q_KEYS = 3
        const val Q_KEY_INTERPOLATION = 4; const val Q_ENVELOPE = 5; const val Q_SELECTED = 6

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
        native.armatureCommand(c, bone, if (args.isEmpty()) null else args, name)

    val exists get() = boneCount >= 0
    /** -1 without a rig. */
    val boneCount get() = native.armatureQuery(Q_SUMMARY).getOrNull(0)?.toInt() ?: -1
    val activeBone get() = native.armatureQuery(Q_SUMMARY).getOrNull(1)?.toInt() ?: -1
    val mode get() = native.armatureQuery(Q_SUMMARY).getOrNull(2)?.toInt() ?: MODE_OBJECT

    fun create() = cmd(CMD_CREATE) == 1
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

    // ---- pose mode ----
    fun poseSelect(bone: Int, extend: Boolean = false) = cmd(CMD_POSE_SELECT, bone, if (extend) 1f else 0f)
    fun isSelected(bone: Int) = native.armatureQuery(Q_SELECTED, bone).getOrNull(0) == 1f
    fun poseTranslate(dx: Float, dy: Float) = cmd(CMD_POSE_TRANSLATE, -1, dx, dy)
    fun poseRotate(angle: Float) = cmd(CMD_POSE_ROTATE, -1, angle)
    fun poseScale(factor: Float) = cmd(CMD_POSE_SCALE, -1, factor)
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
    fun keys(bone: Int): FloatArray = native.armatureQuery(Q_KEYS, bone)
    /** Evaluates the action at [frame] (the backend also does it whenever the scene frame changes). */
    fun evaluate(frame: Float) = cmd(CMD_EVALUATE, -1, frame)

    /** Parent with Armature / Empty Groups / Automatic Weights / Envelope Weights; adds the Armature modifier. */
    fun bind(mode: Int) = cmd(CMD_BIND, -1, mode.toFloat())
}
