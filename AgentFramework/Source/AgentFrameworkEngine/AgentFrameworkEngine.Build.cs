// Copyright AgentFramework. All Rights Reserved.

using UnrealBuildTool;

public class AgentFrameworkEngine : ModuleRules
{
	public AgentFrameworkEngine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"AgentFrameworkCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"SourceControl",
			"Settings",
			"ContentBrowser",
			"EditorSubsystem",
			"Slate",
			"SlateCore",
			"InputCore",
			// Phase 1: AgentFrameworkIgnoreController + AgentFrameworkFileContextTracker use IDirectoryWatcher
			"DirectoryWatcher",
			// Phase 2: AgentFrameworkEnvironmentDetails uses blueprint/level editor APIs
			"LevelEditor",
			"MessageLog",
			"Projects",
		});
	}
}
