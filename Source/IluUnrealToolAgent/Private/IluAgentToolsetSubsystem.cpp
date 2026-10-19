#include "IluAgentToolsetSubsystem.h"
#include "IluBlueprintToolset.h"
#include "IluMaterialToolset.h"
#include "IluMeshToolset.h"
#include "IluUnrealToolCore.h"

#include "Subsystems/SubsystemCollection.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

TArray<UClass*> UIluAgentToolsetSubsystem::GetToolsetClasses()
{
	return {
		UIluMeshToolset::StaticClass(),
		UIluMaterialToolset::StaticClass(),
		UIluBlueprintToolset::StaticClass(),
	};
}

void UIluAgentToolsetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Без готового реестра RegisterToolsetClass молча ничего не делает — поднимаем его раньше себя.
	Collection.InitializeDependency<UToolsetRegistrySubsystem>();
	for (UClass* ToolsetClass : GetToolsetClasses())
	{
		UToolsetRegistry::RegisterToolsetClass(ToolsetClass);
	}
	UE_LOG(LogIluUnrealTool, Log, TEXT("Agent: MCP-тулсетов зарегистрировано: %d."), GetToolsetClasses().Num());
}

void UIluAgentToolsetSubsystem::Deinitialize()
{
	for (UClass* ToolsetClass : GetToolsetClasses())
	{
		UToolsetRegistry::UnregisterToolsetClass(ToolsetClass);
	}
	Super::Deinitialize();
}
