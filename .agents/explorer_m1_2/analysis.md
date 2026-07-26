# Analysis & Migration Report: Skill Documentation Migration (Explorer 2 - Milestone 1)

## Executive Summary
This document presents the detailed investigation, audit findings, and concrete editing instructions for migrating two target skill instruction sets:
1. `UnrealEngine/skills/generate-assets/SKILL.md`
2. `UnrealEngine/skills/setup-input/SKILL.md`

By cross-referencing `Documentation/PYTHON_FALLBACK_AUDIT.md` (specifically Sections 1.3, 2.2, 4.5, 4.7, and 4.8), we identified tool churn bottlenecks and missing native tool documentation. We have designed exact replacements utilizing new native C++ action routes (`create_pbr_material_from_textures`, `configure_sound_wave_cue`, and `configure_input_mapping_modifiers_triggers`).

---

## 1. Audit Findings & Rationale

### 1.1 `UnrealEngine/skills/generate-assets/SKILL.md`
* **Current Bottlenecks & Tool Churn**:
  - In **Step 3: Material Setup**, the current skill instructs agents to manually build materials via a 5-step, multi-tool sequence: `import_assets_batch` -> `create_material` -> `add_material_expression` (for each texture sample) -> `connect_material_expression` (for each pin: BaseColor, Normal, Roughness, Metallic) -> `assign_material`.
  - This requires **6+ sequential tool calls** per generated asset, introducing high token overhead, potential pin disconnection errors, and retry loops.
  - In **Step 2: Audio Import**, the current skill only mentions importing raw `.wav` files into `/Game/GenAI/Audio/` using `import_assets_batch`. It lacks instructions for configuring audio parameters (looping, volume, pitch) or encapsulating imported waves inside a `USoundCue` with attenuation settings.
* **Native Tool Replacements**:
  - **`create_pbr_material_from_textures`**: Replaces the multi-step material expression tool churn with an atomic, one-shot PBR material graph creation. It creates the `UMaterial` asset, instantiates `TextureSample` expression nodes for BaseColor, Normal, Roughness, Metallic, and AO, wires them to standard PBR material inputs, compiles, and saves in a single operation.
  - **`configure_sound_wave_cue`**: Automates configuring `USoundWave` properties (`bLooping`, `VolumeMultiplier`, `PitchMultiplier`) and optionally instantiates a `USoundCue` wrapping the wave with a `USoundNodeWavePlayer` and `USoundAttenuation` asset.

### 1.2 `UnrealEngine/skills/setup-input/SKILL.md`
* **Current Bottlenecks & Gaps**:
  - Step 1 and Step 2 describe creating Input Actions and Input Mapping Contexts in abstract text without citing the native C++ MCP tools (`create_input_action`, `create_input_mapping_context`, `add_input_mapping`).
  - Lacks instructions for configuring Enhanced Input key mapping **Modifiers** (e.g. `Negate` for inverted directional axes, `SwizzleAxis` for 2D/3D movement vectors) and **Triggers** (e.g. `Hold` with time thresholds, `Pressed`, `Tap`, `Pulse`).
* **Native Tool Replacements**:
  - Document `create_input_action`, `create_input_mapping_context`, and `add_input_mapping` for standard setup.
  - Document **`configure_input_mapping_modifiers_triggers`**: Enables attaching `Modifiers` array (`Negate`, `SwizzleAxis`, `Scalar`, `DeadZone`) and `Triggers` array (`Pressed`, `Released`, `Hold`, `Tap`, `Pulse`, `ChordAction`) directly to key mappings inside a `UInputMappingContext`.

---

## 2. Native C++ MCP Action API Specifications

### 2.1 `create_pbr_material_from_textures`
* **Module**: `AgentFrameworkMaterialActions` (`FAgentFrameworkMaterialActions`)
* **Tool Name**: `create_pbr_material_from_textures`
* **JSON Payload Schema**:
```json
{
  "MaterialPath": "/Game/GenAI/Materials/M_GenAI_Barrel",
  "BaseColorTexturePath": "/Game/GenAI/Textures/T_Barrel_BaseColor",
  "NormalTexturePath": "/Game/GenAI/Textures/T_Barrel_Normal",
  "RoughnessTexturePath": "/Game/GenAI/Textures/T_Barrel_Roughness",
  "MetallicTexturePath": "/Game/GenAI/Textures/T_Barrel_Metallic",
  "AOTexturePath": "/Game/GenAI/Textures/T_Barrel_AO",
  "BlendMode": "Opaque",
  "ShadingModel": "DefaultLit"
}
```

