using UnrealBuildTool;

/**
 * AI agent section: MCP toolsets (ToolsetRegistry) and the agent skill describing them.
 * Раздел для ИИ-агентов: MCP-тулсеты (ToolsetRegistry) и agent skill с их описанием.
 */
public class IluUnrealToolAgent : ModuleRules
{
	public IluUnrealToolAgent(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"EditorSubsystem",
			"ToolsetRegistry",
			"IluUnrealToolCore",
		});
	}
}
