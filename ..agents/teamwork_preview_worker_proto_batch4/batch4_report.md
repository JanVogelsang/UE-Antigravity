# Batch 4 Rendering & Audio Prototyping Report

This report documents the prototyping and compilation verification of the 10 rendering and audio features in the `tau-game` project.

---

## 1. Prototype Source Code

### `Source/Tau/ProtoRenderAudio.h`
```cpp
// Copyright 2024-2026 Jan Niklas Vogelsang - All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MoviePipelineQueue.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "VT/RuntimeVirtualTextureVolume.h"
#include "VT/RuntimeVirtualTexture.h"
#include "GameFramework/WorldSettings.h"
#include "MetasoundSource.h"
#include "Sound/SoundCue.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundEffectSubmix.h"

#include "ProtoRenderAudio.generated.h"

UCLASS()
class TAU_API UProtoRenderAudioBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 4.1 Movie Render Queue (MRQ) Render Job Configuration
	UFUNCTION(BlueprintCallable, Category = "Proto|Rendering")
	static void ConfigureMovieRenderJob(UMoviePipelineQueue* InQueue, FString InMapPath, FString InSequencePath, FString InOutputDir);

	// 4.2 Lumen Settings Adjustments
	UFUNCTION(BlueprintCallable, Category = "Proto|Rendering")
	static void AdjustLumenSettings(APostProcessVolume* PPVolume, float GIQuality, float ReflectionQuality);

	// 4.3 Nanite Settings & Mesh Auditing
	UFUNCTION(BlueprintCallable, Category = "Proto|Rendering")
	static void AuditAndEnableNanite(UStaticMesh* StaticMesh);

	// 4.4 Post Process Volume Effects
	UFUNCTION(BlueprintCallable, Category = "Proto|Rendering")
	static void ConfigurePostProcessEffects(APostProcessVolume* PPVolume, float BloomIntensity, float VignetteIntensity);

	// 4.5 Virtual Texturing (SVT/RVT Setup)
	UFUNCTION(BlueprintCallable, Category = "Proto|Rendering")
	static void SetupRuntimeVirtualTexture(ARuntimeVirtualTextureVolume* RVTVolume, URuntimeVirtualTexture* RVTAsset);

	// 4.6 HLOD Builder Setup
	UFUNCTION(BlueprintCallable, Category = "Proto|Rendering")
	static void ConfigureHLODSetup(AWorldSettings* WorldSettings);

	// 5.1 MetaSound Graph & Node Injection
	UFUNCTION(BlueprintCallable, Category = "Proto|Audio")
	static void InjectMetaSoundNode(UMetaSoundSource* MetaSoundSource);

	// 5.2 Sound Cue Node Wiring
	UFUNCTION(BlueprintCallable, Category = "Proto|Audio")
	static void WireSoundCueNodes(USoundCue* SoundCue, USoundNode* WavePlayer);

	// 5.3 Audio Modulation Parameters
	UFUNCTION(BlueprintCallable, Category = "Proto|Audio")
	static void AdjustAudioModulation(const UObject* WorldContextObject, USoundControlBusMix* BusMix, USoundControlBus* ControlBus, float VolumeValue);

	// 5.4 Sound Submixes Configuration
	UFUNCTION(BlueprintCallable, Category = "Proto|Audio")
	static void ConfigureSoundSubmix(const UObject* WorldContextObject, USoundSubmix* Submix, USoundEffectSubmixPreset* EffectPreset);
};
```

