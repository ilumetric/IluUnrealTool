#include "IluAssetToolset.h"
#include "IluUnrealToolCore.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

FString UIluAssetToolset::ImportFile(const FString& SourceFile, const FString& DestinationPath, const FString& DestinationName, bool bReplaceExisting)
{
	const FString File = FPaths::ConvertRelativePathToFull(SourceFile.Replace(TEXT("\\"), TEXT("/")));
	if (!FPaths::FileExists(File))
	{
		return FString::Printf(TEXT("Error: file not found: %s"), *File);
	}
	FString Folder = DestinationPath;
	Folder.RemoveFromEnd(TEXT("/"));
	if (!FPackageName::IsValidLongPackageName(Folder))
	{
		return FString::Printf(TEXT("Error: DestinationPath must be a Content Browser folder like /Game/Art/Trees, got '%s'."), *DestinationPath);
	}

	// bAutomated — без диалогов настроек; Interchange берёт пайплайн по умолчанию, фабрики — свои настройки по умолчанию.
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = File;
	Task->DestinationPath = Folder;
	Task->DestinationName = DestinationName;
	Task->bReplaceExisting = bReplaceExisting;
	Task->bReplaceExistingSettings = false;
	Task->bAutomated = true;
	Task->bSave = false;
	Task->bAsync = false;
	FAssetToolsModule::GetModule().Get().ImportAssetTasks({Task});

	TArray<FString> Lines;
	for (const UObject* Object : Task->GetObjects())
	{
		if (Object)
		{
			Lines.Add(FString::Printf(TEXT("%s %s"), *Object->GetClass()->GetName(), *Object->GetPathName()));
		}
	}
	if (Lines.IsEmpty())
	{
		for (const FString& Path : Task->ImportedObjectPaths)
		{
			Lines.Add(Path);
		}
	}
	if (Lines.IsEmpty())
	{
		return FString::Printf(TEXT("Error: nothing was imported from %s — check the Output Log (LogInterchange* / LogUsd*)."), *File);
	}
	UE_LOG(LogIluUnrealTool, Log, TEXT("Agent: импорт %s → %s: %d ассет(ов)."), *FPaths::GetCleanFilename(File), *Folder, Lines.Num());
	return FString::Printf(TEXT("OK: imported %d object(s) from %s:\n%s"), Lines.Num(), *FPaths::GetCleanFilename(File),
		*FString::Join(Lines, TEXT("\n")));
}
