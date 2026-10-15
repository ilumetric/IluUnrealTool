#include "IluMeshOperations.h"
#include "IluUnrealToolCore.h"

#include "Async/ParallelFor.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/ColorVertexBuffer.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshComponentLODInfo.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectIterator.h"

namespace IluMeshOps
{
	/** LOD с собственной геометрией (не сгенерированные редукцией). */
	static TArray<int32> EditableLods(UStaticMesh* Mesh)
	{
		TArray<int32> Lods;
		for (int32 LodIndex = 0; LodIndex < Mesh->GetNumSourceModels(); ++LodIndex)
		{
			if (Mesh->IsMeshDescriptionValid(LodIndex) && Mesh->GetMeshDescription(LodIndex))
			{
				Lods.Add(LodIndex);
			}
		}
		return Lods;
	}

	static void Rebuild(UStaticMesh* Mesh)
	{
		Mesh->Build();
		Mesh->PostEditChange();
		Mesh->MarkPackageDirty();
	}
}

FString FIluMeshOperations::MergeMaterialSlots(UStaticMesh* Mesh, int32 KeepSlot)
{
	if (!Mesh)
	{
		return TEXT("Error: Mesh is null.");
	}

	TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
	if (!Materials.IsValidIndex(KeepSlot))
	{
		return FString::Printf(TEXT("Error: KeepSlot %d is out of range (slots: %d)."), KeepSlot, Materials.Num());
	}
	if (Materials.Num() == 1)
	{
		return TEXT("OK: already a single slot.");
	}

	const FStaticMaterial Kept = Materials[KeepSlot];
	const FName KeptSlotName = Kept.ImportedMaterialSlotName;
	const int32 OldSlotCount = Materials.Num();

	Mesh->Modify();

	int32 LodsChanged = 0;
	for (int32 LodIndex = 0; LodIndex < Mesh->GetNumSourceModels(); ++LodIndex)
	{
		// У LOD, сгенерированных редукцией, своего MeshDescription нет — они пересоберутся из LOD0.
		FMeshDescription* Description = Mesh->IsMeshDescriptionValid(LodIndex) ? Mesh->GetMeshDescription(LodIndex) : nullptr;
		if (!Description || Description->PolygonGroups().Num() == 0)
		{
			continue;
		}

		FPolygonGroupID Target = INDEX_NONE;
		for (const FPolygonGroupID GroupID : Description->PolygonGroups().GetElementIDs())
		{
			Target = GroupID;
			break;
		}

		// Все полигоны — в одну группу, пустые группы удаляем.
		TArray<FPolygonID> Polygons;
		for (const FPolygonID PolygonID : Description->Polygons().GetElementIDs())
		{
			Polygons.Add(PolygonID);
		}
		for (const FPolygonID PolygonID : Polygons)
		{
			if (Description->GetPolygonPolygonGroup(PolygonID) != Target)
			{
				Description->SetPolygonPolygonGroup(PolygonID, Target);
			}
		}

		TArray<FPolygonGroupID> Unused;
		for (const FPolygonGroupID GroupID : Description->PolygonGroups().GetElementIDs())
		{
			if (GroupID != Target)
			{
				Unused.Add(GroupID);
			}
		}
		for (const FPolygonGroupID GroupID : Unused)
		{
			Description->DeletePolygonGroup(GroupID);
		}

		// Сборщик ищет слот по имени группы — имя группы = имя оставшегося слота.
		FStaticMeshAttributes Attributes(*Description);
		Attributes.GetPolygonGroupMaterialSlotNames()[Target] = KeptSlotName;

		FElementIDRemappings Remappings;
		Description->Compact(Remappings);

		Mesh->CommitMeshDescription(LodIndex);
		++LodsChanged;
	}

	if (LodsChanged == 0)
	{
		return TEXT("Error: mesh has no editable source geometry (MeshDescription).");
	}

	Materials.Reset();
	Materials.Add(Kept);

	// Старые привязки «секция → слот» ссылаются на удалённые слоты — сбрасываем, сборка выставит секцию 0 → слот 0.
	Mesh->GetSectionInfoMap().Clear();
	Mesh->GetOriginalSectionInfoMap().Clear();

	IluMeshOps::Rebuild(Mesh);

	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: слоты материалов слиты в «%s» (было %d, LOD с геометрией: %d)."),
		*Mesh->GetName(), *KeptSlotName.ToString(), OldSlotCount, LodsChanged);
	return FString::Printf(TEXT("OK: %d slots -> 1 ('%s'), LODs rebuilt: %d."), OldSlotCount, *KeptSlotName.ToString(), LodsChanged);
}

