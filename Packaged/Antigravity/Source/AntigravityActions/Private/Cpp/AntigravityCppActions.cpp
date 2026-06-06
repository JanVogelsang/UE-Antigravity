// Copyright 2026 Antigravity. All Rights Reserved.

#include "Cpp/AntigravityCppActions.h"
#include "AntigravityCoreModule.h"
#include "AntigravitySettings.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "DesktopPlatformModule.h"
#include "Interfaces/IMainFrameModule.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

FAntigravityCppActions::FAntigravityCppActions() {}
FAntigravityCppActions::~FAntigravityCppActions() {}

FName FAntigravityCppActions::GetActionName() const { return FName(TEXT("Cpp")); }
FText FAntigravityCppActions::GetDisplayName() const { return FText::FromString(TEXT("C++ Generation Actions")); }
EAntigravityActionCategory FAntigravityCppActions::GetCategory() const { return EAntigravityActionCategory::Cpp; }
EAntigravityRiskLevel FAntigravityCppActions::GetDefaultRiskLevel() const { return EAntigravityRiskLevel::High; }
bool FAntigravityCppActions::CanUndo() const { return false; }
bool FAntigravityCppActions::UndoAction() { return false; }

TArray<FString> FAntigravityCppActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_cpp_class"),
		TEXT("modify_cpp_file"),
		TEXT("trigger_compile"),
		TEXT("regenerate_project_files"),
		TEXT("macro_create_cpp_class")
	};
}

bool FAntigravityCppActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	if (ToolName == TEXT("create_cpp_class"))
	{
		if (!Params->HasField(TEXT("class_name")) || !Params->HasField(TEXT("header_code")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for create_cpp_class: class_name, header_code"));
			return false;
		}
	}
	else if (ToolName == TEXT("modify_cpp_file"))
	{
		if (!Params->HasField(TEXT("file_path")) || !Params->HasField(TEXT("content")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for modify_cpp_file: file_path, content"));
			return false;
		}
	}
	else if (ToolName == TEXT("macro_create_cpp_class"))
	{
		if (!Params->HasField(TEXT("class_name")) || !Params->HasField(TEXT("parent_class")) || !Params->HasField(TEXT("module_name")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for macro_create_cpp_class: class_name, parent_class, module_name"));
			return false;
		}
	}

	// For code gen, validate that the code doesn't contain dangerous patterns
	FString HeaderCode, CppCode, Content;
	if (Params->TryGetStringField(TEXT("header_code"), HeaderCode))
	{
		TArray<FString> Violations;
		if (!ValidateCodeSafety(HeaderCode, Violations))
		{
			for (const FString& V : Violations) OutErrors.Add(FString::Printf(TEXT("Header: %s"), *V));
			return false;
		}
	}
	if (Params->TryGetStringField(TEXT("cpp_code"), CppCode))
	{
		TArray<FString> Violations;
		if (!ValidateCodeSafety(CppCode, Violations))
		{
			for (const FString& V : Violations) OutErrors.Add(FString::Printf(TEXT("Cpp: %s"), *V));
			return false;
		}
	}
	if (Params->TryGetStringField(TEXT("content"), Content))
	{
		TArray<FString> Violations;
		if (!ValidateCodeSafety(Content, Violations))
		{
			for (const FString& V : Violations) OutErrors.Add(FString::Printf(TEXT("Content: %s"), *V));
			return false;
		}
	}
	return true;
}

FAntigravityActionPlan FAntigravityCppActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionPlan Plan;
	Plan.Summary = TEXT("C++ code generation (HIGH RISK — requires compilation)");
	Plan.MaxRiskLevel = EAntigravityRiskLevel::High;

	FAntigravityAction Action;
	Action.Description = Plan.Summary;
	Action.Category = EAntigravityActionCategory::Cpp;
	Action.RiskLevel = EAntigravityRiskLevel::High;

	FString ClassName;
	if (Params->TryGetStringField(TEXT("class_name"), ClassName))
	{
		FString HeaderPath = FPaths::Combine(FPaths::GameSourceDir(), FApp::GetProjectName(), TEXT("Public"), ClassName + TEXT(".h"));
		FString CppPath = FPaths::Combine(FPaths::GameSourceDir(), FApp::GetProjectName(), TEXT("Private"), ClassName + TEXT(".cpp"));
		Action.AffectedPaths.Add(HeaderPath);
		Action.AffectedPaths.Add(CppPath);
	}

	Plan.Actions.Add(Action);
	return Plan;
}

FAntigravityActionResult FAntigravityCppActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	if (Params->TryGetStringField(TEXT("_tool_name"), ToolName) || Params->TryGetStringField(TEXT("tool_name"), ToolName))
	{
		if (ToolName == TEXT("macro_create_cpp_class"))
		{
			return ExecuteMacroCreateCppClass(Params, Result);
		}
	}

	if (Params->HasField(TEXT("class_name")) && Params->HasField(TEXT("header_code")))
	{
		return ExecuteCreateCppClass(Params, Result);
	}
	else if (Params->HasField(TEXT("file_path")) && Params->HasField(TEXT("content")))
	{
		return ExecuteModifyCppFile(Params, Result);
	}
	else if (Params->HasField(TEXT("compile")) && Params->GetBoolField(TEXT("compile")))
	{
		return ExecuteTriggerCompile(Result);
	}
	else if (Params->HasField(TEXT("regenerate_project")))
	{
		return ExecuteRegenerateProjectFiles(Result);
	}

	Result.Errors.Add(TEXT("Could not determine C++ action from parameters."));
	return Result;
}

