// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Python/AgentFrameworkPythonActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkSettings.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "HAL/PlatformFileManager.h"

// IPythonScriptPlugin interface â€” conditional include
#if WITH_PYTHON
#include "IPythonScriptPlugin.h"
#endif

#define LOCTEXT_NAMESPACE "AgentFrameworkPythonActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkPythonActions::FAgentFrameworkPythonActions()
{
	InitializeDenylist();
}

FAgentFrameworkPythonActions::~FAgentFrameworkPythonActions() {}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

FName FAgentFrameworkPythonActions::GetActionName() const { return FName(TEXT("Python")); }

TArray<FString> FAgentFrameworkPythonActions::GetSupportedToolNames() const
{
	return { TEXT("execute_python_script") };
}

bool FAgentFrameworkPythonActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	if (!Params->HasField(TEXT("script")) && !Params->HasField(TEXT("script_content")))
	{
		OutErrors.Add(TEXT("Missing required field: 'script' or 'script_content'"));
		return false;
	}
	
	FString Justification;
	if (!Params->TryGetStringField(TEXT("justification_why_native_tools_or_skills_are_insufficient"), Justification) || Justification.TrimStartAndEnd().IsEmpty())
	{
		OutErrors.Add(TEXT("Missing or empty required field: 'justification_why_native_tools_or_skills_are_insufficient'. You MUST provide a detailed justification explaining why native tools or dedicated skills cannot accomplish this task."));
		return false;
	}
	
	FString Trimmed = Justification.TrimStartAndEnd();
	if (Trimmed.Len() < 10)
	{
		OutErrors.Add(TEXT("The provided 'justification_why_native_tools_or_skills_are_insufficient' is too short. Please provide a detailed, meaningful explanation."));
		return false;
	}
	
	if (Trimmed.Len() > 1000)
	{
		OutErrors.Add(TEXT("The provided 'justification_why_native_tools_or_skills_are_insufficient' is too long (max 1000 characters). Please keep it concise."));
		return false;
	}

	TArray<FString> Words;
	Trimmed.ParseIntoArrayWS(Words);
	if (Words.Num() < 4)
	{
		OutErrors.Add(TEXT("The provided 'justification_why_native_tools_or_skills_are_insufficient' is too vague. Please write a complete explanation (at least 4 words) explaining why native tools or skills are insufficient."));
		return false;
	}

	// Basic low-effort phrase check
	TArray<FString> LowEffortPhrases = {
		TEXT("no native tools"),
		TEXT("python is better"),
		TEXT("just testing"),
		TEXT("test script"),
		TEXT("testing python"),
		TEXT("needed for python"),
		TEXT("not available"),
		TEXT("custom script"),
		TEXT("none exists")
	};

	FString LowerJustification = Trimmed.ToLower();
	for (const FString& Phrase : LowEffortPhrases)
	{
		if (LowerJustification == Phrase || LowerJustification.Contains(Phrase))
		{
			OutErrors.Add(FString::Printf(TEXT("The justification '%s' is flagged as a low-effort placeholder. Please provide a genuine, detailed reason."), *Trimmed));
			return false;
		}
	}
	
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkPythonActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	// Security gate: require Full Access mode
	const UAgentFrameworkDeveloperSettings* Settings = UAgentFrameworkDeveloperSettings::Get();
	if (!Settings || Settings->SecurityMode != EAgentFrameworkSecurityMode::FullAccess)
	{
		Result.Errors.Add(TEXT("Python script execution requires Full Access security mode. "
			"Change this in Project Settings > Plugins > AgentFramework > Safety > Security Mode."));
		return Result;
	}

	// Route (only one tool for now)
	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
	{
		Params->TryGetStringField(TEXT("tool_name"), Action);
	}

	return ExecutePythonScript(Params, Result);
}

