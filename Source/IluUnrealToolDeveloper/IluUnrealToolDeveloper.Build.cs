using UnrealBuildTool;

/**
 * Developer section: level toolbar tools (Play Here, Place Actor, options panel), Time Of Day wheel, Mesh Ops menu.
 * Раздел разработчика: инструменты тулбара уровня (Play Here, Place Actor, подпанель настроек), колесо Time Of Day, меню Mesh Ops.
 */
public class IluUnrealToolDeveloper : ModuleRules
{
	public IluUnrealToolDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"LevelEditor",
			"ToolMenus",
			"EngineSettings",
			"PropertyEditor",
			"Projects",
			"IluUnrealToolCore",
		});
	}
}
