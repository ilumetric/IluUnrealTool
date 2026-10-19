#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "IluMeshToolset.generated.h"

class AActor;
class UStaticMesh;

/**
 * Static mesh helpers for MCP that the stock StaticMeshTools lack.
 * Помощники для статических мешей через MCP — то, чего нет в штатном StaticMeshTools.
 */
UCLASS()
class UIluMeshToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Moves all geometry of every LOD into one material slot and removes the other slots (like Modeling > Attributes >
	 * Edit Materials, but for the whole mesh at once). The kept slot keeps its name and material. Save the asset afterwards.
	 * Переносит всю геометрию всех LOD в один слот материала и удаляет остальные слоты (как Modeling > Attributes >
	 * Edit Materials, но сразу на весь меш). Оставшийся слот сохраняет имя и материал. После — сохранить ассет.
	 * @param Mesh The static mesh asset.
	 * @param KeepSlot Index of the slot to keep (usually 0).
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString MergeMaterialSlots(UStaticMesh* Mesh, int32 KeepSlot);

	/**
	 * Sets the vertex color of every vertex of every LOD with source geometry to one value (RGBA, linear 0..1). Save afterwards.
	 * Заливает вертекс-колор всех вершин всех LOD с исходной геометрией одним значением (RGBA, линейно 0..1). После — сохранить.
	 * @param Mesh The static mesh asset.
	 * @param Color The color to write, e.g. (0,0,0,0).
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString FillVertexColor(UStaticMesh* Mesh, FLinearColor Color);

	/**
	 * Bakes ambient occlusion into the vertex color alpha (1 = open, 0 = fully occluded); RGB is kept. Rays are cast over the
	 * hemisphere around each vertex normal against the mesh itself. Save afterwards.
	 * Запекает ambient occlusion в альфу вертекс-колора (1 — открыто, 0 — полностью закрыто); RGB не трогается. Лучи
	 * по полусфере вокруг нормали каждой вершины, препятствие — сам меш. После — сохранить.
	 * @param Mesh The static mesh asset.
	 * @param RayCount Rays per vertex (16..512, 64 is a good default).
	 * @param MaxDistance Occluders farther than this (cm) do not count.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString BakeAOToVertexAlpha(UStaticMesh* Mesh, int32 RayCount, float MaxDistance);

	/**
	 * Removes every UV channel except UV0 on all LODs with source geometry. Turns off "Generate Lightmap UVs" (otherwise the
	 * build recreates the lightmap channel) and points the lightmap coordinate index at UV0. Nothing else changes. Save afterwards.
	 * Удаляет все UV-каналы, кроме UV0, на всех LOD с исходной геометрией. Выключает «Generate Lightmap UVs» (иначе сборка
	 * создаст лайтмап-канал заново) и ставит индекс лайтмап-UV на 0. Больше ничего не меняет. После — сохранить.
	 * @param Mesh The static mesh asset.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString KeepOnlyUV0(UStaticMesh* Mesh);

	/**
	 * Read-only diagnostics of vertex colors: min/max RGBA in the source geometry and in the built LOD0 color buffer, and how many
	 * components in the editor level override the colors per instance (Mesh Paint on an instance hides the asset colors).
	 * Диагностика вертекс-колора (только чтение): мин/макс RGBA в исходной геометрии и в собранном буфере LOD0, и сколько
	 * компонентов на уровне редактора перекрывают цвет у экземпляра (Mesh Paint по экземпляру скрывает цвет ассета).
	 * @param Mesh The static mesh asset.
	 * @return A one-line report.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString InspectVertexColors(UStaticMesh* Mesh);

	/**
	 * Sets the collision complexity of the mesh to Project Default (instead of e.g. "Use Complex Collision As Simple") and
	 * rebuilds its physics data. Reports how many simple shapes the mesh has (0 = nothing for simple queries). Save afterwards.
	 * Ставит сложность коллизии меша в Project Default (вместо, например, «Use Complex Collision As Simple») и пересобирает
	 * физданные. В отчёте — сколько у меша простых форм (0 — простым запросам не за что зацепиться). После — сохранить.
	 * @param Mesh The static mesh asset.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString SetCollisionProjectDefault(UStaticMesh* Mesh);

	/**
	 * Deletes every LOD except LOD0, imported LODs included (useful for Nanite meshes). Closes the mesh editor if it is open
	 * for this asset. Save afterwards.
	 * Удаляет все LOD, кроме LOD0, включая импортные (удобно для Nanite-мешей). Закрывает открытый редактор этого меша.
	 * После — сохранить.
	 * @param Mesh The static mesh asset.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString RemoveLods(UStaticMesh* Mesh);

	/**
	 * Bakes the visible mesh components of the actors (Dynamic Mesh — e.g. Geometry Script blockouts — and Static Mesh) into ONE
	 * new static mesh asset; one material slot per unique material, collision = complex as simple. An existing asset is not
	 * overwritten. Save the asset afterwards.
	 * Запекает видимые меш-компоненты акторов (Dynamic Mesh — например, блокауты на Geometry Script — и Static Mesh) в ОДИН
	 * новый ассет статик-меша; слот на уникальный материал, коллизия — complex as simple. Существующий ассет не перезаписывается.
	 * После — сохранить ассет.
	 * @param Actors Level actors to merge (e.g. from GetSelectedActors).
	 * @param AssetPath New asset path, e.g. "/Game/Blockout/SM_Blockout_02".
	 * @param bPivotAtBottomCenter True: pivot at the bottom center of the merged bounds; false: pivot at the world origin.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString BakeActorsToStaticMesh(const TArray<AActor*>& Actors, const FString& AssetPath, bool bPivotAtBottomCenter);

	/**
	 * Merges the visible mesh components of the actors (Dynamic Mesh and Static Mesh) into one mesh and writes it straight to
	 * a file on disk: .fbx or .obj by the extension, an existing file is overwritten. Nothing is added to Content.
	 * Сливает видимые меш-компоненты акторов (Dynamic Mesh и Static Mesh) в один меш и пишет его сразу в файл на диске:
	 * .fbx или .obj по расширению, существующий файл перезаписывается. В Content ничего не появляется.
	 * @param Actors Level actors to merge (e.g. from GetSelectedActors).
	 * @param FilePath Absolute file path ending with .fbx or .obj, e.g. "D:/Export/Blockout.fbx".
	 * @param bPivotAtBottomCenter True: pivot at the bottom center of the merged bounds; false: pivot at the world origin.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMeshToolset")
	static FString ExportActorsToFile(const TArray<AActor*>& Actors, const FString& FilePath, bool bPivotAtBottomCenter);
};