FString FIluMeshOperations::FillVertexColor(UStaticMesh* Mesh, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return TEXT("Error: Mesh is null.");
	}
	const TArray<int32> Lods = IluMeshOps::EditableLods(Mesh);
	if (Lods.Num() == 0)
	{
		return TEXT("Error: mesh has no editable source geometry (MeshDescription).");
	}

	Mesh->Modify();
	const FVector4f Value(Color.R, Color.G, Color.B, Color.A);
	int32 Instances = 0;
	for (const int32 LodIndex : Lods)
	{
		FMeshDescription* Description = Mesh->GetMeshDescription(LodIndex);
		FStaticMeshAttributes Attributes(*Description);
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		if (!Colors.IsValid())
		{
			continue;
		}
		for (const FVertexInstanceID InstanceID : Description->VertexInstances().GetElementIDs())
		{
			Colors[InstanceID] = Value;
			++Instances;
		}
		Mesh->CommitMeshDescription(LodIndex);
	}
	IluMeshOps::Rebuild(Mesh);

	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: вертекс-колор залит (%s), вершин: %d."), *Mesh->GetName(), *Color.ToString(), Instances);
	return FString::Printf(TEXT("OK: %d vertex instances set to %s."), Instances, *Color.ToString());
}

FString FIluMeshOperations::BakeAOToVertexAlpha(UStaticMesh* Mesh, int32 RayCount, float MaxDistance)
{
	using namespace UE::Geometry;

	if (!Mesh)
	{
		return TEXT("Error: Mesh is null.");
	}
	const TArray<int32> Lods = IluMeshOps::EditableLods(Mesh);
	if (Lods.Num() == 0)
	{
		return TEXT("Error: mesh has no editable source geometry (MeshDescription).");
	}
	RayCount = FMath::Clamp(RayCount, 4, 1024);
	MaxDistance = FMath::Max(MaxDistance, 0.1f);

	// Направления — равномерно по косинусному распределению полусферы вокруг +Z (Фибоначчи), без шума.
	TArray<FVector3d> LocalDirections;
	LocalDirections.Reserve(RayCount);
	const double Golden = 0.5 * (1.0 + FMath::Sqrt(5.0));
	for (int32 i = 0; i < RayCount; ++i)
	{
		const double U1 = (i + 0.5) / RayCount;
		const double U2 = FMath::Frac(i / Golden);
		const double R = FMath::Sqrt(U1);
		const double Phi = 2.0 * UE_DOUBLE_PI * U2;
		LocalDirections.Add(FVector3d(R * FMath::Cos(Phi), R * FMath::Sin(Phi), FMath::Sqrt(FMath::Max(0.0, 1.0 - U1))));
	}

	Mesh->Modify();
	int32 Instances = 0;
	double AOSum = 0.0;
	for (const int32 LodIndex : Lods)
	{
		FMeshDescription* Description = Mesh->GetMeshDescription(LodIndex);
		FStaticMeshAttributes Attributes(*Description);
		TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesConstRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		if (!Colors.IsValid() || !Normals.IsValid())
		{
			continue;
		}

		// Препятствия — «суп» треугольников: у каждого свои вершины, неманифолдная геометрия не теряется.
		FDynamicMesh3 Soup;
		for (const FTriangleID TriangleID : Description->Triangles().GetElementIDs())
		{
			TArrayView<const FVertexID> Corners = Description->GetTriangleVertices(TriangleID);
			const int32 A = Soup.AppendVertex(FVector3d(Positions[Corners[0]]));
			const int32 B = Soup.AppendVertex(FVector3d(Positions[Corners[1]]));
			const int32 C = Soup.AppendVertex(FVector3d(Positions[Corners[2]]));
			Soup.AppendTriangle(A, B, C);
		}
		FDynamicMeshAABBTree3 Tree(&Soup, true);

		// Сдвиг старта луча от поверхности, чтобы не попадать в свой же треугольник.
		const double Bias = FMath::Max(0.02, Soup.GetBounds().MaxDim() * 1e-5);

		TArray<FVertexInstanceID> InstanceIDs;
		for (const FVertexInstanceID InstanceID : Description->VertexInstances().GetElementIDs())
		{
			InstanceIDs.Add(InstanceID);
		}
		TArray<float> AO;
		AO.SetNumZeroed(InstanceIDs.Num());

		ParallelFor(InstanceIDs.Num(), [&](int32 Index)
		{
			const FVertexInstanceID InstanceID = InstanceIDs[Index];
			FVector3d Normal(Normals[InstanceID]);
			if (!Normal.Normalize())
			{
				AO[Index] = 1.f;
				return;
			}
			FVector3d TangentX, TangentY;
			Normal.FindBestAxisVectors(TangentX, TangentY);
			const FVector3d Origin = FVector3d(Positions[Description->GetVertexInstanceVertex(InstanceID)]) + Normal * Bias;

			IMeshSpatial::FQueryOptions Options(MaxDistance);
			int32 Hits = 0;
			for (const FVector3d& Local : LocalDirections)
			{
				const FVector3d Direction = TangentX * Local.X + TangentY * Local.Y + Normal * Local.Z;
				double HitT = 0.0;
				int32 HitTriangle = IndexConstants::InvalidID;
				if (Tree.FindNearestHitTriangle(FRay3d(Origin, Direction), HitT, HitTriangle, Options) && HitT <= MaxDistance)
				{
					++Hits;
				}
			}
			AO[Index] = 1.f - float(Hits) / float(LocalDirections.Num());
		});

		for (int32 Index = 0; Index < InstanceIDs.Num(); ++Index)
		{
			Colors[InstanceIDs[Index]].W = AO[Index];
			AOSum += AO[Index];
		}
		Instances += InstanceIDs.Num();
		Mesh->CommitMeshDescription(LodIndex);
	}
	if (Instances == 0)
	{
		return TEXT("Error: no vertex colors / normals to bake into.");
	}
	IluMeshOps::Rebuild(Mesh);

	const double AverageAO = AOSum / Instances;
	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: AO запечён в альфу (лучей %d, дистанция %.0f см), вершин: %d, среднее AO %.2f."),
		*Mesh->GetName(), RayCount, MaxDistance, Instances, AverageAO);
	return FString::Printf(TEXT("OK: AO baked into alpha for %d vertex instances, average %.2f."), Instances, AverageAO);
}

