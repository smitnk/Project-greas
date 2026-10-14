@file:OptIn(ExperimentalMaterial3Api::class, androidx.compose.foundation.layout.ExperimentalLayoutApi::class)
package com.smitnk.projectgrease.ui

import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.runtime.NonSkippableComposable
import androidx.compose.ui.platform.testTag
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.platform.LocalContext
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.input.pointer.pointerInput
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.FeatureId
import com.smitnk.projectgrease.editor.FeatureRegistry
import com.smitnk.projectgrease.editor.FeatureState
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.GreaseTemplates
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.BrushPreset
import com.smitnk.projectgrease.editor.EraserMode

private val Accent = Color(0xFFE84F7B)
private val CanvasBg = Color(0xFF121315)
private enum class Screen { HOME, NEW, EDITOR, SETTINGS }
private enum class Sheet { NONE, PROJECT, TOOLS, LAYERS, MATERIALS, ONION, ADVANCED, MORE, REFERENCE, ROTOSCOPE, LAYER_ADVANCED, MENU, DRAW_SETTINGS, TIMELINE, EFFECTS }
private data class Preset(val name:String,val width:Int,val height:Int,val fps:Int)
private data class ToolEntry(val tool:GreaseTool,val icon:ImageVector,val label:String,val feature:FeatureId)
private val presets=listOf(
    Preset("YouTube 1080p",1920,1080,30),Preset("YouTube 720p",1280,720,12),
    Preset("Instagram 1:1",1080,1080,30),Preset("TikTok 1080p",1080,1920,30),Preset("Vimeo 1080p",1920,1080,30)
)
private val tools=listOf(
    ToolEntry(GreaseTool.DRAW,Icons.Default.Edit,"Draw",FeatureId.FREEHAND),
    ToolEntry(GreaseTool.ERASE,Icons.Default.Clear,"Erase",FeatureId.ERASER),
    ToolEntry(GreaseTool.SELECT,Icons.Default.TouchApp,"Select",FeatureId.SELECT),
    ToolEntry(GreaseTool.LASSO,Icons.Default.Gesture,"Lasso",FeatureId.LASSO),
    ToolEntry(GreaseTool.FILL,Icons.Default.FormatColorFill,"Fill",FeatureId.FILL),
    ToolEntry(GreaseTool.EYEDROPPER,Icons.Default.Colorize,"Pick",FeatureId.STROKE_COLOR),
    ToolEntry(GreaseTool.LINE,Icons.Default.Remove,"Line",FeatureId.LINE),
    ToolEntry(GreaseTool.RECTANGLE,Icons.Default.CropSquare,"Rect",FeatureId.RECTANGLE),
    ToolEntry(GreaseTool.CIRCLE,Icons.Default.RadioButtonUnchecked,"Circle",FeatureId.CIRCLE),
    ToolEntry(GreaseTool.ARC,Icons.Default.Timeline,"Arc",FeatureId.ARC),
    ToolEntry(GreaseTool.POLYLINE,Icons.Default.Timeline,"Polyline",FeatureId.POLYLINE),
    ToolEntry(GreaseTool.CURVE,Icons.Default.ShowChart,"Curve",FeatureId.CURVE),
    ToolEntry(GreaseTool.ANNOTATE,Icons.Default.EditNote,"Annotate",FeatureId.ANNOTATIONS),
    ToolEntry(GreaseTool.MOVE,Icons.Default.OpenWith,"Move",FeatureId.MOVE),
    ToolEntry(GreaseTool.ROTATE,Icons.Default.RotateRight,"Rotate",FeatureId.ROTATE),
    ToolEntry(GreaseTool.SCALE,Icons.Default.ZoomIn,"Scale",FeatureId.SCALE),
    ToolEntry(GreaseTool.MIRROR,Icons.Default.Flip,"Mirror",FeatureId.MIRROR),
    ToolEntry(GreaseTool.PAN,Icons.Default.PanTool,"Pan",FeatureId.PAN),
    ToolEntry(GreaseTool.SCULPT,Icons.Default.AutoFixHigh,"Sculpt",FeatureId.SCULPT),
    ToolEntry(GreaseTool.BOX_SELECT,Icons.Default.SelectAll,"Box",FeatureId.SELECT_BOX),
    ToolEntry(GreaseTool.CIRCLE_SELECT,Icons.Default.TripOrigin,"Circle sel",FeatureId.SELECT_CIRCLE)
)
data class GreaseUiState(
    val projectName:String="Project Grease",val canvasFocus:Boolean=false,
    val showTools:Boolean=true,val showTimeline:Boolean=true,val showProperties:Boolean=false
)

@Composable
fun ProjectGreaseApp(controller:EditorController,blenderViewport:@Composable BoxScope.()->Unit){
    val context=LocalContext.current
    val projectStore=remember{ProjectStore(context)}
    var projects by remember{mutableStateOf(projectStore.load())}
    var screen by remember{mutableStateOf(Screen.HOME)}
    var name by remember{mutableStateOf("Project Grease")}
    var preset by remember{mutableStateOf(presets[1])}
    var templateId by remember{mutableStateOf("2d_animation")}
    var state by remember{mutableStateOf(GreaseUiState())}
    var themeMode by remember{mutableStateOf(ProjectGreaseThemeMode.SYSTEM)}

    fun saveCurrentProject() {
        controller.saveDocumentJson()?.let { projectStore.saveDocument(controller.document.projectName, it) }
        projectStore.upsert(ProjectRecord(
            controller.document.projectName,
            controller.document.canvasWidth,
            controller.document.canvasHeight,
            controller.animation.fps,
            System.currentTimeMillis()
        ))
        projects=projectStore.load()
        controller.document.markSaved()
    }

    LaunchedEffect(screen) {
        if (screen == Screen.EDITOR) {
            while (true) {
                kotlinx.coroutines.delay(5000L)
                saveCurrentProject()
            }
        }
    }

    BackHandler(enabled=screen!=Screen.HOME){
        if (screen == Screen.EDITOR) saveCurrentProject()
        screen=when(screen){
            Screen.NEW,Screen.SETTINGS,Screen.EDITOR->Screen.HOME
            Screen.HOME->Screen.HOME
        }
    }

    ProjectGreaseTheme(mode=themeMode){
        when(screen){
            Screen.HOME->Home(projects,{record->
                controller.document.projectName=record.name
                controller.document.canvasWidth=record.width
                controller.document.canvasHeight=record.height
                controller.animation.setFps(record.fps)
                projectStore.loadDocument(record.name)?.let { raw -> controller.runWhenAttached { controller.loadDocumentJson(raw) } }
                screen=Screen.EDITOR
            },{screen=Screen.NEW},{screen=Screen.SETTINGS})
            Screen.NEW->NewProject(name,{name=it},preset,{preset=it},templateId,{templateId=it},controller,{screen=Screen.HOME}){
                // Never overwrite an existing project: pick "Name (2)", "Name (3)", ... if taken.
                controller.document.projectName=projectStore.uniqueProjectName(name.ifBlank{"Project Grease"})
                controller.document.canvasWidth=preset.width
                controller.document.canvasHeight=preset.height
                controller.animation.setFps(preset.fps)
                val projectName=controller.document.projectName
                val template=GreaseTemplates.byId(templateId)
                controller.runWhenAttached {
                    controller.resetDocument()
                    controller.createFrame(1)
                    // Layers, material slots, fps and end frame come from the chosen template.
                    template?.let { controller.applyTemplate(it) }
                    controller.applyProjectSettings(com.smitnk.projectgrease.editor.ProjectSettings().apply {
                        width=controller.document.canvasWidth; height=controller.document.canvasHeight
                        fps=controller.animation.fps; frameEnd=(template?.endFrame ?: 250).coerceAtLeast(1)
                    })
                    controller.saveDocumentJson()?.let { projectStore.saveDocument(projectName,it) }
                }
                val record=ProjectRecord(controller.document.projectName,preset.width,preset.height,controller.animation.fps,System.currentTimeMillis())
                projectStore.upsert(record)
                projects=projectStore.load()
                state=state.copy(projectName=controller.document.projectName)
                screen=Screen.EDITOR
            }
            Screen.EDITOR->Editor(controller,state,{state=it},{screen=Screen.HOME},{screen=Screen.SETTINGS},blenderViewport)
            Screen.SETTINGS->Settings(
                controller,
                themeMode,
                {themeMode=it},
                {screen=Screen.HOME}
            )
        }
    }
}

/**
 * Curve tool handles (start, end and the two Bezier control points) over the viewport, mapped with
 * the same fit/zoom/pan as the EGL surface's canvasPoint(), plus Confirm / Cancel.
 */
@Composable private fun CurveHandlesOverlay(controller:EditorController,tick:Int,redraw:()->Unit){
    @Suppress("UNUSED_VARIABLE") val observed=tick
    if(!controller.curveEditing) return
    val handles=controller.curveHandles()
    Box(Modifier.fillMaxSize()){
        Canvas(Modifier.fillMaxSize()){
            val cw=controller.document.canvasWidth.coerceAtLeast(1).toFloat()
            val ch=controller.document.canvasHeight.coerceAtLeast(1).toFloat()
            val fit=minOf(size.width/cw,size.height/ch)*0.92f*controller.view.zoom
            val ox=(size.width-cw*fit)*0.5f+controller.view.panX
            val oy=(size.height-ch*fit)*0.5f+controller.view.panY
            fun screen(p:Pair<Float,Float>)=Offset(ox+p.first*fit,oy+p.second*fit)
            if(handles.size==4){
                val handleLine=Color(0xAAFFFFFF)
                drawLine(handleLine,screen(handles[0]),screen(handles[2]),2f)
                drawLine(handleLine,screen(handles[1]),screen(handles[3]),2f)
                handles.forEachIndexed{i,h->
                    drawCircle(Color.Black,11f,screen(h))
                    drawCircle(if(i<2) Color.White else Accent,8f,screen(h))
                }
            }
        }
        Row(Modifier.align(Alignment.BottomCenter).padding(8.dp),horizontalArrangement=Arrangement.spacedBy(8.dp)){
            Button(onClick={controller.confirmCurve();redraw()}){Text("Confirm curve")}
            OutlinedButton(onClick={controller.cancelCurve();redraw()}){Text("Cancel")}
        }
    }
}

/** Lasso path feedback: the open noose (dashed, closed back to its start) in canvas mapping. */
@Composable private fun LassoPathOverlay(controller:EditorController){
    val path=controller.lassoPath
    if(path.size<2) return
    Canvas(Modifier.fillMaxSize().testTag("lassoPath")){
        val cw=controller.document.canvasWidth.coerceAtLeast(1).toFloat()
        val ch=controller.document.canvasHeight.coerceAtLeast(1).toFloat()
        val fit=minOf(size.width/cw,size.height/ch)*0.92f*controller.view.zoom
        val ox=(size.width-cw*fit)*0.5f+controller.view.panX
        val oy=(size.height-ch*fit)*0.5f+controller.view.panY
        val p=androidx.compose.ui.graphics.Path()
        path.forEachIndexed{i,(x,y)->if(i==0)p.moveTo(ox+x*fit,oy+y*fit) else p.lineTo(ox+x*fit,oy+y*fit)}
        p.close()
        val dash=androidx.compose.ui.graphics.PathEffect.dashPathEffect(floatArrayOf(10f,6f))
        drawPath(p,Color.Black,style=androidx.compose.ui.graphics.drawscope.Stroke(width=3f))
        drawPath(p,Color.White,style=androidx.compose.ui.graphics.drawscope.Stroke(width=1.5f,pathEffect=dash))
    }
}

@NonSkippableComposable
@Composable private fun DrawingGuidesOverlay(controller:EditorController){
    Canvas(Modifier.fillMaxSize()) {
        if(controller.view.showGrid){
            val step=controller.view.gridSize.coerceAtLeast(8f)
            var x=0f
            while(x<size.width){
                drawLine(Color(0x33222222),Offset(x,0f),Offset(x,size.height),1f)
                x+=step
            }
            var y=0f
            while(y<size.height){
                drawLine(Color(0x33222222),Offset(0f,y),Offset(size.width,y),1f)
                y+=step
            }
        }
        if(controller.view.showGuides){
            val gx=size.width*controller.view.guideX
            val gy=size.height*controller.view.guideY
            drawLine(Color(0xAAE84F7B),Offset(gx,0f),Offset(gx,size.height),2f)
            drawLine(Color(0xAAE84F7B),Offset(0f,gy),Offset(size.width,gy),2f)
        }
    }
}

