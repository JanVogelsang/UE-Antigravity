// Copyright 2026 Antigravity. All Rights Reserved.

#include "Niagara/AntigravityNiagaraActions.h"
#include "AntigravityCoreModule.h"
#include "AntigravitySettings.h"

// Niagara Runtime & Actor
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraComponent.h"
#include "NiagaraActor.h"
#include "NiagaraFunctionLibrary.h"

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
#include "ImageUtils.h"
#include "Viewport/AntigravityViewportActions.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

FAntigravityNiagaraActions::FAntigravityNiagaraActions() {}
FAntigravityNiagaraActions::~FAntigravityNiagaraActions() {}

FName FAntigravityNiagaraActions::GetActionName() const { return FName(TEXT("Niagara")); }
FText FAntigravityNiagaraActions::GetDisplayName() const { return FText::FromString(TEXT("Niagara Actions")); }
EAntigravityActionCategory FAntigravityNiagaraActions::GetCategory() const { return EAntigravityActionCategory::Level; }
EAntigravityRiskLevel FAntigravityNiagaraActions::GetDefaultRiskLevel() const { return EAntigravityRiskLevel::Medium; }
bool FAntigravityNiagaraActions::CanUndo() const { return true; }
bool FAntigravityNiagaraActions::UndoAction() { return false; }

TArray<FString> FAntigravityNiagaraActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_niagara_system"),
		TEXT("add_niagara_emitter"),
		TEXT("add_niagara_module"),
		TEXT("set_niagara_module_pin"),
		TEXT("compile_niagara_system"),
		TEXT("capture_niagara_system_isolated")
	};
}

bool FAntigravityNiagaraActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	if (ToolName == TEXT("create_niagara_system"))
	{
		if (!Params->HasField(TEXT("asset_path")))
		{
			OutErrors.Add(TEXT("Missing required field for create_niagara_system: asset_path"));
			return false;
		}
	}
	else
	{
		// All other tools require system_path (or asset_path as a fallback)
		if (!Params->HasField(TEXT("system_path")) && !Params->HasField(TEXT("asset_path")))
		{
			OutErrors.Add(TEXT("Missing required field: system_path"));
			return false;
		}

		if (ToolName == TEXT("add_niagara_emitter"))
		{
			if (!Params->HasField(TEXT("emitter_template")) || !Params->HasField(TEXT("emitter_name")))
			{
				OutErrors.Add(TEXT("Missing required field(s) for add_niagara_emitter: emitter_template, emitter_name"));
				return false;
			}
		}
		else if (ToolName == TEXT("add_niagara_module"))
		{
			if (!Params->HasField(TEXT("emitter_name")) || !Params->HasField(TEXT("phase")) || !Params->HasField(TEXT("module_type")))
			{
				OutErrors.Add(TEXT("Missing required field(s) for add_niagara_module: emitter_name, phase, module_type"));
				return false;
			}
		}
		else if (ToolName == TEXT("set_niagara_module_pin"))
		{
			if (!Params->HasField(TEXT("emitter_name")) || !Params->HasField(TEXT("phase")) ||
				!Params->HasField(TEXT("module_type")) || !Params->HasField(TEXT("pin_name")) || !Params->HasField(TEXT("value")))
			{
				OutErrors.Add(TEXT("Missing required field(s) for set_niagara_module_pin: emitter_name, phase, module_type, pin_name, value"));
				return false;
			}
		}
	}

	return true;
}

FAntigravityActionPlan FAntigravityNiagaraActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionPlan Plan;
	FString TargetPath = Params->HasField(TEXT("asset_path")) ? Params->GetStringField(TEXT("asset_path")) : Params->GetStringField(TEXT("system_path"));
	Plan.Summary = FString::Printf(TEXT("Niagara operation on %s"), *TargetPath);

	FAntigravityAction Action;
	Action.Description = Plan.Summary;
	Action.Category = EAntigravityActionCategory::Level;
	Action.RiskLevel = EAntigravityRiskLevel::Medium;
	Action.AffectedAssets.Add(TargetPath);
	Plan.Actions.Add(Action);
	Plan.MaxRiskLevel = EAntigravityRiskLevel::Medium;

	return Plan;
}

