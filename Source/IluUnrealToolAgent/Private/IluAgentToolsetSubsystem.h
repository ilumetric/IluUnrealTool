#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "IluAgentToolsetSubsystem.generated.h"

/**
 * Registers the IluUnrealTool toolsets with the ToolsetRegistry (MCP) for the editor lifetime.
 * To add a toolset: create a UToolsetDefinition subclass and add it to GetToolsetClasses().
 * Регистрирует тулсеты IluUnrealTool в ToolsetRegistry (MCP) на время жизни редактора.
 * Новый тулсет: наследник UToolsetDefinition + строка в GetToolsetClasses().
 */
UCLASS()
class UIluAgentToolsetSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	static TArray<UClass*> GetToolsetClasses();
};
