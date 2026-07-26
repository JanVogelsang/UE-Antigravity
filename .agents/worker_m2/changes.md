# Changes Summary — worker_m2 (Milestone 2: Material Actions)

## Summary of Modified Files

### 1. `AgentFramework/Source/AgentFrameworkActions/Public/Material/AgentFrameworkMaterialActions.h`
- Added declaration for `ExecuteCreatePBRMaterialFromTextures(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)` inside `FAgentFrameworkMaterialActions` class.

### 2. `AgentFramework/Source/AgentFrameworkActions/Private/Material/AgentFrameworkMaterialActions.cpp`
- **Includes**: Added `#include "Materials/MaterialExpressionTextureSampleParameter2D.h"`.
- **Supported Tools**: Registered `create_pbr_material_from_textures` in `GetSupportedToolNames()`.
- **Parameter Validation**: Updated `ValidateParams()` to validate `create_pbr_material_from_textures` and `material/create_pbr_from_textures` actions:
  - Validates `material_path` / `asset_path` / `destination_path`.
  - Validates `base_color_texture_path` / `base_color` (from top-level params or `texture_maps` sub-object).
- **Action Dispatch**: Updated `ExecuteAction()` dispatch table to map `create_pbr_material_from_textures` and `material/create_pbr_from_textures` to `ExecuteCreatePBRMaterialFromTextures()`.
- **Handler Implementation (`ExecuteCreatePBRMaterialFromTextures`)**:
  - Flexible parameter extraction supporting both snake_case (`material_path`, `base_color_texture_path`, `blend_mode`, etc.) and PascalCase (`MaterialPath`, `BaseColorTexturePath`, `BlendMode`, etc.), as well as structured `destination_path` + `material_name` and `texture_maps` dictionary formats.
  - Parses `BlendMode` (`BLEND_Opaque`, `BLEND_Masked`, `BLEND_Translucent`, `BLEND_Additive`, etc.), `ShadingModel` (`MSM_DefaultLit`, `MSM_Unlit`, `MSM_Subsurface`, `MSM_ClearCoat`, etc.), and `TwoSided`.
  - Creates/loads `UMaterial` asset using `AssetTools` and `UMaterialFactoryNew` inside `FScopedTransaction` for full undo/redo support.
  - Instantiates `UMaterialExpressionTextureSampleParameter2D` nodes for BaseColor, Normal, Roughness, Metallic, Ambient Occlusion, Specular, Emissive, and Opacity.
  - Automatically configures parameter names (e.g. `BaseColorMap`, `NormalMap`, `RoughnessMap`) and sampler types (`SAMPLERTYPE_Color`, `SAMPLERTYPE_Normal`, `SAMPLERTYPE_LinearColor`).
  - Auto-places nodes in grid layout (`PosX = -400`, `PosY += 220`).
  - Connects expression outputs to corresponding material property pins (`MP_BaseColor`, `MP_Normal`, `MP_Roughness`, `MP_Metallic`, `MP_AmbientOcclusion`, `MP_Specular`, `MP_EmissiveColor`, `MP_Opacity` / `MP_OpacityMask`) via `UMaterialEditingLibrary::ConnectMaterialProperty()`.
  - Recompiles material, marks package dirty, saves asset to disk, and notifies Asset Registry (`FAssetRegistryModule::AssetCreated`).
  - Returns standard `FAgentFrameworkActionResult` JSON response with `bSuccess`, `ResultMessage`, and `ModifiedAssets`.

### 3. `AgentFramework/Resources/ToolSchemas/material_tools.json`
- Added JSON schema definition for `create_pbr_material_from_textures` under `material_tools` domain.
