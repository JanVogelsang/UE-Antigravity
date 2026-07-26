# Milestone 4 Schema Analysis Report: Context Actions (`enforce_naming_conventions` Spec 12 & `organize_assets_by_type` Spec 14)

## Executive Summary
This report presents the schema investigation and proposed JSON schema definitions for two new Context Action tools in `UE-AgentFramework`:
1. **`enforce_naming_conventions`** (Spec 12 from `PYTHON_FALLBACK_AUDIT.md`)
2. **`organize_assets_by_type`** (Spec 14 from `PYTHON_FALLBACK_AUDIT.md`)

Both tools replace standalone Python utility scripts (`clean_naming_conventions.py` and `organize_assets_by_type.py`) with native C++ action routes registered under `FAgentFrameworkContextActions` (`AgentFrameworkActions/Context`).

To ensure total compatibility with various LLM clients and caller configurations, the JSON schema definitions explicitly define and support parameter keys in **both `snake_case` and `PascalCase`** format (e.g., `folder_path` / `FolderPath`, `recursive` / `Recursive`, `dry_run` / `DryRun`).

---

## 1. Existing `context_tools.json` Schema Structure Analysis

### 1.1 File Information
- **Location**: `AgentFramework/Resources/ToolSchemas/context_tools.json`
- **Domain**: `context_tools`
- **Schema Version**: `1.0.0` (Upgradable to `1.1.0` / `2.0.0` for Milestone 4)
- **Min Plugin Version**: `1.0.0`

### 1.2 Existing Tool Inventory
Currently, `context_tools.json` defines 4 native tools:
1. `list_directory`: Lists files and subdirectories in project-relative paths.
2. `search_assets`: Searches the Unreal Asset Registry by name, class filter, or content path.
3. `read_file_snippet`: Reads line-numbered snippets of source files and configs.
4. `activate_skill`: Dynamically activates lazy-loaded agent skill categories.

### 1.3 Schema Conventions Observed
- Top-level JSON metadata: `schema_version`, `domain`, `min_plugin_version`, `tools` array.
- Each tool contains: `name`, `description`, `input_schema`.
- `input_schema` complies with JSON Schema Draft 7 / OpenAPI specifications (`type: "object"`, `properties`, `required`).
- Descriptions provide explicit operational context, examples, and recommendations (e.g., warning to use `search_assets` over `list_directory` for Unreal assets).

---

## 2. Detailed Tool Specifications & Schema Drafts

### 2.1 Specification 12: `enforce_naming_conventions`

#### Operational Purpose
Batch evaluates asset names under a content folder against Unreal Engine 5 standard class-to-prefix naming conventions (`BP_`, `SM_`, `T_`, `M_`, `MI_`, `NS_`, `WBP_`, `SKM_`, `A_`, `E_`, `S_`, etc.). Automatically renames non-conforming assets or previews proposed renames in dry-run mode.

#### Dual-Case Parameter Design
| Primary Parameter (`snake_case`) | Alias (`PascalCase`) | Type | Default | Description |
|---|---|---|---|---|
| `folder_path` | `FolderPath` | `string` | *(Required)* | Package directory path to scan (e.g., `/Game/Blueprints` or `/Game/`) |
| `recursive` | `Recursive` | `boolean` | `true` | If true, recursively scans subdirectories |
| `dry_run` | `DryRun` | `boolean` | `false` | If true, reports non-conforming assets and proposed names without performing actual renames |

#### Tool Schema JSON Definition
```json
{
  "name": "enforce_naming_conventions",
  "description": "Scan a content directory and enforce UE5 standard asset naming conventions (e.g. BP_ for Blueprints, SM_ for Static Meshes, M_ for Materials, T_ for Textures, WBP_ for Widgets). Renames non-conforming assets automatically or previews proposed renames when dry_run=true. Accepts parameter keys in both snake_case and PascalCase.",
  "input_schema": {
    "type": "object",
    "properties": {
      "folder_path": {
        "type": "string",
        "description": "Package directory path to scan for naming convention enforcement (e.g. '/Game/Blueprints', '/Game/UI', or '/Game/'). Accepts 'folder_path' or 'FolderPath'."
      },
      "FolderPath": {
        "type": "string",
        "description": "PascalCase alias for folder_path."
      },
      "recursive": {
        "type": "boolean",
        "default": true,
        "description": "If true, recursively scan subdirectories. Default: true. Accepts 'recursive' or 'Recursive'."
      },
      "Recursive": {
        "type": "boolean",
        "default": true,
        "description": "PascalCase alias for recursive."
      },
      "dry_run": {
        "type": "boolean",
        "default": false,
        "description": "If true, evaluate naming conventions and return proposed renames without mutating assets. Default: false. Accepts 'dry_run' or 'DryRun'."
      },
      "DryRun": {
        "type": "boolean",
        "default": false,
        "description": "PascalCase alias for dry_run."
      }
    },
    "required": [
      "folder_path"
    ]
  }
}
```

---

### 2.2 Specification 14: `organize_assets_by_type`

