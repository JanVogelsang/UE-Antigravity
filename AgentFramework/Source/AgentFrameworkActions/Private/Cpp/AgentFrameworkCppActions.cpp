// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Cpp/AgentFrameworkCppActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DesktopPlatformModule.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/MetaData.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Interfaces/IPluginManager.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

namespace
{
	/**
	 * Gate for every file this executor writes.
	 *
	 * The previous check accepted anything under FPaths::ProjectDir(), which is far wider than
	 * "project source" — it also covers Config/, Saved/, Content/, the .uproject itself, and every
	 * installed plugin, including AgentFramework's own. A benchmark run hit exactly that, leaving a
	 * UCLASS in the plugin's AgentFrameworkEngine module where it was compiled into the plugin.
	 *
	 * Writes are restricted to real C++ source under the project's own Source/ or a project
	 * plugin's Source/, and the plugin hosting this tool is never writable: an agent must not be
	 * able to edit (or disable the safety checks in) the code executing its own tool calls.
	 */
	bool IsWritableSourcePath(const FString& InPath, FString& OutError)
	{
		FString FullPath = InPath;
		FPaths::NormalizeFilename(FullPath);
		FullPath = FPaths::ConvertRelativePathToFull(FullPath);

		if (FullPath.Contains(TEXT("..")))
		{
			OutError = FString::Printf(TEXT("Path not allowed: '%s' contains a '..' traversal segment."), *InPath);
			return false;
		}

		static const TCHAR* AllowedExtensions[] = { TEXT("h"), TEXT("hpp"), TEXT("inl"), TEXT("cpp"), TEXT("c"), TEXT("cc"), TEXT("cs") };
		const FString Extension = FPaths::GetExtension(FullPath);
		bool bExtensionAllowed = false;
		for (const TCHAR* Allowed : AllowedExtensions)
		{
			if (Extension.Equals(Allowed, ESearchCase::IgnoreCase))
			{
				bExtensionAllowed = true;
				break;
			}
		}
		if (!bExtensionAllowed)
		{
			OutError = FString::Printf(
				TEXT("Path not allowed: '%s'. This tool writes C++ source only (.h/.hpp/.inl/.cpp/.c/.cc) plus module rules (.cs)."),
				*InPath);
			return false;
		}

		const FString ProjectSourceDir = FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir());
		const FString ProjectPluginsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());

		const bool bInProjectSource = FullPath.StartsWith(ProjectSourceDir, ESearchCase::IgnoreCase);
		const bool bInPluginSource = FullPath.StartsWith(ProjectPluginsDir, ESearchCase::IgnoreCase)
			&& FullPath.Contains(TEXT("/Source/"), ESearchCase::IgnoreCase);

		if (!bInProjectSource && !bInPluginSource)
		{
			OutError = FString::Printf(
				TEXT("Path not allowed: '%s'. Writes are limited to the project's Source/ directory or a project plugin's Source/ directory."),
				*InPath);
			return false;
		}

		if (TSharedPtr<IPlugin> SelfPlugin = IPluginManager::Get().FindPlugin(TEXT("AgentFramework")))
		{
			const FString SelfDir = FPaths::ConvertRelativePathToFull(SelfPlugin->GetBaseDir());
			if (FullPath.StartsWith(SelfDir, ESearchCase::IgnoreCase))
			{
				OutError = FString::Printf(
					TEXT("Path not allowed: '%s' is inside the AgentFramework plugin's own source. The plugin cannot modify itself — put game code under the project's Source/ directory instead."),
					*InPath);
				return false;
			}
		}

		return true;
	}
}

FAgentFrameworkCppActions::FAgentFrameworkCppActions() {}
FAgentFrameworkCppActions::~FAgentFrameworkCppActions() {}

FName FAgentFrameworkCppActions::GetActionName() const { return FName(TEXT("Cpp")); }

TArray<FString> FAgentFrameworkCppActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_cpp_class"),
		TEXT("modify_cpp_file"),
		TEXT("trigger_compile"),
		TEXT("regenerate_project_files"),
		TEXT("macro_create_cpp_class"),
		TEXT("get_cpp_reflection_info")
	};
}

