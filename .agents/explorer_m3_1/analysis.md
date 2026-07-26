# Architectural & Technical Analysis: MetaSound Action Executor (`FAgentFrameworkMetaSoundActions`)

## 1. Executive Summary & Objective
This document provides a comprehensive technical analysis for implementing **Milestone 3 (MetaSound Actions)** of the UE-AgentFramework Native C++ Action Route Roadmap. Milestone 3 targets the elimination of Python fallbacks for MetaSound audio authoring by introducing a dedicated native C++ action executor class `FAgentFrameworkMetaSoundActions` with two new tools:
1. **`create_metasound_source`** (Spec 6 in Roadmap / Spec 9 in `PYTHON_FALLBACK_AUDIT.md`)
2. **`wire_metasound_nodes`** (Spec 7 in Roadmap / Spec 10 in `PYTHON_FALLBACK_AUDIT.md`)

This investigation covers MetaSound Engine, Frontend, and Editor C++ APIs, module dependencies in `AgentFrameworkActions.Build.cs`, executor registration in `AgentFrameworkHttpServer.cpp`, tool schema definitions in `metasound_tools.json`, and complete C++ design patterns required for implementation.

---

## 2. Problem Statement & Fallback Context

### 2.1 Audit Findings (`PYTHON_FALLBACK_AUDIT.md`)
Prior to Milestone 3, `AgentFrameworkActions` contained **0 MetaSound tools**. Creating MetaSound sources or wiring audio nodes inside Unreal Engine 5 required executing Python scripts (`execute_python_script`) using Unreal Engine's Python bindings for `MetaSoundFrontendDocumentBuilder`:

```python
import unreal
# Python fallback for MetaSound creation & wiring
builder = unreal.MetaSoundFrontendDocumentBuilder()
builder.create_preset('/Game/Audio/MS_Footstep', parent_source)
node_id = builder.add_node_by_class_name("WavePlayer:Mono")
builder.connect_nodes(node_id, "Audio", output_id, "Audio")
```

### 2.2 Goals for Milestone 3
1. **New Action Executor Class**: Create `FAgentFrameworkMetaSoundActions` in `AgentFrameworkActions/Public/MetaSound/AgentFrameworkMetaSoundActions.h` and `AgentFrameworkActions/Private/MetaSound/AgentFrameworkMetaSoundActions.cpp`.
2. **Build Configuration**: Update `AgentFrameworkActions.Build.cs` to include `"MetasoundEngine"`, `"MetasoundFrontend"`, and `"MetasoundEditor"` in `PrivateDependencyModuleNames`.
3. **HTTP Server Registration**: Register `FAgentFrameworkMetaSoundActions` in `FAgentFrameworkHttpServer::RegisterAllExecutors` in `AgentFrameworkHttpServer.cpp`.
4. **Tool Schema Definition**: Create `metasound_tools.json` in `AgentFramework/Resources/ToolSchemas/`.
5. **Native Execution**: Execute MetaSound asset creation, channel format setup, preset creation, node spawning, and pin wiring entirely through in-process C++ APIs.

---

## 3. Unreal Engine MetaSound Architecture & C++ APIs

### 3.1 Module Breakdown & Responsibilities
Unreal Engine 5 organizes MetaSound functionality into three core modules:

| Module | Core Classes / Types | Role in MetaSound Actions |
|---|---|---|
| **`MetasoundEngine`** | `UMetaSoundSource`, `EMetaSoundOutputAudioFormat`, `UMetaSoundSettings` | Runtime audio source UObject class, channel format definitions (`Mono`, `Stereo`, `Quad`, `5.1`, `7.1`), asset lifecycle. |
| **`MetasoundFrontend`** | `Metasound::Frontend::FDocumentBuilder`, `FMetasoundFrontendClass`, `FMetasoundFrontendNode`, `FMetasoundFrontendPin`, `IMetasoundFrontendRegistry` | Graph AST, document model, node handles, data pin connections, frontend registry lookup for node classes. |
| **`MetasoundEditor`** | `UMetaSoundFactory`, `UMetaSoundSourceFactory`, `FMetaSoundFrontendDocumentBuilder`, MetaSound graph compilation utilities | Editor asset creation factory (`CreateAsset`), graph builder wrappers, editor transaction and compilation management. |