#### Operational Purpose
Scans a target content folder and sorts mixed assets into type-specific target subfolders based on asset class (`Blueprints/`, `Materials/`, `Textures/`, `Meshes/`, `Audio/`, `Effects/`, `UI/`, `Maps/`). Helps eliminate clutter in root content directories or unorganized import folders.

#### Dual-Case Parameter Design
| Primary Parameter (`snake_case`) | Alias (`PascalCase`) | Type | Default | Description |
|---|---|---|---|---|
| `folder_path` | `FolderPath` | `string` | *(Required)* | Target content folder path to organize (e.g., `/Game/` or `/Game/Imports`) |
| `recursive` | `Recursive` | `boolean` | `true` | If true, recursively organizes subdirectories |
| `dry_run` | `DryRun` | `boolean` | `false` | If true, calculates proposed asset move destinations without moving files |

#### Tool Schema JSON Definition
```json
{
  "name": "organize_assets_by_type",
  "description": "Automatically reorganize mixed assets in a content folder into type-specific subdirectories (e.g., Blueprints/, Materials/, Textures/, Meshes/, Audio/, Effects/, UI/, Maps/). Supports preview mode via dry_run=true. Accepts parameter keys in both snake_case and PascalCase.",
  "input_schema": {
    "type": "object",
    "properties": {
      "folder_path": {
        "type": "string",
        "description": "Target content folder path to organize by asset class (e.g. '/Game/', '/Game/Imports'). Accepts 'folder_path' or 'FolderPath'."
      },
      "FolderPath": {
        "type": "string",
        "description": "PascalCase alias for folder_path."
      },
      "recursive": {
        "type": "boolean",
        "default": true,
        "description": "If true, recursively organize assets in subdirectories. Default: true. Accepts 'recursive' or 'Recursive'."
      },
      "Recursive": {
        "type": "boolean",
        "default": true,
        "description": "PascalCase alias for recursive."
      },
      "dry_run": {
        "type": "boolean",
        "default": false,
        "description": "If true, calculate proposed organization moves without executing asset relocations. Default: false. Accepts 'dry_run' or 'DryRun'."
      },
      "DryRun": {
        "type": "boolean",
        "default": false,
        "description": "PascalCase alias for dry_run."
      }
    },
    "required": [
      "folder_path"
    ]
  }
}
```

---

## 3. Complete Updated `context_tools.json` Draft

Below is the complete drafted content for `context_tools.json` combining all existing tools with the newly drafted Spec 12 and Spec 14 tools:

