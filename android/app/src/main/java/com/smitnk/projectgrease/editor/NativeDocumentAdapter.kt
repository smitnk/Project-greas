package com.smitnk.projectgrease.editor

/** [DocumentNative] over the real native bridge; see GPNative for the float layouts. */
class NativeDocumentAdapter(private val native: NativeEditorBridge) : DocumentNative {
    override fun layerCount() = native.layerCount()

    override fun layerRecord(index: Int): LayerRecord? {
        val info = native.layerInfo(index) ?: return null
        if (info.size < 3) return null
        val name = native.layerName(index) ?: ""
        return LayerRecord(name, info[0] != 0f, info[1] != 0f, info[2])
    }

    override fun selectLayer(index: Int) = native.selectLayer(index)
    override fun frameNumbers() = native.frameNumbers()
    override fun selectFrame(frame: Int) = native.selectFrame(frame)
    override fun strokeCount() = native.strokeCount()

    override fun strokeRecord(index: Int): StrokeRecord? {
        val points = ArrayList<FloatArray>()
        while (true) {
            val point = native.getPoint(index, points.size) ?: break
            val color = native.pointColor(index, points.size) ?: FloatArray(4)
            points.add(point.copyOf(StrokeRecord.POINT_SIZE).also { System.arraycopy(color, 0, it, 6, 4) })
        }
        if (points.isEmpty()) return null
        val weights = LinkedHashMap<Int, FloatArray>()
        for (p in points.indices) {
            val pairs = native.pointWeights(index, p)
            if (pairs != null && pairs.size >= 2) weights[p] = pairs
        }
        val info = native.strokeInfo(index)
        if (info == null || info.size < 8) return StrokeRecord(points, weights = weights)
        return StrokeRecord(
            points = points,
            materialIndex = info[0].toInt(),
            thickness = info[1],
            cyclic = info[2] != 0f,
            fillOpacity = info[3],
            fillColor = info.copyOfRange(4, 8),
            weights = weights
        )
    }

    override fun materialCount() = native.materialCount()

    override fun materialRecord(index: Int): MaterialRecord? {
        val info = native.materialInfo(index) ?: return null
        if (info.size < 10) return null
        return MaterialRecord(
            stroke = info.copyOfRange(0, 4),
            fill = info.copyOfRange(4, 8),
            visible = info[8] != 0f,
            fillEnabled = info[9] != 0f
        )
    }

    override fun createLayer(name: String) = native.createLayer(name)

    override fun applyLayerRecord(index: Int, record: LayerRecord): Boolean {
        if (record.name.isNotBlank() && !native.renameLayer(index, record.name)) return false
        return native.setLayerVisibility(index, record.visible) &&
            native.setLayerLocked(index, record.locked) &&
            native.setLayerOpacity(index, record.opacity)
    }

    override fun createFrame(frame: Int) = native.createFrame(frame)

    override fun addStroke(record: StrokeRecord): Boolean {
        val flat = FloatArray(record.points.size * 6)
        val colors = FloatArray(record.points.size * 4)
        record.points.forEachIndexed { i, p ->
            System.arraycopy(p, 0, flat, i * 6, 6)
            // Points built without vertex color (6 values) keep the zero default.
            if (p.size >= StrokeRecord.POINT_SIZE) System.arraycopy(p, 6, colors, i * 4, 4)
        }
        val info = floatArrayOf(
            record.materialIndex.toFloat(), record.thickness, if (record.cyclic) 1f else 0f, record.fillOpacity,
            record.fillColor[0], record.fillColor[1], record.fillColor[2], record.fillColor[3]
        )
        if (!native.addStroke(flat, record.points.size, info, colors)) return false
        if (record.weights.isNotEmpty()) {
            val stroke = native.strokeCount() - 1 // the stroke was appended to the selected frame
            for ((point, values) in record.weights) {
                var i = 0
                while (i + 1 < values.size) {
                    if (!native.setPointWeight(stroke, point, values[i].toInt(), values[i + 1])) return false
                    i += 2
                }
            }
        }
        return true
    }

    override fun modifierCount(layer: Int) = native.modifierCount(layer)

    override fun modifierRecord(layer: Int, index: Int) =
        ModifierStackPacking.unpack(native.modifierGet(layer, index))

    override fun addModifier(layer: Int, record: ModifierRecord): Boolean {
        val index = ModifierStackCommands.add(native, layer, record.type)
        if (index < 0) return false
        return native.modifierSetParams(layer, index, ModifierStackPacking.paramsFor(record.type, record.params)) &&
            native.modifierSetEnabled(layer, index, record.enabled)
    }

    override fun fxCount(layer: Int) = native.fxCount(layer)

    override fun fxRecord(layer: Int, index: Int) = FxPacking.unpack(native.fxGet(layer, index))

    override fun addFx(layer: Int, record: FxRecord): Boolean {
        val index = FxCommands.add(native, layer, record.type)
        if (index < 0) return false
        return native.fxSetParams(layer, index, FxPacking.paramsFor(record.type, record.params)) &&
            native.fxSetEnabled(layer, index, record.enabled)
    }

    override fun vertexGroups(): List<String> =
        (0 until native.vertexGroupCount()).map { native.vertexGroupName(it) ?: "Group" }

    override fun activeVertexGroup() = native.vertexGroupActive()

    override fun restoreVertexGroups(names: List<String>, active: Int): Boolean {
        for (name in names) if (native.vertexGroupAdd(name) < 0) return false
        return active < 0 || active >= names.size || native.setVertexGroupActive(active)
    }

    override fun layerUseMask(layer: Int) = native.layerUseMask(layer)

    override fun layerMasks(layer: Int): List<MaskRecord> =
        (0 until native.maskCount(layer)).mapNotNull { index ->
            val name = native.maskName(layer, index) ?: return@mapNotNull null
            val flags = native.maskFlags(layer, index).takeIf { it >= 0 } ?: return@mapNotNull null
            MaskRecord(name, (flags and 1) != 0, (flags and 2) != 0)
        }

    override fun restoreLayerMasks(layer: Int, useMask: Boolean, masks: List<MaskRecord>): Boolean {
        for (mask in masks) {
            // A name that no layer has any more (hand-edited file) is dropped, not an error.
            val maskLayer = (0 until native.layerCount()).firstOrNull { native.layerName(it) == mask.name } ?: continue
            if (maskLayer == layer || !native.maskAdd(layer, maskLayer)) continue
            val index = native.maskCount(layer) - 1
            val flags = (if (mask.hidden) 1 else 0) or (if (mask.inverted) 2 else 0)
            if (flags != 0 && !native.maskSetFlags(layer, index, flags)) return false
        }
        return native.setLayerUseMask(layer, useMask)
    }

    override fun createMaterial() = native.createMaterial()

    override fun applyMaterialRecord(index: Int, record: MaterialRecord) =
        native.setMaterialColors(index, record.stroke, record.fill) &&
            native.setMaterialVisibility(index, record.visible) &&
            native.setMaterialFillEnabled(index, record.fillEnabled)
}
