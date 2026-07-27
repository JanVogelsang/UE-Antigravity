// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Niagara/AgentFrameworkNiagaraActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"

// Niagara Runtime & Actor
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraComponent.h"
#include "NiagaraActor.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "NiagaraTypes.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"

// Niagara Editor & Graph (WITH_EDITOR context)
#if WITH_EDITOR
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#endif

// Unreal Engine Core / Editor Systems
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Viewport/AgentFrameworkViewportActions.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Sound/SoundBase.h"

FAgentFrameworkNiagaraActions::FAgentFrameworkNiagaraActions() {}
FAgentFrameworkNiagaraActions::~FAgentFrameworkNiagaraActions() {}

FName FAgentFrameworkNiagaraActions::GetActionName() const { return FName(TEXT("Niagara")); }

TArray<FString> FAgentFrameworkNiagaraActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_niagara_system"),
		TEXT("add_niagara_emitter"),
		TEXT("add_niagara_module"),
		TEXT("set_niagara_module_pin"),
		TEXT("compile_niagara_system"),
		TEXT("capture_niagara_system_isolated"),
		TEXT("set_niagara_parameter")
	};
}

bool FAgentFrameworkNiagaraActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, OutErrors, false);

	if (ToolName == TEXT("create_niagara_system"))
	{
		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true))
		{
			return false;
		}
	}
	else
	{
		// All other tools require system_path (or SystemAsset or asset_path as fallbacks)
		if (!Params->HasField(TEXT("system_path")) && !Params->HasField(TEXT("SystemAsset")) && !Params->HasField(TEXT("asset_path")))
		{
			OutErrors.Add(TEXT("Missing required field: system_path or SystemAsset"));
			return false;
		}

		if (ToolName == TEXT("add_niagara_emitter"))
		{
			FString EmitterTemplate, EmitterName;
			if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("emitter_template"), EmitterTemplate, OutErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("emitter_name"), EmitterName, OutErrors, true))
			{
				return false;
			}
		}
		else if (ToolName == TEXT("add_niagara_module"))
		{
			FString EmitterName, Phase, ModuleType;
			if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("emitter_name"), EmitterName, OutErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("phase"), Phase, OutErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("module_type"), ModuleType, OutErrors, true))
			{
				return false;
			}
		}
		else if (ToolName == TEXT("set_niagara_module_pin"))
		{
			FString EmitterName, Phase, ModuleType, PinName, Value;
			if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("emitter_name"), EmitterName, OutErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("phase"), Phase, OutErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("module_type"), ModuleType, OutErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("pin_name"), PinName, OutErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("value"), Value, OutErrors, true))
			{
				return false;
			}
		}
		else if (ToolName == TEXT("set_niagara_parameter"))
		{
			if (!Params->HasField(TEXT("parameter_name")) && !Params->HasField(TEXT("ParameterName")))
			{
				OutErrors.Add(TEXT("Missing required field: parameter_name or ParameterName"));
				return false;
			}
		}
	}

	return true;
}

FAgentFrameworkActionResult FAgentFrameworkNiagaraActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, Result.Errors, false);

	bool bIsReadOnly = (ToolName == TEXT("capture_niagara_system_isolated"));

	TOptional<FScopedTransaction> Transaction;
	if (!bIsReadOnly)
	{
		Transaction.Emplace(FText::FromString(TEXT("AgentFramework Niagara Action")));
	}

	if (ToolName == TEXT("create_niagara_system"))          Result = ExecuteCreateSystem(Params, Result);
	else if (ToolName == TEXT("add_niagara_emitter"))        Result = ExecuteAddEmitter(Params, Result);
	else if (ToolName == TEXT("add_niagara_module"))         Result = ExecuteAddModule(Params, Result);
	else if (ToolName == TEXT("set_niagara_module_pin"))     Result = ExecuteSetModulePin(Params, Result);
	else if (ToolName == TEXT("compile_niagara_system"))     Result = ExecuteCompileSystem(Params, Result);
	else if (ToolName == TEXT("capture_niagara_system_isolated")) Result = ExecuteCaptureIsolated(Params, Result);
	else if (ToolName == TEXT("set_niagara_parameter"))     Result = ExecuteSetNiagaraParameter(Params, Result);
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown Niagara tool: '%s'"), *ToolName));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	if (Transaction.IsSet() && !Result.bSuccess)
	{
		Transaction->Cancel();
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkNiagaraActions::ExecuteCreateSystem(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
#if WITH_EDITOR
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}
	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();
	UNiagaraSystemFactoryNew* Factory = NewObject<UNiagaraSystemFactoryNew>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create UNiagaraSystemFactoryNew instance."));
		return Result;
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UNiagaraSystem::StaticClass(), Factory);
	UNiagaraSystem* NewSystem = Cast<UNiagaraSystem>(NewAsset);

	if (!IsValid(NewSystem))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Niagara System at %s"), *AssetPath));
		return Result;
	}

	NewSystem->Modify();
	UPackage* Package = NewSystem->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();

		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, NewSystem, *PackageFilename, SaveArgs);
		}
	}

	FAssetRegistryModule::AssetCreated(NewSystem);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created empty Niagara System '%s'"), *AssetName);
	Result.ModifiedAssets.Add(AssetPath);
#else
	Result.Errors.Add(TEXT("Niagara System creation is only supported in the Editor."));
