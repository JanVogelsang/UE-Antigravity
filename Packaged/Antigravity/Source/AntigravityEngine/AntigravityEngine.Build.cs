// Copyright Antigravity. All Rights Reserved.

using UnrealBuildTool;

public class AntigravityEngine : ModuleRules
{
	public AntigravityEngine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"AntigravityCore"
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
			// Phase 1: AntigravityIgnoreController + AntigravityFileContextTracker use IDirectoryWatcher
			"DirectoryWatcher",
			// Phase 2: AntigravityEnvironmentDetails uses blueprint/level editor APIs
			"LevelEditor",
			"MessageLog",
			"Projects",
		});
	}
}