FAntigravityActionResult FAntigravityCppActions::ExecuteCreateCppClass(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString ClassName = Params->GetStringField(TEXT("class_name"));
	FString HeaderCode = Params->GetStringField(TEXT("header_code"));
	FString CppCode;
	Params->TryGetStringField(TEXT("cpp_code"), CppCode);

	// Validate code safety
	TArray<FString> Violations;
	if (!ValidateCodeSafety(HeaderCode, Violations) || (!CppCode.IsEmpty() && !ValidateCodeSafety(CppCode, Violations)))
	{
		Result.Errors.Add(TEXT("Generated code failed safety validation:"));
		Result.Errors.Append(Violations);
		return Result;
	}

	// CRITICAL: Detect structural header changes that Live Coding CANNOT handle.
	// Live Coding is great for .cpp implementation changes, but CANNOT handle:
	// - New UCLASS / USTRUCT / UENUM macros in headers
	// - New UPROPERTY / UFUNCTION declarations
	// - Changes to the UObject reflection system
	// Attempting to Live Code these will crash or fail silently.
	bool bHasStructuralHeaderChanges = false;
	if (HeaderCode.Contains(TEXT("UCLASS(")) ||
		HeaderCode.Contains(TEXT("USTRUCT(")) ||
		HeaderCode.Contains(TEXT("UENUM(")) ||
		HeaderCode.Contains(TEXT("GENERATED_BODY()")) ||
		HeaderCode.Contains(TEXT("GENERATED_UCLASS_BODY()")) ||
		HeaderCode.Contains(TEXT("GENERATED_USTRUCT_BODY()")))
	{
		bHasStructuralHeaderChanges = true;
		Result.Warnings.Add(TEXT("⚠ STRUCTURAL HEADER CHANGE DETECTED: This file contains UCLASS/USTRUCT/UENUM macros. "
			"Live Coding CANNOT compile these changes. You MUST close and restart the Unreal Editor, "
			"then compile from your IDE (Visual Studio / Rider) for these changes to take effect."));
	}

	// Determine target paths
	FString ModuleName = FApp::GetProjectName();
	FString PublicDir = FPaths::Combine(FPaths::GameSourceDir(), ModuleName, TEXT("Public"));
	FString PrivateDir = FPaths::Combine(FPaths::GameSourceDir(), ModuleName, TEXT("Private"));

	// Allow custom subdirectory
	FString SubDir;
	if (Params->TryGetStringField(TEXT("subdirectory"), SubDir))
	{
		PublicDir = FPaths::Combine(PublicDir, SubDir);
		PrivateDir = FPaths::Combine(PrivateDir, SubDir);
	}

	FString HeaderPath = FPaths::Combine(PublicDir, ClassName + TEXT(".h"));
	FString CppPath = FPaths::Combine(PrivateDir, ClassName + TEXT(".cpp"));

	// Ensure directories exist
	IFileManager::Get().MakeDirectory(*PublicDir, true);
	IFileManager::Get().MakeDirectory(*PrivateDir, true);

	// Write header
	if (!WriteFileWithBackup(HeaderPath, HeaderCode))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to write header: %s"), *HeaderPath));
		return Result;
	}
	Result.ModifiedPaths.Add(HeaderPath);

	// Write cpp (if provided)
	if (!CppCode.IsEmpty())
	{
		if (!WriteFileWithBackup(CppPath, CppCode))
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to write cpp: %s"), *CppPath));
			return Result;
		}
		Result.ModifiedPaths.Add(CppPath);
	}

	UE_LOG(LogAntigravity, Log, TEXT("CppActions: Created class %s at %s"), *ClassName, *HeaderPath);

	// Auto-trigger compilation if requested — but BLOCK if structural header changes detected
	bool bAutoCompile = false;
	Params->TryGetBoolField(TEXT("auto_compile"), bAutoCompile);
	if (bAutoCompile)
	{
		if (bHasStructuralHeaderChanges)
		{
			Result.Warnings.Add(TEXT("Auto-compile SKIPPED: Structural header changes (UCLASS/USTRUCT/UENUM) detected. "
				"Live Coding cannot handle these. Restart the editor and compile from your IDE."));
		}
		else
		{
			FAntigravityActionResult CompileResult;
			ExecuteTriggerCompile(CompileResult);
			if (!CompileResult.bSuccess)
			{
				Result.Warnings.Add(TEXT("Code written but compilation failed. Check build output."));
				Result.Warnings.Append(CompileResult.Errors);
			}
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created C++ class '%s' (%s, %s)"),
		*ClassName, *HeaderPath, CppCode.IsEmpty() ? TEXT("header only") : *CppPath);
	return Result;
}

