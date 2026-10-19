#include "IluMeshToolset.h"
#include "IluMeshBake.h"
#include "IluMeshOperations.h"

// Тулсет — тонкая обёртка: сами операции живут в Core (FIluMeshOperations) и общие с меню разработчика.

FString UIluMeshToolset::MergeMaterialSlots(UStaticMesh* Mesh, int32 KeepSlot)
{
	return FIluMeshOperations::MergeMaterialSlots(Mesh, KeepSlot);
}

FString UIluMeshToolset::FillVertexColor(UStaticMesh* Mesh, FLinearColor Color)
{
	return FIluMeshOperations::FillVertexColor(Mesh, Color);
}

FString UIluMeshToolset::BakeAOToVertexAlpha(UStaticMesh* Mesh, int32 RayCount, float MaxDistance)
{
	return FIluMeshOperations::BakeAOToVertexAlpha(Mesh, RayCount, MaxDistance);
}

FString UIluMeshToolset::KeepOnlyUV0(UStaticMesh* Mesh)
{
	return FIluMeshOperations::KeepOnlyUV0(Mesh);
}

FString UIluMeshToolset::InspectVertexColors(UStaticMesh* Mesh)
{
	return FIluMeshOperations::InspectVertexColors(Mesh);
}

FString UIluMeshToolset::SetCollisionProjectDefault(UStaticMesh* Mesh)
{
	return FIluMeshOperations::SetCollisionProjectDefault(Mesh);
}

FString UIluMeshToolset::RemoveLods(UStaticMesh* Mesh)
{
	return FIluMeshOperations::RemoveLods(Mesh);
}

FString UIluMeshToolset::BakeActorsToStaticMesh(const TArray<AActor*>& Actors, const FString& AssetPath, bool bPivotAtBottomCenter)
{
	return FIluMeshBake::BakeActorsToStaticMesh(Actors, AssetPath, bPivotAtBottomCenter);
}

FString UIluMeshToolset::ExportActorsToFile(const TArray<AActor*>& Actors, const FString& FilePath, bool bPivotAtBottomCenter)
{
	return FIluMeshBake::ExportActorsToFile(Actors, FilePath, bPivotAtBottomCenter);
}