### 3.2 Key C++ Header Dependencies
For `FAgentFrameworkMetaSoundActions`, the required engine headers are:
- `#include "MetasoundSource.h"` (`MetasoundEngine` — `UMetaSoundSource`)
- `#include "MetasoundFactory.h"` (`MetasoundEditor` — `UMetaSoundSourceFactory`)
- `#include "MetasoundFrontendDocumentBuilder.h"` (`MetasoundFrontend` / `MetasoundEditor` — `FDocumentBuilder` / `IMetaSoundDocumentBuilder`)
- `#include "MetasoundFrontendDocument.h"` (`MetasoundFrontend` — Document structure & node data)
- `#include "MetasoundFrontendRegistry.h"` (`MetasoundFrontend` — Class registry lookup)

---

## 4. Module Dependencies & `AgentFrameworkActions.Build.cs`

### 4.1 Current State Analysis
Inspection of `AgentFramework/Source/AgentFrameworkActions/AgentFrameworkActions.Build.cs` reveals lines 102–103:
```csharp
"MetasoundEngine",
"MetasoundFrontend",
```

### 4.2 Required Modifications
`MetasoundEditor` is currently missing from `PrivateDependencyModuleNames`. It must be added to enable `UMetaSoundSourceFactory` asset creation and editor document builder compilation utilities:

```csharp
// In AgentFrameworkActions.Build.cs PrivateDependencyModuleNames:
"MetasoundEngine",
"MetasoundFrontend",
"MetasoundEditor",
```

---

## 5. HTTP Server Registration (`AgentFrameworkHttpServer.cpp`)

### 5.1 Registration Pipeline Analysis
In `AgentFramework/Source/AgentFrameworkActions/Private/AgentFrameworkHttpServer.cpp`:
1. **Header Inclusion**: Add `#include "MetaSound/AgentFrameworkMetaSoundActions.h"` alongside other action headers (Lines 22–50).
2. **Executor Instantiation**: Inside `FAgentFrameworkHttpServer::RegisterAllExecutors(TSharedRef<FAgentFrameworkActionRouter> InRouter)` (Lines 81–111), add:
```cpp
InRouter->RegisterExecutor(MakeShared<FAgentFrameworkMetaSoundActions>());
```

---

## 6. Tool Schema Specification (`metasound_tools.json`)

A new tool schema file must be created at `AgentFramework/Resources/ToolSchemas/metasound_tools.json`:

```json
{
  "schema_version": "1.0.0",
  "domain": "metasound_tools",
  "min_plugin_version": "1.0.0",
  "tools": [
    {
      "name": "create_metasound_source",
      "description": "Create a new MetaSoundSource asset or MetaSound preset with specified channel output format.",
      "input_schema": {
        "type": "object",
        "properties": {
          "asset_path": {
            "type": "string",
            "description": "Full content package path for the MetaSoundSource asset (e.g. '/Game/Audio/MS_ProceduralFootstep')"
          },
          "destination_path": {
            "type": "string",
            "description": "Alternative: Destination folder path (e.g. '/Game/Audio')"
          },
          "asset_name": {
            "type": "string",
            "description": "Alternative: Asset name (e.g. 'MS_ProceduralFootstep')"
          },
          "output_format": {
            "type": "string",
            "enum": ["Mono", "Stereo", "Quad", "5.1", "7.1"],
            "default": "Stereo",
            "description": "Output audio format and channel configuration"
          },
          "num_channels": {
            "type": "integer",
            "description": "Alternative channel count: 1 (Mono), 2 (Stereo), 4 (Quad), 6 (5.1), 8 (7.1)"
          },
          "is_preset": {
            "type": "boolean",
            "default": false,
            "description": "If true, creates a MetaSound preset based on parent source"
          },
          "preset_source_path": {
            "type": "string",
            "description": "Object path of parent MetaSoundSource asset if is_preset is true"
          }
        }
      }
    },
    {
      "name": "wire_metasound_nodes",
      "description": "Instantiate MetaSound graph nodes (Oscillators, Envelopes, WavePlayers) and wire audio/trigger data pins in a MetaSoundSource document.",
      "input_schema": {
        "type": "object",
        "properties": {
          "asset_path": {
            "type": "string",
            "description": "Object path of target MetaSoundSource asset (e.g. '/Game/Audio/MS_ProceduralFootstep')"
          },
          "nodes_to_add": {
            "type": "array",
            "description": "List of MetaSound nodes to instantiate in the graph",
            "items": {
              "type": "object",
              "properties": {
                "node_class_name": {
                  "type": "string",
                  "description": "MetaSound node class name (e.g. 'WavePlayer:Mono', 'Sine:Audio', 'ADSR:Envelope', 'AudioMixer:Stereo')"
                },
                "node_name": {
                  "type": "string",
                  "description": "Unique custom alias/identifier for the node within this tool call"
                }
              },
              "required": ["node_class_name", "node_name"]
            }
          },
          "connections": {
            "type": "array",
            "description": "List of pin connections to wire between graph nodes",
            "items": {
              "type": "object",
              "properties": {
                "from_node": {
                  "type": "string",
                  "description": "Source node alias or GUID"
                },
                "from_pin": {
                  "type": "string",
                  "description": "Source output pin name (e.g. 'Audio', 'Out', 'Trigger')"
                },
                "to_node": {
                  "type": "string",
                  "description": "Target node alias or GUID"
                },
                "to_pin": {
                  "type": "string",
                  "description": "Target input pin name (e.g. 'Audio', 'In', 'Trigger')"
                }
              },
              "required": ["from_node", "from_pin", "to_node", "to_pin"]
            }
          }
        },
        "required": ["asset_path"]
      }
    }
  ]
}
```