bool FAgentFrameworkCppActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	TSharedPtr<FJsonObject> ParamsPtr = Params;
	FString ToolName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("_tool_name"), ToolName, OutErrors, false))
	{
		UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("tool_name"), ToolName, OutErrors, false);
	}

	if (ToolName == TEXT("create_cpp_class"))
	{
		FString ClassName;
		FString HeaderCode;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("class_name"), ClassName, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("header_code"), HeaderCode, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("modify_cpp_file"))
	{
		FString FilePath;
		FString Content;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("file_path"), FilePath, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("content"), Content, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("macro_create_cpp_class"))
	{
		FString ClassName;
		FString ParentClass;
		FString ModuleName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("class_name"), ClassName, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("parent_class"), ParentClass, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("module_name"), ModuleName, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("get_cpp_reflection_info"))
	{
		FString ClassName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("class_name"), ClassName, OutErrors, true))
		{
			return false;
		}
	}

	// For code gen, validate that the code doesn't contain dangerous patterns
	FString HeaderCode;
	UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("header_code"), HeaderCode, OutErrors, false);
	if (!HeaderCode.IsEmpty())
	{
		TArray<FString> Violations;
		if (!ValidateCodeSafety(HeaderCode, Violations))
		{
			for (const FString& V : Violations)
			{
				OutErrors.Add(FString::Printf(TEXT("Header: %s"), *V));
			}
			return false;
		}
	}

	FString CppCode;
	UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("cpp_code"), CppCode, OutErrors, false);
	if (!CppCode.IsEmpty())
	{
		TArray<FString> Violations;
		if (!ValidateCodeSafety(CppCode, Violations))
		{
			for (const FString& V : Violations)
			{
				OutErrors.Add(FString::Printf(TEXT("Cpp: %s"), *V));
			}
			return false;
		}
	}

	FString Content;
	UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("content"), Content, OutErrors, false);
	if (!Content.IsEmpty())
	{
		TArray<FString> Violations;
		if (!ValidateCodeSafety(Content, Violations))
		{
			for (const FString& V : Violations)
			{
				OutErrors.Add(FString::Printf(TEXT("Content: %s"), *V));
			}
			return false;
		}
	}

	return true;
}