FString FIluMeshOperations::KeepOnlyUV0(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return TEXT("Error: Mesh is null.");
	}
	const TArray<int32> Lods = IluMeshOps::EditableLods(Mesh);
	if (Lods.Num() == 0)
	{
		return TEXT("Error: mesh has no editable source geometry (MeshDescription).");
	}

	bool bChanged = false;
	int32 MaxChannelsBefore = 0;
	for (const int32 LodIndex : Lods)
	{
		FMeshDescription* Description = Mesh->GetMeshDescription(LodIndex);
		FMeshBuildSettings& BuildSettings = Mesh->GetSourceModel(LodIndex).BuildSettings;
		FStaticMeshAttributes Attributes(*Description);
		const int32 Channels = Attributes.GetVertexInstanceUVs().GetNumChannels();
		MaxChannelsBefore = FMath::Max(MaxChannelsBefore, Channels);

		// Без этого сборка заново сгенерирует лайтмап-канал.
		if (BuildSettings.bGenerateLightmapUVs || Channels > 1)
		{
			if (!bChanged)
			{
				Mesh->Modify();
				bChanged = true;
			}
			BuildSettings.bGenerateLightmapUVs = false;
			BuildSettings.SrcLightmapIndex = 0;
			BuildSettings.DstLightmapIndex = 0;
		}

		if (Channels > 1)
		{
			// С конца, чтобы индексы оставшихся каналов не сдвигались.
			for (int32 Channel = Channels - 1; Channel >= 1; --Channel)
			{
				FStaticMeshOperations::RemoveUVChannel(*Description, Channel);
			}
			Mesh->CommitMeshDescription(LodIndex);
		}
	}

	if (Mesh->GetLightMapCoordinateIndex() != 0)
	{
		if (!bChanged)
		{
			Mesh->Modify();
			bChanged = true;
		}
		Mesh->SetLightMapCoordinateIndex(0);
	}

	if (!bChanged)
	{
		return TEXT("OK: already UV0 only.");
	}
	IluMeshOps::Rebuild(Mesh);

	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: оставлен только UV0 (было каналов: %d), генерация лайтмап-UV выключена."), *Mesh->GetName(), MaxChannelsBefore);
	return FString::Printf(TEXT("OK: UV channels %d -> 1, lightmap UV generation off."), MaxChannelsBefore);
}