```json
{
  "schema_version": "1.1.0",
  "domain": "context_tools",
  "min_plugin_version": "1.0.0",
  "tools": [
    {
      "name": "list_directory",
      "description": "List files and directories in a project-relative path. Use this to explore Source/ and Config/ directories. For finding Blueprints, Materials, Textures and other Unreal assets, prefer 'search_assets' instead — it searches the asset registry directly and is much faster than navigating directories manually. WARNING: NEVER use this tool to search for Unreal Engine assets (Blueprints, Materials, etc.). ALWAYS use the search_assets tool instead, which is significantly faster and more accurate.",
      "input_schema": {
        "type": "object",
        "properties": {
          "directory": {
            "type": "string",
            "description": "Project-relative directory path to list. Use empty string '' for project root. Examples: 'Source', 'Content/Blueprints', 'Config', 'Source/MyGame'"
          }
        },
        "required": [
          "directory"
        ]
      }
    },
    {
      "name": "search_assets",
      "description": "Search the Unreal asset registry by name, class type, or path. Returns matching assets with their class and full content path. Use this FIRST when looking for Blueprints, Materials, Textures, Static Meshes, Skeletal Meshes, Animation assets, etc. This is MUCH faster than browsing directories with list_directory. CRITICAL: Use this tool FIRST to discover EXACT asset paths before calling tools like spawn_actor or set_component_properties.",
      "input_schema": {
        "type": "object",
        "properties": {
          "query": {
            "type": "string",
            "description": "Search term to match against asset names and paths (e.g. 'ThirdPerson', 'BP_Player', 'M_Rock')"
          },
          "class_filter": {
            "type": "string",
            "description": "Filter by asset class (e.g. 'Blueprint', 'Material', 'StaticMesh', 'Texture2D', 'SkeletalMesh', 'AnimSequence')"
          },
          "path_filter": {
            "type": "string",
            "description": "Filter by content path substring (e.g. '/Game/Characters/', '/Game/ThirdPerson/')"
          },
          "max_results": {
            "type": "integer",
            "description": "Maximum results to return (1-200, default 50)"
          }
        },
        "required": [
          "query"
        ]
      }
    },
    {
      "name": "read_file_snippet",
      "description": "Read a section of a source file with line numbers. Use this to inspect C++ headers, implementations, config files, or other text files. Supports .h, .cpp, .cs, .ini, .json, .txt, .xml, .yaml, .md, .py, .uplugin, .uproject files.",
      "input_schema": {
        "type": "object",
        "properties": {
          "file_path": {
            "type": "string",
            "description": "Project-relative file path (e.g. 'Source/MyGame/MyActor.h')"
          },
          "start_line": {
            "type": "integer",
            "description": "First line to read (1-based, default 1)"
          },
          "end_line": {
            "type": "integer",
            "description": "Last line to read (default: start_line + 99, max 500 lines per read)"
          }
        },
        "required": [
          "file_path"
        ]
      }
    },
    {
      "name": "activate_skill",
      "description": "Activate one or more lazy-loaded skill categories by adding them to the project's active_skills.json configuration.",
      "input_schema": {
        "type": "object",
        "properties": {
          "skill_name": {
            "type": "string",
            "description": "Name of the skill category to activate (e.g. 'animation', 'gas', 'pcg')"
          },
          "skills": {
            "type": "array",
            "items": {
              "type": "string"
            },
            "description": "Alternative array of skill names to activate"
          }
        }
      }
    },
    {
      "name": "enforce_naming_conventions",
      "description": "Scan a content directory and enforce UE5 standard asset naming conventions (e.g. BP_ for Blueprints, SM_ for Static Meshes, M_ for Materials, T_ for Textures, WBP_ for Widgets). Renames non-conforming assets automatically or previews proposed renames when dry_run=true. Accepts parameter keys in both snake_case and PascalCase.",
      "input_schema": {
        "type": "object",
        "properties": {
          "folder_path": {
            "type": "string",
            "description": "Package directory path to scan for naming convention enforcement (e.g. '/Game/Blueprints', '/Game/UI', or '/Game/'). Accepts 'folder_path' or 'FolderPath'."
          },
          "FolderPath": {
            "type": "string",
            "description": "PascalCase alias for folder_path."
          },
          "recursive": {
            "type": "boolean",
            "default": true,
            "description": "If true, recursively scan subdirectories. Default: true. Accepts 'recursive' or 'Recursive'."
          },
          "Recursive": {
            "type": "boolean",
            "default": true,
            "description": "PascalCase alias for recursive."
          },
          "dry_run": {
            "type": "boolean",
            "default": false,
            "description": "If true, evaluate naming conventions and return proposed renames without mutating assets. Default: false. Accepts 'dry_run' or 'DryRun'."
          },
          "DryRun": {
            "type": "boolean",
            "default": false,
            "description": "PascalCase alias for dry_run."
          }
        },
        "required": [
          "folder_path"
        ]
      }
    },
    {
      "name": "organize_assets_by_type",
      "description": "Automatically reorganize mixed assets in a content folder into type-specific subdirectories (e.g., Blueprints/, Materials/, Textures/, Meshes/, Audio/, Effects/, UI/, Maps/). Supports preview mode via dry_run=true. Accepts parameter keys in both snake_case and PascalCase.",
      "input_schema": {
        "type": "object",
        "properties": {
          "folder_path": {
            "type": "string",
            "description": "Target content folder path to organize by asset class (e.g. '/Game/', '/Game/Imports'). Accepts 'folder_path' or 'FolderPath'."
          },
          "FolderPath": {
            "type": "string",
            "description": "PascalCase alias for folder_path."
          },
          "recursive": {
            "type": "boolean",
            "default": true,
            "description": "If true, recursively organize assets in subdirectories. Default: true. Accepts 'recursive' or 'Recursive'."
          },
          "Recursive": {
            "type": "boolean",
            "default": true,
            "description": "PascalCase alias for recursive."
          },
          "dry_run": {
            "type": "boolean",
            "default": false,
            "description": "If true, calculate proposed organization moves without executing asset relocations. Default: false. Accepts 'dry_run' or 'DryRun'."
          },
          "DryRun": {
            "type": "boolean",
            "default": false,
            "description": "PascalCase alias for dry_run."
          }
        },
        "required": [
          "folder_path"
        ]
      }
    }
  ]
}
```

---

## 4. Implementation Guidelines for C++ Action Executor (`FAgentFrameworkContextActions`)

To ensure seamless dual-case parameter parsing when implementing the C++ actions in `AgentFrameworkActions/Private/Context/AgentFrameworkContextActions.cpp`:

1. **Parameter Resolution Pattern**:
   Use fallback parameter extraction:
   ```cpp
   FString FolderPath;
   if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("folder_path"), FolderPath, Result.Errors, false) || FolderPath.IsEmpty())
   {
       UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("FolderPath"), FolderPath, Result.Errors, true);
   }

   bool bRecursive = true;
   if (!UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("recursive"), bRecursive, Result.Errors, false))
   {
       UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("Recursive"), bRecursive, Result.Errors, false);
   }

   bool bDryRun = false;
   if (!UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("dry_run"), bDryRun, Result.Errors, false))
   {
       UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("DryRun"), bDryRun, Result.Errors, false);
   }
   ```

2. **Tool Name Registration**:
   Update `FAgentFrameworkContextActions::GetSupportedToolNames()` to include:
   - `TEXT("enforce_naming_conventions")`
   - `TEXT("organize_assets_by_type")`

3. **Response Schema Alignment**:
   Return standardized JSON response objects with `bSuccess`, `folder_path`, counts (`renamed_assets_count` / `moved_assets_count`), details array, `ResultMessage`, and `Errors` array.