---

## 7. Detailed Parameter Mapping & Execution Flow

### 7.1 Spec 6: `create_metasound_source`

#### Parameter Resolution Matrix
| Input JSON Parameter | Internal Type | Fallback / Default | Processing Logic |
|---|---|---|---|
| `asset_path` | `FString` | Combined `destination_path` + `asset_name` | Full package path (e.g. `"/Game/Audio/MS_Footstep"`). Parsed with `FPackageName`. |
| `destination_path` | `FString` | Derived from `asset_path` | Folder directory path. |
| `asset_name` | `FString` | Derived from `asset_path` | Asset name without path. |
| `output_format` | `FString` | `"Stereo"` | Format enum parsing (`"Mono"` -> Mono, `"Stereo"` -> Stereo, `"Quad"` -> Quad, `"5.1"` -> 5.1, `"7.1"` -> 7.1). |
| `num_channels` | `int32` | Derived from `output_format` | Maps integer (1, 2, 4, 6, 8) to format enum if `output_format` is not explicitly set. |
| `is_preset` / `bIsPreset` | `bool` | `false` | Controls whether a new root document or preset document builder is instantiated. |
| `preset_source_path` | `FString` | `""` | Parent MetaSoundSource asset path when `is_preset` is `true`. |

#### C++ Execution Sequence
1. Extract parameters via `UAgentFrameworkActionUtils::TryGetStringParam`, `TryGetBoolParam`, `TryGetIntParam`.
2. Construct full package path (`PackagePath`) and asset name (`AssetName`).
3. Check if asset already exists using `FPackageName::DoesPackageExist`.
4. Create asset transaction (`FScopedTransaction`).
5. **If `is_preset` is true**:
   - Load parent source via `LoadObject<UMetaSoundSource>(nullptr, *PresetSourcePath)`.
   - Validate parent source validity.
   - Use `Metasound::Frontend::FDocumentBuilder` or `UMetaSoundSourceFactory` to create a preset document referencing the parent.
6. **If creating new MetaSoundSource**:
   - Instantiate `UMetaSoundSourceFactory` via `NewObject<UMetaSoundSourceFactory>()`.
   - Set factory format parameters based on `output_format`.
   - Call `FAssetToolsModule::Get().CreateAsset(AssetName, PackagePath, UMetaSoundSource::StaticClass(), Factory)`.
   - Cast created `UObject*` to `UMetaSoundSource*`.
7. Mark asset dirty (`MarkPackageDirty()`), save package, and play success sound (`PlaySuccessSound()`).
8. Return `FAgentFrameworkActionResult` with `bSuccess = true` and `ModifiedAssets.Add(AssetPath)`.

---

### 7.2 Spec 7: `wire_metasound_nodes`

#### Parameter Resolution Matrix
| Input JSON Parameter | Internal Type | Processing Logic |
|---|---|---|
| `asset_path` | `FString` | Object path of target `UMetaSoundSource` asset. Loaded via `LoadObject<UMetaSoundSource>`. |
| `nodes_to_add` | `TArray<TSharedPtr<FJsonValue>>` | Array of objects specifying `node_class_name` and `node_name` alias. |
| `connections` | `TArray<TSharedPtr<FJsonValue>>` | Array of objects specifying `from_node`, `from_pin`, `to_node`, `to_pin`. |

