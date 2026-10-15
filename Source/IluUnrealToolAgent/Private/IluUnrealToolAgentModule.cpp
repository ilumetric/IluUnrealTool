#include "Modules/ModuleManager.h"

// Toolsets are registered by UIluAgentToolsetSubsystem (it waits for the ToolsetRegistry subsystem).
// Тулсеты регистрирует UIluAgentToolsetSubsystem (он дожидается подсистемы ToolsetRegistry).
IMPLEMENT_MODULE(FDefaultModuleImpl, IluUnrealToolAgent)