// ============================================================================
// execute_python_script
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPythonActions::ExecutePythonScript(
	const TSharedRef<FJsonObject>& Params,
	FAgentFrameworkActionResult& Result)
{
	// 1. Check Python plugin availability
	if (!IsPythonPluginAvailable())
	{
		Result.Errors.Add(
			TEXT("Python Script Plugin is not available. Enable it in Edit > Plugins > Scripting > Python Editor Script Plugin, then restart the editor."));
		return Result;
	}

	// 2. Get the script content
	FString Script;
	if (!Params->TryGetStringField(TEXT("script"), Script) || Script.IsEmpty())
	{
		if (!Params->TryGetStringField(TEXT("script_content"), Script) || Script.IsEmpty())
		{
			Result.Errors.Add(TEXT("Missing or empty 'script' or 'script_content' field."));
			return Result;
		}
	}

	// 3. Validate against denylist
	TArray<FString> Violations;
	if (!ValidatePythonScript(Script, Violations))
	{
		Result.Errors.Add(TEXT("Python script failed security validation:"));
		for (const FString& V : Violations)
		{
			Result.Errors.Add(FString::Printf(TEXT("  - %s"), *V));
		}
		return Result;
	}

	// 4. Get timeout
	int32 TimeoutSeconds = 30;
	Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
	TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 5, 120);

	// 5. Prepare temp file paths
	FString ScriptsDir = GetScriptsDir();
	IFileManager::Get().MakeDirectory(*ScriptsDir, true);

	FString ScriptId = FGuid::NewGuid().ToString(EGuidFormats::Short);
	FString ScriptPath = FPaths::Combine(ScriptsDir, FString::Printf(TEXT("agentframework_script_%s.py"), *ScriptId));
	FString OutputPath = FPaths::Combine(ScriptsDir, FString::Printf(TEXT("agentframework_output_%s.txt"), *ScriptId));

	// 6. Wrap the user script with output capture boilerplate
	// This redirects stdout/stderr to a file so we can capture the output
	FString WrappedScript = FString::Printf(TEXT(
		"import sys\n"
		"import io\n"
		"import traceback\n"
		"\n"
		"_agentframework_output_path = r'%s'\n"
		"_agentframework_stdout = io.StringIO()\n"
		"_agentframework_stderr = io.StringIO()\n"
		"_agentframework_old_stdout = sys.stdout\n"
		"_agentframework_old_stderr = sys.stderr\n"
		"sys.stdout = _agentframework_stdout\n"
		"sys.stderr = _agentframework_stderr\n"
		"\n"
		"try:\n"
		"    # === USER SCRIPT BEGIN ===\n"
		"%s\n"
		"    # === USER SCRIPT END ===\n"
		"except Exception as _agentframework_e:\n"
		"    print(f'ERROR: {type(_agentframework_e).__name__}: {_agentframework_e}', file=sys.stderr)\n"
		"    traceback.print_exc(file=sys.stderr)\n"
		"finally:\n"
		"    sys.stdout = _agentframework_old_stdout\n"
		"    sys.stderr = _agentframework_old_stderr\n"
		"    _out = _agentframework_stdout.getvalue()\n"
		"    _err = _agentframework_stderr.getvalue()\n"
		"    _combined = ''\n"
		"    if _out:\n"
		"        _combined += _out\n"
		"    if _err:\n"
		"        _combined += '\\n=== STDERR ===\\n' + _err\n"
		"    if not _combined:\n"
		"        _combined = '(script produced no output)'\n"
		"    with open(_agentframework_output_path, 'w', encoding='utf-8') as _f:\n"
		"        _f.write(_combined)\n"
	), *OutputPath.Replace(TEXT("\\"), TEXT("\\\\")), *IndentScript(Script));

	// 7. Write the wrapped script to disk
	if (!FFileHelper::SaveStringToFile(WrappedScript, *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to write temp script to: %s"), *ScriptPath));
		return Result;
	}

	// 8. Execute via IPythonScriptPlugin
	FString Justification;
	Params->TryGetStringField(TEXT("justification_why_native_tools_or_skills_are_insufficient"), Justification);
	UE_LOG(LogAgentFramework, Log, TEXT("PythonActions: Executing script %s (timeout: %ds). Justification: %s"), *ScriptPath, TimeoutSeconds, *Justification);

	double StartTime = FPlatformTime::Seconds();

#if WITH_PYTHON
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin)
	{
		// ExecPythonCommand runs synchronously on the game thread
		FString ExecCommand = FString::Printf(TEXT("exec(open(r'%s', encoding='utf-8').read())"), *ScriptPath);
		PythonPlugin->ExecPythonCommand(*ExecCommand);
	}
	else
	{
		Result.Errors.Add(TEXT("IPythonScriptPlugin::Get() returned null despite the module being loaded."));
		// Cleanup
		IFileManager::Get().Delete(*ScriptPath);
		return Result;
	}
#else
	Result.Errors.Add(TEXT("This build of AgentFramework was compiled without Python support (WITH_PYTHON=0)."));
	IFileManager::Get().Delete(*ScriptPath);
	return Result;