FAgentFrameworkActionResult FAgentFrameworkCppActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	TSharedPtr<FJsonObject> ParamsPtr = Params;
	FString ToolName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("_tool_name"), ToolName, Result.Errors, false))
	{
		UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("tool_name"), ToolName, Result.Errors, false);
	}

	if (ToolName == TEXT("macro_create_cpp_class"))
	{
		ExecuteMacroCreateCppClass(Params, Result);
	}
	else if (ToolName == TEXT("get_cpp_reflection_info"))
	{
		ExecuteGetCppReflectionInfo(Params, Result);
	}
	else if (ToolName == TEXT("create_cpp_class") || (Params->HasField(TEXT("class_name")) && Params->HasField(TEXT("header_code"))))
	{
		ExecuteCreateCppClass(Params, Result);
	}
	else if (ToolName == TEXT("modify_cpp_file") || (Params->HasField(TEXT("file_path")) && Params->HasField(TEXT("content"))))
	{
		ExecuteModifyCppFile(Params, Result);
	}
	else if (ToolName == TEXT("trigger_compile"))
	{
		bool bWaitForCompletion = true;
		UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("wait_for_completion"), bWaitForCompletion, Result.Errors, false);
		ExecuteTriggerCompile(Result, bWaitForCompletion);
	}
	else if (ToolName == TEXT("regenerate_project_files"))
	{
		ExecuteRegenerateProjectFiles(Result);
	}
	else if (Params->HasField(TEXT("compile")))
	{
		bool bCompile = false;
		if (UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("compile"), bCompile, Result.Errors, false) && bCompile)
		{
			ExecuteTriggerCompile(Result);
		}
	}
	else if (Params->HasField(TEXT("regenerate_project")))
	{
		ExecuteRegenerateProjectFiles(Result);
	}
	else
	{
		Result.Errors.Add(TEXT("Could not determine C++ action from parameters."));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkCppActions::ExecuteCreateCppClass(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	TSharedPtr<FJsonObject> ParamsPtr = Params;
	FString ClassName;
	FString HeaderCode;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("class_name"), ClassName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("header_code"), HeaderCode, Result.Errors, true))
	{
		return Result;
	}

	FString CppCode;
	UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("cpp_code"), CppCode, Result.Errors, false);

	// Validate code safety
	TArray<FString> Violations;
	if (!ValidateCodeSafety(HeaderCode, Violations) || (!CppCode.IsEmpty() && !ValidateCodeSafety(CppCode, Violations)))
	{
		Result.Errors.Add(TEXT("Generated code failed safety validation:"));
		Result.Errors.Append(Violations);
		return Result;
	}

	// CRITICAL: Detect structural header changes that Live Coding CANNOT handle.
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
	UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("subdirectory"), SubDir, Result.Errors, false);
	if (!SubDir.IsEmpty())
	{
		PublicDir = FPaths::Combine(PublicDir, SubDir);
		PrivateDir = FPaths::Combine(PrivateDir, SubDir);
	}

	FString HeaderPath = FPaths::Combine(PublicDir, ClassName + TEXT(".h"));
	FString CppPath = FPaths::Combine(PrivateDir, ClassName + TEXT(".cpp"));

	// Validate before creating anything. WriteFileWithBackup would refuse a bad path anyway,
	// but MakeDirectory runs first, so a rejected 'subdirectory' would still leave empty
	// directories behind — including inside the plugin the agent was just told not to touch.
	FString WriteError;
	if (!IsWritableSourcePath(HeaderPath, WriteError) ||
		(!CppCode.IsEmpty() && !IsWritableSourcePath(CppPath, WriteError)))
	{
		Result.Errors.Add(WriteError);
		return Result;
	}

	// Ensure directories exist
	IFileManager::Get().MakeDirectory(*PublicDir, true);
	IFileManager::Get().MakeDirectory(*PrivateDir, true);

	// Write header
	if (!WriteFileWithBackup(HeaderPath, HeaderCode, WriteError))
	{
		Result.Errors.Add(WriteError);
		return Result;
	}
	Result.ModifiedPaths.Add(HeaderPath);

	// Write cpp (if provided)
	if (!CppCode.IsEmpty())
	{
		if (!WriteFileWithBackup(CppPath, CppCode, WriteError))
		{
			Result.Errors.Add(WriteError);
			return Result;
		}
		Result.ModifiedPaths.Add(CppPath);
	}

	UE_LOG(LogAgentFramework, Log, TEXT("CppActions: Created class %s at %s"), *ClassName, *HeaderPath);

	// Auto-trigger compilation if requested — but BLOCK if structural header changes detected
	bool bAutoCompile = false;
	UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("auto_compile"), bAutoCompile, Result.Errors, false);
	if (bAutoCompile)
	{
		if (bHasStructuralHeaderChanges)
		{
			Result.Warnings.Add(TEXT("Auto-compile SKIPPED: Structural header changes (UCLASS/USTRUCT/UENUM) detected. "
				"Live Coding cannot handle these. Restart the editor and compile from your IDE."));
		}
		else
		{
			FAgentFrameworkActionResult CompileResult;
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

FAgentFrameworkActionResult FAgentFrameworkCppActions::ExecuteModifyCppFile(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	TSharedPtr<FJsonObject> ParamsPtr = Params;
	FString FilePath;
	FString Content;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("file_path"), FilePath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("content"), Content, Result.Errors, true))
	{
		return Result;
	}

	FPaths::NormalizeFilename(FilePath);
	FilePath = FPaths::ConvertRelativePathToFull(FilePath);

	// Path validation happens in WriteFileWithBackup so no write path can bypass it.

	// Validate code safety
	TArray<FString> Violations;
	if (!ValidateCodeSafety(Content, Violations))
	{
		Result.Errors.Add(TEXT("Code failed safety validation:"));
		Result.Errors.Append(Violations);
		return Result;
	}

	FString WriteError;
	if (!WriteFileWithBackup(FilePath, Content, WriteError))
	{
		Result.Errors.Add(WriteError);
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Modified file: %s"), *FilePath);
	Result.ModifiedPaths.Add(FilePath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkCppActions::ExecuteTriggerCompile(FAgentFrameworkActionResult& Result, bool bWaitForCompletion)
{
#if WITH_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (LiveCoding)
	{
		// A compile already in flight is the root cause of the "compilation is already in
		// progress" lockouts: the agent fires trigger_compile, gets a bare "triggered" reply,
		// polls, and every retry bounces off the lock. Report it with the state the agent
		// needs to act on instead of a bare failure.
		if (LiveCoding->IsCompiling())
		{
			Result.bSuccess = false;
			Result.Errors.Add(TEXT("A Live Coding compile is already running (started by a previous trigger_compile or by the editor's auto-compile). "
				"Do NOT retry immediately — that just bounces off the same lock. Wait for the editor's Live Coding console to finish "
				"(typically 30-180s), then call trigger_compile once more only if your changes did not take effect."));
			Result.ResultMessage = TEXT("Compilation blocked — a compile is already in progress.");
			return Result;
		}

		if (!LiveCoding->IsEnabledForSession() && !LiveCoding->CanEnableForSession())
		{
			Result.bSuccess = false;
			Result.Errors.Add(FString::Printf(TEXT("Live Coding cannot be enabled for this session: %s. Compile from your IDE (Visual Studio / Rider) instead."),
				*LiveCoding->GetEnableErrorText().ToString()));
			Result.ResultMessage = TEXT("Live Coding unavailable for this session.");
			return Result;
		}

		UE_LOG(LogAgentFramework, Log, TEXT("CppActions: Triggering Live Coding compilation (wait=%s)..."), bWaitForCompletion ? TEXT("true") : TEXT("false"));

		// Compile() enables the session itself and reports why it could not start, so the
		// result enum — not a bare bool — is what tells the agent what actually happened.
		// WaitForCompletion self-pumps the patch loop, so blocking here is safe on the game
		// thread and is what the engine's own LiveCoding.Compile console command does.
		ELiveCodingCompileResult CompileResult = ELiveCodingCompileResult::Failure;
		LiveCoding->Compile(bWaitForCompletion ? ELiveCodingCompileFlags::WaitForCompletion : ELiveCodingCompileFlags::None, &CompileResult);

		switch (CompileResult)
		{
		case ELiveCodingCompileResult::Success:
			Result.bSuccess = true;
			Result.ResultMessage = TEXT("Live Coding compilation succeeded and the patch was applied.");
			break;

		case ELiveCodingCompileResult::NoChanges:
			Result.bSuccess = true;
			Result.ResultMessage = TEXT("Live Coding reported NO CHANGES to compile.");
			Result.Warnings.Add(TEXT("Live Coding found nothing to compile. If you just added or edited a file, this usually means the change is one Live Coding "
				"cannot patch — new UCLASS/USTRUCT/UENUM types and new source files require regenerate_project_files plus a full editor restart and an IDE build."));
			break;

		case ELiveCodingCompileResult::InProgress:
			Result.bSuccess = true;
			Result.ResultMessage = TEXT("Live Coding compilation started. It is still running — check the Live Coding console in the editor for the result.");
			break;

		case ELiveCodingCompileResult::CompileStillActive:
			Result.bSuccess = false;
			Result.Errors.Add(TEXT("A Live Coding compile is still active. Wait for it to finish before triggering another."));
			Result.ResultMessage = TEXT("Compilation blocked — a compile is already in progress.");
			break;

		case ELiveCodingCompileResult::NotStarted:
			Result.bSuccess = false;
			Result.Errors.Add(FString::Printf(TEXT("The Live Coding console could not be started: %s. Compile from your IDE (Visual Studio / Rider) instead."),
				*LiveCoding->GetEnableErrorText().ToString()));
			Result.ResultMessage = TEXT("Live Coding failed to start.");
			break;

		case ELiveCodingCompileResult::Cancelled:
			Result.bSuccess = false;
			Result.Errors.Add(TEXT("Live Coding compilation was cancelled."));
			Result.ResultMessage = TEXT("Compilation cancelled.");
			break;

		case ELiveCodingCompileResult::Failure:
		default:
			Result.bSuccess = false;
			Result.Errors.Add(TEXT("Live Coding compilation FAILED. Open the Live Coding console in the editor (or read Saved/Logs) for the compiler errors, "
				"fix them with modify_cpp_file, then call trigger_compile again."));
			Result.ResultMessage = TEXT("Compilation failed.");
			break;
		}

		if (Result.bSuccess)
		{
			// New reflected types only become discoverable once the asset registry sees them.
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			AssetRegistryModule.Get().Tick(-1.0f);
		}

		return Result;
	}
#endif

	Result.bSuccess = false;
	Result.Warnings.Add(TEXT("Live Coding not available. Please compile manually from your IDE or enable Live Coding in Editor Preferences."));
	Result.ResultMessage = TEXT("Live Coding not available — manual compilation required.");
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkCppActions::ExecuteRegenerateProjectFiles(FAgentFrameworkActionResult& Result)
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

bool FAgentFrameworkCppActions::ValidateCodeSafety(const FString& Code, TArray<FString>& OutViolations) const
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

bool FAgentFrameworkCppActions::WriteFileWithBackup(const FString& FilePath, const FString& Content, FString& OutError)
{
	// Single choke point for every write this executor performs — guard here so no caller can
	// bypass it, whether the path came from the agent or was derived from tool parameters.
	if (!IsWritableSourcePath(FilePath, OutError))
	{
		UE_LOG(LogAgentFramework, Warning, TEXT("CppActions: Blocked write outside allowed source paths: %s"), *FilePath);
		return false;
	}

	// Backup existing file if it exists
	if (FPaths::FileExists(FilePath))
	{
		FString BackupDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AgentFramework"), TEXT("Backups"),
			FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")));
		IFileManager::Get().MakeDirectory(*BackupDir, true);

		FString BackupPath = FPaths::Combine(BackupDir, FPaths::GetCleanFilename(FilePath));
		IFileManager::Get().Copy(*BackupPath, *FilePath);
		UE_LOG(LogAgentFramework, Log, TEXT("CppActions: Backed up %s -> %s"), *FilePath, *BackupPath);
	}

	if (!FFileHelper::SaveStringToFile(Content, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Failed to write file: %s"), *FilePath);
		return false;
	}

	return true;
}

FAgentFrameworkActionResult FAgentFrameworkCppActions::ExecuteMacroCreateCppClass(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	TSharedPtr<FJsonObject> ParamsPtr = Params;
	FString ClassName;
	FString ParentClass;
	FString ModuleName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("class_name"), ClassName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("parent_class"), ParentClass, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("module_name"), ModuleName, Result.Errors, true))
	{
		return Result;
	}
	
	FString ParentHeader;
	UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("parent_class_header"), ParentHeader, Result.Errors, false);
	if (ParentHeader.IsEmpty())
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

	// Validate before creating anything, so a rejected 'module_name' cannot leave empty
	// directories behind (see the equivalent guard in ExecuteCreateCppClass).
	FString WriteError;
	if (!IsWritableSourcePath(HeaderPath, WriteError) || !IsWritableSourcePath(CppPath, WriteError))
	{
		Result.Errors.Add(WriteError);
		return Result;
	}

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

	if (!WriteFileWithBackup(HeaderPath, HeaderContent, WriteError))
	{
		Result.Errors.Add(WriteError);
		return Result;
	}
	Result.ModifiedPaths.Add(HeaderPath);

	if (!WriteFileWithBackup(CppPath, CppContent, WriteError))
	{
		Result.Errors.Add(WriteError);
		return Result;
	}
	Result.ModifiedPaths.Add(CppPath);

	Result.bSuccess = true;
	// This template always emits UCLASS + GENERATED_BODY in a brand-new file, which Live
	// Coding cannot patch. Pointing the agent at trigger_compile here is what produced the
	// compile-lock retry loops, so name the only path that actually works.
	Result.Warnings.Add(TEXT("⚠ NEW REFLECTED TYPE: this class declares UCLASS/GENERATED_BODY in a new file. Live Coding CANNOT compile it — "
		"trigger_compile will report 'no changes' or bounce off the compile lock. Call regenerate_project_files, then ask the user to close the "
		"editor and build from their IDE (Visual Studio / Rider) before you reference this class from Blueprints."));
	Result.ResultMessage = FString::Printf(TEXT("Generated C++ class %s (%s, %s)."), *FullClassName, *HeaderPath, *CppPath);

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkCppActions::ExecuteGetCppReflectionInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	TSharedPtr<FJsonObject> ParamsPtr = Params;
	FString ClassName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("class_name"), ClassName, Result.Errors, true))
	{
		return Result;
	}

	bool bIncludeProperties = true;
	UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("include_properties"), bIncludeProperties, Result.Errors, false);
	bool bIncludeFunctions = true;
	UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("include_functions"), bIncludeFunctions, Result.Errors, false);
	bool bIncludeInterfaces = true;
	UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("include_interfaces"), bIncludeInterfaces, Result.Errors, false);
	bool bIncludeMetadata = true;
	UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("include_metadata"), bIncludeMetadata, Result.Errors, false);

	// Locate class, handling prefixes A/U
	UClass* Class = FindFirstObject<UClass>(*ClassName);
	if (!IsValid(Class) && ClassName.Len() > 1 && (ClassName.StartsWith(TEXT("A")) || ClassName.StartsWith(TEXT("U")) || ClassName.StartsWith(TEXT("I"))))
	{
		Class = FindFirstObject<UClass>(*ClassName.RightChop(1));
	}

	if (!IsValid(Class))
	{
		Result.bSuccess = false;
		Result.Errors.Add(FString::Printf(TEXT("Class '%s' not found in Unreal reflection system."), *ClassName));
		return Result;
	}

	TSharedRef<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetStringField(TEXT("class_name"), Class->GetName());
	ResponseObj->SetStringField(TEXT("parent_class"), IsValid(Class->GetSuperClass()) ? Class->GetSuperClass()->GetName() : TEXT("None"));
	ResponseObj->SetBoolField(TEXT("is_abstract"), Class->HasAnyClassFlags(CLASS_Abstract));

	bool bBlueprintSpawnable = false;