FAntigravityActionResult FAntigravityCppActions::ExecuteModifyCppFile(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString FilePath = Params->GetStringField(TEXT("file_path"));
	FString Content = Params->GetStringField(TEXT("content"));

	FPaths::NormalizeFilename(FilePath);

	// Validate the path is within project source
	if (!FilePath.StartsWith(FPaths::GameSourceDir()) && !FilePath.StartsWith(FPaths::ProjectDir()))
	{
		Result.Errors.Add(FString::Printf(TEXT("Path not allowed: %s (must be within project source)"), *FilePath));
		return Result;
	}

	// Validate code safety
	TArray<FString> Violations;
	if (!ValidateCodeSafety(Content, Violations))
	{
		Result.Errors.Add(TEXT("Code failed safety validation:"));
		Result.Errors.Append(Violations);
		return Result;
	}

	if (!WriteFileWithBackup(FilePath, Content))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to write file: %s"), *FilePath));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Modified file: %s"), *FilePath);
	Result.ModifiedPaths.Add(FilePath);
	return Result;
}

FAntigravityActionResult FAntigravityCppActions::ExecuteTriggerCompile(FAntigravityActionResult& Result)
{
#if WITH_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (LiveCoding && LiveCoding->IsEnabledForSession())
	{
		if (LiveCoding->IsCompiling())
		{
			Result.bSuccess = false;
			Result.Errors.Add(TEXT("Live Coding compilation is already in progress. Please wait for it to complete."));
			Result.ResultMessage = TEXT("Compilation blocked — compile already in progress.");
			return Result;
		}

		UE_LOG(LogAntigravity, Log, TEXT("CppActions: Triggering Live Coding compilation..."));
		LiveCoding->Compile();
		Result.bSuccess = true;
		Result.ResultMessage = TEXT("Live Coding compilation triggered. Check editor status bar for progress.");
		return Result;
	}
#endif

	Result.bSuccess = false;
	Result.Warnings.Add(TEXT("Live Coding not available. Please compile manually from your IDE or enable Live Coding in Editor Preferences."));
	Result.ResultMessage = TEXT("Live Coding not available — manual compilation required.");
	return Result;
}

