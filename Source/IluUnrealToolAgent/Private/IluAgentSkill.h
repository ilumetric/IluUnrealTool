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
		Description = TEXT("IluUnrealTool: batch static mesh fixes (material slots, vertex color, AO bake, UVs, LODs, collision) "
			"and material graph helpers (named reroutes, comment boxes, color curve keys) that the stock toolsets lack.");
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
			"- IluUnrealToolAgent.IluMaterialToolset — material graph helpers on top of MaterialTools.\n"
			"  * LinkNamedRerouteUsage(Usage, DeclarationName) — after creating both reroute nodes with MaterialTools.add_expression; recompile afterwards.\n"
			"  * SetCommentBounds(Comment, X, Y, SizeX, SizeY) — size a Comment created with MaterialTools.add_expression.\n"
			"  * SetLinearColorCurveKeys(Curve, Colors, bStepped) — colors are LINEAR (convert sRGB hex first); bStepped=true gives flat bands "
			"for palette atlases, false a smooth gradient.\n"
			"Object references are passed as {\"refPath\": \"/Game/Path/Asset.Asset\"}. For multi-step jobs prefer one "
			"ProgrammaticToolset.execute_tool_script call that loops over assets instead of many single calls.");
	}
};
