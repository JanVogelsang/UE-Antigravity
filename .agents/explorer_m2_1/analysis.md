# Architectural & Technical Analysis: PBR Material Auto-Wiring Action (`create_pbr_material_from_textures`)

## 1. Objective & Scope
This document provides a comprehensive technical analysis for implementing Spec 5 / Spec 7 (`create_pbr_material_from_textures`) as part of Milestone 2 (Material Actions). The goal is to consolidate multi-step PBR material construction (which currently requires 5–6 individual tool calls in Python/REST) into a single, high-performance C++ action in `FAgentFrameworkMaterialActions`.

---

## 2. Existing Material Actions & Code Architecture

### 2.1 File Locations
- **Header File**: `AgentFramework/Source/AgentFrameworkActions/Public/Material/AgentFrameworkMaterialActions.h`
- **Source File**: `AgentFramework/Source/AgentFrameworkActions/Private/Material/AgentFrameworkMaterialActions.cpp`
- **Tool Schema File**: `AgentFramework/Resources/ToolSchemas/material_tools.json`
- **Registration Point**: `AgentFramework/Source/AgentFrameworkActions/Private/AgentFrameworkHttpServer.cpp` (Line 95: `InRouter->RegisterExecutor(MakeShared<FAgentFrameworkMaterialActions>());`)

### 2.2 Current Class Structure
`FAgentFrameworkMaterialActions` inherits from `IAgentFrameworkActionExecutor`.
- `GetActionName()` returns `FName(TEXT("Material"))`.
- `GetSupportedToolNames()` currently returns:
  - `create_material`
  - `create_material_instance`
  - `add_material_expression`
  - `connect_material_property`
  - `capture_material`
- Parameter validation is performed in `ValidateParams()`, using `UAgentFrameworkActionUtils::TryGetStringParam()`.
- Action execution is dispatched in `ExecuteAction()` based on the `_tool_name` parameter.

---

## 3. C++ Pattern Identification & API Usage

Based on analysis of `AgentFrameworkMaterialActions.cpp` (Lines 120–283 & 285–381) and engine APIs:

### 3.1 Material Asset Creation & Factory Pattern
- **Package & Name Resolution**:
  ```cpp
  FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
  FString AssetName = FPackageName::GetShortName(AssetPath);
  ```
- **Asset Creation via AssetTools**:
  ```cpp
  FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
  IAssetTools& AssetTools = AssetToolsModule.Get();
  UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
  UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory);
  UMaterial* NewMaterial = Cast<UMaterial>(NewAsset);
  ```
- **Transaction & Undo Support**:
  ```cpp
  FScopedTransaction Transaction(FText::FromString(TEXT("Create PBR Material From Textures")));
  NewMaterial->Modify();
  ```

### 3.2 Material Properties (BlendMode, ShadingModel, TwoSided)
- **Blend Mode Setup**:
  Parse `blend_mode` string parameter into `EBlendMode`:
  - `"Opaque"` -> `BLEND_Opaque` (default)
  - `"Masked"` -> `BLEND_Masked`
  - `"Translucent"` -> `BLEND_Translucent`
  - `"Additive"` -> `BLEND_Additive`
  Assign to `NewMaterial->BlendMode`.
- **Shading Model Setup**:
  Parse `shading_model` string parameter into `EMaterialShadingModel`:
  - `"DefaultLit"` -> `MSM_DefaultLit` (default)
  - `"Unlit"` -> `MSM_Unlit`
  - `"Subsurface"` -> `MSM_Subsurface`
  - `"ClearCoat"` -> `MSM_ClearCoat`
  Assign to `NewMaterial->SetShadingModel(ShadingModel)` or update `NewMaterial->ShadingModels`.
- **Two-Sided Flag**:
  Parse `two_sided` / `bTwoSided` boolean parameter. Set `NewMaterial->TwoSided = bTwoSided`.

### 3.3 Texture Parameter Expression Node Spawning
Instead of non-parameterized `UMaterialExpressionTextureSample`, `UMaterialExpressionTextureSampleParameter2D` should be spawned so that created materials can be dynamically overridden by Material Instances.