### 2.2 `configure_sound_wave_cue`
* **Module**: `AgentFrameworkMediaActions` (`FAgentFrameworkMediaActions`)
* **Tool Name**: `configure_sound_wave_cue`
* **JSON Payload Schema**:
```json
{
  "SoundWaveAsset": "/Game/GenAI/Audio/SW_GenAI_VillainLaugh",
  "CueAssetPath": "/Game/GenAI/Audio/SC_GenAI_VillainLaugh",
  "bLooping": false,
  "VolumeMultiplier": 1.0,
  "PitchMultiplier": 1.0,
  "AttenuationAssetPath": "/Game/Audio/ATT_Default3D"
}
```

### 2.3 `configure_input_mapping_modifiers_triggers`
* **Module**: `AgentFrameworkInputActions` (`FAgentFrameworkInputActions`)
* **Tool Name**: `configure_input_mapping_modifiers_triggers`
* **JSON Payload Schema**:
```json
{
  "ContextAsset": "/Game/Input/IMC_Default",
  "InputActionAsset": "/Game/Input/Actions/IA_Move",
  "Key": "S",
  "Modifiers": [
    { "Type": "Negate" }
  ],
  "Triggers": [
    { "Type": "Pressed" }
  ]
}
```

---

## 3. Step-by-Step Editing Instructions

### Target 1: `UnrealEngine/skills/generate-assets/SKILL.md`

#### Analysis of File Structure (63 lines total)
- Lines 1-5: Frontmatter header.
- Lines 7-38: Requirements, Step 1 (Generative AI Script Invocation for 3D Models & Audio).
- Lines 39-51: Step 2 (Importing Mesh, Textures, and Audio via `import_mesh` & `import_assets_batch`).
- Lines 52-63: Step 3 (Material Setup multi-step expression tool churn).

#### Concrete Editing Plan for `generate-assets/SKILL.md`:
1. **Update Step 2 (Audio)**: Expand Step 2 for audio to include sound wave & sound cue configuration via `configure_sound_wave_cue`.
2. **Replace Step 3 (Material Setup)**: Replace lines 52-63 with updated Step 3 using `create_pbr_material_from_textures` for one-shot material creation, followed by assigning the material to the mesh using `configure_static_mesh` or `set_component_properties`.
3. **Add Step 4 (Audio Configuration)**: Document `configure_sound_wave_cue` explicitly.

