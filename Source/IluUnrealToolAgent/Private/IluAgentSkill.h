#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/AgentSkill.h"

#include "IluAgentSkill.generated.h"

/**
 * Agent skill (AgentSkillToolset.ListSkills / GetSkills) explaining when and how to use the IluUnrealTool toolsets.
 * Found by the ToolsetRegistry automatically as a UAgentSkill subclass — no registration needed.
 * Agent skill (AgentSkillToolset.ListSkills / GetSkills): когда и как пользоваться тулсетами IluUnrealTool.
 * ToolsetRegistry находит его сам как наследника UAgentSkill — регистрировать не нужно.
 */
UCLASS()
class UIluUnrealToolAgentSkill : public UAgentSkill
{
	GENERATED_BODY()

public:
	UIluUnrealToolAgentSkill()
	{
		Description = TEXT("IluUnrealTool: batch static mesh fixes (material slots, vertex color, AO bake, UVs, LODs, collision), "
			"material graph helpers (named reroutes, comment boxes, color curve keys), Blueprint helpers (Blueprint enums, "
			"variable type change), PCG Custom HLSL code read/write and placing PCG plugin nodes that the stock toolsets lack, driving an open asset editor "
			"(select graph nodes to switch the PCG / Procedural Vegetation preview, capture the preview viewport, run editor commands, close it).");
		Instructions = TEXT(
			"Toolsets (call through the MCP call_tool with toolset_name and tool_name):\n"
			"- IluUnrealToolAgent.IluMeshToolset — static mesh asset operations. Every tool edits the ASSET (all its instances on all "
			"levels), marks it dirty and returns \"OK: ...\" or \"Error: ...\". There is no undo: confirm with the user before bulk "
			"changes and save the assets afterwards (AssetTools.save_assets).\n"
			"  * InspectVertexColors(Mesh) — read-only; run it first when vertex colors look wrong (per-instance Mesh Paint hides asset colors).\n"
			"  * MergeMaterialSlots(Mesh, KeepSlot) — all geometry into one slot, other slots deleted.\n"
			"  * FillVertexColor(Mesh, Color) — linear RGBA 0..1, e.g. {r:0,g:0,b:0,a:0} to clear.\n"
			"  * BakeAOToVertexAlpha(Mesh, RayCount, MaxDistance) — RayCount 64 and MaxDistance 50 cm are good defaults; RGB is kept.\n"
			"  * KeepOnlyUV0(Mesh) — removes UV1+ and lightmap UV generation.\n"
			"  * RemoveLods(Mesh) — keeps LOD0 only (for Nanite meshes).\n"
			"  * SetCollisionProjectDefault(Mesh) — warns when the mesh has no simple collision.\n"
			"  * BakeActorsToStaticMesh(Actors, AssetPath, bPivotAtBottomCenter) — merges the visible Dynamic Mesh (e.g. Geometry Script "
			"blockouts) and Static Mesh components of level actors into ONE new static mesh asset (an existing asset is not overwritten); save it afterwards.\n"
			"  * ExportActorsToFile(Actors, FilePath, bPivotAtBottomCenter) — the same merge written straight to an absolute .fbx/.obj path; nothing is added to Content.\n"
			"- IluUnrealToolAgent.IluMaterialToolset — material graph helpers on top of MaterialTools.\n"
			"  * LinkNamedRerouteUsage(Usage, DeclarationName) — after creating both reroute nodes with MaterialTools.add_expression; recompile afterwards.\n"
			"  * SetCommentBounds(Comment, X, Y, SizeX, SizeY) — size a Comment created with MaterialTools.add_expression.\n"
			"  * SetLinearColorCurveKeys(Curve, Colors, bStepped) — colors are LINEAR (convert sRGB hex first); bStepped=true gives flat bands "
			"for palette atlases, false a smooth gradient.\n"
			"- IluUnrealToolAgent.IluBlueprintToolset — Blueprint enums and variable types (BlueprintTools.add_variable only knows primitives).\n"
			"  * CreateEnum(AssetPath, Entries) — new Blueprint enum, Entries are display names in order; save it afterwards.\n"
			"  * SetEnumEntries(Enum, Entries) — rename by index, add/remove at the end; renaming keeps stored values, removing resets them.\n"
			"  * GetEnumEntries(Enum) — read-only, works for native enums too.\n"
			"  * SetVariableType(Blueprint, VariableName, Type) — Type is bool/byte/int/int64/float/name/string/text or an enum, struct or "
			"class path. To add an enum variable: BlueprintTools.add_variable as byte, then SetVariableType. Nodes using the variable are "
			"reconstructed (check the graph with read_graph_dsl), values on placed instances are reset — restore them; then "
			"compile_blueprint and save.\n"
			"- IluUnrealToolAgent.IluPCGToolset — HLSL code of PCG Custom HLSL nodes and PCG Compute Source assets (PCGToolset and "
			"ObjectTools cannot read it) and PCG nodes from plugins. For the HLSL tools Target is the node path from PCGToolset.GetGraphStructure (e.g. /Game/F/G.G:CustomHLSL_0), "
			"its settings object or a Compute Source asset.\n"
			"  * GetHLSLSource(Target, bIncludeDeclarations) — read-only; call it with bIncludeDeclarations=true before writing code: "
			"the declarations list the exact accessor functions of every pin (e.g. Landscape_GetHeight, Out_SetPosition) and helpers.\n"
			"  * SetHLSLSource(Target, Source) / SetHLSLFunctions(Target, Functions) — replace the kernel body / the functions block, "
			"undoable, the graph recompiles. Pins and kernel type are set with PCGToolset.UpdateNode, not here. Then run the graph with "
			"PCGToolset.ExecuteGraphInstance to catch shader errors and save the graph (AssetTools.save_assets).\n"
			"  * ListNodeTypes(Filter) / AddNodeByClass(Graph, NodeType, NodeName, NodeTitle, XPosition, YPosition) — the stock "
			"PCGToolset.AddNode only places /Script/PCG nodes; plugin nodes (Mesh Partition Query, Geometry Script, ...) are added "
			"here by class path from ListNodeTypes. The result lists the node path and pin labels; then PCGToolset.UpdateNode, "
			"ConnectNodePins and save the graph as usual.\n"
			"- IluUnrealToolAgent.IluAssetEditorToolset — drive an OPEN asset editor (EditorAppToolset.OpenEditorForAsset first) without "
			"clicking its UI; any editor built on the standard graph editor and viewport (PCG, Procedural Vegetation, Blueprint, Material).\n"
			"  * SelectGraphNodes(Asset, Nodes, bFrame) — replaces the selection like a click; in PCG / Procedural Vegetation editors the "
			"preview shows the selected node (e.g. an Export node to see one plant variation). Names as in PCGToolset.GetGraphStructure "
			"(Export_0) or display titles; unknown names return the node list. The preview regenerates asynchronously: wait a few seconds.\n"
			"  * CapturePreview(Asset, FilePath, Yaw, Pitch, FOV) — PNG of the preview viewport from Yaw/Pitch framed to the content (floor "
			"and sky skipped); the viewport renders it (same light and exposure as the user sees), its camera is restored after, the image "
			"has the viewport size; FOV 0 keeps the viewport's; empty FilePath saves to "
			"<Project>/Saved/IluUnrealTool/Captures/<Asset>.png. The result also has the viewport overlay text (PVE stats: Triangles, "
			"Foliage Instances). Read the PNG to look at it. Typical: Yaw 0..360, Pitch -8, FOV 30.\n"
			"  * ExecuteEditorCommand(Asset, CommandContext, CommandName) — e.g. PCGEditor / ForceGraphRegeneration (Force Regen).\n"
			"  * CloseAssetEditor(Asset) — no save prompt; save first (AssetTools.save_assets).\n"
			"Object references are passed as {\"refPath\": \"/Game/Path/Asset.Asset\"}. For multi-step jobs prefer one "
			"ProgrammaticToolset.execute_tool_script call that loops over assets instead of many single calls.");
	}
};