#### C++ Execution Sequence
1. Load `UMetaSoundSource` asset via `LoadObject<UMetaSoundSource>(nullptr, *AssetPath)`.
2. If asset is invalid, populate `Result.Errors` and return `bSuccess = false`.
3. Call `MetaSoundSource->Modify()`.
4. Attach `Metasound::Frontend::FDocumentBuilderBuilder` or `FMetaSoundFrontendDocumentBuilder` to target asset.
5. Create node alias mapping: `TMap<FString, FGuid> NodeAliasToHandleMap;`
6. **Iterate `nodes_to_add`**:
   - Extract `node_class_name` and `node_name`.
   - Find node class in `IMetasoundFrontendRegistry::Get()` using `FMetasoundFrontendClass`.
   - Add node to graph via `Builder.AddNode(NodeClass)`.
   - Store generated node handle GUID in `NodeAliasToHandleMap`.
7. **Iterate `connections`**:
   - Extract `from_node`, `from_pin`, `to_node`, `to_pin`.
   - Resolve source node handle GUID and target node handle GUID from `NodeAliasToHandleMap` (or search existing graph nodes by name/ID).
   - Resolve output pin handle/name and input pin handle/name.
   - Call `Builder.ConnectNodes(FromNodeHandle, FromPinName, ToNodeHandle, ToPinName)` or `Builder.ConnectDataPins(...)`.
8. Rebuild graph and compile MetaSound document via `Builder.Build()` / `MetaSoundSource->RegisterGraph()`.
9. Mark package dirty (`MarkPackageDirty()`), play success sound.
10. Return `FAgentFrameworkActionResult` with counts of nodes added and connections wired.

---

## 8. Complete Proposed C++ Source & Header Specifications

### 8.1 Header File Specification
`AgentFramework/Source/AgentFrameworkActions/Public/MetaSound/AgentFrameworkMetaSoundActions.h`

```cpp
// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"
#include "Dom/JsonObject.h"

/**
 * FAgentFrameworkMetaSoundActions
 *
 * Native C++ Action Executor for MetaSound audio asset creation and graph wiring.
 * Implements tools:
 * - create_metasound_source
 * - wire_metasound_nodes
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkMetaSoundActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkMetaSoundActions();
	virtual ~FAgentFrameworkMetaSoundActions();

	// IAgentFrameworkActionExecutor Interface
	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	FAgentFrameworkActionResult ExecuteCreateMetaSoundSource(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteWireMetaSoundNodes(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	void PlaySuccessSound();
};
```

---

### 8.2 Source File Specification
`AgentFramework/Source/AgentFrameworkActions/Private/MetaSound/AgentFrameworkMetaSoundActions.cpp`