#endif
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkNiagaraActions::ExecuteAddEmitter(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
#if WITH_EDITOR
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) || SystemPath.IsEmpty())
	{
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), SystemPath, Result.Errors, true))
		{
			return Result;
		}
	}
	FString EmitterTemplate, EmitterName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("emitter_template"), EmitterTemplate, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("emitter_name"), EmitterName, Result.Errors, true))
	{
		return Result;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!IsValid(System))
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	// Determine Template Emitter Path inside Engine/Niagara content
	FString TemplatePath;
	if (EmitterTemplate == TEXT("SpriteBurst"))       TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst");
	else if (EmitterTemplate == TEXT("RibbonTrail"))   TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon");
	else if (EmitterTemplate == TEXT("MeshDebris"))    TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst");
	else if (EmitterTemplate == TEXT("GPUSimulation")) TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/DirectionalBurst");
	else                                              TemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst");

	UNiagaraEmitter* TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
	if (!IsValid(TemplateEmitter))
	{
		// Fallback search in Common locations
		TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));
	}

	if (!IsValid(TemplateEmitter))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load Niagara Template Emitter at %s"), *TemplatePath));
		return Result;
	}

	if (IsGarbageCollecting())
	{
		Result.Errors.Add(TEXT("Cannot modify Niagara System while Garbage Collection is in progress."));
		return Result;
	}

	System->Modify();

	FGuid VersionGuid = TemplateEmitter->GetExposedVersion().VersionGuid;
	if (!VersionGuid.IsValid())
	{
		FVersionedNiagaraEmitterData* EmitterData = TemplateEmitter->GetLatestEmitterData();
		if (EmitterData)
		{
			VersionGuid = EmitterData->Version.VersionGuid;
		}
	}

	const FNiagaraEmitterHandle& AddedHandle = System->AddEmitterHandle(*TemplateEmitter, FName(*EmitterName), VersionGuid);
	if (!AddedHandle.GetId().IsValid())
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to add emitter handle '%s' to system"), *EmitterName));
		return Result;
	}

	UPackage* Package = System->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
	}
	Result.bSuccess = WaitAndReportCompile(System, Result);
	Result.ModifiedAssets.Add(SystemPath);
#else
	Result.Errors.Add(TEXT("Emitter configuration is only supported in the Editor."));
#endif
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkNiagaraActions::ExecuteAddModule(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
#if WITH_EDITOR
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) || SystemPath.IsEmpty())
	{
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), SystemPath, Result.Errors, true))
		{
			return Result;
		}
	}
	FString EmitterName, Phase, ModuleType;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("emitter_name"), EmitterName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("phase"), Phase, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("module_type"), ModuleType, Result.Errors, true))
	{
		return Result;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!IsValid(System))
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	FString FindError;
	UNiagaraGraph* Graph = FindGraphForPhase(System, EmitterName, Phase, FindError);
	if (!IsValid(Graph))
	{
		Result.Errors.Add(FindError);
		return Result;
	}

	// Locate standard module script path
	FString ModulePath;
	if (ModuleType == TEXT("AddVelocity"))                  ModulePath = TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity");
	else if (ModuleType == TEXT("GravityForce"))            ModulePath = TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce");
	else if (ModuleType == TEXT("SpawnBurstInstantaneous")) ModulePath = TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous");
	else if (ModuleType == TEXT("SpawnBurst_Instantaneous"))ModulePath = TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous");
	else if (ModuleType.StartsWith(TEXT("/Niagara/")))       ModulePath = ModuleType;
	else                                                    ModulePath = FString::Printf(TEXT("/Niagara/Modules/Emitter/%s.%s"), *ModuleType, *ModuleType);

	UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
	if (!IsValid(ModuleScript))
	{
		// Search fallback
		ModuleScript = LoadObject<UNiagaraScript>(nullptr, *FString::Printf(TEXT("/Niagara/Modules/Spawn/Velocity/%s.%s"), *ModuleType, *ModuleType));
	}
	if (!IsValid(ModuleScript))
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara Script Module not found at %s"), *ModulePath));
		return Result;
	}

	System->Modify();
	Graph->Modify();

	UNiagaraNodeFunctionCall* NewNode = NewObject<UNiagaraNodeFunctionCall>(Graph, NAME_None, RF_Transactional);
	if (!IsValid(NewNode))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create function call node for module %s"), *ModuleType));
		return Result;
	}
	NewNode->FunctionScript = ModuleScript;
	if (ModuleScript->IsVersioningEnabled())
	{
		NewNode->SelectedScriptVersion = ModuleScript->GetExposedVersion().VersionGuid;
	}
	NewNode->AllocateDefaultPins();
	
	Graph->AddNode(NewNode, true, true);
	Graph->NotifyGraphChanged();

	UPackage* Package = System->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
	}
	Result.bSuccess = WaitAndReportCompile(System, Result);
	Result.ModifiedAssets.Add(SystemPath);
#else
	Result.Errors.Add(TEXT("Graph editing is only supported in the Editor."));
#endif
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkNiagaraActions::ExecuteSetModulePin(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
#if WITH_EDITOR
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) || SystemPath.IsEmpty())
	{
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), SystemPath, Result.Errors, true))
		{
			return Result;
		}
	}
	FString EmitterName, Phase, ModuleType, PinName, Value;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("emitter_name"), EmitterName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("phase"), Phase, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("module_type"), ModuleType, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("pin_name"), PinName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("value"), Value, Result.Errors, true))
	{
		return Result;
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!IsValid(System))
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	FString FindError;
	UNiagaraGraph* Graph = FindGraphForPhase(System, EmitterName, Phase, FindError);
	if (!IsValid(Graph))
	{
		Result.Errors.Add(FindError);
		return Result;
	}

	// Search for the matching function call node inside the graph
	UNiagaraNodeFunctionCall* TargetNode = nullptr;
	TArray<UNiagaraNode*> Nodes;
	Graph->GetNodesOfClass<UNiagaraNode>(Nodes);
	for (UNiagaraNode* Node : Nodes)
	{
		if (!IsValid(Node)) continue;
		UNiagaraNodeFunctionCall* FnCall = Cast<UNiagaraNodeFunctionCall>(Node);
		if (IsValid(FnCall) && IsValid(FnCall->FunctionScript) && FnCall->FunctionScript->GetName().Contains(ModuleType))
		{
			TargetNode = FnCall;
			break;
		}
	}

	if (!IsValid(TargetNode))
	{
		Result.Errors.Add(FString::Printf(TEXT("Module %s not found in phase %s on emitter %s"), *ModuleType, *Phase, *EmitterName));
		return Result;
	}

	// Find target pin
	UEdGraphPin* TargetPin = TargetNode->FindPin(*PinName, EGPD_Input);
	if (!TargetPin)
	{
		// Case insensitive iterate
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
			{
				TargetPin = Pin;
				break;
			}
		}
	}

	if (!TargetPin)
	{
		Result.Errors.Add(FString::Printf(TEXT("Pin '%s' not found on module %s"), *PinName, *ModuleType));
		return Result;
	}

	System->Modify();
	TargetNode->Modify();
	
	TargetPin->DefaultValue = Value;
	Graph->NotifyGraphChanged();

	UPackage* Package = System->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
	}
	Result.bSuccess = WaitAndReportCompile(System, Result);
	Result.ModifiedAssets.Add(SystemPath);