- **Expression Node Instantiation**:
  ```cpp
  UMaterialExpressionTextureSampleParameter2D* TexExpr = Cast<UMaterialExpressionTextureSampleParameter2D>(
      UMaterialEditingLibrary::CreateMaterialExpression(NewMaterial, UMaterialExpressionTextureSampleParameter2D::StaticClass(), PosX, PosY));
  ```
- **Texture Asset Loading & Parameter Naming**:
  ```cpp
  UTexture2D* LoadedTexture = LoadObject<UTexture2D>(nullptr, *TexturePath);
  if (IsValid(LoadedTexture))
  {
      TexExpr->Texture = LoadedTexture;
  }
  TexExpr->ParameterName = FName(*ParamName);
  ```
- **Sampler Type Assignment**:
  - Normal map textures: `TexExpr->SamplerType = SAMPLERTYPE_Normal;`
  - Color textures (BaseColor, Emissive): `TexExpr->SamplerType = SAMPLERTYPE_Color;`
  - Data / Linear textures (Roughness, Metallic, AO, Specular): `TexExpr->SamplerType = SAMPLERTYPE_LinearColor;`

### 3.4 PBR Pin Connections (`EMaterialProperty`)
Pin connections are made via `UMaterialEditingLibrary::ConnectMaterialProperty(Expression, OutputPinName, MatProperty)`.

Supported PBR pin mapping matrix:
| Texture Channel Parameter | `EMaterialProperty` Enum | Parameter Node Name | Output Pin | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `BaseColorTexturePath` / `base_color_texture_path` | `MP_BaseColor` | `BaseColorMap` | `""` (RGB) | Required |
| `NormalTexturePath` / `normal_texture_path` | `MP_Normal` | `NormalMap` | `""` (RGB) | Optional, sets `SAMPLERTYPE_Normal` |
| `RoughnessTexturePath` / `roughness_texture_path` | `MP_Roughness` | `RoughnessMap` | `""` (or `R` if single channel) | Optional, sets `SAMPLERTYPE_LinearColor` |
| `MetallicTexturePath` / `metallic_texture_path` | `MP_Metallic` | `MetallicMap` | `""` (or `R` if single channel) | Optional, sets `SAMPLERTYPE_LinearColor` |
| `AOTexturePath` / `ao_texture_path` | `MP_AmbientOcclusion` | `AOMap` | `""` (or `R` if single channel) | Optional, sets `SAMPLERTYPE_LinearColor` |
| `SpecularTexturePath` / `specular_texture_path` | `MP_Specular` | `SpecularMap` | `""` | Optional |
| `EmissiveTexturePath` / `emissive_texture_path` | `MP_EmissiveColor` | `EmissiveMap` | `""` | Optional |
| `OpacityTexturePath` / `opacity_texture_path` | `MP_Opacity` / `MP_OpacityMask` | `OpacityMap` | `""` | Optional; uses `MP_OpacityMask` if `BlendMode == BLEND_Masked` |

### 3.5 Graph Node Placement & Auto-Layout
To ensure clean graph presentation in the Material Editor:
- Start X position: `-400`
- Y increment per node: `+220` (e.g. BaseColor at `(-400, 0)`, Normal at `(-400, 220)`, Roughness at `(-400, 440)`, Metallic at `(-400, 660)`, AO at `(-400, 880)`).

### 3.6 Material Compilation & Asset Saving/Dirtying
Following the established pattern in `ExecuteCreateMaterial` (Lines 263–277):
```cpp
UMaterialEditingLibrary::RecompileMaterial(NewMaterial);

UPackage* Package = NewMaterial->GetOutermost();
if (IsValid(Package))
{
    Package->MarkPackageDirty();
    FString PackageFilename;
    if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
    {
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Standalone;
        UPackage::SavePackage(Package, NewMaterial, *PackageFilename, SaveArgs);
    }
}
FAssetRegistryModule::AssetCreated(NewMaterial);
```

---

## 4. Proposed JSON Request & Response Schemas

To ensure compatibility with both legacy Python scripts (`PYTHON_FALLBACK_AUDIT.md` Spec 7) and AgentFramework standard parameter casing (`snake_case`), the implementation will handle both parameter casing variants seamlessly.

