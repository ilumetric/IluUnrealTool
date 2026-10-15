#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

/**
 * Static mesh asset operations shared by the MCP toolset (Agent) and the "..." toolbar menu (Developer).
 * Every operation changes the asset itself (all its instances on all levels) and marks it dirty — saving is up to the caller.
 * Result: "OK: ..." on success, otherwise an error description.
 *
 * Операции над ассетом статик-меша, общие для MCP-тулсета (Agent) и меню «...» на тулбаре (Developer).
 * Каждая операция меняет сам ассет (все его экземпляры на всех уровнях) и помечает его изменённым — сохраняет вызывающий.
 * Результат: «OK: ...» при успехе, иначе описание ошибки.
 */
struct ILUUNREALTOOLCORE_API FIluMeshOperations
{
	/** True for a result string reporting success / True, если строка результата сообщает об успехе. */
	static bool IsSuccess(const FString& Result) { return Result.StartsWith(TEXT("OK")); }

	/** All geometry of every LOD into one material slot, other slots removed; the kept slot keeps its name and material.
	 *  Вся геометрия всех LOD — в один слот материала, остальные слоты удаляются; оставшийся сохраняет имя и материал. */
	static FString MergeMaterialSlots(UStaticMesh* Mesh, int32 KeepSlot);

	/** Vertex color of every vertex of every LOD with source geometry set to one value (RGBA, linear 0..1).
	 *  Вертекс-колор всех вершин всех LOD с исходной геометрией — одним значением (RGBA, линейно 0..1). */
	static FString FillVertexColor(UStaticMesh* Mesh, const FLinearColor& Color);

	/** Ambient occlusion baked into the vertex color alpha (1 — open, 0 — occluded), RGB kept; the mesh occludes itself.
	 *  Ambient occlusion в альфу вертекс-колора (1 — открыто, 0 — закрыто), RGB не трогается; препятствие — сам меш. */
	static FString BakeAOToVertexAlpha(UStaticMesh* Mesh, int32 RayCount, float MaxDistance);

	/** Remove every UV channel except UV0, turn off Generate Lightmap UVs, point the lightmap at UV0.
	 *  Удалить все UV-каналы, кроме UV0, выключить Generate Lightmap UVs, перевести лайтмап на UV0. */
	static FString KeepOnlyUV0(UStaticMesh* Mesh);

	/** Read-only vertex color report: source and built LOD0 min/max, level components overriding colors per instance.
	 *  Отчёт по вертекс-колору (только чтение): мин/макс исходника и собранного LOD0, компоненты уровня с цветом экземпляра. */
	static FString InspectVertexColors(UStaticMesh* Mesh);

	/** Collision complexity -> Project Default, physics data rebuilt; reports the simple shape count.
	 *  Сложность коллизии -> Project Default, физданные пересобраны; в отчёте число простых форм. */
	static FString SetCollisionProjectDefault(UStaticMesh* Mesh);

	/** Keep LOD0 only, imported LODs included (closes the open mesh editor first).
	 *  Оставить только LOD0, включая импортные LOD (открытый редактор меша закрывается). */
	static FString RemoveLods(UStaticMesh* Mesh);
};