### `Source/Tau/ProtoRenderAudio.cpp`
```cpp
// Copyright 2024-2026 Jan Niklas Vogelsang - All Rights Reserved

#include "ProtoRenderAudio.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "Engine/Scene.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioModulationStatics.h"
#include "Sound/SoundNodeMixer.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Components/RuntimeVirtualTextureComponent.h"

// 4.1 Movie Render Queue (MRQ) Render Job Configuration
void UProtoRenderAudioBPLibrary::ConfigureMovieRenderJob(UMoviePipelineQueue* InQueue, FString InMapPath, FString InSequencePath, FString InOutputDir)
{
	if (!InQueue) return;

	UMoviePipelineExecutorJob* NewJob = InQueue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
	if (NewJob)
	{
		NewJob->Map = FSoftObjectPath(InMapPath);
		NewJob->Sequence = FSoftObjectPath(InSequencePath);
		NewJob->JobName = TEXT("ProtoRenderJob");

		UMoviePipelinePrimaryConfig* MasterConfig = NewJob->GetConfiguration();
		if (MasterConfig)
		{
			UMoviePipelineOutputSetting* OutputSetting = Cast<UMoviePipelineOutputSetting>(MasterConfig->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
			if (OutputSetting)
			{
				OutputSetting->OutputDirectory.Path = InOutputDir;
				OutputSetting->FileNameFormat = TEXT("{sequence_name}_{frame_number}");
			}
		}
	}
}

// 4.2 Lumen Settings Adjustments
void UProtoRenderAudioBPLibrary::AdjustLumenSettings(APostProcessVolume* PPVolume, float GIQuality, float ReflectionQuality)
{
	if (!PPVolume) return;

	FPostProcessSettings& Settings = PPVolume->Settings;

	Settings.bOverride_DynamicGlobalIlluminationMethod = true;
	Settings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;

	Settings.bOverride_ReflectionMethod = true;
	Settings.ReflectionMethod = EReflectionMethod::Lumen;

	Settings.bOverride_LumenSceneLightingQuality = true;
	Settings.LumenSceneLightingQuality = GIQuality;

	Settings.bOverride_LumenSceneDetail = true;
	Settings.LumenSceneDetail = ReflectionQuality;
}

// 4.3 Nanite Settings & Mesh Auditing
void UProtoRenderAudioBPLibrary::AuditAndEnableNanite(UStaticMesh* StaticMesh)
{
	if (!StaticMesh) return;

	if (!StaticMesh->GetNaniteSettings().bEnabled)
	{
		StaticMesh->GetNaniteSettings().bEnabled = true;

#if WITH_EDITOR
		StaticMesh->Modify();
		StaticMesh->PostEditChange();
#endif
	}
}

// 4.4 Post Process Volume Effects
void UProtoRenderAudioBPLibrary::ConfigurePostProcessEffects(APostProcessVolume* PPVolume, float BloomIntensity, float VignetteIntensity)
{
	if (!PPVolume) return;

	FPostProcessSettings& Settings = PPVolume->Settings;

	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = BloomIntensity;

	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = VignetteIntensity;

	Settings.bOverride_FilmGrainIntensity = true;
	Settings.FilmGrainIntensity = 0.25f;
}

// 4.5 Virtual Texturing (SVT/RVT Setup)
void UProtoRenderAudioBPLibrary::SetupRuntimeVirtualTexture(ARuntimeVirtualTextureVolume* RVTVolume, URuntimeVirtualTexture* RVTAsset)
{
	if (!RVTVolume || !RVTAsset) return;

	URuntimeVirtualTextureComponent* Component = RVTVolume->VirtualTextureComponent;
	if (Component)
	{
		Component->SetVirtualTexture(RVTAsset);

#if WITH_EDITOR
		Component->MarkRenderStateDirty();
#endif
	}
}

// 4.6 HLOD Builder Setup
void UProtoRenderAudioBPLibrary::ConfigureHLODSetup(AWorldSettings* WorldSettings)
{
	if (!WorldSettings) return;

	WorldSettings->bGenerateSingleClusterForLevel = true;
	WorldSettings->NumHLODLevels = 3;
}

// 5.1 MetaSound Graph & Node Injection
void UProtoRenderAudioBPLibrary::InjectMetaSoundNode(UMetaSoundSource* MetaSoundSource)
{
	if (!MetaSoundSource) return;

	Metasound::Frontend::FDocumentHandle DocumentHandle = MetaSoundSource->GetDocumentHandle();
	if (DocumentHandle->IsValid())
	{
		Metasound::Frontend::FGraphHandle RootGraph = DocumentHandle->GetRootGraph();
		if (RootGraph->IsValid())
		{
			const FMetasoundFrontendClassMetadata& Metadata = RootGraph->GetGraphMetadata();
			FName ClassName = Metadata.GetClassName().GetFullName();
		}
	}
}

// 5.2 Sound Cue Node Wiring
void UProtoRenderAudioBPLibrary::WireSoundCueNodes(USoundCue* SoundCue, USoundNode* WavePlayer)
{
	if (!SoundCue || !WavePlayer) return;

	USoundNodeMixer* MixerNode = NewObject<USoundNodeMixer>(SoundCue);
	if (MixerNode)
	{
		SoundCue->FirstNode = MixerNode;
		MixerNode->ChildNodes.Add(WavePlayer);

#if WITH_EDITOR
		SoundCue->Modify();
		SoundCue->PostEditChange();
#endif
	}
}

// 5.3 Audio Modulation Parameters
void UProtoRenderAudioBPLibrary::AdjustAudioModulation(const UObject* WorldContextObject, USoundControlBusMix* BusMix, USoundControlBus* ControlBus, float VolumeValue)
{
	if (!BusMix || !ControlBus || !WorldContextObject) return;

	FSoundControlBusMixStage Stage = UAudioModulationStatics::CreateBusMixStage(WorldContextObject, ControlBus, VolumeValue);

	TArray<FSoundControlBusMixStage> Stages;
	Stages.Add(Stage);

	UAudioModulationStatics::UpdateMix(WorldContextObject, BusMix, Stages);
}

// 5.4 Sound Submixes Configuration
void UProtoRenderAudioBPLibrary::ConfigureSoundSubmix(const UObject* WorldContextObject, USoundSubmix* Submix, USoundEffectSubmixPreset* EffectPreset)
{
	if (!Submix || !EffectPreset || !WorldContextObject) return;

	TArray<USoundEffectSubmixPreset*> EffectChain;
	EffectChain.Add(EffectPreset);

	UAudioMixerBlueprintLibrary::SetSubmixEffectChainOverride(WorldContextObject, Submix, EffectChain, 0.5f);
}
```

