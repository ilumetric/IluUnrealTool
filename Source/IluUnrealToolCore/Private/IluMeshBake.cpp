#include "IluMeshBake.h"
#include "IluUnrealToolCore.h"

#include "AssetExportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/StaticMesh.h"
#include "Exporters/Exporter.h"
#include "Exporters/FbxExportOption.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

using namespace UE::Geometry;

namespace IluMeshBake
{
	/** Общий меш акторов в пространстве пивота + материалы по индексу MaterialID. */
	struct FMerged
	{
		FDynamicMesh3 Mesh;
		TArray<UMaterialInterface*> Materials;
		int32 Components = 0;
		FVector Pivot = FVector::ZeroVector;
	};

	/** Компоненты, которые видны в игре: редакторные помощники (иконки, гизмо, скрытые в игре) не запекаем. */
	static bool IsBakeable(const UPrimitiveComponent* Component)
	{
		return Component && Component->IsRegistered() && Component->IsVisible() && !Component->bHiddenInGame
			&& !Component->IsEditorOnly() && !Component->IsA<UInstancedStaticMeshComponent>();
	}

	/** Геометрия компонента в его локальном пространстве и материалы по индексу MaterialID. */
	static bool ReadComponentMesh(UPrimitiveComponent* Component, FDynamicMesh3& OutMesh, TArray<UMaterialInterface*>& OutMaterials)
	{
		if (const UDynamicMeshComponent* DynamicComponent = Cast<UDynamicMeshComponent>(Component))
		{
			DynamicComponent->ProcessMesh([&OutMesh](const FDynamicMesh3& Mesh) { OutMesh = Mesh; });
			for (int32 Index = 0; Index < DynamicComponent->GetNumMaterials(); ++Index)
			{
				OutMaterials.Add(DynamicComponent->GetMaterial(Index));
			}
			return OutMesh.TriangleCount() > 0;
		}

		if (const UStaticMeshComponent* StaticComponent = Cast<UStaticMeshComponent>(Component))
		{
			const UStaticMesh* StaticMesh = StaticComponent->GetStaticMesh();
			const FMeshDescription* Description = StaticMesh ? StaticMesh->GetMeshDescription(0) : nullptr;
			if (!Description)
			{
				return false;
			}
			// MaterialID после конвертации = номер группы полигонов; слот материала ищем по имени группы.
			FMeshDescriptionToDynamicMesh Converter;
			Converter.Convert(Description, OutMesh);
			const FStaticMeshConstAttributes Attributes(*Description);
			const TPolygonGroupAttributesConstRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
			for (const FPolygonGroupID GroupID : Description->PolygonGroups().GetElementIDs())
			{
				int32 Slot = StaticMesh->GetMaterialIndex(SlotNames[GroupID]);
				if (Slot == INDEX_NONE)
				{
					Slot = GroupID.GetValue();
				}
				if (OutMaterials.Num() <= GroupID.GetValue())
				{
					OutMaterials.SetNumZeroed(GroupID.GetValue() + 1);
				}
				OutMaterials[GroupID.GetValue()] = StaticComponent->GetMaterial(Slot);
			}
			return OutMesh.TriangleCount() > 0;
		}

		return false;
	}

