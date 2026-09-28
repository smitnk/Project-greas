@file:OptIn(ExperimentalMaterial3Api::class)
package com.smitnk.projectgrease.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.FeatureId
import com.smitnk.projectgrease.editor.FeatureRegistry
import com.smitnk.projectgrease.editor.FeatureState
import com.smitnk.projectgrease.editor.GreaseTool

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
    ToolEntry(GreaseTool.FILL,Icons.Default.FormatColorFill,"Fill",FeatureId.ADVANCED_FILL),
    ToolEntry(GreaseTool.EYEDROPPER,Icons.Default.Colorize,"Pick",FeatureId.STROKE_COLOR),
    ToolEntry(GreaseTool.LINE,Icons.Default.Remove,"Line",FeatureId.LINE),
    ToolEntry(GreaseTool.RECTANGLE,Icons.Default.CropSquare,"Rect",FeatureId.RECTANGLE),
    ToolEntry(GreaseTool.CIRCLE,Icons.Default.RadioButtonUnchecked,"Circle",FeatureId.CIRCLE),
    ToolEntry(GreaseTool.ARC,Icons.Default.Timeline,"Arc",FeatureId.ARC),
    ToolEntry(GreaseTool.POLYLINE,Icons.Default.Timeline,"Polyline",FeatureId.POLYLINE),
    ToolEntry(GreaseTool.PAN,Icons.Default.PanTool,"Pan",FeatureId.PAN),
    ToolEntry(GreaseTool.SCULPT,Icons.Default.AutoFixHigh,"Sculpt",FeatureId.SCULPT)
)
data class GreaseUiState(
    val projectName:String="Project Grease",val canvasFocus:Boolean=false,
    val showTools:Boolean=true,val showTimeline:Boolean=true,val showProperties:Boolean=false
)

@Composable
fun ProjectGreaseApp(controller:EditorController,blenderViewport:@Composable BoxScope.()->Unit){
    var screen by remember{mutableStateOf(Screen.HOME)}
    var name by remember{mutableStateOf("Project Grease")}
    var preset by remember{mutableStateOf(presets[1])}
    var state by remember{mutableStateOf(GreaseUiState())}
    when(screen){
        Screen.HOME->Home({screen=Screen.NEW},{screen=Screen.EDITOR},{screen=Screen.SETTINGS})
        Screen.NEW->NewProject(name,{name=it},preset,{preset=it},controller,{screen=Screen.HOME}){
            controller.document.projectName=name.ifBlank{"Project Grease"}
            controller.animation.setFps(preset.fps)
            state=state.copy(projectName=controller.document.projectName);screen=Screen.EDITOR
        }
        Screen.EDITOR->Editor(controller,state,{state=it},{screen=Screen.HOME},{screen=Screen.SETTINGS},blenderViewport)
        Screen.SETTINGS->Settings{screen=Screen.HOME}
    }
}