#endif

	double ElapsedSeconds = FPlatformTime::Seconds() - StartTime;

	// 9. Read captured output
	FString CapturedOutput;
	if (FFileHelper::LoadFileToString(CapturedOutput, *OutputPath))
	{
		// Truncate very long output to prevent context window bloat
		const int32 MaxOutputChars = 50000;
		if (CapturedOutput.Len() > MaxOutputChars)
		{
			CapturedOutput = CapturedOutput.Left(MaxOutputChars)
				+ FString::Printf(TEXT("\n\n... [OUTPUT TRUNCATED â€” showing first %d of %d characters]"),
					MaxOutputChars, CapturedOutput.Len());
		}
	}
	else
	{
		CapturedOutput = TEXT("(no output captured â€” the script may have crashed before the output redirect initialized)");
	}

	// 10. Check for errors in output
	bool bHasErrors = CapturedOutput.Contains(TEXT("=== STDERR ==="))
		|| CapturedOutput.Contains(TEXT("ERROR:"))
		|| CapturedOutput.Contains(TEXT("Traceback"));

	// 11. Build result
	Result.bSuccess = !bHasErrors;
	Result.ExecutionTimeSeconds = static_cast<float>(ElapsedSeconds);
	Result.ResultMessage = FString::Printf(
		TEXT("=== Python Script Result (%.1fs) ===\n%s"),
		ElapsedSeconds, *CapturedOutput);

	if (bHasErrors)
	{
		Result.Warnings.Add(TEXT("Script produced error output. Check the result for details."));
	}

	// 12. Cleanup temp files
	IFileManager::Get().Delete(*ScriptPath);
	IFileManager::Get().Delete(*OutputPath);

	UE_LOG(LogAgentFramework, Log, TEXT("PythonActions: Script completed in %.1fs (success: %s)"),
		ElapsedSeconds, Result.bSuccess ? TEXT("true") : TEXT("false"));

	return Result;
}

// ============================================================================
// Helpers
// ============================================================================

FString FAgentFrameworkPythonActions::IndentScript(const FString& Script)
{
	// Indent each line by 4 spaces (to fit inside the try: block)
	TArray<FString> Lines;
	Script.ParseIntoArrayLines(Lines);

	FString Indented;
	for (const FString& Line : Lines)
	{
		Indented += TEXT("    ") + Line + TEXT("\n");
	}
	return Indented;
}

bool FAgentFrameworkPythonActions::IsPythonPluginAvailable() const
{
#if WITH_PYTHON
	return IPythonScriptPlugin::Get() != nullptr;
#else
	return false;
#endif
}

FString FAgentFrameworkPythonActions::GetScriptsDir() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AgentFramework"), TEXT("Scripts"));
}

void FAgentFrameworkPythonActions::InitializeDenylist()
{
	// Block dangerous Python operations
	ScriptDenylist = {
		// System execution
		TEXT("os.system"),
		TEXT("os.popen"),
		TEXT("os.exec"),
		TEXT("subprocess"),
		TEXT("Popen"),
		// File destruction (allow os.path, os.listdir, etc.)
		TEXT("shutil.rmtree"),
		TEXT("os.rmdir"),
		TEXT("os.remove"),
		TEXT("os.unlink"),
		TEXT("os.rename"),
		// Network
		TEXT("urllib"),
		TEXT("requests"),
		TEXT("http.client"),
		TEXT("socket.connect"),
		TEXT("ftplib"),
		TEXT("smtplib"),
		// Code loading
		TEXT("__import__"),
		TEXT("importlib"),
		TEXT("compile("),
		TEXT("eval("),
		TEXT("exec("),       // We allow our wrapper exec, but block user scripts from nesting exec
		// Environment
		TEXT("os.environ"),
		TEXT("getenv"),
		TEXT("putenv"),
	};
}

bool FAgentFrameworkPythonActions::ValidatePythonScript(const FString& Script, TArray<FString>& OutViolations) const
{
	bool bValid = true;

	for (const FString& Pattern : ScriptDenylist)
	{
		if (Script.Contains(Pattern, ESearchCase::CaseSensitive))
		{
			OutViolations.Add(FString::Printf(TEXT("Blocked pattern detected: '%s'"), *Pattern));
			bValid = false;
		}
	}

	// Block absolute path writes outside project
	// Allow writes to project dir but catch attempts to write to system dirs
	if (Script.Contains(TEXT("C:\\Windows"), ESearchCase::IgnoreCase)
		|| Script.Contains(TEXT("/usr/"), ESearchCase::CaseSensitive)
		|| Script.Contains(TEXT("/etc/"), ESearchCase::CaseSensitive)
		|| Script.Contains(TEXT("C:\\Program"), ESearchCase::IgnoreCase))
	{
		OutViolations.Add(TEXT("Script attempts to access system directories."));
		bValid = false;
	}

	return bValid;
}

#undef LOCTEXT_NAMESPACE
