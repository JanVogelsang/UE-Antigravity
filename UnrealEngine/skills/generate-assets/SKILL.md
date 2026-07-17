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
2. Use the `import_assets_batch` MCP tool for the texture `.png` files.
3. Destination path: `/Game/GenAI/Meshes/` (or similar logical path).

**For Audio (.wav):**
1. Use the `import_assets_batch` MCP tool.
2. Destination path: `/Game/GenAI/Audio/`.

## Step 3: Material Setup (3D Models Only)

Since AI-generated meshes require material assignments, build the material dynamically:
1. Import all downloaded textures (`base_color`, `normal`, `roughness`, `metallic`) using `import_assets_batch`.
2. Use the `create_material` MCP tool to create a new material at `/Game/GenAI/Materials/M_GenAI_{Name}`.
3. Use the `add_material_expression` MCP tool to create `TextureSample` nodes for each texture.
4. Use the `connect_material_expression` MCP tool to connect:
   - BaseColor texture sample to the material's `BaseColor` input.
   - Normal texture sample to the material's `Normal` input.
   - Metallic and Roughness texture samples to their respective inputs.
5. Use `assign_material` or equivalent material tools to set the material on the newly imported Static Mesh.
