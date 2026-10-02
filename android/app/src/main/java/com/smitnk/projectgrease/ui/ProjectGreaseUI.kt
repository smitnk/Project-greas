@file:OptIn(ExperimentalMaterial3Api::class)
package com.smitnk.projectgrease.ui

import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
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
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.platform.LocalContext
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.FeatureId
import com.smitnk.projectgrease.editor.FeatureRegistry
import com.smitnk.projectgrease.editor.FeatureState
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.BrushPreset
import com.smitnk.projectgrease.editor.EraserMode

private val Accent = Color(0xFFE84F7B)
private val CanvasBg = Color(0xFF121315)
private enum class Screen { HOME, NEW, EDITOR, SETTINGS }
private enum class Sheet { NONE, PROJECT, TOOLS, LAYERS, MATERIALS, ONION, ADVANCED, MORE }
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
    ToolEntry(GreaseTool.MOVE,Icons.Default.OpenWith,"Move",FeatureId.MOVE),
    ToolEntry(GreaseTool.ROTATE,Icons.Default.RotateRight,"Rotate",FeatureId.ROTATE),
    ToolEntry(GreaseTool.SCALE,Icons.Default.ZoomIn,"Scale",FeatureId.SCALE),
    ToolEntry(GreaseTool.MIRROR,Icons.Default.Flip,"Mirror",FeatureId.MIRROR),
    ToolEntry(GreaseTool.PAN,Icons.Default.PanTool,"Pan",FeatureId.PAN),
    ToolEntry(GreaseTool.SCULPT,Icons.Default.AutoFixHigh,"Sculpt",FeatureId.SCULPT)
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
                projectStore.loadDocument(record.name)?.let { controller.loadDocumentJson(it) }
                screen=Screen.EDITOR
            },{screen=Screen.NEW},{screen=Screen.SETTINGS})
            Screen.NEW->NewProject(name,{name=it},preset,{preset=it},controller,{screen=Screen.HOME}){
                controller.resetDocument()
                controller.document.projectName=name.ifBlank{"Project Grease"}
                controller.document.canvasWidth=preset.width
                controller.document.canvasHeight=preset.height
                controller.animation.setFps(preset.fps)
                controller.createFrame(1)
                controller.saveDocumentJson()?.let { projectStore.saveDocument(controller.document.projectName,it) }
                val record=ProjectRecord(controller.document.projectName,preset.width,preset.height,preset.fps,System.currentTimeMillis())
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

@Composable private fun NewProject(name:String,onName:(String)->Unit,preset:Preset,onPreset:(Preset)->Unit,controller:EditorController,onBack:()->Unit,onCreate:()->Unit){
    Scaffold(topBar={TopAppBar(title={Text("New Project")},navigationIcon={IconButton(onClick=onBack){Icon(Icons.Default.ArrowBack,"Back")}})}){pad->
        Column(Modifier.fillMaxSize().padding(pad).verticalScroll(rememberScrollState()).padding(16.dp)){
            OutlinedTextField(name,onName,Modifier.fillMaxWidth(),label={Text("Project name")});Spacer(Modifier.height(16.dp))
            Text("Canvas presets",color=Accent,fontWeight=FontWeight.Bold)
            presets.forEach{p->ListItem(headlineContent={Text(p.name)},supportingContent={Text(p.width.toString()+" × "+p.height+" • "+p.fps+" FPS")},
                trailingContent={if(p==preset)Icon(Icons.Default.Check,null,tint=Accent)},modifier=Modifier.clickable{onPreset(p);controller.animation.setFps(p.fps)})}
            Button(onClick=onCreate,Modifier.fillMaxWidth()){Text("Create Project")}
        }
    }
}

@Composable private fun Editor(controller:EditorController,state:GreaseUiState,onState:(GreaseUiState)->Unit,onExit:()->Unit,onSettings:()->Unit,viewport:@Composable BoxScope.()->Unit){
    var sheet by remember{mutableStateOf(Sheet.NONE)}
    var refresh by remember{mutableIntStateOf(0)}
    var fpsDialog by remember{mutableStateOf(false)}
    var savedTick by remember{mutableIntStateOf(0)}
    var overlayTick by remember{mutableIntStateOf(0)}
    val context=LocalContext.current
    fun redraw(){refresh++}
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

    BackHandler(enabled=sheet!=Sheet.NONE || state.canvasFocus){
        if(sheet!=Sheet.NONE) sheet=Sheet.NONE
        else onState(state.copy(canvasFocus=false))
    }

    if(state.canvasFocus){
        Box(Modifier.fillMaxSize().background(CanvasBg)){
            viewport()
            DrawingGuidesOverlay(controller)
            CurveHandlesOverlay(controller,overlayTick,::redraw)
            IconButton(onClick={onState(state.copy(canvasFocus=false))},Modifier.align(Alignment.TopStart).padding(8.dp)){
                Icon(Icons.Default.CloseFullscreen,"Exit canvas")
            }
        }
        return
    }

    Column(Modifier.fillMaxSize()){
        Surface(tonalElevation=3.dp){
            Row(Modifier.fillMaxWidth().height(56.dp),verticalAlignment=Alignment.CenterVertically){
                IconButton(onClick={sheet=Sheet.PROJECT}){Icon(Icons.Default.Menu,"Project")}
                Text(controller.document.projectName,maxLines=1,modifier=Modifier.widthIn(max=120.dp))
                Spacer(Modifier.weight(1f))
                IconButton(onClick={sheet=Sheet.LAYERS}){Icon(Icons.Default.Layers,"Layers")}
                IconButton(onClick={sheet=Sheet.MATERIALS}){Icon(Icons.Default.Palette,"Materials")}
                IconButton(onClick={sheet=Sheet.ADVANCED}){Icon(Icons.Default.Tune,"Advanced")}
                IconButton(onClick={
                    persistProject()
                    Toast.makeText(context,"Project saved",Toast.LENGTH_SHORT).show()
                }){Icon(Icons.Default.Save,"Save")}
                IconButton(enabled=controller.history.canUndo,onClick={controller.undo();redraw()}){Icon(Icons.Default.Undo,"Undo")}
                IconButton(enabled=controller.history.canRedo,onClick={controller.redo();redraw()}){Icon(Icons.Default.Redo,"Redo")}
                IconButton(onClick={onState(state.copy(canvasFocus=true))}){Icon(Icons.Default.Fullscreen,"Canvas")}
                IconButton(onClick={sheet=Sheet.MORE}){Icon(Icons.Default.MoreVert,"More")}
            }
        }
        ModeBrushBar(controller,::redraw)
        Row(Modifier.fillMaxWidth().weight(1f)){
            if(state.showTools)ToolRail(controller,{onState(state.copy())},{sheet=Sheet.TOOLS})
            Box(Modifier.weight(1f).fillMaxHeight().background(CanvasBg),contentAlignment=Alignment.Center){
                viewport()
                DrawingGuidesOverlay(controller)
                CurveHandlesOverlay(controller,overlayTick,::redraw)
            }
            if(state.showProperties)Properties(controller,::redraw)
        }
        if(state.showTimeline)Timeline(controller,::redraw,{fpsDialog=true})
    }
    if(fpsDialog) FpsDialog(controller,{fpsDialog=false},::redraw)
    when(sheet){
        Sheet.PROJECT->ProjectSheet({sheet=Sheet.NONE},onSettings,onExit,controller,context,::persistProject)
        Sheet.TOOLS->ToolsSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.LAYERS->LayersSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.MATERIALS->MaterialsSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.ONION->OnionSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.ADVANCED->AdvancedSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.MORE->MoreSheet(controller,{sheet=Sheet.NONE},onSettings,::redraw)
        Sheet.NONE->Unit
    }
}

/** Weight Paint mode: vertex group chips with add/rename/remove and the weight to paint toward. */
@Composable private fun WeightPaintBar(controller:EditorController,redraw:()->Unit){
    var renameOpen by remember{mutableStateOf(false)}
    var renameText by remember{mutableStateOf("")}
    val groups=controller.vertexGroups()
    Row(
        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal=8.dp,vertical=2.dp),
        verticalAlignment=Alignment.CenterVertically
    ){
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

@Composable private fun ModeBrushBar(controller:EditorController,redraw:()->Unit){
    Surface(tonalElevation=2.dp){
        Column(Modifier.fillMaxWidth()){
            Row(
                Modifier.fillMaxWidth()
                    .horizontalScroll(rememberScrollState())
                    .padding(horizontal=8.dp,vertical=5.dp),
                verticalAlignment=Alignment.CenterVertically
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
                Spacer(Modifier.width(7.dp))
                Text("Brush",fontWeight=FontWeight.Bold,fontSize=11.sp,modifier=Modifier.padding(end=5.dp))
                BrushPreset.entries.forEach{preset->
                    FilterChip(
                        selected=controller.brushes.preset==preset,
                        onClick={controller.selectBrush(preset);redraw()},
                        label={Text(preset.name,fontSize=10.sp)},
                        modifier=Modifier.padding(end=3.dp)
                    )
                }
            }
            if (controller.mode == GreaseMode.VERTEX_PAINT) {
                Row(
                    Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal=8.dp,vertical=2.dp),
                    verticalAlignment=Alignment.CenterVertically
                ) {
                    Text("Paint",fontWeight=FontWeight.Bold,fontSize=10.sp,modifier=Modifier.padding(end=6.dp))
                    listOf(
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_DRAW to "Draw",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_BLUR to "Blur",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_AVERAGE to "Average",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_SMEAR to "Smear",
                        com.smitnk.projectgrease.editor.ProjectGreaseSelect.VPAINT_REPLACE to "Replace"
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
            if (controller.tools.activeTool == GreaseTool.ERASE) {
                Row(
                    Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal=8.dp,vertical=2.dp),
                    verticalAlignment=Alignment.CenterVertically
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
                Text("Size "+controller.brushes.size.toInt(),fontSize=10.sp,modifier=Modifier.width(58.dp))
                Slider(
                    value=controller.brushes.size,
                    onValueChange={controller.brushes.setSize(it);redraw()},
                    valueRange=.5f..100f,
                    modifier=Modifier.weight(1f).padding(horizontal=4.dp)
                )
                Text("Strength "+(controller.brushes.strength*100).toInt()+"%",fontSize=10.sp,modifier=Modifier.width(76.dp))
                Slider(
                    value=controller.brushes.strength,
                    onValueChange={
                        controller.setBrushStrength(it)
                        redraw()
                    },
                    valueRange=0f..1f,
                    modifier=Modifier.weight(1f).padding(horizontal=4.dp)
                )
            }
        }
    }
}

@Composable private fun ToolRail(controller:EditorController,onState:()->Unit,onTools:()->Unit){
    val groups=listOf(
        "DRAW" to listOf(GreaseTool.DRAW,GreaseTool.ERASE,GreaseTool.FILL,GreaseTool.EYEDROPPER,GreaseTool.LINE,GreaseTool.RECTANGLE,GreaseTool.CIRCLE,GreaseTool.ARC,GreaseTool.POLYLINE,GreaseTool.CURVE,GreaseTool.PAN),
        "EDIT" to listOf(GreaseTool.SELECT,GreaseTool.LASSO,GreaseTool.MOVE,GreaseTool.ROTATE,GreaseTool.SCALE,GreaseTool.MIRROR),
        "SCULPT" to listOf(GreaseTool.SCULPT)
    )
    Column(Modifier.width(86.dp).fillMaxHeight().background(MaterialTheme.colorScheme.surfaceVariant).verticalScroll(rememberScrollState()),horizontalAlignment=Alignment.CenterHorizontally){
        groups.forEach{(title,group)->
            Text(title,fontSize=9.sp,fontWeight=FontWeight.Bold,color=MaterialTheme.colorScheme.onSurfaceVariant,modifier=Modifier.padding(top=6.dp,bottom=2.dp))
            group.forEach{tool->
                val entry=tools.first{it.tool==tool}
                val capability=FeatureRegistry.capability(entry.feature)
                val enabled=capability.state!=FeatureState.NOT_IMPLEMENTED
                val selected=controller.tools.activeTool==tool
                Column(Modifier.fillMaxWidth().clickable(enabled){controller.selectTool(tool);onState()}.padding(horizontal=4.dp,vertical=2.dp),horizontalAlignment=Alignment.CenterHorizontally){
                    Surface(shape=RoundedCornerShape(18.dp),color=if(selected)MaterialTheme.colorScheme.primaryContainer else Color.Transparent){
                        Icon(entry.icon,entry.label,Modifier.padding(8.dp),tint=if(enabled)MaterialTheme.colorScheme.onSurface else Color.Gray)
                    }
                    Text(entry.label,fontSize=9.sp,maxLines=1)
                }
            }
        }
        IconButton(onClick=onTools){Icon(Icons.Default.Apps,"Tools")}
    }
}
@Composable private fun Properties(controller:EditorController,redraw:()->Unit){
    var thickness by remember{mutableFloatStateOf(controller.materials.thickness)};var opacity by remember{mutableFloatStateOf(controller.materials.opacity)}
    Column(Modifier.width(210.dp).fillMaxHeight().verticalScroll(rememberScrollState()).padding(10.dp)){
        Text("Brush",fontWeight=FontWeight.Bold);Text("Thickness "+thickness.toInt());Slider(thickness, {thickness=it;controller.materials.setThickness(it);redraw()}, valueRange = .5f..100f)
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

@Composable private fun Timeline(controller:EditorController,redraw:()->Unit,onFps:()->Unit){
    Surface(tonalElevation=4.dp){
        Column(Modifier.fillMaxWidth().heightIn(min=120.dp,max=190.dp)){
            Row(
                Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                verticalAlignment=Alignment.CenterVertically
            ){
                IconButton(onClick={controller.selectFrame(controller.animation.currentFrame-1);redraw()}){Icon(Icons.Default.SkipPrevious,"Previous")}
                IconButton(onClick={controller.animation.togglePlayback();redraw()}){Icon(if(controller.animation.playing)Icons.Default.Pause else Icons.Default.PlayArrow,"Play")}
                IconButton(onClick={controller.selectFrame(controller.animation.currentFrame+1);redraw()}){Icon(Icons.Default.SkipNext,"Next")}
                Text("Frame "+controller.animation.currentFrame+" / "+controller.animation.timelineEnd)
                TextButton(onClick=onFps){Text(controller.animation.fps.toString()+" FPS")}
                TextButton(
                    enabled=controller.animation.currentFrame > 1 && controller.animation.currentFrame < controller.animation.timelineEnd,
                    onClick={if(controller.animation.interpolateAt(controller.animation.currentFrame)){redraw()}}
                ){Text("Interpolate")}
                Spacer(Modifier.weight(1f))
                FilterChip(selected=controller.animation.loop,onClick={controller.animation.toggleLoop();redraw()},label={Text("Loop")})
                TextButton(onClick={controller.createFrame(controller.animation.currentFrame+1);redraw()}){Text("+ Frame")}
                TextButton(onClick={controller.animation.duplicateFrame(controller.animation.currentFrame,controller.animation.currentFrame+1);redraw()}){Text("Duplicate")}
                TextButton(onClick={if(controller.animation.frameCount>1){controller.animation.deleteFrame(controller.animation.currentFrame);redraw()}}){Text("Delete")}
                // GPENCIL_OT_blank_frame_add / GPENCIL_OT_frame_clean_duplicate; the strip below re-reads
                // the native frame numbers, and the controller refreshes frame count/end.
                TextButton(onClick={if(controller.insertBlankFrame())redraw()}){Text("Insert blank keyframe")}
                TextButton(onClick={if(controller.cleanDuplicateFrames())redraw()}){Text("Clean duplicate frames")}
            }
            Row(Modifier.horizontalScroll(rememberScrollState()).padding(5.dp)){
                val keyframes=controller.frameNumbers().toSet()
                (1..controller.animation.timelineEnd.coerceAtLeast(1)).forEach{frame->
                    val key=frame in keyframes
                    Surface(
                        Modifier.width(52.dp).height(54.dp).padding(2.dp).clickable{controller.selectFrame(frame);redraw()},
                        shape=RoundedCornerShape(8.dp),
                        tonalElevation=if(frame==controller.animation.currentFrame)5.dp else 0.dp
                    ){Column(horizontalAlignment=Alignment.CenterHorizontally,verticalArrangement=Arrangement.Center){
                        Text(frame.toString())
                        Text(if(key) "KEY" else "HOLD",fontSize=8.sp)
                    }}
                }
                Surface(Modifier.width(64.dp).height(54.dp).padding(2.dp).clickable{
                    controller.createFrame((controller.animation.timelineEnd+1).coerceAtLeast(1));redraw()
                },shape=RoundedCornerShape(8.dp)){
                    Box(contentAlignment=Alignment.Center){Text("+")}
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
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
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Project",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        ListItem(headlineContent={Text("Open project")},modifier=Modifier.clickable{onDismiss()})
        ListItem(headlineContent={Text("Save")},modifier=Modifier.clickable{onSave();Toast.makeText(context,"Project saved",Toast.LENGTH_SHORT).show();onDismiss()})
        ListItem(headlineContent={Text("Save as")},modifier=Modifier.clickable{onSave();Toast.makeText(context,"Project saved",Toast.LENGTH_SHORT).show();onDismiss()})
        ListItem(headlineContent={Text("Export")},modifier=Modifier.clickable{exportOpen=true})
        ListItem(headlineContent={Text("Settings")},modifier=Modifier.clickable{onDismiss();onSettings()});ListItem(headlineContent={Text("Close editor")},modifier=Modifier.clickable{onDismiss();onExit()});Spacer(Modifier.height(20.dp))}
}

@OptIn(ExperimentalMaterial3Api::class)
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
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),verticalAlignment=Alignment.CenterVertically){
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
@Composable private fun LayerEffectsSection(controller:EditorController,redraw:()->Unit){
    val layer=controller.selectedLayer
    var tick by remember{mutableStateOf(0)}
    var addMenu by remember{mutableStateOf(false)}
    var expanded by remember(layer){mutableStateOf(setOf<Int>())}
    val effects=remember(tick,layer){controller.effects(layer)}
    fun changed(ok:Boolean){if(ok){tick++;redraw()}}
    Text("Effects",Modifier.padding(horizontal=12.dp,vertical=8.dp),fontWeight=FontWeight.Bold)
    if(effects.isEmpty())Text("No effects. Effects are drawn live over this layer; the strokes stay untouched. Sizes are canvas pixels.",Modifier.padding(horizontal=12.dp))
    effects.forEachIndexed{index,effect->
        val open=index in expanded
        Column(Modifier.fillMaxWidth().padding(horizontal=12.dp,vertical=4.dp).border(1.dp,MaterialTheme.colorScheme.outline,RoundedCornerShape(8.dp)).padding(8.dp)){
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
                Text(com.smitnk.projectgrease.editor.FxType.name(effect.type),Modifier.weight(1f).clickable{expanded=if(open)expanded-index else expanded+index},fontWeight=FontWeight.Bold)
                Switch(checked=effect.enabled,onCheckedChange={changed(controller.setEffectEnabled(index,it))})
            }
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),verticalAlignment=Alignment.CenterVertically){
                TextButton(onClick={expanded=if(open)expanded-index else expanded+index}){Text(if(open)"Hide" else "Edit")}
                TextButton(onClick={changed(controller.moveEffect(index,-1))},enabled=index>0){Text("Up")}
                TextButton(onClick={changed(controller.moveEffect(index,1))},enabled=index<effects.size-1){Text("Down")}
                TextButton(onClick={expanded=emptySet();changed(controller.removeEffect(index))}){Text("Remove")}
            }
            if(open){
                com.smitnk.projectgrease.editor.FxSpecs.specs(effect.type).forEach{spec->
                    val value=effect.params.getOrElse(spec.index){0f}
                    when(spec.kind){
                        com.smitnk.projectgrease.editor.ParamKind.BOOL->Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
                            Text(spec.label,Modifier.weight(1f));Switch(checked=value!=0f,onCheckedChange={changed(controller.setEffectParam(index,spec.index,if(it)1f else 0f))})
                        }
                        com.smitnk.projectgrease.editor.ParamKind.ENUM->OutlinedButton(
                            onClick={changed(controller.setEffectParam(index,spec.index,((value.toInt()+1)%spec.options.size).toFloat()))},
                            modifier=Modifier.fillMaxWidth()
                        ){Text(spec.label+": "+spec.options.getOrElse(value.toInt()){"?"})}
                        else->{
                            val shown=value*spec.displayFactor
                            Text(spec.label+" "+(if(spec.kind==com.smitnk.projectgrease.editor.ParamKind.INT)shown.toInt().toString() else "%.2f".format(shown)))
                            Slider(
                                value=value.coerceIn(spec.min,spec.max),
                                onValueChange={changed(controller.setEffectParam(index,spec.index,if(spec.kind==com.smitnk.projectgrease.editor.ParamKind.INT)Math.round(it).toFloat() else it,commit=false))},
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
                DropdownMenuItem(text={Text(com.smitnk.projectgrease.editor.FxType.name(type))},onClick={addMenu=false;changed(controller.addEffect(type))})
            }
        }
    }
}

@Composable private fun LayersSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    // Switch and name state come from the native layer, not from fixed defaults, and are re-read
    // whenever the selected layer or the layer list changes.
    val layerKey=controller.selectedLayer to controller.layerCount()
    val layerState=controller.layerState()
    var visible by remember(layerKey){mutableStateOf(layerState?.visible ?: true)}
    var locked by remember(layerKey){mutableStateOf(layerState?.locked ?: false)}
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
        Row(Modifier.fillMaxWidth().padding(horizontal=12.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Visible",Modifier.weight(1f))
            Switch(checked=visible,onCheckedChange={visible=it;controller.setLayerVisibility(controller.selectedLayer,it);redraw()})
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=12.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Locked",Modifier.weight(1f))
            Switch(checked=locked,onCheckedChange={locked=it;controller.setLayerLocked(controller.selectedLayer,it);redraw()})
        }
        LayerMaskSection(controller,redraw)
        LayerEffectsSection(controller,redraw)
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
@Composable private fun MaterialsSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    var thickness by remember{mutableFloatStateOf(controller.materials.thickness)}
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
            Row(Modifier.horizontalScroll(rememberScrollState()).padding(20.dp),horizontalArrangement=Arrangement.spacedBy(10.dp)){
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
            Text("Thickness "+thickness.toInt(),Modifier.padding(horizontal=20.dp))
            Slider(thickness,{thickness=it;controller.materials.setThickness(it);redraw()},valueRange=.5f..100f)
            Text("Opacity "+(opacity*100).toInt().toString()+"%",Modifier.padding(horizontal=20.dp))
            Slider(opacity,{opacity=it;controller.materials.setOpacity(it);controller.setMaterialColor(controller.materials.colorArgb);redraw()},valueRange=0f..1f)
            Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
                Text("Fill closed strokes",Modifier.weight(1f))
                Switch(
                    checked=controller.materials.fillEnabled,
                    onCheckedChange={controller.setMaterialFillEnabled(it);redraw()}
                )
            }
            Text("Active material: "+controller.materials.activeMaterial,Modifier.padding(horizontal=20.dp))
            TextButton(onClick={controller.selectMaterial(controller.materials.activeMaterial+1);redraw()},Modifier.padding(horizontal=20.dp)){Text("Next brush/material")}
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun OnionSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    var opacity by remember{mutableFloatStateOf(controller.onion.opacity)}
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Onion Skin",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){Text("Enable",Modifier.weight(1f));Switch(checked=controller.onion.enabled,onCheckedChange={controller.setOnionSkin(it,controller.onion.beforeFrames,controller.onion.afterFrames,controller.onion.opacity);redraw()})}
        Text("Previous "+controller.onion.beforeFrames,Modifier.padding(horizontal=20.dp));Slider(controller.onion.beforeFrames.toFloat(), {val v=it.toInt();controller.setOnionSkin(controller.onion.enabled,v,controller.onion.afterFrames,controller.onion.opacity);redraw()}, valueRange = 0f..12f)
        Text("Next "+controller.onion.afterFrames,Modifier.padding(horizontal=20.dp));Slider(controller.onion.afterFrames.toFloat(), {val v=it.toInt();controller.setOnionSkin(controller.onion.enabled,controller.onion.beforeFrames,v,controller.onion.opacity);redraw()}, valueRange = 0f..12f)
        Text("Opacity "+(opacity*100).toInt().toString()+"%",Modifier.padding(horizontal=20.dp));Slider(opacity, {opacity=it;controller.setOnionSkin(controller.onion.enabled,controller.onion.beforeFrames,controller.onion.afterFrames,it);redraw()}, valueRange = 0f..1f);Spacer(Modifier.height(20.dp))}
}

/**
 * The selected layer's live modifier stack (non-destructive): modifiers run top to bottom on a copy
 * of the frame that is drawn; the strokes change only when one is applied.
 */
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
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),verticalAlignment=Alignment.CenterVertically){
                TextButton(onClick={expanded=if(open)expanded-index else expanded+index}){Text(if(open)"Hide" else "Edit")}
                TextButton(onClick={changed(controller.moveModifier(index,-1))},enabled=index>0){Text("Up")}
                TextButton(onClick={changed(controller.moveModifier(index,1))},enabled=index<stack.size-1){Text("Down")}
                TextButton(onClick={expanded=emptySet();changed(controller.applyLayerModifier(index))}){Text("Apply")}
                TextButton(onClick={expanded=emptySet();changed(controller.removeModifier(index))}){Text("Remove")}
            }
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
@Composable private fun AdvancedSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){
      Column(Modifier.verticalScroll(rememberScrollState())){
        Text("Advanced drawing",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        Row(Modifier.fillMaxWidth().padding(horizontal=20.dp),verticalAlignment=Alignment.CenterVertically){
            Text("Stabilization",Modifier.weight(1f))
            Switch(checked=controller.stabilizerEnabled,onCheckedChange={controller.setStabilizer(it);redraw()})
        }
        Text("Stabilizer factor " + (controller.stabilizerFactor*100).toInt() + "%",Modifier.padding(horizontal=20.dp))
        Slider(controller.stabilizerFactor,{controller.setStabilizer(controller.stabilizerEnabled,it);redraw()},valueRange=0f..1f,modifier=Modifier.padding(horizontal=20.dp))
        Text("Pressure curve " + "%.2f".format(controller.brushes.pressureCurve),Modifier.padding(horizontal=20.dp))
        Slider(controller.brushes.pressureCurve,{controller.brushes.setPressureCurve(it);redraw()},valueRange=.25f..3f,modifier=Modifier.padding(horizontal=20.dp))
        Text("Legacy GP sculpt brush",Modifier.padding(horizontal=20.dp,vertical=8.dp),fontWeight=FontWeight.Bold)
        listOf(
            com.smitnk.projectgrease.editor.SculptBrush.SMOOTH to "Smooth",
            com.smitnk.projectgrease.editor.SculptBrush.THICKNESS to "Thickness",
            com.smitnk.projectgrease.editor.SculptBrush.STRENGTH to "Strength",
            com.smitnk.projectgrease.editor.SculptBrush.GRAB to "Grab",
            com.smitnk.projectgrease.editor.SculptBrush.PUSH to "Push",
            com.smitnk.projectgrease.editor.SculptBrush.PINCH to "Pinch",
            com.smitnk.projectgrease.editor.SculptBrush.TWIST to "Twist",
            com.smitnk.projectgrease.editor.SculptBrush.RANDOMIZE to "Randomize"
        ).forEach { (brush,label) ->
            Button(
                onClick={controller.sculpt.select(brush);redraw()},
                modifier=Modifier.fillMaxWidth().padding(horizontal=20.dp,vertical=2.dp)
            ){Text(label)}
        }
        Text("Legacy GP operations",Modifier.padding(horizontal=20.dp,vertical=10.dp),fontWeight=FontWeight.Bold)
        listOf(
            "Bring to front" to { controller.arrangeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ARRANGE_TOP) },
            "Bring forward" to { controller.arrangeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ARRANGE_UP) },
            "Send backward" to { controller.arrangeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ARRANGE_DOWN) },
            "Send to back" to { controller.arrangeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ARRANGE_BOTTOM) },
            "Assign active material" to { controller.assignActiveMaterialToSelection() },
            "Reset vertex color" to { controller.resetSelectionVertexColor() },
            "Flip direction" to { controller.flipSelection() },
            "Toggle closed" to { controller.setSelectionCyclic(com.smitnk.projectgrease.editor.ProjectGreaseSelect.CYCLIC_TOGGLE) },
            "Snap to grid" to { controller.snapSelectionToGrid() },
            "Duplicate selection" to { controller.duplicateSelection() },
            "Dissolve points" to { controller.dissolveSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.DISSOLVE_POINTS) },
            "Dissolve between" to { controller.dissolveSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.DISSOLVE_BETWEEN) },
            "Dissolve unselected" to { controller.dissolveSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.DISSOLVE_UNSELECT) },
            "Split selection" to { controller.splitSelection() },
            "Join selected strokes" to { controller.joinSelection() },
            "Mirror copy (X)" to { controller.mirrorSelectionCopy(true, false) },
            "Mirror copy (Y)" to { controller.mirrorSelectionCopy(false, true) },
            "Mirror copy (X+Y)" to { controller.mirrorSelectionCopy(true, true) },
            "Thickness x2 by weight" to { controller.applyThicknessModifierWithWeights(2f) },
            "Select by vertex color" to { controller.selectByVertexColor() },
            "Normalize thickness" to { controller.normalizeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.NORMALIZE_THICKNESS, 1f) },
            "Normalize opacity" to { controller.normalizeSelection(com.smitnk.projectgrease.editor.ProjectGreaseSelect.NORMALIZE_OPACITY, 1f) },
            "Simplify (fixed)" to { controller.simplifySelectionFixed() },
            "Resample (8 px)" to { controller.sampleSelection(8f) },
            "Extrude ends" to { controller.extrudeSelection() },
            "Select random 50%" to { controller.selectRandom() },
            "Insert blank keyframe" to { controller.insertBlankFrame() },
            "Use color as fill color" to { controller.setFillColor(controller.materials.colorArgb) },
            "Clean loose points" to { controller.cleanLoosePoints() },
            "Clean duplicate frames" to { controller.cleanDuplicateFrames() },
            "Set vertex color" to { controller.setSelectionVertexColor() },
            "Invert vertex color" to { controller.invertSelectionVertexColor() },
            "Vertex color brighter" to { controller.selectionVertexColorBrightnessContrast(0.1f, 0f) },
            "Vertex color more contrast" to { controller.selectionVertexColorBrightnessContrast(0f, 0.2f) },
            "Vertex color hue +30°" to { controller.selectionVertexColorHsv(h = 0.5f + 1f / 12f) },
            "Vertex color levels x0.8" to { controller.selectionVertexColorLevels(0f, 0.8f) }
        ).forEach { (label, action) ->
            Button(onClick={ if (action()) redraw() }, modifier=Modifier.fillMaxWidth().padding(horizontal=20.dp,vertical=2.dp)){Text(label)}
        }
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
@Composable private fun MoreSheet(controller:EditorController,onDismiss:()->Unit,onSettings:()->Unit,redraw:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Edit actions",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        ListItem(headlineContent={Text("Move selected stroke")},modifier=Modifier.clickable{controller.translateSelectedStroke(20f,20f);redraw()})
        ListItem(headlineContent={Text("Rotate selected stroke 90°")},modifier=Modifier.clickable{controller.rotateSelectedStroke((Math.PI/2.0).toFloat());redraw()})
        ListItem(headlineContent={Text("Scale selected stroke 110%")},modifier=Modifier.clickable{controller.scaleSelectedStroke(1.1f,1.1f);redraw()})
        ListItem(headlineContent={Text("Mirror selected stroke X")},modifier=Modifier.clickable{controller.mirrorSelectedStroke(true,false);redraw()})
        ListItem(headlineContent={Text("Mirror selected stroke Y")},modifier=Modifier.clickable{controller.mirrorSelectedStroke(false,true);redraw()})
        ListItem(headlineContent={Text("Delete selection")},modifier=Modifier.clickable{if(!controller.deleteSelectedStrokes())controller.deleteSelectedStroke();redraw();onDismiss()})
        ListItem(headlineContent={Text("Duplicate selection")},modifier=Modifier.clickable{if(!controller.duplicateSelection())controller.duplicateSelectedStroke();redraw();onDismiss()})
        ListItem(headlineContent={Text("Split selected stroke at point 3")},modifier=Modifier.clickable{controller.splitSelectedStroke(2);redraw();onDismiss()})
        ListItem(headlineContent={Text("Subdivide selected stroke")},modifier=Modifier.clickable{controller.subdivideSelectedStroke(1);redraw();onDismiss()})
        ListItem(headlineContent={Text("Close selected stroke")},modifier=Modifier.clickable{controller.closeSelectedStroke();redraw();onDismiss()})
        ListItem(headlineContent={Text("Trim selected stroke at first intersection")},modifier=Modifier.clickable{controller.trimSelectedStrokeToIntersection();redraw();onDismiss()})
        ListItem(headlineContent={Text("Reverse selected stroke (Legacy GP)")},modifier=Modifier.clickable{controller.reverseSelectedStroke();redraw();onDismiss()})
        ListItem(headlineContent={Text("Uniform subdivide selected stroke (Legacy GP)")},modifier=Modifier.clickable{controller.uniformSubdivideSelectedStroke(8);redraw();onDismiss()})
        ListItem(headlineContent={Text("Shrink selected stroke from start (Legacy GP)")},modifier=Modifier.clickable{controller.shrinkSelectedStroke(5f,1);redraw();onDismiss()})
        ListItem(headlineContent={Text("Randomize selected stroke color (Legacy GP)")},modifier=Modifier.clickable{controller.randomizeSelectedStrokeColor();redraw();onDismiss()})
        ListItem(headlineContent={Text("Settings")},modifier=Modifier.clickable{onDismiss();onSettings()})
        Spacer(Modifier.height(20.dp))
    }
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
            Text("Drawing",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            Row(Modifier.fillMaxWidth().padding(horizontal=16.dp),verticalAlignment=Alignment.CenterVertically){
                Text("Stabilization",Modifier.weight(1f))
                Switch(
                    checked=stabilization,
                    onCheckedChange={stabilization=it;controller.setStabilizer(it)}
                )
            }
            Text("Pressure curve " + "%.2f".format(pressureCurve),Modifier.padding(horizontal=16.dp))
            Slider(
                pressureCurve,
                {
                    pressureCurve=it
                    controller.brushes.setPressureCurve(it)
                },
                valueRange=.25f..3f,
                modifier=Modifier.padding(horizontal=16.dp)
            )
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
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal=12.dp)){
                listOf("Linear","Quad","Cubic","Quart","Quint","Sine","Expo","Circ","Back","Bounce").forEachIndexed{type,label->
                    FilterChip(
                        selected=controller.animation.easingType==type,
                        onClick={controller.animation.setEasing(type,controller.animation.easingMode);easingTick++},
                        label={Text(label,fontSize=10.sp)},
                        modifier=Modifier.padding(end=3.dp)
                    )
                }
            }
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal=12.dp)){
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
            Button(onClick={controller.animation.interpolateAt(controller.animation.currentFrame)},enabled=controller.animation.frameNumbers().size>=2,modifier=Modifier.fillMaxWidth().padding(horizontal=16.dp)){Text("Create in-between frame")}
            Text("Editor",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            Button(onClick={controller.view.reset();controller.render()},modifier=Modifier.fillMaxWidth().padding(horizontal=16.dp)){Text("Reset canvas view")}
            Button(onClick={controller.smoothSelectedStroke()},modifier=Modifier.fillMaxWidth().padding(horizontal=16.dp)){Text("Smooth selected stroke")}
        }
    }
}
