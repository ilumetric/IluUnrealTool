#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * Bakes the visible mesh components of actors (Dynamic Mesh — e.g. Geometry Script blockouts — and Static Mesh) into one
 * merged static mesh: a Content asset or a file on disk. Shared by the MCP toolset (Agent) and the "..." toolbar menu (Developer).
 * Materials are merged by identity: one slot per unique material. Result: "OK: ..." on success, otherwise an error description.
 *
 * Запекание видимых меш-компонентов акторов (Dynamic Mesh — например, блокауты на Geometry Script — и Static Mesh) в один
 * общий статик-меш: ассет в Content или файл на диске. Общее для MCP-тулсета (Agent) и меню «...» на тулбаре (Developer).
 * Материалы сливаются по совпадению: один слот на уникальный материал. Результат: «OK: ...» при успехе, иначе описание ошибки.
 */
struct ILUUNREALTOOLCORE_API FIluMeshBake
{
	/** New static mesh asset at AssetPath (/Game/.../SM_Name); an existing asset is not overwritten. Saving is up to the caller.
	 *  bPivotAtBottomCenter: pivot at the bottom center of the merged bounds, otherwise at the world origin.
	 *  Новый ассет статик-меша по пути AssetPath (/Game/.../SM_Name); существующий не перезаписывается. Сохраняет вызывающий.
	 *  bPivotAtBottomCenter: пивот в центре низа общих габаритов, иначе в начале координат мира. */
	static FString BakeActorsToStaticMesh(const TArray<AActor*>& Actors, const FString& AssetPath, bool bPivotAtBottomCenter);

	/** Merged mesh written straight to a file (.fbx or .obj by extension, overwritten), nothing is added to Content.
	 *  Общий меш сразу в файл (.fbx или .obj по расширению, перезаписывается), в Content ничего не появляется. */
	static FString ExportActorsToFile(const TArray<AActor*>& Actors, const FString& FilePath, bool bPivotAtBottomCenter);
};