#if WITH_EDITOR
	bBlueprintSpawnable = Class->HasMetaData(TEXT("BlueprintSpawnableComponent"));
#endif
	ResponseObj->SetBoolField(TEXT("is_blueprint_spawnable"), bBlueprintSpawnable);

	// Extract Class Metadata
	if (bIncludeMetadata)
	{
		TSharedRef<FJsonObject> MetaObj = MakeShared<FJsonObject>();
#if WITH_EDITOR
		const TMap<FName, FString>* MetaMap = FMetaData::GetMapForObject(Class);
		if (MetaMap)
		{
			for (const auto& MetaPair : *MetaMap)
			{
				MetaObj->SetStringField(MetaPair.Key.ToString(), MetaPair.Value);
			}
		}
#endif
		ResponseObj->SetObjectField(TEXT("metadata"), MetaObj);
	}

	// Extract Interfaces
	if (bIncludeInterfaces)
	{
		TArray<TSharedPtr<FJsonValue>> InterfaceList;
		for (const FImplementedInterface& Interface : Class->Interfaces)
		{
			if (IsValid(Interface.Class))
			{
				InterfaceList.Add(MakeShared<FJsonValueString>(Interface.Class->GetName()));
			}
		}
		ResponseObj->SetArrayField(TEXT("interfaces"), InterfaceList);
	}

	// Extract Properties
	if (bIncludeProperties)
	{
		TArray<TSharedPtr<FJsonValue>> PropList;
		for (TFieldIterator<FProperty> PropIt(Class, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			if (Prop)
			{
				TSharedRef<FJsonObject> PropObj = MakeShared<FJsonObject>();
				PropObj->SetStringField(TEXT("name"), Prop->GetName());
				PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());

				// Map Property Flags to Readable Strings
				TArray<TSharedPtr<FJsonValue>> FlagsList;
				uint64 Flags = Prop->GetPropertyFlags();
				if (Flags & CPF_BlueprintVisible) FlagsList.Add(MakeShared<FJsonValueString>(TEXT("CPF_BlueprintVisible")));
				if (Flags & CPF_Edit)             FlagsList.Add(MakeShared<FJsonValueString>(TEXT("CPF_Edit")));
				if (Flags & CPF_BlueprintReadOnly)FlagsList.Add(MakeShared<FJsonValueString>(TEXT("CPF_BlueprintReadOnly")));
				if (Flags & CPF_EditConst)        FlagsList.Add(MakeShared<FJsonValueString>(TEXT("CPF_EditConst")));
				if (Flags & CPF_Net)              FlagsList.Add(MakeShared<FJsonValueString>(TEXT("CPF_Net")));
				PropObj->SetArrayField(TEXT("flags"), FlagsList);

				if (bIncludeMetadata)
				{
					TSharedRef<FJsonObject> PropMeta = MakeShared<FJsonObject>();
#if WITH_EDITOR
					if (const TMap<FName, FString>* MetaMap = Prop->GetMetaDataMap())
					{
						for (const auto& MetaPair : *MetaMap)
						{
							PropMeta->SetStringField(MetaPair.Key.ToString(), MetaPair.Value);
						}
					}
#endif
					PropObj->SetObjectField(TEXT("metadata"), PropMeta);
				}
				PropList.Add(MakeShared<FJsonValueObject>(PropObj));
			}
		}
		ResponseObj->SetArrayField(TEXT("properties"), PropList);
	}

	// Extract Functions
	if (bIncludeFunctions)
	{
		TArray<TSharedPtr<FJsonValue>> FuncList;
		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			if (IsValid(Func))
			{
				TSharedRef<FJsonObject> FuncObj = MakeShared<FJsonObject>();
				FuncObj->SetStringField(TEXT("name"), Func->GetName());
				FuncObj->SetBoolField(TEXT("is_pure"), (Func->FunctionFlags & FUNC_BlueprintPure) != 0);

				TArray<TSharedPtr<FJsonValue>> FlagsList;
				uint32 Flags = Func->FunctionFlags;
				if (Flags & FUNC_BlueprintCallable)FlagsList.Add(MakeShared<FJsonValueString>(TEXT("FUNC_BlueprintCallable")));
				if (Flags & FUNC_BlueprintPure)    FlagsList.Add(MakeShared<FJsonValueString>(TEXT("FUNC_BlueprintPure")));
				if (Flags & FUNC_Net)              FlagsList.Add(MakeShared<FJsonValueString>(TEXT("FUNC_Net")));
				if (Flags & FUNC_Static)           FlagsList.Add(MakeShared<FJsonValueString>(TEXT("FUNC_Static")));
				FuncObj->SetArrayField(TEXT("flags"), FlagsList);

				if (bIncludeMetadata)
				{
					TSharedRef<FJsonObject> FuncMeta = MakeShared<FJsonObject>();
#if WITH_EDITOR
					const TMap<FName, FString>* MetaMap = FMetaData::GetMapForObject(Func);
					if (MetaMap)
					{
						for (const auto& MetaPair : *MetaMap)
						{
							FuncMeta->SetStringField(MetaPair.Key.ToString(), MetaPair.Value);
						}
					}
#endif
					FuncObj->SetObjectField(TEXT("metadata"), FuncMeta);
				}

				// Extract Function Parameters
				TArray<TSharedPtr<FJsonValue>> ParamList;
				for (TFieldIterator<FProperty> ParamIt(Func); ParamIt; ++ParamIt)
				{
					FProperty* Param = *ParamIt;
					if (Param && Param->HasAnyPropertyFlags(CPF_Parm))
					{
						TSharedRef<FJsonObject> ParamObj = MakeShared<FJsonObject>();
						ParamObj->SetStringField(TEXT("name"), Param->GetName());
						ParamObj->SetStringField(TEXT("type"), Param->GetCPPType());

						TArray<TSharedPtr<FJsonValue>> ParamFlagsList;
						uint64 PFlags = Param->GetPropertyFlags();
						if (PFlags & CPF_Parm)      ParamFlagsList.Add(MakeShared<FJsonValueString>(TEXT("CPF_Parm")));
						if (PFlags & CPF_OutParm)   ParamFlagsList.Add(MakeShared<FJsonValueString>(TEXT("CPF_OutParm")));
						if (PFlags & CPF_ReturnParm)ParamFlagsList.Add(MakeShared<FJsonValueString>(TEXT("CPF_ReturnParm")));
						ParamObj->SetArrayField(TEXT("flags"), ParamFlagsList);

						ParamList.Add(MakeShared<FJsonValueObject>(ParamObj));
					}
				}
				FuncObj->SetArrayField(TEXT("parameters"), ParamList);
				FuncList.Add(MakeShared<FJsonValueObject>(FuncObj));
			}
		}
		ResponseObj->SetArrayField(TEXT("functions"), FuncList);
	}

	// Serialize response to string
	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj, Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}

void FAgentFrameworkCppActions::PlaySuccessSound()
{
#if WITH_EDITOR
	if (IsValid(GEditor))
	{
		USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
		if (IsValid(SuccessSound))
		{
			GEditor->PlayEditorSound(SuccessSound);
		}
	}
#endif
}