### 4.1 Input Payload Schema (`create_pbr_material_from_textures`)
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "material_path": {
      "type": "string",
      "description": "Destination package path for Material asset (e.g. '/Game/Materials/M_CharacterPBR')"
    },
    "base_color_texture_path": {
      "type": "string",
      "description": "Object path of BaseColor texture (e.g. '/Game/Textures/T_Character_BC')"
    },
    "normal_texture_path": {
      "type": "string",
      "description": "Optional: Object path of Normal map texture"
    },
    "roughness_texture_path": {
      "type": "string",
      "description": "Optional: Object path of Roughness map texture"
    },
    "metallic_texture_path": {
      "type": "string",
      "description": "Optional: Object path of Metallic map texture"
    },
    "ao_texture_path": {
      "type": "string",
      "description": "Optional: Object path of Ambient Occlusion texture"
    },
    "specular_texture_path": {
      "type": "string",
      "description": "Optional: Object path of Specular texture"
    },
    "emissive_texture_path": {
      "type": "string",
      "description": "Optional: Object path of Emissive texture"
    },
    "opacity_texture_path": {
      "type": "string",
      "description": "Optional: Object path of Opacity texture"
    },
    "blend_mode": {
      "type": "string",
      "enum": ["Opaque", "Masked", "Translucent", "Additive"],
      "default": "Opaque"
    },
    "shading_model": {
      "type": "string",
      "enum": ["DefaultLit", "Unlit", "Subsurface", "ClearCoat"],
      "default": "DefaultLit"
    },
    "two_sided": {
      "type": "boolean",
      "default": false
    }
  },
  "required": ["material_path", "base_color_texture_path"]
}
```

*Note: Alternate parameter names supported: `MaterialPath`, `BaseColorTexturePath`, `NormalTexturePath`, `RoughnessTexturePath`, `MetallicTexturePath`, `AOTexturePath`, `BlendMode`, `ShadingModel`, `TwoSided`.*

### 4.2 Output Payload Structure (`FAgentFrameworkActionResult`)
```json
{
  "bSuccess": true,
  "ResultMessage": "Created PBR Material '/Game/Materials/M_CharacterPBR' with 5 texture parameters connected.",
  "ModifiedAssets": [
    "/Game/Materials/M_CharacterPBR"
  ],
  "Errors": [],
  "Warnings": []
}
```

---

## 5. Error Handling & Validation Rules

1. **Validation Phase (`ValidateParams`)**:
   - `material_path` (or `MaterialPath`) must be provided and non-empty.
   - `base_color_texture_path` (or `BaseColorTexturePath`) must be provided and non-empty.
2. **Execution Phase Errors**:
   - `BaseColor` texture fail: If `LoadObject<UTexture2D>(nullptr, *BaseColorPath)` returns `nullptr`, record error in `Result.Errors` and terminate execution.
   - Optional texture fail: If an optional texture path (e.g. `NormalTexturePath`) is provided but cannot be loaded, emit a warning into `Result.Warnings` and continue without creating that expression node.
   - Asset creation failure: If `AssetTools.CreateAsset` fails or returns `nullptr`, record error in `Result.Errors` and abort transaction.
3. **Transaction Rollback**:
   - If `!Result.bSuccess`, `Transaction.Cancel()` is automatically called in `ExecuteAction()`.

---

## 6. Implementation Changes Summary

1. **`AgentFrameworkMaterialActions.h`**:
   - Add method declaration: `FAgentFrameworkActionResult ExecuteCreatePBRMaterialFromTextures(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);`
2. **`AgentFrameworkMaterialActions.cpp`**:
   - Add `TEXT("create_pbr_material_from_textures")` to `GetSupportedToolNames()`.
   - Update `ValidateParams()` to check required fields for `create_pbr_material_from_textures`.
   - Update `ExecuteAction()` dispatch table to call `ExecuteCreatePBRMaterialFromTextures()` when `ToolName == TEXT("create_pbr_material_from_textures")`.
   - Implement `ExecuteCreatePBRMaterialFromTextures()`.
3. **`material_tools.json`**:
   - Add schema entry for `create_pbr_material_from_textures`.