	static FString Merge(const TArray<AActor*>& Actors, bool bPivotAtBottomCenter, FMerged& Out)
	{
		struct FPart
		{
			FDynamicMesh3 Mesh;
			TArray<UMaterialInterface*> Materials;
		};
		TArray<FPart> Parts;
		int32 NumUVLayers = 1;

		for (AActor* Actor : Actors)
		{
			if (!Actor)
			{
				continue;
			}
			TArray<UPrimitiveComponent*> Components;
			Actor->GetComponents(Components, true);
			for (UPrimitiveComponent* Component : Components)
			{
				if (!IsBakeable(Component))
				{
					continue;
				}
				FPart Part;
				if (!ReadComponentMesh(Component, Part.Mesh, Part.Materials))
				{
					continue;
				}
				if (!Part.Mesh.HasAttributes())
				{
					Part.Mesh.EnableAttributes();
					FMeshNormals::InitializeOverlayToPerVertexNormals(Part.Mesh.Attributes()->PrimaryNormals());
				}
				if (!Part.Mesh.Attributes()->HasMaterialID())
				{
					Part.Mesh.Attributes()->EnableMaterialID();
				}
				// В мировое пространство; отрицательный масштаб — с разворотом треугольников.
				MeshTransforms::ApplyTransform(Part.Mesh, FTransformSRT3d(Component->GetComponentTransform()), true);
				NumUVLayers = FMath::Max(NumUVLayers, Part.Mesh.Attributes()->NumUVLayers());
				Parts.Add(MoveTemp(Part));
			}
		}

		if (Parts.Num() == 0)
		{
			return TEXT("Error: no visible Dynamic Mesh or Static Mesh components with geometry on the given actors.");
		}

		Out.Mesh.EnableAttributes();
		Out.Mesh.Attributes()->SetNumUVLayers(NumUVLayers);
		Out.Mesh.Attributes()->EnableMaterialID();
		FDynamicMeshEditor Editor(&Out.Mesh);

		for (FPart& Part : Parts)
		{
			Part.Mesh.Attributes()->SetNumUVLayers(NumUVLayers);

			// Локальные MaterialID -> общий список уникальных материалов (в список попадают только реально использованные).
			FDynamicMeshMaterialAttribute* MaterialIDs = Part.Mesh.Attributes()->GetMaterialID();
			TMap<int32, int32> LocalToGlobal;
			for (const int32 TriangleID : Part.Mesh.TriangleIndicesItr())
			{
				const int32 LocalID = MaterialIDs->GetValue(TriangleID);
				const int32* Known = LocalToGlobal.Find(LocalID);
				int32 GlobalID;
				if (Known)
				{
					GlobalID = *Known;
				}
				else
				{
					UMaterialInterface* Material = Part.Materials.IsValidIndex(LocalID) ? Part.Materials[LocalID] : nullptr;
					GlobalID = Out.Materials.AddUnique(Material);
					LocalToGlobal.Add(LocalID, GlobalID);
				}
				MaterialIDs->SetValue(TriangleID, GlobalID);
			}

			FMeshIndexMappings Mappings;
			Editor.AppendMesh(&Part.Mesh, Mappings);
			++Out.Components;
		}

		if (bPivotAtBottomCenter)
		{
			const FAxisAlignedBox3d Bounds = Out.Mesh.GetBounds();
			Out.Pivot = FVector(Bounds.Center().X, Bounds.Center().Y, Bounds.Min.Z);
			MeshTransforms::Translate(Out.Mesh, -FVector3d(Out.Pivot));
		}
		return TEXT("OK");
	}

	/** Статик-меш из общего меша: слот на материал, без Nanite и генерации лайтмап-UV. */
	static UStaticMesh* CreateStaticMesh(UObject* Outer, FName Name, EObjectFlags Flags, const FMerged& Merged, bool bComplexCollision)
	{
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Outer, Name, Flags);
		StaticMesh->SetNumSourceModels(1);
		FMeshBuildSettings& BuildSettings = StaticMesh->GetSourceModel(0).BuildSettings;
		BuildSettings.bRecomputeNormals = false;
		BuildSettings.bRecomputeTangents = true;
		BuildSettings.bGenerateLightmapUVs = false;

		TArray<FStaticMaterial> StaticMaterials;
		TSet<FName> UsedNames;
		const int32 NumSlots = FMath::Max(1, Merged.Materials.Num());
		for (int32 Index = 0; Index < NumSlots; ++Index)
		{
			UMaterialInterface* Material = Merged.Materials.IsValidIndex(Index) ? Merged.Materials[Index] : nullptr;
			if (!Material)
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
			FName SlotName = Material->GetFName();
			for (int32 Suffix = 1; UsedNames.Contains(SlotName); ++Suffix)
			{
				SlotName = FName(*FString::Printf(TEXT("%s_%d"), *Material->GetName(), Suffix));
			}
			UsedNames.Add(SlotName);
			FStaticMaterial StaticMaterial(Material, SlotName, SlotName);
			StaticMaterial.UVChannelData = FMeshUVChannelInfo(1.f);
			StaticMaterials.Add(StaticMaterial);
		}
		StaticMesh->SetStaticMaterials(StaticMaterials);

		FMeshDescription* Description = StaticMesh->CreateMeshDescription(0);
		FDynamicMeshToMeshDescription Converter;
		Converter.Convert(&Merged.Mesh, *Description, false);

		// Группа полигонов N = MaterialID N = слот N.
		FStaticMeshAttributes Attributes(*Description);
		TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
		TArray<int32> SectionMaterials;
		for (const FPolygonGroupID GroupID : Description->PolygonGroups().GetElementIDs())
		{
			const int32 MaterialIndex = StaticMaterials.IsValidIndex(GroupID.GetValue()) ? GroupID.GetValue() : 0;
			SlotNames[GroupID] = StaticMaterials[MaterialIndex].MaterialSlotName;
			SectionMaterials.Add(MaterialIndex);
		}
		StaticMesh->CommitMeshDescription(0);

		FMeshSectionInfoMap SectionInfoMap;
		for (int32 SectionIndex = 0; SectionIndex < SectionMaterials.Num(); ++SectionIndex)
		{
			SectionInfoMap.Set(0, SectionIndex, FMeshSectionInfo(SectionMaterials[SectionIndex]));
		}
		StaticMesh->GetSectionInfoMap().CopyFrom(SectionInfoMap);
		StaticMesh->GetOriginalSectionInfoMap().CopyFrom(SectionInfoMap);