FAntigravityActionResult FAntigravityNiagaraActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	bool bIsReadOnly = (ToolName == TEXT("capture_niagara_system_isolated"));

	TOptional<FScopedTransaction> Transaction;
	if (!bIsReadOnly)
	{
		Transaction.Emplace(FText::FromString(TEXT("Antigravity Niagara Action")));
	}

	FAntigravityActionResult Result;
	Result.bSuccess = false;

	if (ToolName == TEXT("create_niagara_system"))          Result = ExecuteCreateSystem(Params, Result);
	else if (ToolName == TEXT("add_niagara_emitter"))        Result = ExecuteAddEmitter(Params, Result);
	else if (ToolName == TEXT("add_niagara_module"))         Result = ExecuteAddModule(Params, Result);
	else if (ToolName == TEXT("set_niagara_module_pin"))     Result = ExecuteSetModulePin(Params, Result);
	else if (ToolName == TEXT("compile_niagara_system"))     Result = ExecuteCompileSystem(Params, Result);
	else if (ToolName == TEXT("capture_niagara_system_isolated")) Result = ExecuteCaptureIsolated(Params, Result);
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown Niagara tool: '%s'"), *ToolName));
	}

	if (Transaction.IsSet() && !Result.bSuccess)
	{
		Transaction->Cancel();
	}

	return Result;
}

FAntigravityActionResult FAntigravityNiagaraActions::ExecuteCreateSystem(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
#if WITH_EDITOR
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UNiagaraSystemFactoryNew* Factory = NewObject<UNiagaraSystemFactoryNew>();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UNiagaraSystem::StaticClass(), Factory);
	UNiagaraSystem* NewSystem = Cast<UNiagaraSystem>(NewAsset);

	if (!NewSystem)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Niagara System at %s"), *AssetPath));
		return Result;
	}

	NewSystem->Modify();
	NewSystem->GetOutermost()->MarkPackageDirty();

	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(NewSystem->GetOutermost()->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(NewSystem->GetOutermost(), NewSystem, *PackageFilename, SaveArgs);
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

FAntigravityActionResult FAntigravityNiagaraActions::ExecuteAddEmitter(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
#if WITH_EDITOR
	FString SystemPath = Params->GetStringField(TEXT("system_path"));
	FString EmitterTemplate = Params->GetStringField(TEXT("emitter_template"));
	FString EmitterName = Params->GetStringField(TEXT("emitter_name"));

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	// Determine Template Emitter Path inside Engine/Niagara content
	FString TemplatePath;
	if (EmitterTemplate == TEXT("SpriteBurst"))       TemplatePath = TEXT("/Niagara/Templates/Emitters/SimpleSprite");
	else if (EmitterTemplate == TEXT("RibbonTrail"))   TemplatePath = TEXT("/Niagara/Templates/Emitters/SimpleRibbon");
	else if (EmitterTemplate == TEXT("MeshDebris"))    TemplatePath = TEXT("/Niagara/Templates/Emitters/SimpleMesh");
	else if (EmitterTemplate == TEXT("GPUSimulation")) TemplatePath = TEXT("/Niagara/Templates/Emitters/SimpleGPU");
	else                                              TemplatePath = TEXT("/Niagara/Templates/Emitters/SimpleSprite");

	UNiagaraEmitter* TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
	if (!TemplateEmitter)
	{
		// Fallback search in Common locations
		TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, TEXT("/Niagara/SimpleSprite.SimpleSprite"));
	}

	System->Modify();

	UNiagaraEmitter* NewEmitter = nullptr;
	if (TemplateEmitter)
	{
		NewEmitter = Cast<UNiagaraEmitter>(StaticDuplicateObject(TemplateEmitter, System, *EmitterName));
	}
	else
	{
		// Create blank fallback emitter
		NewEmitter = NewObject<UNiagaraEmitter>(System, *EmitterName, RF_Transactional);
	}

	if (!NewEmitter)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create emitter instance for %s"), *EmitterName));
		return Result;
	}

	NewEmitter->Modify();
	System->AddEmitterHandle(*NewEmitter, FName(*EmitterName), NewEmitter->GetExposedVersion().VersionGuid);

	System->GetOutermost()->MarkPackageDirty();
	Result.bSuccess = WaitAndReportCompile(System, Result);
	Result.ModifiedAssets.Add(SystemPath);