FAntigravityActionResult FAntigravityCppActions::ExecuteRegenerateProjectFiles(FAntigravityActionResult& Result)
{
	FString ProjectPath = FPaths::GetProjectFilePath();

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		bool bSuccess = DesktopPlatform->GenerateProjectFiles(
			FPaths::RootDir(),
			ProjectPath,
			GWarn
		);

		if (bSuccess)
		{
			Result.bSuccess = true;
			Result.ResultMessage = TEXT("Project files regenerated successfully.");
		}
		else
		{
			Result.Errors.Add(TEXT("Failed to regenerate project files."));
		}
	}
	else
	{
		Result.Errors.Add(TEXT("Desktop platform module not available."));
	}

	return Result;
}

bool FAntigravityCppActions::ValidateCodeSafety(const FString& Code, TArray<FString>& OutViolations) const
{
	static const TArray<FString> DenyPatterns = {
		TEXT("system("),
		TEXT("exec("),
		TEXT("popen("),
		TEXT("ShellExecute"),
		TEXT("CreateProcess"),
		TEXT("WinExec"),
		TEXT("DeleteFileA"),
		TEXT("DeleteFileW"),
		TEXT("RemoveDirectoryA"),
		TEXT("RemoveDirectoryW"),
		TEXT("FPlatformProcess::CreateProc"),  // Only allowed via our own BuildActions
		TEXT("#include <windows.h>"),
		TEXT("#include <stdlib.h>"),
		TEXT("__asm"),
		TEXT("asm("),
		TEXT("__declspec(dllexport)"),
	};

	bool bSafe = true;
	for (const FString& Pattern : DenyPatterns)
	{
		if (Code.Contains(Pattern))
		{
			OutViolations.Add(FString::Printf(TEXT("Denied pattern: '%s'"), *Pattern));
			bSafe = false;
		}
	}

	return bSafe;
}