FString FIluMeshOperations::InspectVertexColors(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return TEXT("Error: Mesh is null.");
	}

	FString Report = Mesh->GetName() + TEXT(":");

	// Исходная геометрия (то, что меняют наши операции).
	if (FMeshDescription* Description = Mesh->IsMeshDescriptionValid(0) ? Mesh->GetMeshDescription(0) : nullptr)
	{
		FStaticMeshAttributes Attributes(*Description);
		TVertexInstanceAttributesConstRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		// Все четыре компоненты явно: конструктор FVector4f(x) ставит W = 1.
		const float Big = TNumericLimits<float>::Max();
		FVector4f Min(Big, Big, Big, Big), Max(-Big, -Big, -Big, -Big);
		int32 Count = 0;
		if (Colors.IsValid())
		{
			for (const FVertexInstanceID InstanceID : Description->VertexInstances().GetElementIDs())
			{
				const FVector4f C = Colors[InstanceID];
				Min = FVector4f(FMath::Min(Min.X, C.X), FMath::Min(Min.Y, C.Y), FMath::Min(Min.Z, C.Z), FMath::Min(Min.W, C.W));
				Max = FVector4f(FMath::Max(Max.X, C.X), FMath::Max(Max.Y, C.Y), FMath::Max(Max.Z, C.Z), FMath::Max(Max.W, C.W));
				++Count;
			}
		}
		Report += Count > 0
			? FString::Printf(TEXT(" source %d verts, min (%.2f %.2f %.2f %.2f) max (%.2f %.2f %.2f %.2f);"), Count, Min.X, Min.Y, Min.Z, Min.W, Max.X, Max.Y, Max.Z, Max.W)
			: FString(TEXT(" source: no colors;"));
	}

	// Собранный буфер LOD0 (то, что видит рендер без Nanite и fallback).
	const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	if (RenderData && RenderData->LODResources.Num() > 0)
	{
		const FColorVertexBuffer& Buffer = RenderData->LODResources[0].VertexBuffers.ColorVertexBuffer;
		const uint32 Num = Buffer.GetNumVertices();
		if (Num == 0)
		{
			Report += TEXT(" built: no color buffer (= white);");
		}
		else if (!Buffer.GetVertexData())
		{
			Report += FString::Printf(TEXT(" built: %u colors (CPU data released);"), Num);
		}
		else
		{
			FColor Min(255, 255, 255, 255), Max(0, 0, 0, 0);
			for (uint32 i = 0; i < Num; ++i)
			{
				const FColor& C = Buffer.VertexColor(i);
				Min = FColor(FMath::Min(Min.R, C.R), FMath::Min(Min.G, C.G), FMath::Min(Min.B, C.B), FMath::Min(Min.A, C.A));
				Max = FColor(FMath::Max(Max.R, C.R), FMath::Max(Max.G, C.G), FMath::Max(Max.B, C.B), FMath::Max(Max.A, C.A));
			}
			Report += FString::Printf(TEXT(" built %u, min (%d %d %d %d) max (%d %d %d %d);"), Num, Min.R, Min.G, Min.B, Min.A, Max.R, Max.G, Max.B, Max.A);
		}
	}

	// Компоненты уровня редактора с этим мешем и прокрашенным цветом экземпляра.
	int32 Components = 0, Overrides = 0;
	const UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		if (It->GetStaticMesh() != Mesh || It->GetWorld() != EditorWorld || It->IsTemplate())
		{
			continue;
		}
		++Components;
		for (const FStaticMeshComponentLODInfo& LODInfo : It->LODData)
		{
			if (LODInfo.OverrideVertexColors)
			{
				++Overrides;
				break;
			}
		}
	}
	Report += FString::Printf(TEXT(" level components %d, with instance colors %d."), Components, Overrides);
	return Report;
}