@Composable private fun Home(onNew:()->Unit,onOpen:()->Unit,onSettings:()->Unit){
    Scaffold(topBar={
        Row(Modifier.fillMaxWidth().height(64.dp),verticalAlignment=Alignment.CenterVertically){
            IconButton(onClick=onSettings){Icon(Icons.Default.Menu,"Menu")};Spacer(Modifier.weight(1f))
            Text("PROJECT GREASE",fontWeight=FontWeight.Bold,letterSpacing=1.sp);Spacer(Modifier.weight(1f))
            IconButton(onClick={}){Icon(Icons.Default.Search,"Search")}
        }
    },floatingActionButton={FloatingActionButton(onClick=onNew,containerColor=Accent){Icon(Icons.Default.Add,"New")}}){pad->
        Column(Modifier.fillMaxSize().padding(pad).padding(16.dp)){
            Text("Projects",style=MaterialTheme.typography.headlineSmall,fontWeight=FontWeight.Bold);Spacer(Modifier.height(16.dp))
            listOf("Sketch","Animation","Character","Storyboard","Practice").forEach{item->
                Card(Modifier.fillMaxWidth().padding(vertical=5.dp).clickable(onClick=onOpen)){
                    Row(Modifier.padding(14.dp),verticalAlignment=Alignment.CenterVertically){
                        Box(Modifier.size(72.dp).background(Color(0xFFE8E3E5),RoundedCornerShape(8.dp)));Spacer(Modifier.width(14.dp))
                        Column{Text(item,fontWeight=FontWeight.Bold);Text("Project Grease • 12 FPS",color=MaterialTheme.colorScheme.onSurfaceVariant)}
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
    var sheet by remember{mutableStateOf(Sheet.NONE)};var refresh by remember{mutableIntStateOf(0)};fun redraw(){refresh++}
    @Suppress("UNUSED_VARIABLE") val unused=refresh
    if(state.canvasFocus){Box(Modifier.fillMaxSize().background(CanvasBg)){viewport();IconButton(onClick={onState(state.copy(canvasFocus=false))},Modifier.align(Alignment.TopStart).padding(8.dp)){Icon(Icons.Default.CloseFullscreen,"Exit canvas")}};return}
    Column(Modifier.fillMaxSize()){
        Surface(tonalElevation=3.dp){Row(Modifier.fillMaxWidth().height(56.dp),verticalAlignment=Alignment.CenterVertically){
            IconButton(onClick={sheet=Sheet.PROJECT}){Icon(Icons.Default.Menu,"Project")};Text(controller.document.projectName,maxLines=1);Spacer(Modifier.weight(1f))
            IconButton(enabled=controller.history.canUndo,onClick={controller.undo();redraw()}){Icon(Icons.Default.Undo,"Undo")}
            IconButton(enabled=controller.history.canRedo,onClick={controller.redo();redraw()}){Icon(Icons.Default.Redo,"Redo")}
            IconButton(onClick={onState(state.copy(canvasFocus=true))}){Icon(Icons.Default.Fullscreen,"Canvas")}
            IconButton(onClick={sheet=Sheet.MORE}){Icon(Icons.Default.MoreVert,"More")}
        }}
        Row(Modifier.fillMaxWidth().weight(1f)){
            if(state.showTools)ToolRail(controller,{onState(state.copy())},{sheet=Sheet.TOOLS})
            Box(Modifier.weight(1f).fillMaxHeight().background(CanvasBg),contentAlignment=Alignment.Center){viewport()}
            if(state.showProperties)Properties(controller,::redraw)
        }
        if(state.showTimeline)Timeline(controller,::redraw)
    }
    when(sheet){
        Sheet.PROJECT->ProjectSheet({sheet=Sheet.NONE},onSettings,onExit)
        Sheet.TOOLS->ToolsSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.LAYERS->LayersSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.MATERIALS->MaterialsSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.ONION->OnionSheet(controller,{sheet=Sheet.NONE},::redraw)
        Sheet.ADVANCED->AdvancedSheet{sheet=Sheet.NONE}
        Sheet.MORE->MoreSheet(controller,{sheet=Sheet.NONE},onSettings,::redraw)
        Sheet.NONE->Unit
    }
}

@Composable private fun ToolRail(controller:EditorController,onState:()->Unit,onTools:()->Unit){
    Column(Modifier.width(78.dp).fillMaxHeight().background(MaterialTheme.colorScheme.surfaceVariant).verticalScroll(rememberScrollState()),horizontalAlignment=Alignment.CenterHorizontally){
        tools.forEach{entry->val c=FeatureRegistry.capability(entry.feature);val enabled=c.state!=FeatureState.NOT_IMPLEMENTED;val selected=controller.tools.activeTool==entry.tool
            Column(Modifier.fillMaxWidth().clickable(enabled){controller.selectTool(entry.tool);onState()}.padding(5.dp),horizontalAlignment=Alignment.CenterHorizontally){
                Surface(shape=RoundedCornerShape(22.dp),color=if(selected)MaterialTheme.colorScheme.primaryContainer else Color.Transparent){Icon(entry.icon,entry.label,Modifier.padding(9.dp),tint=if(enabled)MaterialTheme.colorScheme.onSurface else Color.Gray)}
                Text(entry.label,fontSize=10.sp)
            }
        };IconButton(onClick=onTools){Icon(Icons.Default.Apps,"Tools")}
    }
}

@Composable private fun Properties(controller:EditorController,redraw:()->Unit){
    var thickness by remember{mutableFloatStateOf(controller.materials.thickness)};var opacity by remember{mutableFloatStateOf(controller.materials.opacity)}
    Column(Modifier.width(210.dp).fillMaxHeight().verticalScroll(rememberScrollState()).padding(10.dp)){
        Text("Brush",fontWeight=FontWeight.Bold);Text("Thickness "+thickness.toInt());Slider(thickness, {thickness=it;controller.materials.setThickness(it);redraw()}, valueRange = .5f..100f)
        Text("Opacity "+(opacity*100).toInt().toString()+"%");Slider(opacity, {opacity=it;controller.materials.setOpacity(it);redraw()}, valueRange = 0f..1f)
        CapabilityRow("Grid",FeatureId.GRID);CapabilityRow("Snapping",FeatureId.SNAPPING);CapabilityRow("Onion skin",FeatureId.ONION_SKIN)
    }
}

@Composable private fun Timeline(controller:EditorController,redraw:()->Unit){
    Surface(tonalElevation=4.dp){Column(Modifier.fillMaxWidth().heightIn(min=120.dp,max=190.dp)){
        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically){
            IconButton(onClick={controller.selectFrame(controller.animation.currentFrame-1);redraw()}){Icon(Icons.Default.SkipPrevious,"Previous")}
            IconButton(onClick={controller.animation.togglePlayback();redraw()}){Icon(if(controller.animation.playing)Icons.Default.Pause else Icons.Default.PlayArrow,"Play")}
            IconButton(onClick={controller.selectFrame(controller.animation.currentFrame+1);redraw()}){Icon(Icons.Default.SkipNext,"Next")}
            Text("Frame "+controller.animation.currentFrame);Spacer(Modifier.width(8.dp));Text(controller.animation.fps.toString()+" FPS");Spacer(Modifier.weight(1f))
            FilterChip(controller.animation.loop,{controller.animation.toggleLoop();redraw()},label={Text("Loop")})
            TextButton(onClick={controller.createFrame(controller.animation.currentFrame+1);redraw()}){Text("+ Frame")}
        }
        Row(Modifier.horizontalScroll(rememberScrollState()).padding(5.dp)){(1..60).forEach{frame->Surface(Modifier.width(54.dp).height(54.dp).padding(2.dp).clickable{controller.selectFrame(frame);redraw()},shape=RoundedCornerShape(8.dp),tonalElevation=if(frame==controller.animation.currentFrame)5.dp else 0.dp){Box(contentAlignment=Alignment.Center){Text(frame.toString())}}}}
    }}
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun ProjectSheet(onDismiss:()->Unit,onSettings:()->Unit,onExit:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Project",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        CapabilityRow("Open project",FeatureId.OPEN_PROJECT);CapabilityRow("Save",FeatureId.SAVE);CapabilityRow("Save as",FeatureId.SAVE_AS);CapabilityRow("Export",FeatureId.EXPORT)
        ListItem(headlineContent={Text("Settings")},modifier=Modifier.clickable{onDismiss();onSettings()});ListItem(headlineContent={Text("Close editor")},modifier=Modifier.clickable{onDismiss();onExit()});Spacer(Modifier.height(20.dp))}
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun ToolsSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Tools",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        tools.forEach{CapabilityRow(it.label,it.feature)};TextButton(onClick={controller.view.toggleGrid();redraw()},Modifier.padding(horizontal=20.dp)){Text("Toggle grid")};Spacer(Modifier.height(20.dp))}
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun LayersSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Layers",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        Text("Layer "+(controller.selectedLayer+1),Modifier.padding(horizontal=20.dp));Button(onClick={controller.createLayer("Layer "+(controller.layerCount()+1));redraw()},Modifier.padding(20.dp)){Text("Add layer")}
        CapabilityRow("Visibility",FeatureId.LAYER_VISIBILITY);CapabilityRow("Locking",FeatureId.LAYER_LOCKING);CapabilityRow("Ordering",FeatureId.LAYER_ORDERING);CapabilityRow("Rename",FeatureId.LAYER_RENAME);Spacer(Modifier.height(20.dp))}
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun MaterialsSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    var thickness by remember{mutableFloatStateOf(controller.materials.thickness)};var opacity by remember{mutableFloatStateOf(controller.materials.opacity)}
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Brush & Materials",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        Text("Thickness "+thickness.toInt(),Modifier.padding(horizontal=20.dp));Slider(thickness, {thickness=it;controller.materials.setThickness(it);redraw()}, valueRange = .5f..100f)
        Text("Opacity "+(opacity*100).toInt().toString()+"%",Modifier.padding(horizontal=20.dp));Slider(opacity, {opacity=it;controller.materials.setOpacity(it);redraw()}, valueRange = 0f..1f)
        CapabilityRow("Fill color",FeatureId.FILL_COLOR);CapabilityRow("Create material",FeatureId.CREATE_MATERIAL);Spacer(Modifier.height(20.dp))}
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun OnionSheet(controller:EditorController,onDismiss:()->Unit,redraw:()->Unit){
    var opacity by remember{mutableFloatStateOf(controller.onion.opacity)}
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Onion Skin",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall);CapabilityRow("Enable",FeatureId.ONION_SKIN)
        Text("Previous "+controller.onion.beforeFrames,Modifier.padding(horizontal=20.dp));Slider(controller.onion.beforeFrames.toFloat(), {controller.onion.setBefore(it.toInt());redraw()}, valueRange = 0f..12f)
        Text("Next "+controller.onion.afterFrames,Modifier.padding(horizontal=20.dp));Slider(controller.onion.afterFrames.toFloat(), {controller.onion.setAfter(it.toInt());redraw()}, valueRange = 0f..12f)
        Text("Opacity "+(opacity*100).toInt().toString()+"%",Modifier.padding(horizontal=20.dp));Slider(opacity, {opacity=it;controller.onion.setOpacity(it);redraw()}, valueRange = 0f..1f);Spacer(Modifier.height(20.dp))}
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun AdvancedSheet(onDismiss:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){Text("Advanced",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        listOf(FeatureId.SCULPT,FeatureId.STABILIZATION,FeatureId.ADVANCED_FILL,FeatureId.STROKE_TEXTURES,FeatureId.FILL_TEXTURES,FeatureId.MODIFIERS,FeatureId.NOISE,FeatureId.DASH,FeatureId.OUTLINE,FeatureId.MULTIFRAME,FeatureId.ADVANCED_ONION_SKIN,FeatureId.VISUAL_EFFECTS).forEach{CapabilityRow(it.name.replace('_',' '),it)}
        Spacer(Modifier.height(20.dp))}
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable private fun MoreSheet(controller:EditorController,onDismiss:()->Unit,onSettings:()->Unit,redraw:()->Unit){
    ModalBottomSheet(onDismissRequest=onDismiss){Text("More / Edit",Modifier.padding(20.dp),style=MaterialTheme.typography.headlineSmall)
        listOf(FeatureId.MOVE,FeatureId.ROTATE,FeatureId.SCALE,FeatureId.MIRROR,FeatureId.DUPLICATE,FeatureId.DELETE,FeatureId.SPLIT,FeatureId.SUBDIVIDE,FeatureId.TRIM,FeatureId.CLOSE).forEach{CapabilityRow(it.name.replace('_',' '),it)}
        ListItem(headlineContent={Text("Delete selected stroke")},modifier=Modifier.clickable{controller.deleteSelectedStroke();redraw();onDismiss()});ListItem(headlineContent={Text("Duplicate selected stroke")},modifier=Modifier.clickable{controller.duplicateSelectedStroke();redraw();onDismiss()})
        ListItem(headlineContent={Text("Settings")},modifier=Modifier.clickable{onDismiss();onSettings()});Spacer(Modifier.height(20.dp))}
}

@Composable private fun CapabilityRow(label:String,id:FeatureId){
    val c=FeatureRegistry.capability(id)
    ListItem(headlineContent={Text(label)},supportingContent={Text(c.state.name.replace('_',' '))},trailingContent={AssistChip(onClick={},label={Text(c.state.name.replace('_',' '),fontSize=9.sp)})})
}

@Composable private fun Settings(onBack:()->Unit){
    var theme by remember{mutableStateOf(ProjectGreaseThemeMode.SYSTEM)}
    Scaffold(topBar={TopAppBar(title={Text("Settings")},navigationIcon={IconButton(onClick=onBack){Icon(Icons.Default.ArrowBack,"Back")}})}){pad->
        Column(Modifier.fillMaxSize().padding(pad).verticalScroll(rememberScrollState())){
            Text("Theme",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            ProjectGreaseThemeMode.entries.forEach{mode->ListItem(headlineContent={Text(mode.name.lowercase().replaceFirstChar{it.uppercase()})},trailingContent={if(theme==mode)Icon(Icons.Default.Check,null,tint=Accent)},modifier=Modifier.clickable{theme=mode})}
            Text("Drawing",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            CapabilityRow("Pressure",FeatureId.PRESSURE);CapabilityRow("Smoothing",FeatureId.SMOOTHING);CapabilityRow("Stabilization",FeatureId.STABILIZATION);CapabilityRow("Grid",FeatureId.GRID);CapabilityRow("Guides",FeatureId.GUIDES);CapabilityRow("Snapping",FeatureId.SNAPPING)
            Text("Animation",Modifier.padding(16.dp),color=Accent,fontWeight=FontWeight.Bold)
            CapabilityRow("Playback",FeatureId.PLAYBACK);CapabilityRow("Loop",FeatureId.LOOP);CapabilityRow("Interpolation",FeatureId.INTERPOLATION)
        }
    }
}