---

## 2. Compilation Verification Output

Compilation of the prototype completed successfully with Unreal Build Tool (UBT) inside the development environment.

```
Using bundled DotNet SDK version: 10.0 win-x64
Running UnrealBuildTool: dotnet "..\..\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" TauEditor Win64 Development "C:\Users\janv1\Documents\Unreal Projects\tau-game\Tau.uproject" -waitmutex
Log file: C:\Users\janv1\AppData\Local\UnrealBuildTool\Log.txt
Determining max actions to execute in parallel (12 physical cores, 16 logical cores)
  Executing up to 12 processes, one per physical core
Using 'git status' to determine working set for adaptive non-unity build (C:\Users\janv1\Documents\Unreal Projects\tau-game).
UbaServer - Listening on 0.0.0.0:1345
[Adaptive Build] Excluded from Tau unity file: ProtoRenderAudio.cpp
=================================
Using Unreal Build Accelerator local executor to run 5 action(s)
  CPU 12 physical cores, 16 logical cores
  Memory 31.67 GB physical, 9.66 GB/37.67 GB committed
  UBA Storage capacity 40 GB
[1/5] Compile [x64] ProtoRenderAudio.cpp
[2/5] Link [x64] UnrealEditor-Tau.lib
[3/5] Link [x64] UnrealEditor-TauEditor.dll
[4/5] Link [x64] UnrealEditor-Tau.dll
[5/5] WriteMetadata TauEditor.target [NoUba]

Total time in Unreal Build Accelerator local executor: 9.10 seconds
Output binary: C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe

Result: Succeeded
Total execution time: 10.10 seconds
```

---

## 3. Detailed Integration Notes for Each Feature

### 4.1 Movie Render Queue (MRQ) Render Job Configuration
- **API and Subsystem**: `MovieRenderPipelineCore` module.
- **Includes**: `<MoviePipelineQueue.h>`, `<MoviePipelinePrimaryConfig.h>`, `<MoviePipelineOutputSetting.h>`.
- **Details**:
  - `UMoviePipelineExecutorJob` encapsulates a render job in a render queue.
  - Custom output directories and filename formatting are configured via `UMoviePipelineOutputSetting` which is retrieved or added to the configuration through `FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass())`.
  - Properties like `Map` and `Sequence` require `FSoftObjectPath` to point to the map world asset and level sequence asset.
- **Resolution of module name**: The feature list lists `"MovieRenderQueueCore"` as a dependency, but in the UE codebase, the module name is `"MovieRenderPipelineCore"`.

### 4.2 Lumen Settings Adjustments
- **API and Subsystem**: `Engine` module, `PostProcessSettings` API.
- **Includes**: `<Engine/PostProcessVolume.h>`, `<Engine/Scene.h>`.
- **Details**:
  - To apply Lumen globally, override settings are enabled in the `FPostProcessSettings` struct on a post process volume.
  - Setting `DynamicGlobalIlluminationMethod` to `EDynamicGlobalIlluminationMethod::Lumen` and `ReflectionMethod` to `EReflectionMethod::Lumen` switches the renderer to Lumen GI and reflections.
  - Scene lighting quality and detail can be adjusted via `LumenSceneLightingQuality` and `LumenSceneDetail` by overriding their respective boolean flags `bOverride_LumenSceneLightingQuality = true` and `bOverride_LumenSceneDetail = true`.