#### Full Proposed Content for `UnrealEngine/skills/generate-assets/SKILL.md`:
```markdown
---
name: generate-assets
description: Generates 3D models and audio assets via AI APIs (Meshy, ElevenLabs) and imports them into Unreal Engine.
argument-hint: "What you want to generate (e.g. '3D model of a rusty barrel', 'Audio of a villain laughing')"
---

# Generative AI Workflow

You have access to a Python utility script `generative_utils.py` located inside the plugin's ExternalServer directory. This script securely interfaces with third-party generative AI services to create assets for the Unreal Engine project.

## Requirements

The user MUST provide the required API keys in a `.env` file located at the root of the project.
- For 3D Models (Meshy): `MESHY_API_KEY`
- For Audio (ElevenLabs): `ELEVENLABS_API_KEY`

If the script fails due to a missing key, kindly ask the user to add the key to the `.env` file in the project workspace.

## Step 1: Generate the Asset

To generate an asset, you will run the Python script using a local terminal. The script will poll the API, download the asset, and save it.

Resolve the path to `generative_utils.py` dynamically:
- Script location: `{AGENT_PLUGIN_DIR}/ExternalServer/src/generative_utils.py`
- If `{AGENT_PLUGIN_DIR}` is not in your environment, use `.agents/plugins/UnrealEngine/ExternalServer/src/generative_utils.py` relative to the project root.

**For 3D Models:**
```bash
python "{AGENT_PLUGIN_DIR}/ExternalServer/src/generative_utils.py" --project-root "{PROJECT_ROOT}" meshy --prompt "Your detailed prompt here"
```
*Note: This will download an .fbx file and its associated PBR texture maps (.png).*

**For Audio / Text-to-Speech:**
```bash
python "{AGENT_PLUGIN_DIR}/ExternalServer/src/generative_utils.py" --project-root "{PROJECT_ROOT}" elevenlabs --prompt "Text to speak"
```
*Note: This will download a proper RIFF/WAV file.*

## Step 2: Import into Unreal Engine

Once the Python script succeeds, it will print the absolute paths of the downloaded files. You MUST import these files into the Unreal Editor immediately.

**For 3D Models:**
1. Use the `import_mesh` MCP tool for the `.fbx` model.
2. Use the `import_assets_batch` MCP tool for the texture `.png` files (`base_color`, `normal`, `roughness`, `metallic`, `ao`).
3. Destination paths: `/Game/GenAI/Meshes/` for models, `/Game/GenAI/Textures/` for texture maps.

**For Audio (.wav):**
1. Use the `import_assets_batch` MCP tool.
2. Destination path: `/Game/GenAI/Audio/`.

## Step 3: One-Shot PBR Material Setup (3D Models Only)

After importing the mesh and texture maps, build and assign the material using the native one-shot PBR material action:

1. **Create PBR Material Graph**: Use `create_pbr_material_from_textures` to instantiate texture samples and wire `BaseColor`, `Normal`, `Roughness`, `Metallic`, and `AO` pins in a single atomic tool call:
   ```json
   {
     "MaterialPath": "/Game/GenAI/Materials/M_GenAI_{Name}",
     "BaseColorTexturePath": "/Game/GenAI/Textures/T_{Name}_BaseColor",
     "NormalTexturePath": "/Game/GenAI/Textures/T_{Name}_Normal",
     "RoughnessTexturePath": "/Game/GenAI/Textures/T_{Name}_Roughness",
     "MetallicTexturePath": "/Game/GenAI/Textures/T_{Name}_Metallic",
     "BlendMode": "Opaque",
     "ShadingModel": "DefaultLit"
   }
   ```
2. **Assign Material**: Use `configure_static_mesh` or `set_component_properties` to assign `/Game/GenAI/Materials/M_GenAI_{Name}` to the static mesh material slot.

## Step 4: Sound Wave & Cue Setup (Audio Assets Only)

After importing raw `.wav` audio files into `/Game/GenAI/Audio/`:

1. **Configure Sound Wave & Sound Cue**: Use `configure_sound_wave_cue` to set playback parameters (`bLooping`, `VolumeMultiplier`, `PitchMultiplier`) and optionally generate a `USoundCue` asset with 3D spatial attenuation:
   ```json
   {
     "SoundWaveAsset": "/Game/GenAI/Audio/SW_GenAI_{Name}",
     "CueAssetPath": "/Game/GenAI/Audio/SC_GenAI_{Name}",
     "bLooping": false,
     "VolumeMultiplier": 1.0,
     "PitchMultiplier": 1.0,
     "AttenuationAssetPath": "/Game/Audio/ATT_Default3D"
   }
   ```
```

---

### Target 2: `UnrealEngine/skills/setup-input/SKILL.md`

#### Analysis of File Structure (57 lines total)
- Lines 1-4: Frontmatter.
- Lines 5-9: Skill description & argument declaration.
- Lines 10-14: Steps 1 & 2 (Abstract description of creating Input Action and adding to IMC).
- Lines 15-46: C++ header & source setup.
- Lines 48-51: Blueprint binding alternative.
- Lines 53-57: System requirements and notes.

#### Concrete Editing Plan for `setup-input/SKILL.md`:
1. **Update Step 1**: Specify tool `create_input_action` with parameter breakdown.
2. **Update Step 2**: Specify tools `create_input_mapping_context` and `add_input_mapping`.
3. **Add Step 2b (Key Mapping Modifiers & Triggers)**: Detail `configure_input_mapping_modifiers_triggers` with example payloads for `Negate`, `SwizzleAxis`, `Hold`, and `Pressed`.