#else
	Result.Errors.Add(TEXT("Emitter configuration is only supported in the Editor."));
#endif
	return Result;
}

FAntigravityActionResult FAntigravityNiagaraActions::ExecuteAddModule(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
#if WITH_EDITOR
	FString SystemPath = Params->GetStringField(TEXT("system_path"));
	FString EmitterName = Params->GetStringField(TEXT("emitter_name"));
	FString Phase = Params->GetStringField(TEXT("phase"));
	FString ModuleType = Params->GetStringField(TEXT("module_type"));

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	FString FindError;
	UNiagaraGraph* Graph = FindGraphForPhase(System, EmitterName, Phase, FindError);
	if (!Graph)
	{
		Result.Errors.Add(FindError);
		return Result;
	}

	// Locate standard module script path
	FString ModulePath;
	if (ModuleType == TEXT("AddVelocity"))           ModulePath = TEXT("/Niagara/Modules/Physics/AddVelocity.AddVelocity");
	else if (ModuleType == TEXT("GravityForce"))     ModulePath = TEXT("/Niagara/Modules/Physics/GravityForce.GravityForce");
	else if (ModuleType == TEXT("Drag"))             ModulePath = TEXT("/Niagara/Modules/Physics/Drag.Drag");
	else if (ModuleType == TEXT("Collision"))        ModulePath = TEXT("/Niagara/Modules/Physics/Collision.Collision");
	else if (ModuleType == TEXT("LightRenderer"))    ModulePath = TEXT("/Niagara/Modules/Rendering/LightRenderer.LightRenderer");
	else if (ModuleType == TEXT("AccelerationForce"))ModulePath = TEXT("/Niagara/Modules/Physics/AccelerationForce.AccelerationForce");
	else                                             ModulePath = FString::Printf(TEXT("/Niagara/Modules/Physics/%s.%s"), *ModuleType, *ModuleType);

	UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
	if (!ModuleScript)
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara Script Module not found at %s"), *ModulePath));
		return Result;
	}

	System->Modify();
	Graph->Modify();

	UNiagaraNodeFunctionCall* NewNode = NewObject<UNiagaraNodeFunctionCall>(Graph, NAME_None, RF_Transactional);
	NewNode->FunctionScript = ModuleScript;
	if (ModuleScript->IsVersioningEnabled())
	{
		NewNode->SelectedScriptVersion = ModuleScript->GetExposedVersion().VersionGuid;
	}
	NewNode->AllocateDefaultPins();
	
	Graph->AddNode(NewNode, true, true);
	Graph->NotifyGraphChanged();

	System->GetOutermost()->MarkPackageDirty();
	Result.bSuccess = WaitAndReportCompile(System, Result);
	Result.ModifiedAssets.Add(SystemPath);
#else
	Result.Errors.Add(TEXT("Graph editing is only supported in the Editor."));
#endif
	return Result;
}