#else
	Result.Errors.Add(TEXT("Graph editing is only supported in the Editor."));
#endif
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkNiagaraActions::ExecuteCompileSystem(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) || SystemPath.IsEmpty())
	{
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), SystemPath, Result.Errors, true))
		{
			return Result;
		}
	}

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!IsValid(System))
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	System->RequestCompile(true);
	Result.bSuccess = WaitAndReportCompile(System, Result);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkNiagaraActions::ExecuteCaptureIsolated(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) || SystemPath.IsEmpty())
	{
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), SystemPath, Result.Errors, true))
		{
			return Result;
		}
	}
	double DurationSeconds = 2.0;
	UAgentFrameworkActionUtils::TryGetDoubleParam(Params, TEXT("duration_seconds"), DurationSeconds, Result.Errors, false);
	int32 MaxDimension = 512;
	UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("max_dimension"), MaxDimension, Result.Errors, false);

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!IsValid(System))
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("Active Editor World Context not found."));
		return Result;
	}

	// 1. Spawn Transient Niagara Actor and assign System
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	ANiagaraActor* NiagaraActor = World->SpawnActor<ANiagaraActor>(ANiagaraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!IsValid(NiagaraActor) || !IsValid(NiagaraActor->GetNiagaraComponent()))
	{
		Result.Errors.Add(TEXT("Failed to spawn transient Niagara Actor."));
		return Result;
	}

	UNiagaraComponent* Component = NiagaraActor->GetNiagaraComponent();
	Component->SetAsset(System);
	Component->Activate(true);

	// Force compilation check
	System->WaitForCompilationComplete(true, false);

	// 2. Spawn Transient Scene Capture Actor and configure
	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FVector(0, -300, 100), FRotator(0, 90, 0), SpawnParams);
	USceneCaptureComponent2D* CaptureComponent = IsValid(CaptureActor) ? CaptureActor->GetCaptureComponent2D() : nullptr;
	if (!IsValid(CaptureComponent))
	{
		if (IsValid(NiagaraActor))
		{
			World->DestroyActor(NiagaraActor);
		}
		Result.Errors.Add(TEXT("Failed to spawn transient Scene Capture 2D Actor."));
		return Result;
	}

	// Create Transient Render Target
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(World);
	if (!IsValid(RenderTarget))
	{
		if (IsValid(NiagaraActor)) World->DestroyActor(NiagaraActor);
		if (IsValid(CaptureActor)) World->DestroyActor(CaptureActor);
		Result.Errors.Add(TEXT("Failed to create transient RenderTarget."));
		return Result;
	}
	RenderTarget->InitAutoFormat(MaxDimension / 2, MaxDimension / 2); // each slice is half dimensions
	RenderTarget->ClearColor = FLinearColor(0.12f, 0.12f, 0.12f, 1.0f); // neutral dark gray

	CaptureComponent->TextureTarget = RenderTarget;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->ShowOnlyComponents.Add(Component);

	// Frame Camera automatically using Niagara bounding box size
	FBoxSphereBounds Bounds = Component->Bounds;
	float MaxBoundsSize = FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z);
	if (MaxBoundsSize < 10.0f) MaxBoundsSize = 100.0f; // fallback for unpopulated simulation bounds
	
	FVector CamPos = Bounds.Origin - FVector(0.0f, MaxBoundsSize * 2.5f, 0.0f); // look from Front Y axis
	CaptureActor->SetActorLocation(CamPos);
	CaptureActor->SetActorRotation(FRotator(0, 90, 0)); // Rotated to look down Y axis

	// 3. Render 4 Chronological Keyframes (Top-Left, Top-Right, Bottom-Left, Bottom-Right)
	TArray<FColor> StitchedPixels;
	StitchedPixels.AddZeroed(MaxDimension * MaxDimension); // Grid buffer

	int32 SliceWidth = MaxDimension / 2;
	int32 SliceHeight = MaxDimension / 2;

	TArray<double> Times = { DurationSeconds * 0.1, DurationSeconds * 0.3, DurationSeconds * 0.6, DurationSeconds * 0.9 };

	Component->ResetSystem();
	double LastTime = 0.0f;

	for (int32 i = 0; i < 4; ++i)
	{
		double TargetTime = Times[i];
		double Step = TargetTime - LastTime;
		if (Step > 0.0)
		{
			Component->AdvanceSimulation(FMath::RoundToInt(Step * 60.0f), 1.0f / 60.0f);
		}
		LastTime = TargetTime;

		// Force Scene Capture
		CaptureComponent->CaptureScene();

		// Read pixels from Render Target
		FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
		if (RTResource)
		{
			TArray<FColor> OutColor;
			RTResource->ReadPixels(OutColor);

			if (OutColor.Num() == SliceWidth * SliceHeight)
			{
				// Copy quadrant pixels to stitched buffer
				int32 QuadX = (i % 2) * SliceWidth;
				int32 QuadY = (i / 2) * SliceHeight;

				for (int32 y = 0; y < SliceHeight; ++y)
				{
					for (int32 x = 0; x < SliceWidth; ++x)
					{
						int32 DestX = QuadX + x;
						int32 DestY = QuadY + y;
						int32 DestIndex = DestY * MaxDimension + DestX;
						int32 SrcIndex = y * SliceWidth + x;

						StitchedPixels[DestIndex] = OutColor[SrcIndex];
					}
				}
			}
		}
	}

	// 4. Add scale bar visual reference (draw 1m baseline overlay in Bottom-Left quadrant)
	// 1 meter = 100 Unreal Units. Draw horizontal line.
	// Frame size covers MaxBoundsSize * 2.0. So 1m scale in pixels is approximately:
	float PixelsPerUnit = (float)SliceWidth / (MaxBoundsSize * 2.0f);
	int32 LineWidthPixels = FMath::Clamp(FMath::RoundToInt(100.0f * PixelsPerUnit), 10, SliceWidth - 20);

	int32 StartLineX = 20;
	int32 EndLineX = StartLineX + LineWidthPixels;
	int32 LineY = MaxDimension - 20; // 20 pixels from bottom margin

	for (int32 x = StartLineX; x <= EndLineX; ++x)
	{
		int32 DestIndex = LineY * MaxDimension + x;
		StitchedPixels[DestIndex] = FColor::White;
	}

	// Draw ticks at ends of scale line
	for (int32 tickY = LineY - 5; tickY <= LineY + 5; ++tickY)
	{
		int32 StartTickIndex = tickY * MaxDimension + StartLineX;
		int32 EndTickIndex = tickY * MaxDimension + EndLineX;
		
		StitchedPixels[StartTickIndex] = FColor::White;
		StitchedPixels[EndTickIndex]   = FColor::White;
	}

	// 5. Encode to JPEG and Save to Disk
	FString FilePath = FAgentFrameworkViewportActions::SavePixelsToDisk(StitchedPixels, MaxDimension, MaxDimension, MaxDimension, 90);

	// 6. Cleanup transient Actors
	if (IsValid(NiagaraActor)) World->DestroyActor(NiagaraActor);
	if (IsValid(CaptureActor)) World->DestroyActor(CaptureActor);

	// 7. Populate metadata response
	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	if (!FilePath.IsEmpty())
	{
		ResponseObj->SetStringField(TEXT("image_path"), FilePath);
	}
	
	TArray<TSharedPtr<FJsonValue>> TimesArray;
	for (double T : Times) TimesArray.Add(MakeShared<FJsonValueNumber>(T));
	ResponseObj->SetArrayField(TEXT("quadrant_times"), TimesArray);

	TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
	BoundsObj->SetNumberField(TEXT("x"), Bounds.BoxExtent.X * 2.0);
	BoundsObj->SetNumberField(TEXT("y"), Bounds.BoxExtent.Y * 2.0);
	BoundsObj->SetNumberField(TEXT("z"), Bounds.BoxExtent.Z * 2.0);
	ResponseObj->SetObjectField(TEXT("bounds_cm"), BoundsObj);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}