		StaticMesh->GetNaniteSettings().bEnabled = false;
		StaticMesh->SetLightMapCoordinateIndex(0);
		if (bComplexCollision)
		{
			StaticMesh->CreateBodySetup();
			StaticMesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
		}
		StaticMesh->Build(true);
		return StaticMesh;
	}

	static FString Summary(const FMerged& Merged)
	{
		return FString::Printf(TEXT("components %d, triangles %d, materials %d, pivot (%.0f, %.0f, %.0f)"),
			Merged.Components, Merged.Mesh.TriangleCount(), FMath::Max(1, Merged.Materials.Num()),
			Merged.Pivot.X, Merged.Pivot.Y, Merged.Pivot.Z);
	}
}

FString FIluMeshBake::BakeActorsToStaticMesh(const TArray<AActor*>& Actors, const FString& AssetPath, bool bPivotAtBottomCenter)
{
	FText Reason;
	if (!FPackageName::IsValidLongPackageName(AssetPath, false, &Reason))
	{
		return FString::Printf(TEXT("Error: invalid asset path '%s': %s"), *AssetPath, *Reason.ToString());
	}
	if (FPackageName::DoesPackageExist(AssetPath) || FindPackage(nullptr, *AssetPath))
	{
		return FString::Printf(TEXT("Error: asset '%s' already exists, pick another name."), *AssetPath);
	}

	IluMeshBake::FMerged Merged;
	const FString MergeResult = IluMeshBake::Merge(Actors, bPivotAtBottomCenter, Merged);
	if (!MergeResult.StartsWith(TEXT("OK")))
	{
		return MergeResult;
	}

	UPackage* Package = CreatePackage(*AssetPath);
	Package->FullyLoad();
	const FName AssetName(*FPackageName::GetLongPackageAssetName(AssetPath));
	UStaticMesh* StaticMesh = IluMeshBake::CreateStaticMesh(Package, AssetName, RF_Public | RF_Standalone, Merged, true);
	FAssetRegistryModule::AssetCreated(StaticMesh);
	StaticMesh->MarkPackageDirty();

	const FString Info = IluMeshBake::Summary(Merged);
	UE_LOG(LogIluUnrealTool, Log, TEXT("Запечён статик-меш %s: %s."), *StaticMesh->GetPathName(), *Info);
	return FString::Printf(TEXT("OK: %s created (%s). Save it."), *StaticMesh->GetPathName(), *Info);
}

FString FIluMeshBake::ExportActorsToFile(const TArray<AActor*>& Actors, const FString& FilePath, bool bPivotAtBottomCenter)
{
	const FString Extension = FPaths::GetExtension(FilePath).ToLower();
	if (Extension != TEXT("fbx") && Extension != TEXT("obj"))
	{
		return FString::Printf(TEXT("Error: '%s' must end with .fbx or .obj."), *FilePath);
	}
	if (FPaths::IsRelative(FilePath))
	{
		return FString::Printf(TEXT("Error: '%s' must be an absolute path."), *FilePath);
	}

	IluMeshBake::FMerged Merged;
	const FString MergeResult = IluMeshBake::Merge(Actors, bPivotAtBottomCenter, Merged);
	if (!MergeResult.StartsWith(TEXT("OK")))
	{
		return MergeResult;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);

	// Временный меш вне Content: имя объекта станет именем меша в файле.
	const FName MeshName = MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(), FName(*FPaths::GetBaseFilename(FilePath)));
	UStaticMesh* StaticMesh = IluMeshBake::CreateStaticMesh(GetTransientPackage(), MeshName, RF_Transient, Merged, false);

	UAssetExportTask* Task = NewObject<UAssetExportTask>();
	Task->Object = StaticMesh;
	Task->Filename = FilePath;
	Task->bSelected = false;
	Task->bReplaceIdentical = true;
	Task->bPrompt = false;
	Task->bAutomated = true;
	if (Extension == TEXT("fbx"))
	{
		UFbxExportOption* Options = NewObject<UFbxExportOption>();
		Options->Collision = false;
		Options->LevelOfDetail = false;
		Task->Options = Options;
	}

	const bool bExported = UExporter::RunAssetExportTask(Task);
	StaticMesh->MarkAsGarbage();

	const int64 FileSize = IFileManager::Get().FileSize(*FilePath);
	if (!bExported || FileSize <= 0)
	{
		return FString::Printf(TEXT("Error: export to '%s' failed (see the output log)."), *FilePath);
	}

	const FString Info = IluMeshBake::Summary(Merged);
	UE_LOG(LogIluUnrealTool, Log, TEXT("Экспортирован общий меш акторов в %s: %s."), *FilePath, *Info);
	return FString::Printf(TEXT("OK: %s written, %lld bytes (%s)."), *FilePath, FileSize, *Info);
}