FAntigravityActionResult FAntigravityNiagaraActions::ExecuteSetModulePin(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
#if WITH_EDITOR
	FString SystemPath = Params->GetStringField(TEXT("system_path"));
	FString EmitterName = Params->GetStringField(TEXT("emitter_name"));
	FString Phase = Params->GetStringField(TEXT("phase"));
	FString ModuleType = Params->GetStringField(TEXT("module_type"));
	FString PinName = Params->GetStringField(TEXT("pin_name"));
	FString Value = Params->GetStringField(TEXT("value"));

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	FString FindError;
	UNiagaraGraph* Graph = FindGraphForPhase(System, EmitterName, Phase, FindError);
	if (!Graph)
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
		UNiagaraNodeFunctionCall* FnCall = Cast<UNiagaraNodeFunctionCall>(Node);
		if (FnCall && FnCall->FunctionScript && FnCall->FunctionScript->GetName().Contains(ModuleType))
		{
			TargetNode = FnCall;
			break;
		}
	}

	if (!TargetNode)
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

	System->GetOutermost()->MarkPackageDirty();
	Result.bSuccess = WaitAndReportCompile(System, Result);
	Result.ModifiedAssets.Add(SystemPath);
#else
	Result.Errors.Add(TEXT("Graph editing is only supported in the Editor."));
#endif
	return Result;
}

FAntigravityActionResult FAntigravityNiagaraActions::ExecuteCompileSystem(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString SystemPath = Params->GetStringField(TEXT("system_path"));
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	System->RequestCompile(true);
	Result.bSuccess = WaitAndReportCompile(System, Result);
	return Result;
}

FAntigravityActionResult FAntigravityNiagaraActions::ExecuteCaptureIsolated(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString SystemPath = Params->GetStringField(TEXT("system_path"));
	double DurationSeconds = 2.0;
	Params->TryGetNumberField(TEXT("duration_seconds"), DurationSeconds);
	int32 MaxDimension = 512;
	Params->TryGetNumberField(TEXT("max_dimension"), MaxDimension);

	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (!System)
	{
		Result.Errors.Add(FString::Printf(TEXT("Niagara System not found at %s"), *SystemPath));
		return Result;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Result.Errors.Add(TEXT("Active Editor World Context not found."));
		return Result;
	}

	// 1. Spawn Transient Niagara Actor and assign System
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	ANiagaraActor* NiagaraActor = World->SpawnActor<ANiagaraActor>(ANiagaraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!NiagaraActor || !NiagaraActor->GetNiagaraComponent())
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
	USceneCaptureComponent2D* CaptureComponent = CaptureActor ? CaptureActor->GetCaptureComponent2D() : nullptr;
	if (!CaptureComponent)
	{
		World->DestroyActor(NiagaraActor);
		Result.Errors.Add(TEXT("Failed to spawn transient Scene Capture 2D Actor."));
		return Result;
	}

	// Create Transient Render Target
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(World);
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
	FString FilePath = FAntigravityViewportActions::SavePixelsToDisk(StitchedPixels, MaxDimension, MaxDimension, MaxDimension, 90);

	// 6. Cleanup transient Actors
	World->DestroyActor(NiagaraActor);
	World->DestroyActor(CaptureActor);

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

UNiagaraGraph* FAntigravityNiagaraActions::FindGraphForPhase(UNiagaraSystem* System, const FString& EmitterName, const FString& PhaseStr, FString& OutError) const
{
#if WITH_EDITOR
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
	if (!Emitter)
	{
		OutError = FString::Printf(TEXT("Underlying UNiagaraEmitter is null for emitter handle '%s'."), *EmitterName);
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

	if (!TargetScript)
	{
		OutError = FString::Printf(TEXT("Niagara script for phase %s not found on emitter %s."), *PhaseStr, *EmitterName);
		return nullptr;
	}

	UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(TargetScript->GetSource(TargetScript->GetExposedVersion().VersionGuid));
	if (!ScriptSource || !ScriptSource->NodeGraph)
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

bool FAntigravityNiagaraActions::WaitAndReportCompile(UNiagaraSystem* System, FAntigravityActionResult& Result) const
{
	// WaitForCompilationComplete blocks game thread until compilation completes.
	System->WaitForCompilationComplete(true, false);

	// Extract compilation logs and performance metrics
	// If compiling failed, extract errors from output log or script states
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter;
		if (!Emitter) continue;

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
			if (!Script) continue;
			
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
		if (!Emitter) continue;

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