UNiagaraGraph* FAgentFrameworkNiagaraActions::FindGraphForPhase(UNiagaraSystem* System, const FString& EmitterName, const FString& PhaseStr, FString& OutError) const
{
#if WITH_EDITOR
	if (!IsValid(System))
	{
		OutError = TEXT("Niagara System pointer is invalid.");
		return nullptr;
	}

	// Find emitter handle by name
	FNiagaraEmitterHandle* TargetHandle = nullptr;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetName().ToString() == EmitterName)
		{
			TargetHandle = &Handle;
			break;
		}
	}

	if (!TargetHandle)
	{
		OutError = FString::Printf(TEXT("Emitter handle '%s' not found inside Niagara System."), *EmitterName);
		return nullptr;
	}

	UNiagaraEmitter* Emitter = TargetHandle->GetInstance().Emitter;
	if (!IsValid(Emitter))
	{
		OutError = FString::Printf(TEXT("Underlying UNiagaraEmitter is null or invalid for emitter handle '%s'."), *EmitterName);
		return nullptr;
	}

	UNiagaraScript* TargetScript = nullptr;
	FVersionedNiagaraEmitterData* EmitterData = Emitter->GetLatestEmitterData();
	if (EmitterData)
	{
		if (PhaseStr == TEXT("EmitterSpawn"))
		{
#if WITH_EDITORONLY_DATA
			TargetScript = EmitterData->EmitterSpawnScriptProps.Script;
#endif
		}
		else if (PhaseStr == TEXT("EmitterUpdate"))
		{
#if WITH_EDITORONLY_DATA
			TargetScript = EmitterData->EmitterUpdateScriptProps.Script;
#endif
		}
		else if (PhaseStr == TEXT("ParticleSpawn"))
		{
			TargetScript = EmitterData->SpawnScriptProps.Script;
		}
		else if (PhaseStr == TEXT("ParticleUpdate"))
		{
			TargetScript = EmitterData->UpdateScriptProps.Script;
		}
	}

	if (!IsValid(TargetScript))
	{
		OutError = FString::Printf(TEXT("Niagara script for phase %s not found on emitter %s."), *PhaseStr, *EmitterName);
		return nullptr;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(TargetScript->GetSource(TargetScript->GetExposedVersion().VersionGuid));
	if (!IsValid(ScriptSource) || !IsValid(ScriptSource->NodeGraph))
	{
		OutError = FString::Printf(TEXT("Niagara graph source missing for phase %s script on emitter %s."), *PhaseStr, *EmitterName);
		return nullptr;
	}

	return ScriptSource->NodeGraph;
#else
	OutError = TEXT("Graph retrieval is only supported in Editor builds.");
	return nullptr;
#endif
}

