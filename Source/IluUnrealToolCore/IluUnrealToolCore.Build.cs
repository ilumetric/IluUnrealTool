using UnrealBuildTool;

/**
 * Shared layer for the Agent and Developer modules: log category, notifications, static mesh operations.
 * Общий слой для модулей Agent и Developer: лог-категория, уведомления, операции над статик-мешами.
 */
public class IluUnrealToolCore : ModuleRules
{
	public IluUnrealToolCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"MeshDescription",
			"StaticMeshDescription",
			"GeometryCore",
		});
	}
}
