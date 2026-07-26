# Task Description — Worker (Milestone 2: Material Actions)

Working Directory: `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m2`

## MANDATORY INTEGRITY WARNING
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

## Objective
Implement `create_pbr_material_from_textures` (Spec 5 in `Documentation/PYTHON_FALLBACK_AUDIT.md`) in `FAgentFrameworkMaterialActions`:
- Header: `AgentFramework/Source/AgentFrameworkActions/Public/Material/AgentFrameworkMaterialActions.h`
- Source: `AgentFramework/Source/AgentFrameworkActions/Private/Material/AgentFrameworkMaterialActions.cpp`

## Reference Inputs
Read the analysis and handoff report from `explorer_m2_1`:
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m2_1/analysis.md`
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m2_1/handoff.md`
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/Documentation/PYTHON_FALLBACK_AUDIT.md` (Section 4, Spec 5)

## Requirements
- Parse request JSON (`destination_path`, `material_name`, `texture_maps`, `blend_mode`, `shading_model`, `two_sided`).
- Load or create material using `AssetTools` / `UMaterialFactoryNew`.
- Configure material settings (`BlendMode`, `ShadingModel`, `TwoSided`).
- Create and auto-layout `UMaterialExpressionTextureSampleParameter2D` nodes for provided texture slots (base_color, metallic, roughness, specular, normal, opacity, emissive, ambient_occlusion, etc.) and connect expression output pins to corresponding material property input pins (MP_BaseColor, MP_Metallic, MP_Roughness, MP_Normal, etc.).
- Call `UMaterial::PostEditChange()` and `FMaterialEditorUtilities::UpdateMaterialAfterGraphChange()` or `Material->PreEditChange()` / `PostEditChange()`.
- Return standard JSON response with `status: 200`, `material_path`, `textures_assigned`.
- Output changes summary to `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m2/changes.md` and handoff to `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m2/handoff.md`.