bool FAgentFrameworkNiagaraActions::WaitAndReportCompile(UNiagaraSystem* System, FAgentFrameworkActionResult& Result) const
{
	if (!IsValid(System))
	{
		Result.Errors.Add(TEXT("Niagara System pointer is invalid for compilation check."));
		return false;
	}

	// Request compile asynchronously to prevent blocking the Game Thread HTTP listener
	System->RequestCompile(false);

	// Extract compilation logs and performance metrics
	// If compiling failed, extract errors from output log or script states
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter;
		if (!IsValid(Emitter)) continue;

		FVersionedNiagaraEmitterData* EmitterData = Emitter->GetLatestEmitterData();
		if (!EmitterData) continue;

		TArray<UNiagaraScript*> ActiveScripts;
		if (EmitterData->SpawnScriptProps.Script) ActiveScripts.Add(EmitterData->SpawnScriptProps.Script);
		if (EmitterData->UpdateScriptProps.Script) ActiveScripts.Add(EmitterData->UpdateScriptProps.Script);
#if WITH_EDITORONLY_DATA
		if (EmitterData->EmitterSpawnScriptProps.Script) ActiveScripts.Add(EmitterData->EmitterSpawnScriptProps.Script);
		if (EmitterData->EmitterUpdateScriptProps.Script) ActiveScripts.Add(EmitterData->EmitterUpdateScriptProps.Script);
#endif

		for (UNiagaraScript* Script : ActiveScripts)
		{
			if (!IsValid(Script)) continue;
			
#if WITH_EDITORONLY_DATA
			const FNiagaraVMExecutableData& VMData = Script->GetVMExecutableData();
			for (const FNiagaraCompileEvent& CompileEvent : VMData.LastCompileEvents)
			{
				FString Msg = FString::Printf(TEXT("Emitter [%s] %s: %s"), *Handle.GetName().ToString(), 
					CompileEvent.Severity == FNiagaraCompileEventSeverity::Error ? TEXT("ERROR") : TEXT("WARNING"),
					*CompileEvent.Message);

				if (CompileEvent.Severity == FNiagaraCompileEventSeverity::Error)
				{
					Result.Errors.Add(Msg);
				}
				else
				{
					Result.Warnings.Add(Msg);
				}
			}
#endif
		}
	}

	// Check for standard performance optimization bottlenecks
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter;
		if (!IsValid(Emitter)) continue;

		FVersionedNiagaraEmitterData* EmitterData = Emitter->GetLatestEmitterData();
		if (EmitterData && EmitterData->SimTarget == ENiagaraSimTarget::CPUSim)
		{
			Result.Warnings.Add(FString::Printf(TEXT("Performance Warning: Emitter '%s' uses CPU Simulation. For AAA particle counts, consider changing SimTarget to GPU Simulation."), *Handle.GetName().ToString()));
		}
	}

	bool bCompileSuccess = (Result.Errors.Num() == 0);

	if (!bCompileSuccess)
	{
		Result.Errors.Add(TEXT("Niagara System compiled with compilation errors. Review errors array."));
	}

	return bCompileSuccess;
}

void FAgentFrameworkNiagaraActions::PlaySuccessSound()
{
#if WITH_EDITOR
	if (GEditor)
	{
		USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
		if (IsValid(SuccessSound))
		{
			GEditor->PlayEditorSound(SuccessSound);
		}
	}
#endif
}