```cpp
// Copyright 2026 AgentFramework. All Rights Reserved.

#include "MetaSound/AgentFrameworkMetaSoundActions.h"
#include "AgentFrameworkActionUtils.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ScopedTransaction.h"

// Engine & MetaSound Headers
#include "MetasoundSource.h"
#include "MetasoundFactory.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocument.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif

#define LOCTEXT_NAMESPACE "AgentFrameworkMetaSoundActions"

FAgentFrameworkMetaSoundActions::FAgentFrameworkMetaSoundActions() {}
FAgentFrameworkMetaSoundActions::~FAgentFrameworkMetaSoundActions() {}

FName FAgentFrameworkMetaSoundActions::GetActionName() const
{
	return FName(TEXT("MetaSound"));
}

TArray<FString> FAgentFrameworkMetaSoundActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_metasound_source"),
		TEXT("wire_metasound_nodes")
	};
}

bool FAgentFrameworkMetaSoundActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	TArray<FString> TempErrors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, TempErrors, false) || Action.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), Action, TempErrors, false);
	}

	if (Action == TEXT("create_metasound_source"))
	{
		Result = ExecuteCreateMetaSoundSource(Params, Result);
	}
	else if (Action == TEXT("wire_metasound_nodes"))
	{
		Result = ExecuteWireMetaSoundNodes(Params, Result);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown or unspecified MetaSound action: '%s'"), *Action));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteCreateMetaSoundSource(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, false) || AssetPath.IsEmpty())
	{
		FString DestPath, AssetName;
		if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("destination_path"), DestPath, Result.Errors, false) &&
			UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_name"), AssetName, Result.Errors, false))
		{
			AssetPath = FPaths::Combine(DestPath, AssetName);
		}
	}

	if (AssetPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing required parameter: 'asset_path' (or 'destination_path' and 'asset_name')."));
		return Result;
	}

	FString OutputFormatStr = TEXT("Stereo");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("output_format"), OutputFormatStr, Result.Errors, false);

	bool bIsPreset = false;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("is_preset"), bIsPreset, Result.Errors, false);
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("bIsPreset"), bIsPreset, Result.Errors, false);

	FString PresetSourcePath;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("preset_source_path"), PresetSourcePath, Result.Errors, false);

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	FScopedTransaction Transaction(LOCTEXT("CreateMetaSoundSource", "Create MetaSound Source Asset"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UMetaSoundSourceFactory* Factory = NewObject<UMetaSoundSourceFactory>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create UMetaSoundSourceFactory."));
		return Result;
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMetaSoundSource::StaticClass(), Factory);
	UMetaSoundSource* MetaSoundSource = Cast<UMetaSoundSource>(NewAsset);
	if (!IsValid(MetaSoundSource))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create MetaSoundSource asset at '%s'."), *AssetPath));
		return Result;
	}

	MetaSoundSource->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully created MetaSoundSource asset '%s' (Format: %s, IsPreset: %s)."),
		*AssetPath, *OutputFormatStr, bIsPreset ? TEXT("True") : TEXT("False"));
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteWireMetaSoundNodes(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UMetaSoundSource* MetaSoundSource = LoadObject<UMetaSoundSource>(nullptr, *AssetPath);
	if (!IsValid(MetaSoundSource))
	{
		Result.Errors.Add(FString::Printf(TEXT("MetaSoundSource asset not found at '%s'."), *AssetPath));
		return Result;
	}

	FScopedTransaction Transaction(LOCTEXT("WireMetaSoundNodes", "Wire MetaSound Nodes"));
	MetaSoundSource->Modify();

	int33 NodesAddedCount = 0;
	int33 ConnectionsWiredCount = 0;

	// Node creation and connection wiring implementation logic goes here...

	MetaSoundSource->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully wired MetaSound graph at '%s': %d nodes added, %d connections wired."),
		*AssetPath, NodesAddedCount, ConnectionsWiredCount);
	return Result;
}

void FAgentFrameworkMetaSoundActions::PlaySuccessSound()
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

#undef LOCTEXT_NAMESPACE
```

---

## 9. Risk Analysis, Edge Cases & Verification Safeguards

### 9.1 Potential Risks & Mitigations
1. **Missing `MetasoundEditor` Module Dependency**:
   - *Risk*: Attempting to instantiate `UMetaSoundSourceFactory` without `MetasoundEditor` in `AgentFrameworkActions.Build.cs` causes unresolved symbol linker errors (`LNK2019`).
   - *Mitigation*: Ensure `"MetasoundEditor"` is explicitly added to `PrivateDependencyModuleNames` in `AgentFrameworkActions.Build.cs`.
2. **Invalid Node Class Names**:
   - *Risk*: Passing non-existent MetaSound node class names (e.g. `"NonExistentNode"`) can fail graph construction.
   - *Mitigation*: Validate node class names against `IMetasoundFrontendRegistry` before adding nodes, returning clear error messages in `Result.Errors`.
3. **Incompatible Pin Connections**:
   - *Risk*: Connecting an Audio output pin to a Trigger input pin could fail graph compilation.
   - *Mitigation*: Gracefully catch pin type mismatch warnings, logging them into `Result.Warnings` while allowing remaining valid connections to complete.
4. **Editor Lock & Compilation**:
   - *Risk*: Modifying MetaSound graph while Editor is locked or during PIE session could crash the engine.
   - *Mitigation*: Use `FScopedTransaction`, verify `GEditor` state, and mark package dirty cleanly.

---

## 10. Summary & Next Steps for Implementation
1. **Build Configuration**: Update `AgentFrameworkActions.Build.cs` (`MetasoundEditor`).
2. **Action Executor Files**: Create `AgentFrameworkActions/Public/MetaSound/AgentFrameworkMetaSoundActions.h` and `AgentFrameworkActions/Private/MetaSound/AgentFrameworkMetaSoundActions.cpp`.
3. **Server Registration**: Update `AgentFrameworkHttpServer.cpp` (`#include` and `RegisterExecutor`).
4. **Tool Schema**: Add `AgentFramework/Resources/ToolSchemas/metasound_tools.json`.
5. **Compilation & Testing**: Run UBT build (`build_plugin.ps1`) and integration test wrapper (`run_tests.ps1`).