@Composable private fun Home(
    projects:List<ProjectRecord>,
    onOpen:(ProjectRecord)->Unit,
    onNew:()->Unit,
    onSettings:()->Unit
){
    Scaffold(
        topBar={
            Row(Modifier.fillMaxWidth().height(64.dp),verticalAlignment=Alignment.CenterVertically){
                IconButton(onClick=onSettings){Icon(Icons.Default.Menu,"Menu")}
                Spacer(Modifier.weight(1f))
                Text("PROJECT GREASE",fontWeight=FontWeight.Bold,letterSpacing=1.sp)
                Spacer(Modifier.weight(1f))
                IconButton(onClick={}){Icon(Icons.Default.Search,"Search")}
            }
        },
        floatingActionButton={
            FloatingActionButton(onClick=onNew,containerColor=Accent){Icon(Icons.Default.Add,"New")}
        }
    ){pad->
        Column(
            Modifier.fillMaxSize().padding(pad).padding(horizontal=16.dp,vertical=12.dp)
                .verticalScroll(rememberScrollState())
        ){
            Text("Projects",style=MaterialTheme.typography.headlineSmall,fontWeight=FontWeight.Bold)
            Spacer(Modifier.height(12.dp))
            if(projects.isEmpty()){
                Text("No projects yet",color=MaterialTheme.colorScheme.onSurfaceVariant)
                Spacer(Modifier.height(8.dp))
                Text("Create a project to start drawing.",color=MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                projects.forEach{item->
                    Card(
                        Modifier.fillMaxWidth().padding(vertical=5.dp).clickable{onOpen(item)}
                    ){
                        Row(Modifier.padding(14.dp),verticalAlignment=Alignment.CenterVertically){
                            Box(
                                Modifier.size(72.dp).background(
                                    Color(0xFFE8E3E5),RoundedCornerShape(8.dp)
                                )
                            )
                            Spacer(Modifier.width(14.dp))
                            Column(Modifier.weight(1f)){
                                Text(item.name,fontWeight=FontWeight.Bold)
                                Text(
                                    "${item.width} × ${item.height} • ${item.fps} FPS",
                                    color=MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                            Icon(Icons.Default.ChevronRight,"Open")
                        }
                    }
                }
            }
        }
    }
}

@Composable private fun NewProject(name:String,onName:(String)->Unit,preset:Preset,onPreset:(Preset)->Unit,templateId:String,onTemplate:(String)->Unit,controller:EditorController,onBack:()->Unit,onCreate:()->Unit){
    Scaffold(topBar={TopAppBar(title={Text("New Project")},navigationIcon={IconButton(onClick=onBack){Icon(Icons.Default.ArrowBack,"Back")}})}){pad->
        Column(Modifier.fillMaxSize().padding(pad).verticalScroll(rememberScrollState()).padding(16.dp)){
            OutlinedTextField(name,onName,Modifier.fillMaxWidth(),label={Text("Project name")});Spacer(Modifier.height(16.dp))
            Text("Template",color=Accent,fontWeight=FontWeight.Bold)
            GreaseTemplates.ALL.forEach{t->ListItem(headlineContent={Text(t.title)},
                supportingContent={Text(t.layers.joinToString(" · ")+" • "+t.materials.size+" materials • "+t.fps+" FPS • end "+t.endFrame)},
                trailingContent={if(t.id==templateId)Icon(Icons.Default.Check,null,tint=Accent)},modifier=Modifier.clickable{onTemplate(t.id)})}
            Spacer(Modifier.height(16.dp))
            Text("Canvas presets",color=Accent,fontWeight=FontWeight.Bold)
            presets.forEach{p->ListItem(headlineContent={Text(p.name)},supportingContent={Text(p.width.toString()+" × "+p.height+" • "+p.fps+" FPS")},
                trailingContent={if(p==preset)Icon(Icons.Default.Check,null,tint=Accent)},modifier=Modifier.clickable{onPreset(p);controller.animation.setFps(p.fps)})}
            Button(onClick=onCreate,Modifier.fillMaxWidth()){Text("Create Project")}
        }
    }
}

@Composable private fun Editor(controller:EditorController,state:GreaseUiState,onState:(GreaseUiState)->Unit,onExit:()->Unit,onSettings:()->Unit,viewportContent:@Composable BoxScope.()->Unit){
    // One canvas surface for every layout: moving it keeps the EGL surface, document and undo history
    // (a remount would detach the renderer and reload the document).
    val viewport=remember{movableContentWithReceiverOf<BoxScope>{viewportContent()}}
    var sheet by remember{mutableStateOf(Sheet.NONE)}
    var refresh by remember{mutableIntStateOf(0)}
    var fpsDialog by remember{mutableStateOf(false)}
    var savedTick by remember{mutableIntStateOf(0)}
    var overlayTick by remember{mutableIntStateOf(0)}
    val context=LocalContext.current
    fun redraw(){refresh++}
    // Saved texture images (URIs) of an opened project are decoded once the document is loaded.
    LaunchedEffect(controller.rendererReady, refresh){ if(controller.rendererReady) loadPendingTextureImages(context,controller) }
    DisposableEffect(controller){
        controller.onOverlayChanged={overlayTick++}
        onDispose{controller.onOverlayChanged=null}
    }
    fun persistProject(){
        controller.saveDocumentJson()?.let {
            ProjectStore(context).saveDocument(controller.document.projectName,it)
            ProjectStore(context).upsert(ProjectRecord(
                controller.document.projectName,
                controller.document.canvasWidth,
                controller.document.canvasHeight,
                controller.animation.fps,
                System.currentTimeMillis()
            ))
            controller.document.markSaved()
            savedTick++
        }
    }
    @Suppress("UNUSED_VARIABLE") val unused=refresh+savedTick

    BackHandler(enabled=sheet!=Sheet.NONE){ sheet=Sheet.NONE }

    DisposableEffect(controller){
        controller.onUiChanged={redraw()}
        controller.onMessage={toast(context,it)}
        controller.useBlenderRenderer=context.getSharedPreferences("project_grease_settings",android.content.Context.MODE_PRIVATE).getBoolean("blender_renderer",true)
        controller.rotoscopeLoader=RotoscopeLoader(context.applicationContext)
        onDispose{controller.onUiChanged=null;controller.rotoscopeLoader=null}
    }
    // One UI: the full-screen canvas with the floating toolbar, option popups, the layer panel with
    // each layer's Advanced panel, the frame strip, and one menu for everything else.
    StudioLayout(controller,refresh,overlayTick,::redraw,{sheet=it},viewport)
    if(fpsDialog) FpsDialog(controller,{fpsDialog=false},::redraw)
    val rotoscopePickers=rememberRotoscopePickers(controller,context,::redraw)
    when(sheet){
        Sheet.PROJECT->ProjectSheet({sheet=Sheet.NONE},onSettings,onExit,controller,context,::persistProject)
        Sheet.TOOLS->ToolsSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.LAYERS->LayersSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.MATERIALS->MaterialsSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.ONION->OnionSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.ADVANCED->AdvancedSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.MORE->MoreSheet(controller,{sheet=Sheet.NONE},onSettings,::redraw,{sheet=Sheet.REFERENCE})
        Sheet.REFERENCE->ReferenceSheet(controller,context,{sheet=Sheet.NONE},::redraw)
        Sheet.ROTOSCOPE->RotoscopeSheet(controller,context,{sheet=Sheet.NONE},::redraw,rotoscopePickers)
        Sheet.LAYER_ADVANCED->LayerAdvancedSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.MENU->StudioMenu(controller,{sheet=it},{persistProject();Toast.makeText(context,"Project saved",Toast.LENGTH_SHORT).show();sheet=Sheet.NONE},onSettings,{sheet=Sheet.NONE})
        Sheet.DRAW_SETTINGS->ModalBottomSheet(onDismissRequest={sheet=Sheet.NONE}){Properties(controller,::redraw)}
        Sheet.EFFECTS->ModalBottomSheet(onDismissRequest={sheet=Sheet.NONE}){Column(Modifier.verticalScroll(rememberScrollState()).padding(bottom=20.dp)){LayerEffectsSection(controller,::redraw)}}
        Sheet.TIMELINE->ModalBottomSheet(onDismissRequest={sheet=Sheet.NONE}){Timeline(controller,::redraw,{fpsDialog=true})}
        Sheet.NONE->Unit
    }
}

/** Weight Paint mode: vertex group chips with add/rename/remove and the weight to paint toward. */
@NonSkippableComposable
@Composable private fun WeightPaintBar(controller:EditorController,redraw:()->Unit){
    var renameOpen by remember{mutableStateOf(false)}
    var renameText by remember{mutableStateOf("")}
    val groups=controller.vertexGroups()
    FlowRow(
        Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=2.dp),
         itemVerticalAlignment=Alignment.CenterVertically
    ){
        Text("Brush",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
        listOf(
            com.smitnk.projectgrease.editor.ToolSession.GPWEIGHT_DRAW to "Draw",
            com.smitnk.projectgrease.editor.ToolSession.GPWEIGHT_BLUR to "Blur",
            com.smitnk.projectgrease.editor.ToolSession.GPWEIGHT_AVERAGE to "Average",
            com.smitnk.projectgrease.editor.ToolSession.GPWEIGHT_SMEAR to "Smear"
        ).forEach{(brush,label)->
            FilterChip(selected=controller.weightPaintBrush==brush,onClick={controller.setWeightPaintBrush(brush);redraw()},
                label={Text(label,fontSize=10.sp)},modifier=Modifier.padding(end=3.dp))
        }
        FilterChip(selected=controller.weightPaintSubtract,onClick={controller.setWeightPaintSubtract(!controller.weightPaintSubtract);redraw()},
            label={Text("Subtract",fontSize=10.sp)},modifier=Modifier.padding(end=6.dp))
        Text("Group",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
        groups.forEachIndexed{index,name->
            FilterChip(
                selected=controller.weightPaintGroup==index,
                onClick={controller.selectVertexGroup(index);redraw()},
                label={Text(name,fontSize=10.sp)},
                modifier=Modifier.padding(end=3.dp)
            )
        }
        TextButton(onClick={controller.addVertexGroup("Group "+(groups.size+1));redraw()}){Text("Add",fontSize=11.sp)}
        TextButton(onClick={renameText=groups.getOrNull(controller.weightPaintGroup)?:"";renameOpen=true},enabled=groups.isNotEmpty()){Text("Rename",fontSize=11.sp)}
        TextButton(onClick={controller.removeVertexGroup(controller.weightPaintGroup);redraw()},enabled=groups.size>1){Text("Remove",fontSize=11.sp)}
    }
    // Vertex-group operators on the selected points, active group (Blender's Vertex Groups panel).
    FlowRow(
        Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=2.dp),
         itemVerticalAlignment=Alignment.CenterVertically
    ){
        Text("Selection",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
        TextButton(onClick={controller.assignSelectionToGroup(controller.weightPaintValue);redraw()},enabled=groups.isNotEmpty()){Text("Assign",fontSize=11.sp)}
        TextButton(onClick={controller.removeSelectionFromGroup();redraw()},enabled=groups.isNotEmpty()){Text("Remove from group",fontSize=11.sp)}
        TextButton(onClick={controller.selectGroupPoints();redraw()},enabled=groups.isNotEmpty()){Text("Select",fontSize=11.sp)}
        TextButton(onClick={controller.deselectGroupPoints();redraw()},enabled=groups.isNotEmpty()){Text("Deselect",fontSize=11.sp)}
        TextButton(onClick={controller.invertGroupWeights();redraw()},enabled=groups.isNotEmpty()){Text("Invert",fontSize=11.sp)}
        TextButton(onClick={controller.normalizeGroupWeights();redraw()},enabled=groups.isNotEmpty()){Text("Normalize",fontSize=11.sp)}
    }
    Row(Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=2.dp),verticalAlignment=Alignment.CenterVertically){
        Text("Weight "+"%.2f".format(controller.weightPaintValue),fontSize=10.sp,modifier=Modifier.width(76.dp))
        Slider(
            value=controller.weightPaintValue,
            onValueChange={controller.setWeightPaintValue(it);redraw()},
            valueRange=0f..1f,
            modifier=Modifier.weight(1f).padding(horizontal=4.dp)
        )
    }
    if(renameOpen){
        AlertDialog(
            onDismissRequest={renameOpen=false},
            title={Text("Rename vertex group")},
            text={OutlinedTextField(value=renameText,onValueChange={renameText=it},singleLine=true)},
            confirmButton={TextButton(onClick={if(renameText.isNotBlank())controller.renameVertexGroup(controller.weightPaintGroup,renameText.trim());renameOpen=false;redraw()}){Text("Rename")}},
            dismissButton={TextButton(onClick={renameOpen=false}){Text("Cancel")}}
        )
    }
}

private val annotationColors=listOf(0xFF0099FF.toInt(),0xFFFF3B30.toInt(),0xFF34C759.toInt(),0xFFFFCC00.toInt(),0xFF000000.toInt(),0xFFFFFFFF.toInt())

/** Annotate tool: draw / erase notes (notes only), color, thickness (screen px), show, clear. */
@NonSkippableComposable
@Composable private fun AnnotationBar(controller:EditorController,redraw:()->Unit){
    val style=controller.annotationStyle()
    var thickness by remember{mutableFloatStateOf(style[4])}
    FlowRow(
        Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=2.dp),
         itemVerticalAlignment=Alignment.CenterVertically
    ){
        Text("Annotate",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
        FilterChip(selected=!controller.annotationEraser,onClick={controller.setAnnotationEraser(false);redraw()},label={Text("Draw",fontSize=10.sp)},modifier=Modifier.padding(end=3.dp))
        FilterChip(selected=controller.annotationEraser,onClick={controller.setAnnotationEraser(true);redraw()},label={Text("Erase notes",fontSize=10.sp)},modifier=Modifier.padding(end=6.dp))
        val current=controller.annotationColorArgb
        annotationColors.forEach{argb->
            Box(Modifier.padding(end=4.dp).size(22.dp).background(Color(argb),CircleShape)
                .border(if(argb==current)3.dp else 1.dp,if(argb==current)Accent else Color.Gray,CircleShape)
                .clickable{controller.setAnnotationColor(argb);redraw()})
        }
        Text("Thickness "+thickness.toInt()+" px",fontSize=10.sp,modifier=Modifier.padding(start=6.dp).width(84.dp))
        Slider(thickness,{thickness=it;controller.setAnnotationThickness(it);redraw()},valueRange=1f..20f,modifier=Modifier.width(140.dp))
        FilterChip(selected=controller.annotationsVisible,onClick={controller.setAnnotationsVisible(!controller.annotationsVisible);redraw()},label={Text("Show",fontSize=10.sp)},modifier=Modifier.padding(horizontal=4.dp))
        TextButton(onClick={if(controller.clearAnnotations())redraw()}){Text("Clear annotations",fontSize=10.sp)}
    }
}

/** Fill tool options: Blender's fill Leak Size, Dilate (negative contracts) and boundary mode. */
@NonSkippableComposable
@Composable private fun FillBar(controller:EditorController,redraw:()->Unit){
    FlowRow(
        Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=2.dp),
         itemVerticalAlignment=Alignment.CenterVertically
    ){
        Text("Fill",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
        listOf(
            com.smitnk.projectgrease.editor.FILL_BOUNDARY_ALL to "All",
            com.smitnk.projectgrease.editor.FILL_BOUNDARY_STROKES to "Strokes",
            com.smitnk.projectgrease.editor.FILL_BOUNDARY_EDIT_LINES to "Edit Lines"
        ).forEach{(value,label)->
            FilterChip(selected=controller.fillBoundary==value,onClick={controller.setFillOptions(boundary=value);redraw()},
                label={Text(label,fontSize=10.sp)},modifier=Modifier.padding(end=3.dp))
        }
        // Leak 0 = Blender's own leak size, ceil(3 x Precision).
        Text(if(controller.fillLeak==0)"Leak auto" else "Leak "+controller.fillLeak+" px",fontSize=10.sp,modifier=Modifier.padding(start=6.dp).width(64.dp))
        Slider(controller.fillLeak.toFloat(),{controller.setFillOptions(leak=it.toInt());redraw()},valueRange=0f..20f,modifier=Modifier.width(120.dp))
        // Precision (fill_factor): resolution of the fill image.
        Text("Precision "+"%.2f".format(controller.fillPrecision),fontSize=10.sp,modifier=Modifier.padding(start=6.dp).width(84.dp))
        Slider(controller.fillPrecision,{controller.setFillPrecision(value=it);redraw()},valueRange=0.05f..8f,modifier=Modifier.width(120.dp))
        Text("Dilate "+controller.fillDilate+" px",fontSize=10.sp,modifier=Modifier.padding(start=6.dp).width(70.dp))
        Slider(controller.fillDilate.toFloat(),{controller.setFillOptions(dilate=Math.round(it));redraw()},valueRange=-10f..10f,modifier=Modifier.width(120.dp))
        // Extend Lines (fill_extend_fac): open stroke ends are prolonged in the fill boundary.
        Text("Extend "+"%.2f".format(controller.fillExtend),fontSize=10.sp,modifier=Modifier.padding(start=6.dp).width(70.dp))
        Slider(controller.fillExtend,{controller.setFillExtend(it);redraw()},valueRange=0f..10f,modifier=Modifier.width(120.dp))
        FilterChip(selected=controller.fillCollide,onClick={controller.setFillPrecision(collide=!controller.fillCollide);redraw()},
            label={Text("Collide",fontSize=10.sp)},modifier=Modifier.padding(start=3.dp))
    }
}

@NonSkippableComposable
@Composable private fun ModeBrushBar(controller:EditorController,redraw:()->Unit){
    Surface(tonalElevation=2.dp){
        Column(Modifier.fillMaxWidth()){
            FlowRow(
                Modifier.fillMaxWidth()
                    
                    .padding(horizontal=8.dp,vertical=5.dp),
                 itemVerticalAlignment=Alignment.CenterVertically
            ){
                Text("Mode",fontWeight=FontWeight.Bold,fontSize=11.sp,modifier=Modifier.padding(end=5.dp))
                GreaseMode.entries.forEach{mode->
                    val selected=controller.mode==mode
                    val supported=selected || when(mode){
                        GreaseMode.DRAW,GreaseMode.EDIT->true
                        GreaseMode.SCULPT->FeatureRegistry.capability(FeatureId.SCULPT).state!=FeatureState.NOT_IMPLEMENTED
                        GreaseMode.VERTEX_PAINT->FeatureRegistry.capability(FeatureId.VERTEX_PAINT).state!=FeatureState.NOT_IMPLEMENTED
                        GreaseMode.WEIGHT_PAINT->FeatureRegistry.capability(FeatureId.WEIGHT_PAINT).state!=FeatureState.NOT_IMPLEMENTED
                    }
                    FilterChip(
                        selected=selected,
                        enabled=supported,
                        onClick={if(controller.setMode(mode))redraw()},
                        label={Text(mode.name.replace('_',' '),fontSize=10.sp)},
                        modifier=Modifier.padding(end=3.dp)
                    )
                }
                if(controller.mode!=GreaseMode.SCULPT){
                Spacer(Modifier.width(7.dp))
                Text("Brush",fontWeight=FontWeight.Bold,fontSize=11.sp,modifier=Modifier.padding(end=5.dp))
                BrushPreset.entries.forEach{preset->
                    FilterChip(
                        selected=controller.brushes.preset==preset,
                        onClick={controller.selectBrush(preset);redraw()},
                        label={Text(preset.label,fontSize=10.sp)},
                        modifier=Modifier.padding(end=3.dp).testTag("brush_"+preset.name)
                    )
                }
                }
            }
            if (controller.mode == GreaseMode.SCULPT) SculptOptions(controller,redraw)
            if (controller.mode == GreaseMode.VERTEX_PAINT) {
                FlowRow(
                    Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=2.dp),
                     itemVerticalAlignment=Alignment.CenterVertically
                ) {
                    Text("Paint",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
                    listOf(
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_DRAW to "Draw",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_BLUR to "Blur",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_AVERAGE to "Average",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_SMEAR to "Smear",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_REPLACE to "Replace",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_TINT to "Tint"
                    ).forEach { (brush,label) ->
                        FilterChip(
                            selected=controller.vertexPaintBrush == brush,
                            onClick={controller.setVertexPaintBrush(brush);redraw()},
                            label={Text(label,fontSize=10.sp)},
                            modifier=Modifier.padding(end=3.dp)
                        )
                    }
                    Spacer(Modifier.width(7.dp))
                    Text("Target",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
                    listOf(
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.PAINT_STROKE to "Stroke",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.PAINT_FILL to "Fill",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.PAINT_BOTH to "Both"
                    ).forEach { (target,label) ->
                        FilterChip(
                            selected=controller.vertexPaintTarget == target,
                            onClick={controller.setVertexPaintTarget(target);redraw()},
                            label={Text(label,fontSize=10.sp)},
                            modifier=Modifier.padding(end=3.dp)
                        )
                    }
                }
            }
            if (controller.mode == GreaseMode.WEIGHT_PAINT) {
                WeightPaintBar(controller,redraw)
            }
            if (controller.mode == GreaseMode.VERTEX_PAINT || controller.mode == GreaseMode.WEIGHT_PAINT) PaintOptionsBar(controller,redraw)
            if (controller.mode == GreaseMode.DRAW || controller.mode == GreaseMode.VERTEX_PAINT) VertexPaletteBar(controller,redraw)
            if (controller.tools.activeTool == GreaseTool.ANNOTATE) AnnotationBar(controller,redraw)
            if (controller.mode == GreaseMode.EDIT) {
                FlowRow(
                    Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=2.dp),
                     itemVerticalAlignment=Alignment.CenterVertically
                ) {
                    Text("Select",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
                    listOf(
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.MODE_POINT to "Point",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.MODE_STROKE to "Stroke",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.MODE_SEGMENT to "Segment"
                    ).forEach { (value,label) ->
                        FilterChip(
                            selected=controller.selectMode == value,
                            onClick={controller.setSelectMode(value);redraw()},
                            label={Text(label,fontSize=10.sp)},
                            modifier=Modifier.padding(end=3.dp)
                        )
                    }
                }
            }
            if (controller.mode == GreaseMode.EDIT || controller.tools.activeTool in com.smitnk.projectgrease.editor.SELECTION_TOOLS) SelectOperatorsBar(controller,redraw)
            if (controller.tools.activeTool == GreaseTool.FILL) FillBar(controller,redraw)
            if (controller.tools.activeTool == GreaseTool.ERASE) {
                FlowRow(
                    Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=2.dp),
                     itemVerticalAlignment=Alignment.CenterVertically
                ) {
                    Text("Eraser",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
                    EraserMode.entries.forEach { mode ->
                        FilterChip(
                            selected=controller.eraserMode == mode,
                            onClick={controller.setEraserMode(mode);redraw()},
                            label={Text(mode.name,fontSize=10.sp)},
                            modifier=Modifier.padding(end=3.dp)
                        )
                    }
                }
            }
            Row(
                Modifier.fillMaxWidth().padding(horizontal=8.dp,vertical=3.dp),
                verticalAlignment=Alignment.CenterVertically
            ){
                // Each tool's own brush, as in Blender: eraser, sculpt / vertex / weight brush, or the pen.
                val erasing=controller.tools.activeTool==GreaseTool.ERASE
                val modeBrush=if(erasing)null else controller.activeModeBrush()
                val size=when{erasing->controller.eraserSize;modeBrush!=null->modeBrush.size;else->controller.brushes.size}
                Text((if(erasing)"Eraser " else "Size ")+size.toInt(),fontSize=10.sp,modifier=Modifier.width(58.dp).testTag("brushSizeLabel"))
                Slider(
                    value=size,
                    onValueChange={when{erasing->controller.setEraserSize(it);modeBrush!=null->modeBrush.setSize(it);else->controller.brushes.setSize(it)};redraw()},
                    valueRange=.5f..500f,
                    modifier=Modifier.weight(1f).padding(horizontal=4.dp)
                )
                val strength=modeBrush?.strength?:controller.brushes.strength
                Text("Strength "+(strength*100).toInt()+"%",fontSize=10.sp,modifier=Modifier.width(76.dp))
                Slider(
                    value=strength,
                    onValueChange={
                        if(modeBrush!=null)modeBrush.setStrength(it) else controller.setBrushStrength(it)
                        redraw()
                    },
                    valueRange=0f..1f,
                    modifier=Modifier.weight(1f).padding(horizontal=4.dp)
                )
            }
        }
    }
}

@NonSkippableComposable
@Composable private fun Properties(controller:EditorController,redraw:()->Unit){
    var opacity by remember{mutableFloatStateOf(controller.materials.opacity)}
    Column(Modifier.width(210.dp).fillMaxHeight().verticalScroll(rememberScrollState()).padding(10.dp)){
        Text("Brush",fontWeight=FontWeight.Bold);Text("Thickness "+controller.brushes.size.toInt());Slider(controller.brushes.size, {controller.brushes.setSize(it);redraw()}, valueRange = .5f..500f)
        Text("Opacity "+(opacity*100).toInt().toString()+"%");Slider(opacity, {opacity=it;controller.materials.setOpacity(it);controller.setMaterialColor(controller.materials.colorArgb);redraw()}, valueRange = 0f..1f)
        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
            Text("Stabilizer",Modifier.weight(1f))
            Switch(checked=controller.stabilizerEnabled,onCheckedChange={controller.setStabilizer(it);redraw()})
        }
        Text("Spacing "+controller.spacing.toInt())
        Slider(controller.spacing,{controller.setSpacing(it);redraw()},valueRange=0f..50f)
        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
            Text("Grid",Modifier.weight(1f))
            Switch(checked=controller.view.showGrid,onCheckedChange={controller.view.toggleGrid();redraw()})
        }
        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
            Text("Snapping",Modifier.weight(1f))
            Switch(checked=controller.view.snapEnabled,onCheckedChange={controller.view.toggleSnapping();redraw()})
        }
        Button(onClick={if(controller.smoothSelectedStroke()){redraw()}}){Text("Smooth selected stroke")}
    }
}

@OptIn(ExperimentalLayoutApi::class)
@NonSkippableComposable
@Composable private fun Timeline(controller:EditorController,redraw:()->Unit,onFps:()->Unit){
    var markersOpen by remember{mutableStateOf(false)}
    var previewOpen by remember{mutableStateOf(false)}
    Surface(tonalElevation=4.dp){
        Column(Modifier.fillMaxWidth()){
            // Wrapping rows (no side-scroll strip): every timeline action stays reachable on narrow screens.
            FlowRow(
                Modifier.fillMaxWidth().padding(horizontal=4.dp),
                verticalArrangement=Arrangement.Center
            ){
                IconButton(onClick={controller.selectFrame(controller.animation.currentFrame-1);redraw()}){Icon(Icons.Default.SkipPrevious,"Previous")}
                IconButton(onClick={controller.animation.togglePlayback();redraw()}){Icon(if(controller.animation.playing)Icons.Default.Pause else Icons.Default.PlayArrow,"Play")}
                IconButton(onClick={controller.selectFrame(controller.animation.currentFrame+1);redraw()}){Icon(Icons.Default.SkipNext,"Next")}
                Text("Frame "+controller.animation.currentFrame+" / "+controller.animation.timelineEnd,
                    Modifier.align(Alignment.CenterVertically).padding(horizontal=4.dp))
                TextButton(onClick=onFps){Text(controller.animation.fps.toString()+" FPS")}
                TextButton(
                    enabled=controller.animation.currentFrame > 1 && controller.animation.currentFrame < controller.animation.timelineEnd,
                    onClick={if(controller.interpolateFrameAt(controller.animation.currentFrame)){redraw()}}
                ){Text("Interpolate")}
                FilterChip(selected=controller.animation.loop,onClick={controller.animation.toggleLoop();redraw()},label={Text("Loop")})
                TextButton(onClick={controller.createFrame(controller.animation.currentFrame+1);redraw()}){Text("+ Frame")}
                TextButton(onClick={controller.duplicateFrame(controller.animation.currentFrame,controller.animation.currentFrame+1);redraw()}){Text("Duplicate")}
                TextButton(onClick={if(controller.animation.frameCount>1){controller.deleteFrame(controller.animation.currentFrame);redraw()}}){Text("Delete")}
                // GPENCIL_OT_blank_frame_add / GPENCIL_OT_frame_clean_duplicate; the strip below re-reads
                // the native frame numbers, and the controller refreshes frame count/end.
                TextButton(onClick={if(controller.insertBlankFrame())redraw()}){Text("Insert blank keyframe")}
                TextButton(onClick={if(controller.cleanDuplicateFrames())redraw()}){Text("Clean duplicate frames")}
                TextButton(onClick={if(controller.interpolateSequence()>0){redraw()}},
                    modifier=Modifier.testTag("interpolateSequence")){Text("Interpolate sequence")}
                FilterChip(selected=controller.multiframeEditing,onClick={controller.setMultiframeEditing(!controller.multiframeEditing);redraw()},
                    label={Text("Multiframe")},modifier=Modifier.testTag("multiframe"))
                // Scene markers (MARKER_OT_*), preview range (ANIM_OT_previewrange_*), scrub snapping.
                TextButton(onClick={if(controller.addMarker())redraw()},modifier=Modifier.testTag("markerAdd")){Text("Add marker")}
                TextButton(onClick={markersOpen=true},modifier=Modifier.testTag("markers")){Text("Markers ("+controller.animation.timeline.markers.size+")")}
                val preview=controller.animation.timeline.preview
                TextButton(onClick={previewOpen=true},modifier=Modifier.testTag("previewRange")){
                    Text(if(preview.enabled)"Preview "+preview.start+"–"+preview.end else "Preview range")
                }
                FilterChip(selected=controller.animation.scrubSnapToKeys,onClick={controller.animation.scrubSnapToKeys=!controller.animation.scrubSnapToKeys},
                    label={Text("Snap to keys")},modifier=Modifier.testTag("scrubSnapKeys"))
            }
            if(markersOpen)MarkersDialog(controller,{markersOpen=false},redraw)
            if(previewOpen)PreviewRangeDialog(controller,{previewOpen=false},redraw)
            val listState=androidx.compose.foundation.lazy.rememberLazyListState()
            Box(Modifier.padding(horizontal=5.dp)){TimelineScrubStrip(controller,listState,52.dp,redraw)}
            // Lazy: only the visible frame cells are composed. The scene end can be up to 100000
            // frames; composing a cell for each one on every recomposition froze the main thread
            // (monkey ANR in Timeline).
            val keyframes=remember(controller.animation.keyframes){controller.animation.keyframes.toSet()}
            var menuFrame by remember{mutableIntStateOf(-1)}
            val frameTotal=controller.animation.timelineEnd.coerceAtLeast(1)
            val markerByFrame=controller.animation.timeline.markers.associateBy{it.frame}
            val previewRange=controller.animation.timeline.preview
            androidx.compose.foundation.lazy.LazyRow(Modifier.fillMaxWidth().padding(5.dp),state=listState){
                items(frameTotal,key={it+1}){index->
                    val frame=index+1
                    val key=frame in keyframes
                    val type=controller.animation.keyTypes[frame]?:0
                    val selectedKey=frame in controller.animation.selectedFrames
                    val marker=markerByFrame[frame]
                    val inPreview=previewRange.enabled&&frame in previewRange.start..previewRange.end
                    Box(if(inPreview)Modifier.background(PreviewRangeColor) else Modifier){
                        // tap: go to the frame; long press: key type / frame selection menu
                        Surface(
                            Modifier.width(52.dp).height(54.dp).padding(2.dp).testTag("frame_$frame")
                                .border(if(selectedKey)2.dp else 0.dp,if(selectedKey)Color(0xFFFF8500) else Color.Transparent,RoundedCornerShape(8.dp))
                                .pointerInput(frame){detectTapGestures(onTap={controller.selectFrame(frame);redraw()},onLongPress={menuFrame=frame})},
                            shape=RoundedCornerShape(8.dp),
                            tonalElevation=if(frame==controller.animation.currentFrame)5.dp else 0.dp
                        ){Column(horizontalAlignment=Alignment.CenterHorizontally,verticalArrangement=Arrangement.Center){
                            Text(frame.toString())
                            if(key) Box(Modifier.background(keyTypeColor(type),RoundedCornerShape(3.dp)).padding(horizontal=3.dp).testTag("keyMark_${frame}_$type")){
                                Text(keyTypeMark(type),fontSize=8.sp,color=Color.Black)
                            } else Text("HOLD",fontSize=8.sp)
                            if(marker!=null)Text(marker.name,fontSize=7.sp,maxLines=1,
                                color=if(marker.selected)MarkerSelectedColor else Color.Unspecified,
                                modifier=Modifier.testTag("markerLabel_$frame"))
                        }}
                        if(menuFrame==frame)KeyframeMenu(controller,frame,{menuFrame=-1},redraw)
                    }
                }
                item(key="add"){
                    Surface(Modifier.width(64.dp).height(54.dp).padding(2.dp).clickable{
                        controller.createFrame((controller.animation.timelineEnd+1).coerceAtLeast(1));redraw()
                    },shape=RoundedCornerShape(8.dp)){
                        Box(contentAlignment=Alignment.Center){Text("+")}
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@NonSkippableComposable
@Composable private fun FpsDialog(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    var fps by remember{mutableIntStateOf(controller.animation.fps)}
    AlertDialog(
        onDismissRequest=onDismiss,
        title={Text("Frame rate")},
        text={
            Column{
                Text("$fps FPS",style=MaterialTheme.typography.titleLarge)
                Slider(
                    value=fps.toFloat(),
                    onValueChange={fps=it.toInt()},
                    valueRange=1f..120f,
                    steps=119
                )
                Row(horizontalArrangement=Arrangement.spacedBy(8.dp)){
                    listOf(12,24,30,60).forEach{v->
                        FilterChip(selected=fps==v,onClick={fps=v},label={Text("$v")})
                    }
                }
            }
        },
        confirmButton={TextButton(onClick={controller.animation.setFps(fps);redraw();onDismiss()}){Text("Apply")}},
        dismissButton={TextButton(onClick=onDismiss){Text("Cancel")}}
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun ProjectSheet(onDismiss:()->Unit,onSettings:()->Unit,onExit:()->Unit,controller:EditorController,context:android.content.Context,onSave:()->Unit){
    var exportOpen by remember{mutableStateOf(false)}
    if(exportOpen)ExportDialog(controller,context,{exportOpen=false;onDismiss()})
    var projectSettingsOpen by remember{mutableStateOf(false)}
    if(projectSettingsOpen)ProjectSettingsDialog(controller,{projectSettingsOpen=false},{onSave()})
    val importSvg=rememberSvgImport(controller,context,onDismiss)
    var traceOpen by remember{mutableStateOf(false)}
    if(traceOpen)TraceImageDialog(controller,context,{traceOpen=false;onDismiss()})
    val saveAs=rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")){uri->
        if(uri==null)return@rememberLauncherForActivityResult
        val json=controller.saveDocumentJson()
        // Serialize fully before opening the destination, then truncate ("wt") and write in one go,
        // so a serialization failure never leaves a truncated file behind.
        val bytes=json?.let{runCatching{it.toByteArray(Charsets.UTF_8)}.getOrNull()}
        val ok=bytes!=null&&runCatching{context.contentResolver.openOutputStream(uri,"wt")?.use{it.write(bytes);it.flush()}!=null}.getOrDefault(false)
        if(ok){
            // The written file becomes the current document: continue under its name.
            val display=runCatching{
                context.contentResolver.query(uri,arrayOf(android.provider.OpenableColumns.DISPLAY_NAME),null,null,null)?.use{c->if(c.moveToFirst())c.getString(0) else null}
            }.getOrNull()
            com.smitnk.projectgrease.editor.SaveAsNaming.projectName(display)?.let{controller.document.projectName=it}
            onSave()
        }
        Toast.makeText(context,if(ok)"Saved as "+controller.document.projectName else "Save as failed",Toast.LENGTH_SHORT).show()
        if(ok)onDismiss()
    }
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Project",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        ListItem(headlineContent={Text("Open project")},modifier=Modifier.clickable{onDismiss()})
        ListItem(headlineContent={Text("Save")},modifier=Modifier.clickable{onSave();Toast.makeText(context,"Project saved",Toast.LENGTH_SHORT).show();onDismiss()})
        ListItem(headlineContent={Text("Save as")},supportingContent={Text("Write the project to a new file and continue under its name")},modifier=Modifier.clickable{saveAs.launch(controller.document.projectName.ifBlank{"Project Grease"}+".gpjson")})
        ListItem(headlineContent={Text("Export")},supportingContent={Text("SVG, PDF, PNG, GIF, PNG sequence")},modifier=Modifier.clickable{exportOpen=true})
        ListItem(headlineContent={Text("Project settings")},supportingContent={Text("Canvas size, FPS, frame range, background")},modifier=Modifier.clickable{projectSettingsOpen=true}.testTag("projectSettings"))
        ListItem(headlineContent={Text("Import SVG")},supportingContent={Text("Shapes become strokes on the active layer")},modifier=Modifier.clickable{importSvg()})
        ListItem(headlineContent={Text("Trace image")},supportingContent={Text("Outlines of an image become filled strokes on a new layer")},modifier=Modifier.clickable{traceOpen=true})
        ListItem(headlineContent={Text("Settings")},modifier=Modifier.clickable{onDismiss();onSettings()});ListItem(headlineContent={Text("Close editor")},modifier=Modifier.clickable{onDismiss();onExit()});Spacer(Modifier.height(20.dp))}
}

@OptIn(ExperimentalMaterial3Api::class)
@NonSkippableComposable
@Composable private fun ToolsSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){
        Text("Tools",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        tools.filter { FeatureRegistry.capability(it.feature).state != FeatureState.NOT_IMPLEMENTED }.chunked(3).forEach { row ->
            Row(Modifier.fillMaxWidth().padding(horizontal=12.dp,vertical=4.dp),horizontalArrangement=Arrangement.spacedBy(8.dp)){
                row.forEach { entry ->
                    val selected=controller.tools.activeTool==entry.tool
                    Surface(
                        modifier=Modifier.weight(1f).height(64.dp).clickable{
                            if(controller.selectTool(entry.tool)){redraw();onDismiss()}
                        },
                        shape=RoundedCornerShape(12.dp),
                        tonalElevation=if(selected)4.dp else 1.dp,
                        color=if(selected)MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant
                    ){
                        Column(horizontalAlignment=Alignment.CenterHorizontally,verticalArrangement=Arrangement.Center){
                            Icon(entry.icon,entry.label)
                            Text(entry.label,fontSize=11.sp)
                        }
                    }
                }
                repeat(3-row.size){Spacer(Modifier.weight(1f))}
            }
        }
        Spacer(Modifier.height(8.dp))
    }
}

@OptIn(ExperimentalMaterial3Api::class)
/** Mask list of the selected layer: it is drawn only where the union of its mask layers has coverage. */
@NonSkippableComposable
@Composable private fun LayerMaskSection(controller:EditorController,redraw:()->Unit){
    val layer=controller.selectedLayer
    var tick by remember{mutableStateOf(0)}
    var addMenu by remember{mutableStateOf(false)}
    val masks=remember(tick,layer,controller.layerCount()){controller.layerMasks(layer)}
    val useMask=remember(tick,layer){controller.layerUsesMask(layer)}
    fun changed(ok:Boolean){if(ok){tick++;redraw()}}
    Text("Masks",Modifier.padding(horizontal=12.dp,vertical=8.dp),fontWeight=FontWeight.Bold)
    Row(Modifier.fillMaxWidth().padding(horizontal=12.dp),verticalAlignment=Alignment.CenterVertically){
        Text("Use mask",Modifier.weight(1f))
        Switch(checked=useMask,onCheckedChange={changed(controller.setLayerUsesMask(it,layer))})
    }
    masks.forEachIndexed{index,mask->
        Column(Modifier.fillMaxWidth().padding(horizontal=12.dp,vertical=3.dp)){
            Text(mask.name,fontWeight=FontWeight.Bold)
            FlowRow(Modifier.fillMaxWidth(), itemVerticalAlignment=Alignment.CenterVertically){
                Text("Invert",Modifier.padding(end=6.dp));Switch(checked=mask.inverted,onCheckedChange={changed(controller.setLayerMaskFlags(index,mask.hidden,it,layer))})
                Spacer(Modifier.width(10.dp))
                Text("Hide",Modifier.padding(end=6.dp));Switch(checked=mask.hidden,onCheckedChange={changed(controller.setLayerMaskFlags(index,it,mask.inverted,layer))})
                TextButton(onClick={changed(controller.removeLayerMask(index,layer))}){Text("Remove")}
            }
        }
    }
    val candidates=(0 until controller.layerCount()).filter{it!=layer && masks.none{m->m.name==controller.layerName(it)}}
    Box(Modifier.padding(horizontal=12.dp)){
        Button(onClick={addMenu=true},enabled=candidates.isNotEmpty(),modifier=Modifier.fillMaxWidth()){Text("Add mask layer")}
        DropdownMenu(expanded=addMenu,onDismissRequest={addMenu=false}){
            candidates.forEach{index->
                DropdownMenuItem(text={Text(controller.layerName(index))},onClick={addMenu=false;changed(controller.addLayerMask(index,layer))})
            }
        }
    }
}

/**
 * Shader effects of the selected layer: a 2D post-pass over the rendered layer, run top to bottom.
 * Sizes are canvas pixels; the strokes are never changed.
 */
@NonSkippableComposable
@Composable private fun LayerEffectsSection(controller:EditorController,redraw:()->Unit,layer:Int=0){
    // Blender: effects belong to the whole drawing (the object); they are kept on the bottom layer's
    // slot and every layer's effects run together as one stack on the whole drawing.
    var tick by remember{mutableStateOf(0)}
    var addMenu by remember{mutableStateOf(false)}
    var expanded by remember(layer){mutableStateOf(setOf<Int>())}
    val effects=remember(tick,layer){controller.effects(layer)}
    fun changed(ok:Boolean){if(ok){tick++;redraw()}}
    Text("Effects",Modifier.padding(horizontal=12.dp,vertical=8.dp),fontWeight=FontWeight.Bold)
    if(effects.isEmpty())Text("No effects. Effects are drawn live over the whole drawing (Blender's Rendered view); the strokes stay untouched. Sizes are canvas pixels.",Modifier.padding(horizontal=12.dp))
    effects.forEachIndexed{index,effect->
        val open=index in expanded
        Column(Modifier.fillMaxWidth().padding(horizontal=12.dp,vertical=4.dp).border(1.dp,MaterialTheme.colorScheme.outline,RoundedCornerShape(8.dp)).padding(8.dp)){
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
                Text(com.smitnk.projectgrease.editor.FxType.name(effect.type),Modifier.weight(1f).clickable{expanded=if(open)expanded-index else expanded+index},fontWeight=FontWeight.Bold)
                Switch(checked=effect.enabled,onCheckedChange={changed(controller.setEffectEnabled(index,it,layer=layer))})
            }
            FlowRow(Modifier.fillMaxWidth(), itemVerticalAlignment=Alignment.CenterVertically){
                TextButton(onClick={expanded=if(open)expanded-index else expanded+index}){Text(if(open)"Hide" else "Edit")}
                TextButton(onClick={changed(controller.moveEffect(index,-1,layer=layer))},enabled=index>0){Text("Up")}
                TextButton(onClick={changed(controller.moveEffect(index,1,layer=layer))},enabled=index<effects.size-1){Text("Down")}
                TextButton(onClick={expanded=emptySet();changed(controller.removeEffect(index,layer=layer))}){Text("Remove")}
            }
            // Target: whole layer (Blender), strokes only or fills only.
            FlowRow(Modifier.fillMaxWidth(), itemVerticalAlignment=Alignment.CenterVertically){
                Text("Applies to",fontSize=11.sp,modifier=Modifier.padding(end=6.dp))
                listOf(com.smitnk.projectgrease.editor.FxTarget.LAYER,com.smitnk.projectgrease.editor.FxTarget.STROKES,com.smitnk.projectgrease.editor.FxTarget.FILLS).forEach{t->
                    FilterChip(selected=effect.target==t,onClick={changed(controller.setEffectTarget(index,t,layer=layer))},
                        label={Text(com.smitnk.projectgrease.editor.FxTarget.label(t),fontSize=10.sp)},modifier=Modifier.padding(end=3.dp))
                }
            }
            if(open){
                com.smitnk.projectgrease.editor.FxSpecs.specs(effect.type).forEach{spec->
                    val value=effect.params.getOrElse(spec.index){0f}
                    when(spec.kind){
                        com.smitnk.projectgrease.editor.ParamKind.BOOL->Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
                            Text(spec.label,Modifier.weight(1f));Switch(checked=value!=0f,onCheckedChange={changed(controller.setEffectParam(index,spec.index,if(it)1f else 0f,layer=layer))})
                        }
                        com.smitnk.projectgrease.editor.ParamKind.ENUM->OutlinedButton(
                            onClick={changed(controller.setEffectParam(index,spec.index,((value.toInt()+1)%spec.options.size).toFloat(),layer=layer))},
                            modifier=Modifier.fillMaxWidth()
                        ){Text(spec.label+": "+spec.options.getOrElse(value.toInt()){"?"})}
                        else->{
                            val shown=value*spec.displayFactor
                            Text(spec.label+" "+(if(spec.kind==com.smitnk.projectgrease.editor.ParamKind.INT)shown.toInt().toString() else "%.2f".format(shown)))
                            Slider(
                                value=value.coerceIn(spec.min,spec.max),
                                onValueChange={changed(controller.setEffectParam(index,spec.index,if(spec.kind==com.smitnk.projectgrease.editor.ParamKind.INT)Math.round(it).toFloat() else it,commit=false,layer=layer))},
                                onValueChangeFinished={controller.commitEffectEdit()},
                                valueRange=spec.min..spec.max
                            )
                        }
                    }
                }
                val note=com.smitnk.projectgrease.editor.FxSpecs.note(effect.type)
                if(note.isNotEmpty())Text(note,style=MaterialTheme.typography.bodySmall)
            }
        }
    }
    Box(Modifier.padding(horizontal=12.dp)){
        Button(onClick={addMenu=true},enabled=effects.size<com.smitnk.projectgrease.editor.FxType.MAX_STACK,modifier=Modifier.fillMaxWidth()){Text("Add effect")}
        DropdownMenu(expanded=addMenu,onDismissRequest={addMenu=false}){
            com.smitnk.projectgrease.editor.FxType.all.forEach{type->
                DropdownMenuItem(text={Text(com.smitnk.projectgrease.editor.FxType.name(type))},onClick={addMenu=false;changed(controller.addEffect(type,layer=layer))})
            }
        }
    }
}

@NonSkippableComposable
@Composable private fun LayersSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    // Switch and name state come from the native layer, not from fixed defaults, and are re-read
    // whenever the selected layer or the layer list changes.
    // opTick: merge / isolate / lock all change native layer flags, so the switches re-read them.
    var opTick by remember{mutableIntStateOf(0)}
    val layerKey=Triple(controller.selectedLayer,controller.layerCount(),opTick)
    val layerState=controller.layerState()
    var visible by remember(layerKey){mutableStateOf(layerState?.visible ?: true)}
    var locked by remember(layerKey){mutableStateOf(layerState?.locked ?: false)}
    var onionOn by remember(layerKey){mutableStateOf(controller.layerOnion())}
    var renameOpen by remember{mutableStateOf(false)}
    var renameText by remember(layerKey){mutableStateOf(layerState?.name?.takeIf{it.isNotBlank()} ?: ("Layer "+(controller.selectedLayer+1)))}
    ModalBottomSheet(onDismissRequest=onDismiss){
      Column(Modifier.verticalScroll(rememberScrollState())){
        Text("Layers",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        Text(layerState?.name?.takeIf{it.isNotBlank()} ?: ("Layer "+(controller.selectedLayer+1)),Modifier.padding(horizontal=20.dp))
        Row(Modifier.fillMaxWidth().padding(12.dp),horizontalArrangement=Arrangement.spacedBy(8.dp)){
            Button(onClick={controller.createLayer("Layer "+(controller.layerCount()+1));redraw()}){Text("Add")}
            Button(onClick={controller.duplicateLayer();redraw()}){Text("Duplicate")}
            Button(onClick={controller.deleteLayer();redraw()},enabled=controller.layerCount()>1){Text("Delete")}
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=12.dp),horizontalArrangement=Arrangement.spacedBy(8.dp)){
            Button(onClick={if(controller.selectedLayer>0){controller.moveLayer(controller.selectedLayer,controller.selectedLayer-1);redraw()}},enabled=controller.selectedLayer>0){Text("Up")}
            Button(onClick={if(controller.selectedLayer<controller.layerCount()-1){controller.moveLayer(controller.selectedLayer,controller.selectedLayer+1);redraw()}},enabled=controller.selectedLayer<controller.layerCount()-1){Text("Down")}
            Button(onClick={renameOpen=true}){Text("Rename")}
        }
        // Layer operators (gpencil_layer_merge / isolate / lock all): native state is re-read afterwards.
        FlowRow(Modifier.fillMaxWidth().padding(horizontal=12.dp,vertical=4.dp),horizontalArrangement=Arrangement.spacedBy(8.dp)){
            OutlinedButton(onClick={if(controller.mergeLayerDown()){opTick++;redraw()}},enabled=controller.selectedLayer>0,
                modifier=Modifier.testTag("layerMergeDown")){Text("Merge down")}
            OutlinedButton(onClick={if(controller.isolateLayer()){opTick++;redraw()}},modifier=Modifier.testTag("layerIsolate")){Text("Isolate")}
            OutlinedButton(onClick={if(controller.lockAllLayers()){opTick++;redraw()}}){Text("Lock all")}
            OutlinedButton(onClick={if(controller.unlockAllLayers()){opTick++;redraw()}}){Text("Unlock all")}
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=12.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Visible",Modifier.weight(1f))
            Switch(checked=visible,onCheckedChange={visible=it;controller.setLayerVisibility(controller.selectedLayer,it);redraw()})
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=12.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Locked",Modifier.weight(1f))
            Switch(checked=locked,onCheckedChange={locked=it;controller.setLayerLocked(controller.selectedLayer,it);redraw()})
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=12.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Use onion skinning",Modifier.weight(1f))
            Switch(checked=onionOn,onCheckedChange={onionOn=it;controller.setLayerOnion(controller.selectedLayer,it);redraw()})
        }
        LayerLookSection(controller,layerKey,redraw)
        TransformOptionsSection(controller,redraw)
        DashSegmentsSection(controller,redraw)
        Text("Move selection to layer",Modifier.padding(horizontal=12.dp,vertical=4.dp),fontWeight=FontWeight.Bold)
        FlowRow(Modifier.fillMaxWidth().padding(horizontal=12.dp)){
            (0 until controller.layerCount()).filter{it!=controller.selectedLayer}.forEach{index->
                TextButton(onClick={if(controller.moveSelectionToLayer(index))redraw()}){Text("Move selection here: "+controller.layerName(index))}
            }
        }
        LayerMaskSection(controller,redraw)
        Spacer(Modifier.height(20.dp))
      }
    }
    if(renameOpen){
        AlertDialog(
            onDismissRequest={renameOpen=false},
            title={Text("Rename layer")},
            text={OutlinedTextField(value=renameText,onValueChange={renameText=it},singleLine=true)},
            confirmButton={TextButton(onClick={controller.renameLayer(name=renameText);renameOpen=false;redraw()}){Text("Rename")}},
            dismissButton={TextButton(onClick={renameOpen=false}){Text("Cancel")}}
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@NonSkippableComposable
@Composable private fun MaterialsSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    var deleteConfirm by remember{mutableStateOf(false)}
    if(deleteConfirm){
        val index=controller.materials.activeMaterial
        AlertDialog(
            onDismissRequest={deleteConfirm=false},
            title={Text("Delete material $index?")},
            text={Text("Every stroke that uses this material is deleted, on all layers and frames. Materials after it move down one slot. You can undo this.")},
            confirmButton={TextButton(onClick={deleteConfirm=false;if(controller.deleteMaterial(index))redraw()}){Text("Delete")}},
            dismissButton={TextButton(onClick={deleteConfirm=false}){Text("Cancel")}}
        )
    }
    var opacity by remember{mutableFloatStateOf(controller.materials.opacity)}
    val palette=listOf(
        Color.Black,Color.White,Color(0xFFE53935),Color(0xFFFF9800),
        Color(0xFFFFEB3B),Color(0xFF4CAF50),Color(0xFF00BCD4),
        Color(0xFF2196F3),Color(0xFF3F51B5),Color(0xFF9C27B0),
        Color(0xFFE91E63),Color(0xFF795548)
    )
    ModalBottomSheet(onDismissRequest=onDismiss){
        Column(Modifier.verticalScroll(rememberScrollState()).padding(bottom=20.dp)){
            Text("Brush & Materials",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
            Text("Stroke color",Modifier.padding(horizontal=20.dp))
            FlowRow(Modifier.padding(20.dp),horizontalArrangement=Arrangement.spacedBy(10.dp)){
                palette.forEach{color->
                    Box(
                        Modifier.size(38.dp).background(color,CircleShape)
                            .border(2.dp,if(controller.materials.colorArgb==color.toArgb())Accent else Color.Transparent,CircleShape)
                            .clickable{controller.setMaterialColor(color.toArgb());redraw()}
                    )
                }
            }
            Text("Color picker",Modifier.padding(horizontal=20.dp))
            BlenderColorPicker(controller.materials.colorArgb,{controller.setMaterialColor(it);redraw()})
            Spacer(Modifier.height(12.dp))
            Text("Thickness "+controller.brushes.size.toInt(),Modifier.padding(horizontal=20.dp))
            Slider(controller.brushes.size,{controller.brushes.setSize(it);redraw()},valueRange=.5f..500f)
            Text("Opacity "+(opacity*100).toInt().toString()+"%",Modifier.padding(horizontal=20.dp))
            Slider(opacity,{opacity=it;controller.materials.setOpacity(it);controller.setMaterialColor(controller.materials.colorArgb);redraw()},valueRange=0f..1f)
            Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
                Text("Fill closed strokes",Modifier.weight(1f))
                Switch(
                    checked=controller.materials.fillEnabled,
                    onCheckedChange={controller.setMaterialFillEnabled(it);redraw()}
                )
            }
            Text("Active material: "+controller.materials.activeMaterial+" of "+controller.materialCount(),Modifier.padding(horizontal=20.dp))
            Row(Modifier.padding(horizontal=20.dp)){
                TextButton(onClick={controller.selectMaterial(controller.materials.activeMaterial+1);redraw()}){Text("Next brush/material")}
                TextButton(onClick={deleteConfirm=true},enabled=controller.materialCount()>1){Text("Delete material")}
            }
            MaterialSlotsSection(controller,redraw)
            MaterialTextureSection(controller,LocalContext.current,redraw)
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@NonSkippableComposable
@Composable private fun OnionSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){Column(Modifier.verticalScroll(rememberScrollState())){OnionContent(controller,redraw)
        Text("Layers can opt out in Layers > Use onion skinning.",Modifier.padding(horizontal=20.dp),fontSize=11.sp);Spacer(Modifier.height(20.dp))}}
}

/** Blender's Onion Skinning panel (data settings shared by the layers). */
@NonSkippableComposable
@Composable private fun OnionContent(controller:EditorController,redraw:()->Unit){
    var opacity by remember{mutableFloatStateOf(controller.onion.opacity)}
    Text("Onion Skin",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){Text("Enable",Modifier.weight(1f));Switch(checked=controller.onion.enabled,onCheckedChange={controller.setOnionSkin(it,controller.onion.beforeFrames,controller.onion.afterFrames,controller.onion.opacity);redraw()})}
        Text("Previous "+controller.onion.beforeFrames,Modifier.padding(horizontal=20.dp));Slider(controller.onion.beforeFrames.toFloat(), {val v=it.toInt();controller.setOnionSkin(controller.onion.enabled,v,controller.onion.afterFrames,controller.onion.opacity);redraw()}, valueRange = 0f..12f)
        Text("Next "+controller.onion.afterFrames,Modifier.padding(horizontal=20.dp));Slider(controller.onion.afterFrames.toFloat(), {val v=it.toInt();controller.setOnionSkin(controller.onion.enabled,controller.onion.beforeFrames,v,controller.onion.opacity);redraw()}, valueRange = 0f..12f)
        Text("Opacity "+(opacity*100).toInt().toString()+"%",Modifier.padding(horizontal=20.dp));Slider(opacity, {opacity=it;controller.setOnionSkin(controller.onion.enabled,controller.onion.beforeFrames,controller.onion.afterFrames,it);redraw()}, valueRange = 0f..1f)
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Column(Modifier.weight(1f)){Text("Fade");Text("Ghosts further from the current frame are fainter",fontSize=11.sp)}
            Switch(checked=controller.onion.fade,onCheckedChange={controller.setOnionFade(it);redraw()})
        }
        // Blender's onion mode and custom ghost colours (Onion Skinning panel).
        Text("Mode",Modifier.padding(horizontal=20.dp,vertical=4.dp))
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp)){
            listOf(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ONION_MODE_RELATIVE to "Keyframes",
                com.smitnk.projectgrease.editor.ProjectGreaseSelect.ONION_MODE_ABSOLUTE to "Frames",
                com.smitnk.projectgrease.editor.ProjectGreaseSelect.ONION_MODE_SELECTED to "Selected").forEach{(m,label)->
                FilterChip(selected=controller.onion.mode==m,onClick={controller.setOnionStyle(mode=m);redraw()},label={Text(label)},modifier=Modifier.padding(end=6.dp))
            }
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Box(Modifier.size(18.dp).background(Color(controller.onion.prevColor),CircleShape));Spacer(Modifier.width(8.dp))
            Text("Use colour before",Modifier.weight(1f))
            Switch(checked=controller.onion.usePrevColor,onCheckedChange={controller.setOnionStyle(usePrevColor=it);redraw()})
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Box(Modifier.size(18.dp).background(Color(controller.onion.nextColor),CircleShape));Spacer(Modifier.width(8.dp))
            Text("Use colour after",Modifier.weight(1f))
            Switch(checked=controller.onion.useNextColor,onCheckedChange={controller.setOnionStyle(useNextColor=it);redraw()})
        }
        OnionFilterSection(controller,redraw)
        
}

/**
 * The selected layer's live modifier stack (non-destructive): modifiers run top to bottom on a copy
 * of the frame that is drawn; the strokes change only when one is applied.
 */
@NonSkippableComposable
@Composable private fun ModifierStackSection(controller:EditorController,redraw:()->Unit){
    val layer=controller.selectedLayer
    var tick by remember{mutableStateOf(0)}
    var addMenu by remember{mutableStateOf(false)}
    var expanded by remember(layer){mutableStateOf(setOf<Int>())}
    val stack=remember(tick,layer){controller.modifiers(layer)}
    fun changed(ok:Boolean){if(ok){tick++;redraw()}}
    Text("Modifier stack (layer "+(layer+1)+")",Modifier.padding(horizontal=20.dp,vertical=10.dp),fontWeight=FontWeight.Bold)
    if(stack.isEmpty())Text("No modifiers. Add one; it is applied live and the strokes stay untouched until you press Apply.",Modifier.padding(horizontal=20.dp))
    stack.forEachIndexed{index,modifier->
        val open=index in expanded
        Column(Modifier.fillMaxWidth().padding(horizontal=20.dp,vertical=4.dp).border(1.dp,MaterialTheme.colorScheme.outline,RoundedCornerShape(8.dp)).padding(8.dp)){
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
                Text(com.smitnk.projectgrease.editor.ModifierType.name(modifier.type),Modifier.weight(1f).clickable{expanded=if(open)expanded-index else expanded+index},fontWeight=FontWeight.Bold)
                Switch(checked=modifier.enabled,onCheckedChange={changed(controller.setModifierEnabled(index,it))})
            }
            FlowRow(Modifier.fillMaxWidth(), itemVerticalAlignment=Alignment.CenterVertically){
                TextButton(onClick={expanded=if(open)expanded-index else expanded+index}){Text(if(open)"Hide" else "Edit")}
                TextButton(onClick={changed(controller.moveModifier(index,-1))},enabled=index>0){Text("Up")}
                TextButton(onClick={changed(controller.moveModifier(index,1))},enabled=index<stack.size-1){Text("Down")}
                TextButton(onClick={expanded=emptySet();changed(controller.applyLayerModifier(index))}){Text("Apply")}
                TextButton(onClick={expanded=emptySet();changed(controller.removeModifier(index))}){Text("Remove")}
            }
            if(open)ModifierInfluenceSection(controller,index,modifier,::changed)
            if(open)com.smitnk.projectgrease.editor.ModifierSpecs.specs(modifier.type).forEach{spec->
                val value=modifier.params.getOrElse(spec.index){0f}
                when(spec.kind){
                    com.smitnk.projectgrease.editor.ParamKind.BOOL->Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
                        Text(spec.label,Modifier.weight(1f));Switch(checked=value!=0f,onCheckedChange={changed(controller.setModifierParam(index,spec.index,if(it)1f else 0f))})
                    }
                    com.smitnk.projectgrease.editor.ParamKind.ENUM->OutlinedButton(
                        onClick={changed(controller.setModifierParam(index,spec.index,((value.toInt()+1)%spec.options.size).toFloat()))},
                        modifier=Modifier.fillMaxWidth()
                    ){Text(spec.label+": "+spec.options.getOrElse(value.toInt()){"?"})}
                    else->{
                        val shown=value*spec.displayFactor
                        Text(spec.label+" "+(if(spec.kind==com.smitnk.projectgrease.editor.ParamKind.INT)shown.toInt().toString() else "%.2f".format(shown)))
                        Slider(
                            value=value.coerceIn(spec.min,spec.max),
                            onValueChange={changed(controller.setModifierParam(index,spec.index,if(spec.kind==com.smitnk.projectgrease.editor.ParamKind.INT)Math.round(it).toFloat() else it,commit=false))},
                            onValueChangeFinished={controller.commitModifierEdit()},
                            valueRange=spec.min..spec.max
                        )
                    }
                }
            }
        }
    }
    Box(Modifier.padding(horizontal=20.dp)){
        Button(onClick={addMenu=true},modifier=Modifier.fillMaxWidth()){Text("Add modifier")}
        DropdownMenu(expanded=addMenu,onDismissRequest={addMenu=false}){
            com.smitnk.projectgrease.editor.ModifierType.all.forEach{type->
                DropdownMenuItem(text={Text(com.smitnk.projectgrease.editor.ModifierType.name(type))},onClick={addMenu=false;changed(controller.addModifier(type))})
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@NonSkippableComposable
@Composable private fun AdvancedSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){
      Column(Modifier.verticalScroll(rememberScrollState())){
        Text("Advanced drawing",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        var tickA by remember{mutableIntStateOf(0)};@Suppress("UNUSED_VARIABLE") val seenA=tickA
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Draw strokes on back (new lines go under the others)",Modifier.weight(1f))
            Switch(checked=controller.drawOnBack,onCheckedChange={controller.drawOnBack=it;tickA++})
        }
        Text("Random colour per stroke (Vertex Color mode): hue ${(controller.randomHue*100).toInt()}%, saturation ${(controller.randomSaturation*100).toInt()}%, value ${(controller.randomValue*100).toInt()}%",Modifier.padding(horizontal=20.dp))
        Slider(controller.randomHue,{controller.randomHue=it;tickA++},modifier=Modifier.padding(horizontal=20.dp))
        Slider(controller.randomSaturation,{controller.randomSaturation=it;tickA++},modifier=Modifier.padding(horizontal=20.dp))
        Slider(controller.randomValue,{controller.randomValue=it;tickA++},modifier=Modifier.padding(horizontal=20.dp))
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Stabilization",Modifier.weight(1f))
            Switch(checked=controller.stabilizerEnabled,onCheckedChange={controller.setStabilizer(it);redraw()})
        }
        Text("Stabilizer factor " + (controller.stabilizerFactor*100).toInt() + "%",Modifier.padding(horizontal=20.dp))
        Slider(controller.stabilizerFactor,{controller.setStabilizer(controller.stabilizerEnabled,it);redraw()},valueRange=0.5f..0.99f,modifier=Modifier.padding(horizontal=20.dp).testTag("stabFactor"))
        Text("Stabilizer radius " + controller.stabilizerRadius.toInt() + " px (the finger moves this far before the line follows)",Modifier.padding(horizontal=20.dp))
        Slider(controller.stabilizerRadius,{controller.setStabilizer(controller.stabilizerEnabled,radius=it);redraw()},valueRange=10f..200f,modifier=Modifier.padding(horizontal=20.dp))
        BrushCurvesSection(controller,redraw)
        DrawingGuideSection(controller,redraw)
        Text("Sculpt brushes are in Sculpt mode: pick Sculpt, then tap the brush button again. Stroke operations are under Selection actions (Options while a selection tool is active, or Menu > Edit actions).",
            Modifier.padding(horizontal=20.dp,vertical=8.dp),fontSize=12.sp)
        ModifierStackSection(controller,redraw)
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Multiframe editing",Modifier.weight(1f))
            Switch(
                checked=controller.multiframeEditing,
                onCheckedChange={controller.setMultiframeEditing(it);redraw()}
            )
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Grid",Modifier.weight(1f));Switch(checked=controller.view.showGrid,onCheckedChange={controller.view.toggleGrid();redraw()})
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Guides",Modifier.weight(1f));Switch(checked=controller.view.showGuides,onCheckedChange={controller.view.toggleGuides();redraw()})
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Snapping",Modifier.weight(1f));Switch(checked=controller.view.snapEnabled,onCheckedChange={controller.view.toggleSnapping();redraw()})
        }
        Spacer(Modifier.height(20.dp))
      }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@NonSkippableComposable
@Composable private fun MoreSheet(controller:EditorController,onDismiss:()->Unit,onSettings:()->Unit,redraw:()->Unit,onReference:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){Column(Modifier.verticalScroll(rememberScrollState())){Text("Edit actions",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        ListItem(headlineContent={Text("3D reference (Line Art)")},supportingContent={Text("Import OBJ meshes and set the camera Line Art will use")},modifier=Modifier.clickable{onReference()})
        SelectionActionsPanel(controller,redraw)
        ListItem(headlineContent={Text("Settings")},modifier=Modifier.clickable{onDismiss();onSettings()})
        Spacer(Modifier.height(20.dp))
    }}
}

@Composable private fun CapabilityRow(label:String,id:FeatureId){
    val c=FeatureRegistry.capability(id)
    if(c.state==FeatureState.NOT_IMPLEMENTED)return
    ListItem(headlineContent={Text(label)},supportingContent={Text("Engine connected")})
}

@Composable private fun Settings(
    controller:EditorController,
    current:ProjectGreaseThemeMode,
    onTheme:(ProjectGreaseThemeMode)->Unit,
    onBack:()->Unit
){
    var stabilization by remember { mutableStateOf(controller.stabilizerEnabled) }
    var pressureCurve by remember { mutableFloatStateOf(controller.brushes.pressureCurve) }
    var grid by remember { mutableStateOf(controller.view.showGrid) }
    var guides by remember { mutableStateOf(controller.view.showGuides) }
    var snapping by remember { mutableStateOf(controller.view.snapEnabled) }
    var loop by remember { mutableStateOf(controller.animation.loop) }
    var fps by remember { mutableFloatStateOf(controller.animation.fps.toFloat()) }

    Scaffold(topBar={TopAppBar(title={Text("Settings")},navigationIcon={IconButton(onClick=onBack){Icon(Icons.Default.ArrowBack,"Back")}})}){pad->
        Column(Modifier.fillMaxSize().padding(pad).verticalScroll(rememberScrollState())){
            Text("Theme",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            ProjectGreaseThemeMode.entries.forEach{mode->
                ListItem(headlineContent={Text(mode.name.lowercase().replaceFirstChar{it.uppercase()})},
                    trailingContent={if(current==mode)Icon(Icons.Default.Check,null,tint=Accent)},
                    modifier=Modifier.clickable{onTheme(mode)})
            }
            Text("Renderer",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            val ctx=LocalContext.current
            var blender by remember { mutableStateOf(controller.useBlenderRenderer) }
            Row(Modifier.fillMaxWidth().padding(horizontal=16.dp),verticalAlignment=Alignment.CenterVertically){
                Column(Modifier.weight(1f)){
                    Text("Use Blender's renderer")
                    Text("Effects, onion skin and rotation as in Blender. Switch off if the canvas goes dark or the app hangs on this phone.",fontSize=12.sp)
                }
                Switch(checked=blender,onCheckedChange={
                    blender=it;controller.useBlenderRenderer=it;controller.render()
                    ctx.getSharedPreferences("project_grease_settings",android.content.Context.MODE_PRIVATE).edit().putBoolean("blender_renderer",it).apply()
                },modifier=Modifier.testTag("settingBlenderRenderer"))
            }
            Text("Drawing",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            Row(Modifier.fillMaxWidth().padding(horizontal=16.dp),verticalAlignment=Alignment.CenterVertically){
                Text("Stabilization",Modifier.weight(1f))
                Switch(
                    checked=stabilization,
                    onCheckedChange={stabilization=it;controller.setStabilizer(it)}
                )
            }
            Text("Pressure and strength curves: Advanced > Brush curves (Blender CurveMapping).",Modifier.padding(horizontal=16.dp),fontSize=12.sp)
            @Suppress("UNUSED_VARIABLE") val keepState=pressureCurve
            Row(Modifier.fillMaxWidth().padding(horizontal=16.dp),verticalAlignment=Alignment.CenterVertically){
                Text("Grid",Modifier.weight(1f))
                Switch(
                    checked=grid,
                    onCheckedChange={grid=it;controller.view.toggleGrid();controller.render()}
                )
            }
            Row(Modifier.fillMaxWidth().padding(horizontal=16.dp),verticalAlignment=Alignment.CenterVertically){
                Text("Guides",Modifier.weight(1f))
                Switch(
                    checked=guides,
                    onCheckedChange={guides=it;controller.view.toggleGuides();controller.render()}
                )
            }
            Row(Modifier.fillMaxWidth().padding(horizontal=16.dp),verticalAlignment=Alignment.CenterVertically){
                Text("Snapping",Modifier.weight(1f))
                Switch(
                    checked=snapping,
                    onCheckedChange={snapping=it;controller.view.toggleSnapping();controller.render()}
                )
            }
            Text("Animation",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            Row(Modifier.fillMaxWidth().padding(horizontal=16.dp),verticalAlignment=Alignment.CenterVertically){
                Text("Loop playback",Modifier.weight(1f))
                Switch(
                    checked=loop,
                    onCheckedChange={loop=it;controller.animation.toggleLoop()}
                )
            }
            Text("FPS " + fps.toInt(),Modifier.padding(horizontal=16.dp))
            Slider(
                fps,
                {
                    fps=it
                    controller.animation.setFps(it.toInt())
                },
                valueRange=1f..60f,
                modifier=Modifier.padding(horizontal=16.dp)
            )
            Button(onClick={controller.animation.togglePlayback()},modifier=Modifier.fillMaxWidth().padding(horizontal=16.dp)){Text(if(controller.animation.playing)"Pause" else "Play")}
            Text("In-between easing",Modifier.padding(horizontal=16.dp,vertical=4.dp),fontWeight=FontWeight.Bold)
            var easingTick by remember{mutableStateOf(0)}
            FlowRow(Modifier.fillMaxWidth().padding(horizontal=12.dp)){
                listOf("Linear","Quad","Cubic","Quart","Quint","Sine","Expo","Circ","Back","Bounce","Elastic").forEachIndexed{type,label->
                    FilterChip(
                        selected=controller.animation.easingType==type,
                        onClick={controller.animation.setEasing(type,controller.animation.easingMode);easingTick++},
                        label={Text(label,fontSize=10.sp)},
                        modifier=Modifier.padding(end=3.dp)
                    )
                }
            }
            FlowRow(Modifier.fillMaxWidth().padding(horizontal=12.dp)){
                listOf("In","Out","In-Out").forEachIndexed{mode,label->
                    FilterChip(
                        selected=controller.animation.easingMode==mode,
                        enabled=controller.animation.easingType!=0,
                        onClick={controller.animation.setEasing(controller.animation.easingType,mode);easingTick++},
                        label={Text(label,fontSize=10.sp)},
                        modifier=Modifier.padding(end=3.dp)
                    )
                }
            }
            if(controller.animation.easingType==com.smitnk.projectgrease.editor.ProjectGreaseSelect.EASE_ELASTIC){
                var amp by remember{mutableFloatStateOf(controller.animation.elasticAmplitude)}
                var per by remember{mutableFloatStateOf(controller.animation.elasticPeriod)}
                Text("Amplitude " + "%.2f".format(amp),Modifier.padding(horizontal=16.dp))
                Slider(amp,{amp=it;controller.animation.setElastic(amp,per)},valueRange=0f..2f,modifier=Modifier.padding(horizontal=16.dp))
                Text("Period " + "%.2f".format(per),Modifier.padding(horizontal=16.dp))
                Slider(per,{per=it;controller.animation.setElastic(amp,per)},valueRange=0f..2f,modifier=Modifier.padding(horizontal=16.dp))
            }
            InterpolationOptionsSection(controller.animation)
            Button(onClick={controller.interpolateFrameAt(controller.animation.currentFrame)},enabled=controller.animation.frameNumbers().size>=2,modifier=Modifier.fillMaxWidth().padding(horizontal=16.dp)){Text("Create in-between frame")}
            Button(onClick={controller.interpolateSequence()},enabled=controller.animation.frameNumbers().size>=2,modifier=Modifier.fillMaxWidth().padding(horizontal=16.dp)){Text("Interpolate sequence (all in-betweens)")}
            Text("Editor",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            Button(onClick={controller.view.reset();controller.render()},modifier=Modifier.fillMaxWidth().padding(horizontal=16.dp)){Text("Reset canvas view")}
            Button(onClick={controller.smoothSelectedStroke()},modifier=Modifier.fillMaxWidth().padding(horizontal=16.dp)){Text("Smooth selected stroke")}
        }
    }
}


// ---- Studio layout: full-screen canvas, floating toolbar, layer panel, frame strip, option popups ----

private enum class StudioPopup { NONE, OPTIONS, COLOR, MODE }

@NonSkippableComposable
@Composable private fun StudioLayout(controller:EditorController,refresh:Int,overlayTick:Int,redraw:()->Unit,
                                     onSheet:(Sheet)->Unit,viewport:@Composable BoxScope.()->Unit){
    var popup by remember{mutableStateOf(StudioPopup.NONE)}
    var layersOpen by androidx.compose.runtime.saveable.rememberSaveable{mutableStateOf(false)}
    var stripOpen by androidx.compose.runtime.saveable.rememberSaveable{mutableStateOf(true)}
    Box(Modifier.fillMaxSize().background(CanvasBg).testTag("studioCanvas")){
        viewport()
        // canvas overlays turn with the two-finger view rotation (about the viewport centre)
        Box(Modifier.fillMaxSize().graphicsLayer{rotationZ=Math.toDegrees(controller.view.rotation.toDouble()).toFloat()}){
            DrawingGuidesOverlay(controller)
            LassoPathOverlay(controller)
            ReferenceOverlay(controller,refresh)
            CurveHandlesOverlay(controller,overlayTick,redraw)
            ShapeEditOverlay(controller,overlayTick,redraw)
            ModifierHandlesOverlay(controller,overlayTick,redraw)
            BoxSelectOverlay(controller,overlayTick)
            WarpOverlay(controller,overlayTick,redraw)
            ReferenceAdjustOverlay(controller,redraw)
        }

        // top: project menu, mode, more
        Row(Modifier.align(Alignment.TopCenter).padding(6.dp),verticalAlignment=Alignment.CenterVertically){
            FloatingIcon(Icons.Default.Menu,"Menu"){onSheet(Sheet.MENU)}
            Spacer(Modifier.width(6.dp))
            Surface(shape=RoundedCornerShape(16.dp),tonalElevation=4.dp,shadowElevation=2.dp,
                modifier=Modifier.clickable{popup=StudioPopup.MODE}.testTag("studioMode")){
                Text(controller.mode.name.replace('_',' ').lowercase().replaceFirstChar{it.uppercase()}+" ▾",
                    Modifier.padding(horizontal=12.dp,vertical=6.dp),fontSize=12.sp)
            }
            Spacer(Modifier.width(6.dp))
            FloatingIcon(Icons.Default.FitScreen,"Fit canvas"){controller.fitCanvas();redraw()}
            FloatingIcon(Icons.Default.Apps,"All tools"){onSheet(Sheet.TOOLS)}
        }

        // floating toolbar
        val tool=controller.tools.activeTool
        Surface(Modifier.align(Alignment.CenterStart).padding(start=6.dp).testTag("studioToolbar"),
            shape=RoundedCornerShape(24.dp),tonalElevation=6.dp,shadowElevation=4.dp){
            Column(Modifier.padding(vertical=4.dp),horizontalAlignment=Alignment.CenterHorizontally){
                // Tap a tool to use it; tap the active tool again for its options.
                fun pick(t:GreaseTool){ if(tool==t) popup=StudioPopup.OPTIONS else { controller.selectTool(t); popup=StudioPopup.NONE }; redraw() }
                ToolbarButton(Icons.Default.Brush,"Brush",tool==GreaseTool.DRAW||tool==GreaseTool.SCULPT){pick(if(controller.mode==GreaseMode.SCULPT)GreaseTool.SCULPT else GreaseTool.DRAW)}
                ToolbarButton(Icons.Default.Clear,"Eraser",tool==GreaseTool.ERASE){pick(GreaseTool.ERASE)}
                ToolbarButton(Icons.Default.Gesture,"Lasso",tool==GreaseTool.LASSO){pick(GreaseTool.LASSO)}
                ToolbarButton(Icons.Default.FormatColorFill,"Fill",tool==GreaseTool.FILL){pick(GreaseTool.FILL)}
                Box(Modifier.padding(6.dp).size(30.dp).background(Color(controller.materials.colorArgb),CircleShape)
                    .border(2.dp,MaterialTheme.colorScheme.outline,CircleShape)
                    .clickable{popup=if(popup==StudioPopup.COLOR)StudioPopup.NONE else StudioPopup.COLOR}.testTag("studioColor"))
                ToolbarButton(Icons.Default.Layers,"Layers",layersOpen){layersOpen=!layersOpen}
                IconButton(enabled=controller.history.canUndo,onClick={controller.undo();redraw()}){Icon(Icons.Default.Undo,"Undo")}
                IconButton(enabled=controller.history.canRedo,onClick={controller.redo();redraw()}){Icon(Icons.Default.Redo,"Redo")}
                ToolbarButton(Icons.Default.Tune,"Options",popup==StudioPopup.OPTIONS){popup=if(popup==StudioPopup.OPTIONS)StudioPopup.NONE else StudioPopup.OPTIONS}
            }
        }

        // Lasso Cut (Blender's Cutter tool): the lasso cuts away the line parts inside it
        if(tool==GreaseTool.LASSO&&!controller.warpActive){
            var cutTick by remember{mutableIntStateOf(0)};@Suppress("UNUSED_VARIABLE") val seenCut=cutTick
            FilterChip(controller.lassoCut,{controller.lassoCut=!controller.lassoCut;cutTick++},{Text(if(controller.lassoCut)"Cut: on (lasso erases inside)" else "Cut")},
                Modifier.align(Alignment.TopStart).padding(top=108.dp,start=8.dp).testTag("lassoCut"))
        }
        // after a lasso selection: its transforms right away (no second tap on the tool)
        if(tool==GreaseTool.LASSO&&!controller.warpActive&&overlayTick+refresh>=0&&controller.selectionBounds()!=null)
            Row(Modifier.align(Alignment.TopCenter).padding(top=52.dp).testTag("lassoQuickBar"),horizontalArrangement=Arrangement.spacedBy(6.dp)){
                Button(onClick={controller.beginWarp(2,2);redraw()},Modifier.testTag("lassoWarp")){Text("Warp")}
                OutlinedButton(onClick={controller.beginWarp(3,3);redraw()}){Text("Warp mesh")}
                OutlinedButton(onClick={popup=StudioPopup.OPTIONS}){Text("More…")}
            }
        if(layersOpen) StudioLayerPanel(controller,Modifier.align(Alignment.TopEnd).padding(top=52.dp,end=6.dp),redraw,{onSheet(Sheet.LAYERS)},
            {onSheet(Sheet.LAYER_ADVANCED)},{onSheet(Sheet.ROTOSCOPE)})

        Column(Modifier.align(Alignment.BottomCenter).fillMaxWidth()){
            Row(Modifier.align(Alignment.End).padding(end=6.dp)){
                FloatingIcon(if(stripOpen)Icons.Default.KeyboardArrowDown else Icons.Default.KeyboardArrowUp,
                    if(stripOpen)"Hide timeline" else "Show timeline"){stripOpen=!stripOpen}
            }
            if(stripOpen) StudioFrameStrip(controller,redraw,{onSheet(Sheet.TIMELINE)})
        }

        when(popup){
            StudioPopup.OPTIONS->StudioCard({popup=StudioPopup.NONE}){
                ModeBrushBar(controller,redraw)
                if(tool==GreaseTool.LASSO||tool in com.smitnk.projectgrease.editor.SELECTION_TOOLS) LassoActions(controller,redraw)
                if(tool in com.smitnk.projectgrease.editor.SELECTION_TOOLS||controller.mode==GreaseMode.EDIT) SelectionActionsPanel(controller,redraw)
            }
            StudioPopup.COLOR->StudioCard({popup=StudioPopup.NONE}){
                Row(Modifier.padding(horizontal=12.dp),verticalAlignment=Alignment.CenterVertically){
                    Text("Color",fontWeight=FontWeight.Bold,modifier=Modifier.padding(end=8.dp))
                    FilterChip(selected=controller.vertexColorMode,onClick={controller.setVertexColorMode(true);redraw()},
                        label={Text("Per stroke (Vertex Color)")},modifier=Modifier.padding(end=4.dp).testTag("colorModeVertex"))
                    FilterChip(selected=!controller.vertexColorMode,onClick={controller.setVertexColorMode(false);redraw()},
                        label={Text("Material")},modifier=Modifier.testTag("colorModeMaterial"))
                }
                BlenderColorPicker(controller.materials.colorArgb,{controller.setMaterialColor(it);redraw()})
                VertexColorOptions(controller,redraw)
                TextButton(onClick={controller.selectTool(GreaseTool.EYEDROPPER);popup=StudioPopup.NONE;redraw()}){Text("Pick from canvas")}
                TextButton(onClick={popup=StudioPopup.NONE;onSheet(Sheet.MATERIALS)}){Text("Materials…")}
            }
            StudioPopup.MODE->StudioCard({popup=StudioPopup.NONE}){
                GreaseMode.entries.forEach{m->
                    ListItem(headlineContent={Text(m.name.replace('_',' ').lowercase().replaceFirstChar{it.uppercase()})},
                        leadingContent={RadioButton(selected=controller.mode==m,onClick=null)},
                        modifier=Modifier.clickable{if(controller.setMode(m))redraw();popup=StudioPopup.NONE}.testTag("studioMode_"+m.name))
                }
            }
            StudioPopup.NONE->Unit
        }
    }
}

@Composable private fun FloatingIcon(icon:ImageVector,label:String,onClick:()->Unit){
    Surface(shape=CircleShape,tonalElevation=4.dp,shadowElevation=2.dp,modifier=Modifier.padding(2.dp)){
        IconButton(onClick=onClick,Modifier.size(40.dp)){Icon(icon,label)}
    }
}

@Composable private fun ToolbarButton(icon:ImageVector,label:String,selected:Boolean,onClick:()->Unit){
    Surface(shape=RoundedCornerShape(16.dp),color=if(selected)MaterialTheme.colorScheme.primaryContainer else Color.Transparent,
        modifier=Modifier.padding(horizontal=4.dp,vertical=1.dp)){
        IconButton(onClick=onClick,Modifier.size(44.dp).testTag("studioTool_$label")){Icon(icon,label)}
    }
}

/** A small floating card for tool options (closes on outside tap). */
@Composable private fun StudioCard(onDismiss:()->Unit,content:@Composable ColumnScope.()->Unit){
    androidx.compose.ui.window.Popup(alignment=Alignment.CenterStart,offset=androidx.compose.ui.unit.IntOffset(170,0),
        onDismissRequest=onDismiss,properties=androidx.compose.ui.window.PopupProperties(focusable=true)){
        Surface(Modifier.widthIn(max=360.dp).heightIn(max=480.dp).testTag("studioPopup"),shape=RoundedCornerShape(16.dp),
            tonalElevation=6.dp,shadowElevation=8.dp){
            Column(Modifier.verticalScroll(rememberScrollState()).padding(vertical=8.dp),content=content)
        }
    }
}

/** Lasso / selection: transform and edit the selection (Blender's Edit menu on the selection). */
@OptIn(ExperimentalLayoutApi::class)
@Composable private fun LassoActions(controller:EditorController,redraw:()->Unit){
    FlowRow(Modifier.padding(horizontal=8.dp)){
        listOf(
            "Move" to {controller.selectTool(GreaseTool.MOVE)},
            "Rotate" to {controller.selectTool(GreaseTool.ROTATE)},
            "Scale" to {controller.selectTool(GreaseTool.SCALE)},
            "Flip H" to {controller.selectionPivot()?.let{controller.mirrorSelectedStrokeAround(true,false,it[0],it[1])}?:false},
            "Flip V" to {controller.selectionPivot()?.let{controller.mirrorSelectedStrokeAround(false,true,it[0],it[1])}?:false},
            "Shear ←" to {controller.shearSelection(-0.15f,0f)},
            "Shear →" to {controller.shearSelection(0.15f,0f)},
            "Warp (box)" to {controller.beginWarp(2,2)},
            "Warp mesh" to {controller.beginWarp(3,3)},
            "Break off" to {controller.breakOffSelection()},
            "Copy" to {controller.copySelection()},
            "Cut" to {controller.cutSelection()},
            "Paste" to {controller.pasteStrokes()},
            "Duplicate" to {controller.duplicateSelection()},
            "Erase" to {controller.deleteSelectedPoints()}
        ).forEach{(label,action)->
            OutlinedButton(onClick={action();redraw()},Modifier.padding(2.dp).testTag("lasso_$label")){Text(label,fontSize=11.sp)}
        }
    }
}

/** FlipaClip-style layers: top layer first, the active one highlighted, eye / lock per row. */
@NonSkippableComposable
@Composable private fun StudioLayerPanel(controller:EditorController,modifier:Modifier,redraw:()->Unit,onMore:()->Unit,
                                          onAdvanced:()->Unit,onReference:()->Unit){
    Surface(modifier.width(220.dp).heightIn(max=420.dp).testTag("studioLayers"),shape=RoundedCornerShape(16.dp),tonalElevation=6.dp,shadowElevation=4.dp){
        Column(Modifier.padding(8.dp)){
            Row(verticalAlignment=Alignment.CenterVertically){
                Text("Layers",fontWeight=FontWeight.Bold,modifier=Modifier.weight(1f))
                IconButton(onClick={controller.createLayer("Layer "+(controller.layerCount()+1));redraw()},Modifier.size(36.dp).testTag("layerAdd")){Icon(Icons.Default.Add,"Add layer")}
                IconButton(onClick={controller.duplicateLayer();redraw()},Modifier.size(36.dp)){Icon(Icons.Default.ContentCopy,"Duplicate layer")}
                IconButton(onClick={controller.deleteLayer();redraw()},enabled=controller.layerCount()>1,modifier=Modifier.size(36.dp)){Icon(Icons.Default.Delete,"Delete layer")}
            }
            Column(Modifier.weight(1f,fill=false).verticalScroll(rememberScrollState())){
                for(i in controller.layerCount()-1 downTo 0){
                    val st=controller.layerState(i)
                    val active=i==controller.selectedLayer
                    Surface(Modifier.fillMaxWidth().padding(vertical=2.dp).clickable{controller.selectLayer(i);redraw()}.testTag("layerRow_$i"),
                        shape=RoundedCornerShape(10.dp),
                        color=if(active)MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
                        border=if(active)androidx.compose.foundation.BorderStroke(2.dp,Accent) else null){
                        Row(verticalAlignment=Alignment.CenterVertically,modifier=Modifier.padding(horizontal=4.dp)){
                            IconButton(onClick={controller.setLayerVisibility(i,!(st?.visible?:true));redraw()},Modifier.size(34.dp)){
                                Icon(if(st?.visible!=false)Icons.Default.Visibility else Icons.Default.VisibilityOff,"Visible")
                            }
                            Text((if(active)"● " else "")+(st?.name?.takeIf{it.isNotBlank()}?:"Layer "+(i+1)),
                                Modifier.weight(1f),maxLines=1,fontWeight=if(active)FontWeight.Bold else FontWeight.Normal,fontSize=13.sp)
                            IconButton(onClick={controller.setLayerLocked(i,!(st?.locked?:false));redraw()},Modifier.size(34.dp)){
                                Icon(if(st?.locked==true)Icons.Default.Lock else Icons.Default.LockOpen,"Lock")
                            }
                            // the layer's Advanced panel: modifiers, effects, onion skin, masks, look
                            IconButton(onClick={controller.selectLayer(i);redraw();onAdvanced()},Modifier.size(34.dp).testTag("layerAdvanced_$i")){
                                Icon(Icons.Default.Tune,"Advanced")
                            }
                        }
                    }
                }
            }
            Row{
                TextButton(onClick={val i=controller.selectedLayer;if(i<controller.layerCount()-1){controller.moveLayer(i,i+1);redraw()}}){Text("Up")}
                TextButton(onClick={val i=controller.selectedLayer;if(i>0){controller.moveLayer(i,i-1);redraw()}}){Text("Down")}
                TextButton(onClick=onMore){Text("More…")}
            }
            OutlinedButton(onClick=onReference,Modifier.fillMaxWidth().testTag("rotoscopeOpen")){
                Icon(Icons.Default.Image,null);Spacer(Modifier.width(6.dp))
                Text(if(controller.rotoscope.isEmpty)"Import photo / video…" else "Reference: "+controller.rotoscope.kind.name.lowercase())
            }
        }
    }
}

/** FlipaClip-style frame strip: play, previous / next, frame cells (keys marked), add frame. */
@NonSkippableComposable
@Composable private fun StudioFrameStrip(controller:EditorController,redraw:()->Unit,onTimeline:()->Unit){
    val anim=controller.animation
    Surface(Modifier.fillMaxWidth().testTag("studioFrames"),tonalElevation=6.dp){
        Row(verticalAlignment=Alignment.CenterVertically){
            IconButton(onClick={anim.togglePlayback();redraw()}){Icon(if(anim.playing)Icons.Default.Pause else Icons.Default.PlayArrow,"Play")}
            IconButton(onClick={controller.selectFrame(anim.currentFrame-1);redraw()}){Icon(Icons.Default.SkipPrevious,"Previous")}
            IconButton(onClick={controller.selectFrame(anim.currentFrame+1);redraw()}){Icon(Icons.Default.SkipNext,"Next")}
            val keys=remember(anim.keyframes){anim.keyframes.toSet()}
            val state=androidx.compose.foundation.lazy.rememberLazyListState()
            androidx.compose.foundation.lazy.LazyRow(Modifier.weight(1f).padding(vertical=4.dp),state=state){
                items(anim.timelineEnd.coerceAtLeast(1),key={it+1}){index->
                    val frame=index+1
                    val current=frame==anim.currentFrame
                    Surface(Modifier.width(44.dp).height(48.dp).padding(2.dp).clickable{controller.selectFrame(frame);redraw()}.testTag("strip_$frame"),
                        shape=RoundedCornerShape(8.dp),
                        color=if(current)MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
                        border=if(current)androidx.compose.foundation.BorderStroke(2.dp,Accent) else null){
                        Column(horizontalAlignment=Alignment.CenterHorizontally,verticalArrangement=Arrangement.Center){
                            Text(frame.toString(),fontSize=12.sp)
                            Box(Modifier.size(8.dp).background(if(frame in keys)keyTypeColor(anim.keyTypes[frame]?:0) else Color.Transparent,CircleShape))
                        }
                    }
                }
            }
            IconButton(onClick={val f=anim.currentFrame+1;controller.createFrame(f);controller.selectFrame(f);redraw()},Modifier.testTag("stripAdd")){Icon(Icons.Default.Add,"New frame")}
            IconButton(onClick=onTimeline,Modifier.testTag("stripMore")){Icon(Icons.Default.MoreHoriz,"Timeline tools")}
        }
    }
}


/**
 * One layer's Advanced panel, laid out like Blender 3.6.23's Properties editor for a GP object:
 * Modifiers and Visual Effects tabs (Blender keeps them per object; here each layer has its own
 * stack), the layer's Onion Skinning switch with the shared Onion Skinning settings, Masks and
 * Adjustments (blend, tint, stroke thickness). Sculpt is a mode in Blender, not a layer setting:
 * its brushes are listed here for reach.
 */
@OptIn(ExperimentalMaterial3Api::class)
@NonSkippableComposable
@Composable private fun LayerAdvancedSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    var tab by androidx.compose.runtime.saveable.rememberSaveable{mutableIntStateOf(0)}
    var tick by remember{mutableIntStateOf(0)}
    @Suppress("UNUSED_VARIABLE") val observed=tick
    val layer=controller.selectedLayer
    ModalBottomSheet(onDismissRequest=onDismiss,modifier=Modifier.testTag("layerAdvancedSheet")){
        Text("Advanced · "+controller.layerName(layer),Modifier.padding(horizontal=20.dp,vertical=8.dp),style=MaterialTheme.typography.titleLarge)
        val tabs=listOf("Modifiers","Onion skin","Masks","Adjustments","Sculpt")
        ScrollableTabRow(selectedTabIndex=tab,edgePadding=8.dp){
            tabs.forEachIndexed{i,t->Tab(selected=tab==i,onClick={tab=i},text={Text(t)},modifier=Modifier.testTag("advTab_$i"))}
        }
        Column(Modifier.verticalScroll(rememberScrollState()).padding(bottom=24.dp)){
            when(tab){
                0->ModifierStackSection(controller,redraw)
                1->{
                    Row(Modifier.fillMaxWidth().padding(horizontal=20.dp,vertical=6.dp),verticalAlignment=Alignment.CenterVertically){
                        Text("Show this layer's onion skin",Modifier.weight(1f))
                        Switch(checked=controller.layerOnion(),onCheckedChange={controller.setLayerOnion(layer,it);tick++;redraw()})
                    }
                    OnionContent(controller,redraw)
                }
                2->LayerMaskSection(controller,redraw)
                3->LayerLookSection(controller,Triple(layer,controller.layerCount(),tick),redraw)
                4->{
                    Text("Sculpt brushes (Sculpt mode)",Modifier.padding(horizontal=20.dp,vertical=6.dp),fontWeight=FontWeight.Bold)
                    com.smitnk.projectgrease.editor.SculptBrush.entries.forEach{b->
                        FilterChip(selected=controller.sculpt.brush==b,onClick={controller.sculpt.select(b);controller.setMode(GreaseMode.SCULPT);tick++;redraw()},
                            label={Text(b.name.lowercase().replaceFirstChar{it.uppercase()})},modifier=Modifier.padding(horizontal=20.dp))
                    }
                    Text("Size "+controller.sculptBrush.size.toInt()+" px",Modifier.padding(horizontal=20.dp))
                    Slider(controller.sculptBrush.size,{controller.sculptBrush.setSize(it);tick++},valueRange=1f..500f,modifier=Modifier.padding(horizontal=20.dp))
                    Text("Strength "+(controller.sculptBrush.strength*100).toInt()+"%",Modifier.padding(horizontal=20.dp))
                    Slider(controller.sculptBrush.strength,{controller.sculptBrush.setStrength(it);tick++},valueRange=0f..1f,modifier=Modifier.padding(horizontal=20.dp))
                }
            }
        }
    }
}


/** The one menu: everything that is not on the toolbar, in the option popups or a layer's panel. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun StudioMenu(controller:EditorController,onSheet:(Sheet)->Unit,onSave:()->Unit,onSettings:()->Unit,onDismiss:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss,modifier=Modifier.testTag("studioMenu")){
        Column(Modifier.verticalScroll(rememberScrollState()).padding(bottom=20.dp)){
            Text(controller.document.projectName,Modifier.padding(horizontal=20.dp,vertical=8.dp),style=MaterialTheme.typography.titleLarge)
            @Composable fun item(icon:ImageVector,label:String,detail:String,tag:String,action:()->Unit)=
                ListItem(headlineContent={Text(label)},supportingContent={Text(detail)},leadingContent={Icon(icon,null)},
                    modifier=Modifier.clickable(onClick=action).testTag("menu_$tag"))
            item(Icons.Default.Save,"Save","Save the project now","save",onSave)
            item(Icons.Default.Folder,"Project","Export, import, project settings, close","project"){onSheet(Sheet.PROJECT)}
            item(Icons.Default.Apps,"All tools","Primitives, select, transform, annotate, pan","tools"){onSheet(Sheet.TOOLS)}
            item(Icons.Default.Palette,"Materials","Material slots, fill, textures","materials"){onSheet(Sheet.MATERIALS)}
            item(Icons.Default.Brush,"Drawing settings","Thickness, opacity, stabilizer, spacing, grid, snapping","draw"){onSheet(Sheet.DRAW_SETTINGS)}
            item(Icons.Default.Tune,"Advanced drawing","Stabilization, brush curves, guides, sculpt brushes","advanced"){onSheet(Sheet.ADVANCED)}
            item(Icons.Default.Layers,"Layers (all settings)","Merge, isolate, lock all, transform options","layers"){onSheet(Sheet.LAYERS)}
            item(Icons.Default.AutoAwesome,"Effects (whole drawing)","Blur, colorize, glow, shadow, … shown in the Rendered view","effects"){onSheet(Sheet.EFFECTS)}
            item(Icons.Default.Animation,"Onion skin","Ghosts before / after, mode, colours, filter","onion"){onSheet(Sheet.ONION)}
            item(Icons.Default.Movie,"Timeline tools","Interpolate, keyframes, markers, preview range, FPS","timeline"){onSheet(Sheet.TIMELINE)}
            item(Icons.Default.Edit,"Edit actions","Stroke operators on the selection","edit"){onSheet(Sheet.MORE)}
            item(Icons.Default.Image,"Import photo / video","A photo, image sequence or video to draw over (rotoscope): move, resize, fade","rotoscope"){onSheet(Sheet.ROTOSCOPE)}
            item(Icons.Default.ViewInAr,"3D reference (Line Art)","OBJ meshes and the Line Art camera","reference"){onSheet(Sheet.REFERENCE)}
            item(Icons.Default.Settings,"Settings","App settings","settings"){onDismiss();onSettings()}
            val context=LocalContext.current
            var crash by remember{mutableStateOf<String?>(null)}
            item(Icons.Default.BugReport,"Last crash report","The error of the last crash, to share","crash"){crash="Renderer: "+controller.rendererInfo()+"\n\n"+(com.smitnk.projectgrease.CrashLog.read(context)?:"No crash recorded.")}
            crash?.let{text->
                AlertDialog(onDismissRequest={crash=null},title={Text("Last crash report")},
                    text={Text(text,fontSize=11.sp,modifier=Modifier.heightIn(max=360.dp).verticalScroll(rememberScrollState()))},
                    confirmButton={TextButton(onClick={com.smitnk.projectgrease.CrashLog.share(context,text)}){Text("Share")}},
                    dismissButton={TextButton(onClick={com.smitnk.projectgrease.CrashLog.clear(context);crash=null}){Text("Clear")}})
            }
            // Activity log: what each option did (tool, touch, result, error, timing), to share.
            var activity by remember{mutableStateOf<String?>(null)}
            val saveLog=androidx.activity.compose.rememberLauncherForActivityResult(
                androidx.activity.result.contract.ActivityResultContracts.CreateDocument("text/plain")){uri->
                if(uri!=null){
                    val ok=runCatching{context.contentResolver.openOutputStream(uri)?.use{it.write(("Renderer: "+controller.rendererInfo()+"\n\n"+com.smitnk.projectgrease.CrashLog.readLog()).toByteArray())}!=null}.getOrDefault(false)
                    toast(context,if(ok)"Activity log saved" else "Couldn't save the activity log")
                }
            }
            item(Icons.Default.BugReport,"Activity log","Every option you use and what it did, to share","activity"){
                activity="Renderer: "+controller.rendererInfo()+"\n\n"+com.smitnk.projectgrease.CrashLog.readLog()}
            activity?.let{text->
                AlertDialog(onDismissRequest={activity=null},title={Text("Activity log")},
                    text={Text(text.takeLast(20_000),fontSize=10.sp,modifier=Modifier.heightIn(max=420.dp).verticalScroll(rememberScrollState(Int.MAX_VALUE)))},
                    confirmButton={Row{
                        TextButton(onClick={saveLog.launch("project_grease_activity_log.txt")}){Text("Save file")}
                        TextButton(onClick={com.smitnk.projectgrease.CrashLog.share(context,text.takeLast(90_000))}){Text("Share")}}},
                    dismissButton={TextButton(onClick={com.smitnk.projectgrease.CrashLog.clearLog();activity=null}){Text("Clear")}})
            }
        }
    }
}

/** Blender's vertex colour settings: brush Mix Factor, Stroke / Fill / Both for Draw mode, the
 *  Vertex Paint selection mask and the viewport's Vertex Paint Opacity. */
@Composable private fun VertexColorOptions(controller:EditorController,redraw:()->Unit){
    var tick by remember{mutableIntStateOf(0)}
    key(tick){Column(Modifier.padding(horizontal=12.dp)){
        if(controller.mode==GreaseMode.DRAW&&controller.vertexColorMode){
            FlowRow(itemVerticalAlignment=Alignment.CenterVertically){
                Text("Mode",fontWeight=FontWeight.Bold,modifier=Modifier.padding(end=6.dp))
                listOf(0 to "Stroke",1 to "Fill",2 to "Both").forEach{(m,l)->
                    FilterChip(selected=controller.drawVertexTarget==m,onClick={controller.updateDrawVertexTarget(m);tick++;redraw()},
                        label={Text(l)},modifier=Modifier.padding(end=4.dp).testTag("vertexTarget_$m"))
                }
            }
        }
        if(controller.mode==GreaseMode.DRAW&&controller.vertexColorMode){
            Text("Mix Factor ${"%.2f".format(controller.vertexFactor)}")
            Slider(controller.vertexFactor,{controller.updateVertexFactor(it);tick++},onValueChangeFinished=redraw,
                modifier=Modifier.testTag("vertexMixFactor"))
        }
        if(controller.mode==GreaseMode.VERTEX_PAINT){
            // brushing tints the fill very slowly (Blender: strength / 1000 per sample); this sets it at once
            Button(onClick={controller.setVertexColorAll();redraw()},Modifier.testTag("vertexSetColor")){Text("Set Color: paint the whole frame now")}
            FlowRow(itemVerticalAlignment=Alignment.CenterVertically){
                Text("Selection mask",fontWeight=FontWeight.Bold,modifier=Modifier.padding(end=6.dp))
                listOf(0 to "Off",com.smitnk.projectgrease.editor.ToolSession.SELECT_MASK_POINT to "Points",
                    com.smitnk.projectgrease.editor.ToolSession.SELECT_MASK_STROKE to "Strokes",
                    com.smitnk.projectgrease.editor.ToolSession.SELECT_MASK_SEGMENT to "Segments").forEach{(m,l)->
                    FilterChip(selected=controller.vertexSelectMask==m,onClick={controller.vertexSelectMask=m;tick++;redraw()},
                        label={Text(l)},modifier=Modifier.padding(end=4.dp).testTag("colorVpaintMask_$m"))
                }
            }
            if(controller.vertexSelectMask!=0) Text("Points: paint only the selected points (lasso part of a stroke). Strokes: whole selected strokes. Segments: a stroke's part between its crossings with other strokes; a stroke that crosses nothing is one segment, so all of it paints.",fontSize=11.sp)
        }
        Text("Vertex Paint Opacity (viewport) ${"%.2f".format(controller.vertexOpacity)}")
        Slider(controller.vertexOpacity,{controller.vertexOpacity=it;tick++;redraw()},modifier=Modifier.testTag("vertexOpacity"))
    }}
}