bool FAntigravityCppActions::WriteFileWithBackup(const FString& FilePath, const FString& Content)
{
	// Backup existing file if it exists
	if (FPaths::FileExists(FilePath))
	{
		FString BackupDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Antigravity"), TEXT("Backups"),
			FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")));
		IFileManager::Get().MakeDirectory(*BackupDir, true);

		FString BackupPath = FPaths::Combine(BackupDir, FPaths::GetCleanFilename(FilePath));
		IFileManager::Get().Copy(*BackupPath, *FilePath);
		UE_LOG(LogAntigravity, Log, TEXT("CppActions: Backed up %s -> %s"), *FilePath, *BackupPath);
	}

	return FFileHelper::SaveStringToFile(Content, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FAntigravityActionResult FAntigravityCppActions::ExecuteMacroCreateCppClass(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString ClassName = Params->GetStringField(TEXT("class_name"));
	FString ParentClass = Params->GetStringField(TEXT("parent_class"));
	FString ModuleName = Params->GetStringField(TEXT("module_name"));
	
	FString ParentHeader;
	if (Params->TryGetStringField(TEXT("parent_class_header"), ParentHeader))
	{
		// Provided by user
	}
	else
	{
		// Default mappings
		if (ParentClass == TEXT("AActor")) ParentHeader = TEXT("GameFramework/Actor.h");
		else if (ParentClass == TEXT("UObject")) ParentHeader = TEXT("UObject/NoExportTypes.h");
		else if (ParentClass == TEXT("UActorComponent")) ParentHeader = TEXT("Components/ActorComponent.h");
		else if (ParentClass == TEXT("USceneComponent")) ParentHeader = TEXT("Components/SceneComponent.h");
		else if (ParentClass == TEXT("APawn")) ParentHeader = TEXT("GameFramework/Pawn.h");
		else if (ParentClass == TEXT("ACharacter")) ParentHeader = TEXT("GameFramework/Character.h");
		else if (ParentClass == TEXT("APlayerController")) ParentHeader = TEXT("GameFramework/PlayerController.h");
		else if (ParentClass == TEXT("AGameModeBase")) ParentHeader = TEXT("GameFramework/GameModeBase.h");
		else if (ParentClass == TEXT("UUserWidget")) ParentHeader = TEXT("Blueprint/UserWidget.h");
		else
		{
			Result.Errors.Add(FString::Printf(TEXT("Parent class '%s' is not in the common dictionary. Please provide 'parent_class_header' parameter."), *ParentClass));
			return Result;
		}
	}

	FString ApiMacro = ModuleName.ToUpper() + TEXT("_API");

	// Determine prefixes
	FString FullClassName = ClassName;
	if (!ClassName.StartsWith(TEXT("A")) && !ClassName.StartsWith(TEXT("U")) && !ClassName.StartsWith(TEXT("F")))
	{
		if (ParentClass.StartsWith(TEXT("A"))) FullClassName = TEXT("A") + ClassName;
		else if (ParentClass.StartsWith(TEXT("U"))) FullClassName = TEXT("U") + ClassName;
		else FullClassName = TEXT("F") + ClassName;
	}
	else
	{
		// If they provided the prefix in the class name, strip it for file names
		ClassName = FullClassName.RightChop(1);
	}

	FString ModuleDir = FPaths::Combine(FPaths::GameSourceDir(), ModuleName);
	FString PublicDir = FPaths::Combine(ModuleDir, TEXT("Public"));
	FString PrivateDir = FPaths::Combine(ModuleDir, TEXT("Private"));

	bool bUsePublicPrivate = IFileManager::Get().DirectoryExists(*PublicDir) || IFileManager::Get().DirectoryExists(*PrivateDir);
	
	FString HeaderPath = bUsePublicPrivate ? FPaths::Combine(PublicDir, ClassName + TEXT(".h")) : FPaths::Combine(ModuleDir, ClassName + TEXT(".h"));
	FString CppPath = bUsePublicPrivate ? FPaths::Combine(PrivateDir, ClassName + TEXT(".cpp")) : FPaths::Combine(ModuleDir, ClassName + TEXT(".cpp"));

	FString HeaderContent = FString::Printf(TEXT(
		"// Fill out your copyright notice in the Description page of Project Settings.\n\n"
		"#pragma once\n\n"
		"#include \"CoreMinimal.h\"\n"
		"#include \"%s\"\n"
		"#include \"%s.generated.h\"\n\n"
		"UCLASS()\n"
		"class %s %s : public %s\n"
		"{\n"
		"\tGENERATED_BODY()\n\n"
		"public:\n"
		"\t%s();\n\n"
		"};\n"
	), *ParentHeader, *ClassName, *ApiMacro, *FullClassName, *ParentClass, *FullClassName);

	FString CppContent = FString::Printf(TEXT(
		"// Fill out your copyright notice in the Description page of Project Settings.\n\n"
		"#include \"%s.h\"\n\n"
		"%s::%s()\n"
		"{\n"
		"\t// Set this object to call Tick() every frame. You can turn this off to improve performance if you don't need it.\n"
		"}\n"
	), *ClassName, *FullClassName, *FullClassName);

	// Ensure directories exist
	if (bUsePublicPrivate)
	{
		IFileManager::Get().MakeDirectory(*PublicDir, true);
		IFileManager::Get().MakeDirectory(*PrivateDir, true);
	}
	else
	{
		IFileManager::Get().MakeDirectory(*ModuleDir, true);
	}

	if (!WriteFileWithBackup(HeaderPath, HeaderContent))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to write header: %s"), *HeaderPath));
		return Result;
	}
	Result.ModifiedPaths.Add(HeaderPath);

	if (!WriteFileWithBackup(CppPath, CppContent))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to write cpp: %s"), *CppPath));
		return Result;
	}
	Result.ModifiedPaths.Add(CppPath);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Generated C++ class %s successfully. [AI HINT: You MUST run regenerate_project_files and trigger_compile (or compile from IDE) for these structural changes to load properly.]"), *FullClassName);
	
	return Result;
}