FString FIluMeshOperations::SetCollisionProjectDefault(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return TEXT("Error: Mesh is null.");
	}
	UBodySetup* Body = Mesh->GetBodySetup();
	if (!Body)
	{
		return TEXT("Error: mesh has no BodySetup (collision).");
	}

	const int32 SimpleShapes = Body->AggGeom.GetElementCount();
	const FString Note = SimpleShapes == 0 ? TEXT(", WARNING: no simple collision") : TEXT("");
	if (Body->CollisionTraceFlag == CTF_UseDefault)
	{
		return FString::Printf(TEXT("OK: already Project Default (simple shapes: %d%s)."), SimpleShapes, *Note);
	}

	Body->Modify();
	Body->CollisionTraceFlag = CTF_UseDefault;
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
	Mesh->MarkPackageDirty();

	// Компоненты на уровне держат старое физсостояние — пересоздаём.
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		if (It->GetStaticMesh() == Mesh && !It->IsTemplate() && It->IsPhysicsStateCreated())
		{
			It->RecreatePhysicsState();
		}
	}

	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: коллизия → Project Default, простых форм: %d."), *Mesh->GetName(), SimpleShapes);
	if (SimpleShapes == 0)
	{
		UE_LOG(LogIluUnrealTool, Warning, TEXT("%s: нет простой коллизии — с Project Default простые запросы (персонаж) сквозь меш."), *Mesh->GetName());
	}
	return FString::Printf(TEXT("OK: collision -> Project Default (simple shapes: %d%s)."), SimpleShapes, *Note);
}

FString FIluMeshOperations::RemoveLods(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return TEXT("Error: Mesh is null.");
	}
	const int32 OldCount = Mesh->GetNumSourceModels();
	if (OldCount <= 1)
	{
		return TEXT("OK: already LOD0 only.");
	}
	// Открытый редактор меша при смене числа LOD падает — закрываем его.
	if (UAssetEditorSubsystem* AssetEditors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr)
	{
		AssetEditors->CloseAllEditorsForAsset(Mesh);
	}
	Mesh->Modify();
	Mesh->SetNumSourceModels(1);
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	UE_LOG(LogIluUnrealTool, Log, TEXT("%s: LOD удалены, оставлен LOD0 (было %d)."), *Mesh->GetName(), OldCount);
	return FString::Printf(TEXT("OK: LODs %d -> 1."), OldCount);
}