#### Full Proposed Content for `UnrealEngine/skills/setup-input/SKILL.md`:
```markdown
---
name: setup-input
description: Configure the Enhanced Input system with Input Actions, Mapping Contexts, Modifiers, and Triggers via native MCP tools.
---
# Skill: Setup Enhanced Input
## Description
Configure the Enhanced Input system with Input Actions, Mapping Contexts, key mapping Modifiers, and Triggers.
## Arguments
- {{arg}}: The action name to set up (e.g., Jump, Sprint, Interact, Move)
## Steps
1. **Create Input Action Asset**: Use the `create_input_action` MCP tool to create `/Game/Input/Actions/IA_{{arg}}`:
   ```json
   {
     "PackagePath": "/Game/Input/Actions",
     "ActionName": "IA_{{arg}}",
     "ValueType": "Digital"
   }
   ```
   *(Supported `ValueType` options: `Digital`, `Axis1D`, `Axis2D`, `Axis3D`)*

2. **Add Key Mapping to Input Mapping Context**:
   - If `/Game/Input/IMC_Default` does not exist, create it via `create_input_mapping_context`:
     ```json
     {
       "PackagePath": "/Game/Input",
       "ContextName": "IMC_Default"
     }
     ```
   - Bind the target key mapping using `add_input_mapping`:
     ```json
     {
       "ContextAsset": "/Game/Input/IMC_Default",
       "InputActionAsset": "/Game/Input/Actions/IA_{{arg}}",
       "Key": "SpaceBar"
     }
     ```

3. **Configure Key Mapping Modifiers & Triggers**:
   Attach key modifiers (e.g. `Negate`, `SwizzleAxis`) and trigger conditions (e.g. `Pressed`, `Hold`, `Tap`) using `configure_input_mapping_modifiers_triggers`:
   - **Example 1: Axis Negation (e.g. Move Backward 'S' key)**:
     ```json
     {
       "ContextAsset": "/Game/Input/IMC_Default",
       "InputActionAsset": "/Game/Input/Actions/IA_Move",
       "Key": "S",
       "Modifiers": [ { "Type": "Negate" } ]
     }
     ```
   - **Example 2: 2D Vector Swizzle (e.g. Move Forward 'W' key)**:
     ```json
     {
       "ContextAsset": "/Game/Input/IMC_Default",
       "InputActionAsset": "/Game/Input/Actions/IA_Move",
       "Key": "W",
       "Modifiers": [ { "Type": "SwizzleAxis", "Order": "YXZ" } ]
     }
     ```
   - **Example 3: Hold Trigger (e.g. Charge Attack / Sprint Hold)**:
     ```json
     {
       "ContextAsset": "/Game/Input/IMC_Default",
       "InputActionAsset": "/Game/Input/Actions/IA_Sprint",
       "Key": "LeftShift",
       "Triggers": [ { "Type": "Hold", "HoldTimeThreshold": 0.5, "bIsOneShot": true } ]
     }
     ```

4. **In the character/pawn header**, add:
   ```cpp
   UPROPERTY(EditDefaultsOnly, Category="Input")
   TObjectPtr<UInputAction> {{arg}}Action;
   void Handle{{arg}}(const FInputActionValue& Value);
   ```

5. **In the character/pawn class**, override `SetupPlayerInputComponent` to bind the action:
   ```cpp
   // Header
   virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

   // Source
   void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
   {
       Super::SetupPlayerInputComponent(PlayerInputComponent);
       
       if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
       {
           EIC->BindAction({{arg}}Action, ETriggerEvent::Triggered, this, &AMyCharacter::Handle{{arg}});
       }
   }
   ```

6. **Implement the handler function** in C++:
   ```cpp
   void AMyCharacter::Handle{{arg}}(const FInputActionValue& Value)
   {
       // Retrieve input value (e.g. float or Vector2D depending on configuration)
       // float AxisValue = Value.Get<float>();
   }
   ```

7. **Blueprint Binding Alternative**:
   - In a Blueprint Character graph, right-click and search for `"EnhancedAction IA_{{arg}}"`.
   - Add the **Enhanced Action IA_{{arg}}** node.
   - Wire your game logic nodes to the **Triggered** or **Started** / **Completed** pins.

## Notes
- Ensure `"EnhancedInput"` is added to `PublicDependencyModuleNames` in the project's `.Build.cs` file.
- Include `EnhancedInput/Public/EnhancedInputComponent.h`.
- The project must have the EnhancedInput plugin enabled.
```

---