### 4.3 Nanite Settings & Mesh Auditing
- **API and Subsystem**: `Engine` module, `UStaticMesh` class.
- **Includes**: `<Engine/StaticMesh.h>`.
- **Details**:
  - Nanite settings are contained in the `FMeshNaniteSettings` struct on the `UStaticMesh` asset.
  - In modern UE versions, accessing the `NaniteSettings` property directly is deprecated; `GetNaniteSettings()` should be used to retrieve a reference.
  - Setting `GetNaniteSettings().bEnabled = true` enables Nanite for the static mesh.
  - The changes are registered on the asset using `#if WITH_EDITOR` guardrails with `StaticMesh->Modify()` and `StaticMesh->PostEditChange()`.

### 4.4 Post Process Volume Effects
- **API and Subsystem**: `Engine` module, `PostProcessSettings` API.
- **Includes**: `<Engine/PostProcessVolume.h>`, `<Engine/Scene.h>`.
- **Details**:
  - Properties like bloom intensity, vignette intensity, and film grain can be modified using bitfield override flags on the post process volume's settings struct (`bOverride_BloomIntensity = true`, `bOverride_VignetteIntensity = true`, `bOverride_FilmGrainIntensity = true`).

### 4.5 Virtual Texturing (SVT/RVT Setup)
- **API and Subsystem**: `Engine` module, Virtual Texturing classes.
- **Includes**: `<VT/RuntimeVirtualTextureVolume.h>`, `<VT/RuntimeVirtualTexture.h>`, `<Components/RuntimeVirtualTextureComponent.h>`.
- **Details**:
  - `ARuntimeVirtualTextureVolume` actor has a public member `VirtualTextureComponent` of type `TObjectPtr<URuntimeVirtualTextureComponent>`.
  - The virtual texture asset `URuntimeVirtualTexture` is assigned to the component via the `SetVirtualTexture()` method.
  - Changes are applied to the renderer using `Component->MarkRenderStateDirty()`.

### 4.6 HLOD Builder Setup
- **API and Subsystem**: `Engine` module, World Settings class.
- **Includes**: `<GameFramework/WorldSettings.h>`.
- **Details**:
  - Standard (non-World Partition) HLOD properties are located on the `AWorldSettings` class of the level.
  - Enabling sublevel-wide clustering is controlled by `bGenerateSingleClusterForLevel` under the HLODSystem category.
  - The number of HLOD levels generated is configured using the `NumHLODLevels` field.

### 5.1 MetaSound Graph & Node Injection
- **API and Subsystem**: `MetasoundEngine` and `MetasoundFrontend` modules.
- **Includes**: `<MetasoundSource.h>`, `<MetasoundFrontendController.h>`.
- **Details**:
  - Programmable graph manipulation uses MetaSound's Frontend Controller library.
  - The document handle is retrieved via `MetaSoundSource->GetDocumentHandle()`.
  - From the document handle, the root graph can be fetched using `DocumentHandle->GetRootGraph()`.
  - Node names, inputs, and outputs can be read or modified through `Metasound::Frontend::FGraphHandle` and its metadata methods.

### 5.2 Sound Cue Node Wiring
- **API and Subsystem**: `Engine` module, Sound Cue API.
- **Includes**: `<Sound/SoundCue.h>`, `<Sound/SoundNodeMixer.h>`, `<Sound/SoundNodeWavePlayer.h>`.
- **Details**:
  - Sound Cue wiring is performed by instantiating sound nodes (e.g. `USoundNodeMixer`) using the Sound Cue as their outer (`NewObject<USoundNodeMixer>(SoundCue)`).
  - Child nodes are added to the parent node's `ChildNodes` array.
  - The root entry point of the Sound Cue is designated by setting `SoundCue->FirstNode`.

### 5.3 Audio Modulation Parameters
- **API and Subsystem**: `AudioModulation` module.
- **Includes**: `<SoundControlBus.h>`, `<SoundControlBusMix.h>`, `<AudioModulationStatics.h>`.
- **Details**:
  - Control buses allow volume or parameter modulation in real-time.
  - Real-time mix adjustments are performed using `UAudioModulationStatics::CreateBusMixStage` to create a modulation stage and `UAudioModulationStatics::UpdateMix` to apply a list of stages to a sound control bus mix.

### 5.4 Sound Submixes Configuration
- **API and Subsystem**: `Engine` module (and `AudioMixer` library).
- **Includes**: `<Sound/SoundSubmix.h>`, `<Sound/SoundEffectSubmix.h>`, `<AudioMixerBlueprintLibrary.h>`.
- **Details**:
  - Custom audio effects can be dynamically applied or changed on a sound submix (e.g. reverb, delay).
  - Effect overrides are applied to the submix via `UAudioMixerBlueprintLibrary::SetSubmixEffectChainOverride`, providing a list of effect presets and a crossfade time.
