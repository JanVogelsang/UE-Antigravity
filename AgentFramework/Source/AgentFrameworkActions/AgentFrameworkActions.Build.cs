// Copyright AgentFramework. All Rights Reserved.

using UnrealBuildTool;

public class AgentFrameworkActions : ModuleRules
{
	public AgentFrameworkActions(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"AgentFrameworkCore",
			"AgentFrameworkEngine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"MessageLog",
			"AssetTools",
			"AssetRegistry",
			"Kismet",
			"KismetCompiler",
			"BlueprintGraph",
			"GraphEditor",
			"MaterialEditor",
			"SourceControl",
			"DesktopPlatform",
			"Settings",
			"EditorScriptingUtilities",
			"MeshDescription",
			"StaticMeshDescription",
			"RenderCore",
			"RHI",
			"NavigationSystem",
			"AIModule",
			"LevelEditor",
			"MainFrame",
			"LiveCoding",
			"HTTPServer",
			"Projects",
			// UMG / Widget Blueprint authoring
			"UMG",
			"UMGEditor",
			// Animation Blueprint authoring
			"AnimationBlueprintEditor",
			"AnimGraph",
			"AnimGraphRuntime",
			// Enhanced Input asset authoring
			"EnhancedInput",
			"InputBlueprintNodes",
			"InputCore",
			// Viewport capture (multimodal vision)
			"ImageWrapper",
			"Slate",
			"SlateCore",
			"WebBrowser",
			// Behavior Tree / AI
			"GameplayTasks",
			// Sequencer / Cinematics
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks",
			"MovieSceneTools",
			// Python scripting (conditional)
			"PythonScriptPlugin",
			// Data Validation (UEditorValidatorSubsystem)
			"DataValidation",
			// Gameplay Ability System
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTagsEditor",
			"GameplayAbilitiesEditor",
			// Niagara Support
			"Niagara",
			"NiagaraCore",
			"NiagaraEditor"
		});

		// Conditionally add Python support
		if (Target.bBuildWithEditorOnlyData)
		{
			PrivateDefinitions.Add("WITH_PYTHON=1");
		}
	}
}