FAgentFrameworkActionResult FAgentFrameworkNiagaraActions::ExecuteSetNiagaraParameter(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
#if WITH_EDITOR
	// 1. Extract system path (supporting system_path, SystemAsset, asset_path)
	FString SystemPath;
	if (!Params->TryGetStringField(TEXT("system_path"), SystemPath) || SystemPath.IsEmpty())
	{
		if (!Params->TryGetStringField(TEXT("SystemAsset"), SystemPath) || SystemPath.IsEmpty())
		{
			if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), SystemPath, Result.Errors, true))
			{
				return Result;
			}
		}
	}

	// 2. Extract ParameterScope (default "User")
	FString Scope = TEXT("User");
	if (Params->HasTypedField<EJson::String>(TEXT("parameter_scope")))
	{
		Scope = Params->GetStringField(TEXT("parameter_scope"));
	}
	else if (Params->HasTypedField<EJson::String>(TEXT("ParameterScope")))
	{
		Scope = Params->GetStringField(TEXT("ParameterScope"));
	}

	// 3. Extract ParameterName
	FString ParamName;
	if (Params->HasTypedField<EJson::String>(TEXT("parameter_name")))
	{
		ParamName = Params->GetStringField(TEXT("parameter_name"));
	}
	else if (Params->HasTypedField<EJson::String>(TEXT("ParameterName")))
	{
		ParamName = Params->GetStringField(TEXT("ParameterName"));
	}

	if (ParamName.IsEmpty())
	{
		Result.Errors.Add(TEXT("Parameter name is missing or empty."));
		return Result;
	}

	// 4. Extract DataType (default "Float")
	FString DataType = TEXT("Float");
	if (Params->HasTypedField<EJson::String>(TEXT("data_type")))
	{
		DataType = Params->GetStringField(TEXT("data_type"));
	}
	else if (Params->HasTypedField<EJson::String>(TEXT("DataType")))
	{
		DataType = Params->GetStringField(TEXT("DataType"));
	}

	// 5. Load UNiagaraSystem asset
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!IsValid(System))
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	// 6. Format parameter variable name as Scope.ParamName
	FString FullParamName;
	if (ParamName.StartsWith(TEXT("User.")) || ParamName.StartsWith(TEXT("System.")) || ParamName.StartsWith(TEXT("Emitter.")))
	{
		FullParamName = ParamName;
	}
	else
	{
		FullParamName = FString::Printf(TEXT("%s.%s"), *Scope, *ParamName);
	}

	// 7. Get Exposed Parameter Store
	FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();

	System->Modify();

	// 8. Handle Parameter Type
	if (DataType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
	{
		float FloatVal = 0.0f;
		const TSharedPtr<FJsonValue>* ValueField = nullptr;
		if (Params->Values.Contains(TEXT("value"))) ValueField = &Params->Values[TEXT("value")];
		else if (Params->Values.Contains(TEXT("Value"))) ValueField = &Params->Values[TEXT("Value")];

		if (ValueField && ValueField->IsValid())
		{
			if ((*ValueField)->Type == EJson::Number) FloatVal = (float)(*ValueField)->AsNumber();
			else if ((*ValueField)->Type == EJson::String) FloatVal = FCString::Atof(*(*ValueField)->AsString());
			else if ((*ValueField)->Type == EJson::Boolean) FloatVal = (*ValueField)->AsBool() ? 1.0f : 0.0f;
		}

		FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetFloatDef();
		FNiagaraVariable Var(TypeDef, FName(*FullParamName));
		if (UserStore.IndexOf(Var) == INDEX_NONE)
		{
			UserStore.AddParameter(Var, true);
		}
		UserStore.SetParameterData((const uint8*)&FloatVal, Var);
	}
	else if (DataType.Equals(TEXT("Vector2"), ESearchCase::IgnoreCase))
	{
		FVector2f VecVal(0.0f, 0.0f);
		const TSharedPtr<FJsonValue>* ValueField = nullptr;
		if (Params->Values.Contains(TEXT("value"))) ValueField = &Params->Values[TEXT("value")];
		else if (Params->Values.Contains(TEXT("Value"))) ValueField = &Params->Values[TEXT("Value")];

		if (ValueField && ValueField->IsValid())
		{
			if ((*ValueField)->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> Obj = (*ValueField)->AsObject();
				double X = 0.0, Y = 0.0;
				if (Obj->HasField(TEXT("x"))) X = Obj->GetNumberField(TEXT("x"));
				else if (Obj->HasField(TEXT("X"))) X = Obj->GetNumberField(TEXT("X"));
				if (Obj->HasField(TEXT("y"))) Y = Obj->GetNumberField(TEXT("y"));
				else if (Obj->HasField(TEXT("Y"))) Y = Obj->GetNumberField(TEXT("Y"));
				VecVal = FVector2f((float)X, (float)Y);
			}
			else if ((*ValueField)->Type == EJson::Array)
			{
				TArray<TSharedPtr<FJsonValue>> Arr = (*ValueField)->AsArray();
				float X = (Arr.Num() > 0) ? (float)Arr[0]->AsNumber() : 0.0f;
				float Y = (Arr.Num() > 1) ? (float)Arr[1]->AsNumber() : 0.0f;
				VecVal = FVector2f(X, Y);
			}
			else if ((*ValueField)->Type == EJson::String)
			{
				FString Str = (*ValueField)->AsString();
				TArray<FString> Parts;
				Str.ParseIntoArray(Parts, TEXT(","), true);
				if (Parts.Num() >= 2)
				{
					VecVal.X = FCString::Atof(*Parts[0]);
					VecVal.Y = FCString::Atof(*Parts[1]);
				}
				else
				{
					float Scalar = FCString::Atof(*Str);
					VecVal = FVector2f(Scalar, Scalar);
				}
			}
			else if ((*ValueField)->Type == EJson::Number)
			{
				float Scalar = (float)(*ValueField)->AsNumber();
				VecVal = FVector2f(Scalar, Scalar);
			}
		}

		FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetVec2Def();
		FNiagaraVariable Var(TypeDef, FName(*FullParamName));
		if (UserStore.IndexOf(Var) == INDEX_NONE)
		{
			UserStore.AddParameter(Var, true);
		}
		UserStore.SetParameterData((const uint8*)&VecVal, Var);
	}
	else if (DataType.Equals(TEXT("Vector3"), ESearchCase::IgnoreCase))
	{
		FVector3f VecVal(0.0f, 0.0f, 0.0f);
		const TSharedPtr<FJsonValue>* ValueField = nullptr;
		if (Params->Values.Contains(TEXT("value"))) ValueField = &Params->Values[TEXT("value")];
		else if (Params->Values.Contains(TEXT("Value"))) ValueField = &Params->Values[TEXT("Value")];

		if (ValueField && ValueField->IsValid())
		{
			if ((*ValueField)->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> Obj = (*ValueField)->AsObject();
				double X = 0.0, Y = 0.0, Z = 0.0;
				if (Obj->HasField(TEXT("x"))) X = Obj->GetNumberField(TEXT("x"));
				else if (Obj->HasField(TEXT("X"))) X = Obj->GetNumberField(TEXT("X"));
				if (Obj->HasField(TEXT("y"))) Y = Obj->GetNumberField(TEXT("y"));
				else if (Obj->HasField(TEXT("Y"))) Y = Obj->GetNumberField(TEXT("Y"));
				if (Obj->HasField(TEXT("z"))) Z = Obj->GetNumberField(TEXT("z"));
				else if (Obj->HasField(TEXT("Z"))) Z = Obj->GetNumberField(TEXT("Z"));
				VecVal = FVector3f((float)X, (float)Y, (float)Z);
			}
			else if ((*ValueField)->Type == EJson::Array)
			{
				TArray<TSharedPtr<FJsonValue>> Arr = (*ValueField)->AsArray();
				float X = (Arr.Num() > 0) ? (float)Arr[0]->AsNumber() : 0.0f;
				float Y = (Arr.Num() > 1) ? (float)Arr[1]->AsNumber() : 0.0f;
				float Z = (Arr.Num() > 2) ? (float)Arr[2]->AsNumber() : 0.0f;
				VecVal = FVector3f(X, Y, Z);
			}
			else if ((*ValueField)->Type == EJson::String)
			{
				FString Str = (*ValueField)->AsString();
				TArray<FString> Parts;
				Str.ParseIntoArray(Parts, TEXT(","), true);
				if (Parts.Num() >= 3)
				{
					VecVal.X = FCString::Atof(*Parts[0]);
					VecVal.Y = FCString::Atof(*Parts[1]);
					VecVal.Z = FCString::Atof(*Parts[2]);
				}
				else
				{
					float Scalar = FCString::Atof(*Str);
					VecVal = FVector3f(Scalar, Scalar, Scalar);
				}
			}
			else if ((*ValueField)->Type == EJson::Number)
			{
				float Scalar = (float)(*ValueField)->AsNumber();
				VecVal = FVector3f(Scalar, Scalar, Scalar);
			}
		}

		FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetVec3Def();
		FNiagaraVariable Var(TypeDef, FName(*FullParamName));
		if (UserStore.IndexOf(Var) == INDEX_NONE)
		{
			UserStore.AddParameter(Var, true);
		}
		UserStore.SetParameterData((const uint8*)&VecVal, Var);
	}
	else if (DataType.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase))
	{
		FLinearColor ColorVal(0.0f, 0.0f, 0.0f, 1.0f);
		const TSharedPtr<FJsonValue>* ValueField = nullptr;
		if (Params->Values.Contains(TEXT("value"))) ValueField = &Params->Values[TEXT("value")];
		else if (Params->Values.Contains(TEXT("Value"))) ValueField = &Params->Values[TEXT("Value")];

		if (ValueField && ValueField->IsValid())
		{
			if ((*ValueField)->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> Obj = (*ValueField)->AsObject();
				double R = 0.0, G = 0.0, B = 0.0, A = 1.0;
				if (Obj->HasField(TEXT("r"))) R = Obj->GetNumberField(TEXT("r"));
				else if (Obj->HasField(TEXT("R"))) R = Obj->GetNumberField(TEXT("R"));
				if (Obj->HasField(TEXT("g"))) G = Obj->GetNumberField(TEXT("g"));
				else if (Obj->HasField(TEXT("G"))) G = Obj->GetNumberField(TEXT("G"));
				if (Obj->HasField(TEXT("b"))) B = Obj->GetNumberField(TEXT("b"));
				else if (Obj->HasField(TEXT("B"))) B = Obj->GetNumberField(TEXT("B"));
				if (Obj->HasField(TEXT("a"))) A = Obj->GetNumberField(TEXT("a"));
				else if (Obj->HasField(TEXT("A"))) A = Obj->GetNumberField(TEXT("A"));
				ColorVal = FLinearColor((float)R, (float)G, (float)B, (float)A);
			}
			else if ((*ValueField)->Type == EJson::Array)
			{
				TArray<TSharedPtr<FJsonValue>> Arr = (*ValueField)->AsArray();
				float R = (Arr.Num() > 0) ? (float)Arr[0]->AsNumber() : 0.0f;
				float G = (Arr.Num() > 1) ? (float)Arr[1]->AsNumber() : 0.0f;
				float B = (Arr.Num() > 2) ? (float)Arr[2]->AsNumber() : 0.0f;
				float A = (Arr.Num() > 3) ? (float)Arr[3]->AsNumber() : 1.0f;
				ColorVal = FLinearColor(R, G, B, A);
			}
			else if ((*ValueField)->Type == EJson::String)
			{
				FString Str = (*ValueField)->AsString();
				if (!ColorVal.InitFromString(Str))
				{
					TArray<FString> Parts;
					Str.ParseIntoArray(Parts, TEXT(","), true);
					if (Parts.Num() >= 3)
					{
						ColorVal.R = FCString::Atof(*Parts[0]);
						ColorVal.G = FCString::Atof(*Parts[1]);
						ColorVal.B = FCString::Atof(*Parts[2]);
						if (Parts.Num() >= 4) ColorVal.A = FCString::Atof(*Parts[3]);
					}
				}
			}
		}

		FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetColorDef();
		FNiagaraVariable Var(TypeDef, FName(*FullParamName));
		if (UserStore.IndexOf(Var) == INDEX_NONE)
		{
			UserStore.AddParameter(Var, true);
		}
		UserStore.SetParameterData((const uint8*)&ColorVal, Var);
	}
	else if (DataType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
	{
		bool bBoolVal = false;
		const TSharedPtr<FJsonValue>* ValueField = nullptr;
		if (Params->Values.Contains(TEXT("value"))) ValueField = &Params->Values[TEXT("value")];
		else if (Params->Values.Contains(TEXT("Value"))) ValueField = &Params->Values[TEXT("Value")];

		if (ValueField && ValueField->IsValid())
		{
			if ((*ValueField)->Type == EJson::Boolean) bBoolVal = (*ValueField)->AsBool();
			else if ((*ValueField)->Type == EJson::String) bBoolVal = (*ValueField)->AsString().ToBool() || (*ValueField)->AsString() == TEXT("1");
			else if ((*ValueField)->Type == EJson::Number) bBoolVal = ((*ValueField)->AsNumber() != 0);
		}

		FNiagaraBool NiagaraBoolVal(bBoolVal);
		FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetBoolDef();
		FNiagaraVariable Var(TypeDef, FName(*FullParamName));
		if (UserStore.IndexOf(Var) == INDEX_NONE)
		{
			UserStore.AddParameter(Var, true);
		}
		UserStore.SetParameterData((const uint8*)&NiagaraBoolVal, Var);
	}
	else if (DataType.Equals(TEXT("Int32"), ESearchCase::IgnoreCase) || DataType.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
	{
		int32 IntVal = 0;
		const TSharedPtr<FJsonValue>* ValueField = nullptr;
		if (Params->Values.Contains(TEXT("value"))) ValueField = &Params->Values[TEXT("value")];
		else if (Params->Values.Contains(TEXT("Value"))) ValueField = &Params->Values[TEXT("Value")];

		if (ValueField && ValueField->IsValid())
		{
			if ((*ValueField)->Type == EJson::Number) IntVal = (int32)(*ValueField)->AsNumber();
			else if ((*ValueField)->Type == EJson::String) IntVal = FCString::Atoi(*(*ValueField)->AsString());
			else if ((*ValueField)->Type == EJson::Boolean) IntVal = (*ValueField)->AsBool() ? 1 : 0;
		}

		FNiagaraTypeDefinition TypeDef = FNiagaraTypeDefinition::GetIntDef();
		FNiagaraVariable Var(TypeDef, FName(*FullParamName));
		if (UserStore.IndexOf(Var) == INDEX_NONE)
		{
			UserStore.AddParameter(Var, true);
		}
		UserStore.SetParameterData((const uint8*)&IntVal, Var);
	}
	else if (DataType.Equals(TEXT("CurveFloat"), ESearchCase::IgnoreCase))
	{
		UCurveFloat* CurveFloatObj = NewObject<UCurveFloat>(System, NAME_None, RF_Transactional);
		if (!IsValid(CurveFloatObj))
		{
			Result.Errors.Add(TEXT("Failed to create transient UCurveFloat object."));
			return Result;
		}

		FRichCurve& RichCurve = CurveFloatObj->FloatCurve;
		RichCurve.Reset();

		const TArray<TSharedPtr<FJsonValue>>* CurveKeysArray = nullptr;
		if (Params->HasTypedField<EJson::Array>(TEXT("curve_keys")))
		{
			CurveKeysArray = &Params->GetArrayField(TEXT("curve_keys"));
		}
		else if (Params->HasTypedField<EJson::Array>(TEXT("CurveKeys")))
		{
			CurveKeysArray = &Params->GetArrayField(TEXT("CurveKeys"));
		}

		if (CurveKeysArray)
		{
			for (const TSharedPtr<FJsonValue>& KeyVal : *CurveKeysArray)
			{
				if (!KeyVal.IsValid() || KeyVal->Type != EJson::Object) continue;
				TSharedPtr<FJsonObject> KeyObj = KeyVal->AsObject();

				float KeyTime = 0.0f;
				if (KeyObj->HasField(TEXT("time"))) KeyTime = (float)KeyObj->GetNumberField(TEXT("time"));
				else if (KeyObj->HasField(TEXT("Time"))) KeyTime = (float)KeyObj->GetNumberField(TEXT("Time"));

				float KeyValue = 0.0f;
				if (KeyObj->HasField(TEXT("value"))) KeyValue = (float)KeyObj->GetNumberField(TEXT("value"));
				else if (KeyObj->HasField(TEXT("Value"))) KeyValue = (float)KeyObj->GetNumberField(TEXT("Value"));

				RichCurve.AddKey(KeyTime, KeyValue);
			}
		}

		FNiagaraTypeDefinition TypeDef(UCurveFloat::StaticClass());
		FNiagaraVariable Var(TypeDef, FName(*FullParamName));
		if (UserStore.IndexOf(Var) == INDEX_NONE)
		{
			UserStore.AddParameter(Var, true);
		}
		UserStore.SetUObject(CurveFloatObj, Var);
	}
	else if (DataType.Equals(TEXT("CurveLinearColor"), ESearchCase::IgnoreCase))
	{
		UCurveLinearColor* CurveColorObj = NewObject<UCurveLinearColor>(System, NAME_None, RF_Transactional);
		if (!IsValid(CurveColorObj))
		{
			Result.Errors.Add(TEXT("Failed to create transient UCurveLinearColor object."));
			return Result;
		}

		CurveColorObj->FloatCurves[0].Reset();
		CurveColorObj->FloatCurves[1].Reset();
		CurveColorObj->FloatCurves[2].Reset();
		CurveColorObj->FloatCurves[3].Reset();

		const TArray<TSharedPtr<FJsonValue>>* CurveKeysArray = nullptr;
		if (Params->HasTypedField<EJson::Array>(TEXT("curve_keys")))
		{
			CurveKeysArray = &Params->GetArrayField(TEXT("curve_keys"));
		}
		else if (Params->HasTypedField<EJson::Array>(TEXT("CurveKeys")))
		{
			CurveKeysArray = &Params->GetArrayField(TEXT("CurveKeys"));
		}

		if (CurveKeysArray)
		{
			for (const TSharedPtr<FJsonValue>& KeyVal : *CurveKeysArray)
			{
				if (!KeyVal.IsValid() || KeyVal->Type != EJson::Object) continue;
				TSharedPtr<FJsonObject> KeyObj = KeyVal->AsObject();

				float KeyTime = 0.0f;
				if (KeyObj->HasField(TEXT("time"))) KeyTime = (float)KeyObj->GetNumberField(TEXT("time"));
				else if (KeyObj->HasField(TEXT("Time"))) KeyTime = (float)KeyObj->GetNumberField(TEXT("Time"));

				FLinearColor KeyColor(0.0f, 0.0f, 0.0f, 1.0f);
				if (KeyObj->HasField(TEXT("r")) || KeyObj->HasField(TEXT("R")))
				{
					if (KeyObj->HasField(TEXT("r"))) KeyColor.R = (float)KeyObj->GetNumberField(TEXT("r"));
					else if (KeyObj->HasField(TEXT("R"))) KeyColor.R = (float)KeyObj->GetNumberField(TEXT("R"));
					if (KeyObj->HasField(TEXT("g"))) KeyColor.G = (float)KeyObj->GetNumberField(TEXT("g"));
					else if (KeyObj->HasField(TEXT("G"))) KeyColor.G = (float)KeyObj->GetNumberField(TEXT("G"));
					if (KeyObj->HasField(TEXT("b"))) KeyColor.B = (float)KeyObj->GetNumberField(TEXT("b"));
					else if (KeyObj->HasField(TEXT("B"))) KeyColor.B = (float)KeyObj->GetNumberField(TEXT("B"));
					if (KeyObj->HasField(TEXT("a"))) KeyColor.A = (float)KeyObj->GetNumberField(TEXT("a"));
					else if (KeyObj->HasField(TEXT("A"))) KeyColor.A = (float)KeyObj->GetNumberField(TEXT("A"));
				}
				else if (KeyObj->HasField(TEXT("value")) || KeyObj->HasField(TEXT("Value")))
				{
					const TSharedPtr<FJsonValue>* ValF = KeyObj->HasField(TEXT("value")) ? &KeyObj->Values[TEXT("value")] : &KeyObj->Values[TEXT("Value")];
					if (ValF && ValF->IsValid())
					{
						if ((*ValF)->Type == EJson::Number)
						{
							float Scalar = (float)(*ValF)->AsNumber();
							KeyColor = FLinearColor(Scalar, Scalar, Scalar, 1.0f);
						}
						else if ((*ValF)->Type == EJson::Array)
						{
							TArray<TSharedPtr<FJsonValue>> Arr = (*ValF)->AsArray();
							KeyColor.R = (Arr.Num() > 0) ? (float)Arr[0]->AsNumber() : 0.0f;
							KeyColor.G = (Arr.Num() > 1) ? (float)Arr[1]->AsNumber() : 0.0f;
							KeyColor.B = (Arr.Num() > 2) ? (float)Arr[2]->AsNumber() : 0.0f;
							KeyColor.A = (Arr.Num() > 3) ? (float)Arr[3]->AsNumber() : 1.0f;
						}
					}
				}

				CurveColorObj->FloatCurves[0].AddKey(KeyTime, KeyColor.R);
				CurveColorObj->FloatCurves[1].AddKey(KeyTime, KeyColor.G);
				CurveColorObj->FloatCurves[2].AddKey(KeyTime, KeyColor.B);
				CurveColorObj->FloatCurves[3].AddKey(KeyTime, KeyColor.A);
			}
		}

		FNiagaraTypeDefinition TypeDef(UCurveLinearColor::StaticClass());
		FNiagaraVariable Var(TypeDef, FName(*FullParamName));
		if (UserStore.IndexOf(Var) == INDEX_NONE)
		{
			UserStore.AddParameter(Var, true);
		}
		UserStore.SetUObject(CurveColorObj, Var);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unsupported Niagara parameter data type: '%s'"), *DataType));
		return Result;
	}

	// 9. Recompile system, mark dirty, save package
	System->RequestCompile(false);

	UPackage* Package = System->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();

		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, System, *PackageFilename, SaveArgs);
		}
	}

	Result.bSuccess = WaitAndReportCompile(System, Result);
	Result.ResultMessage = FString::Printf(TEXT("Successfully set Niagara parameter '%s' (%s) on system '%s'"), *FullParamName, *DataType, *SystemPath);
	Result.ModifiedAssets.Add(SystemPath);
#else
	Result.Errors.Add(TEXT("Parameter configuration is only supported in Editor builds."));
#endif
	return Result;
}


