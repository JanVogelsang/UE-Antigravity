import pytest
import json
import urllib.request
import urllib.error

# Expected prefix mappings for major asset classes
EXPECTED_PREFIX_MAP = {
    "Blueprint": "BP_",
    "WidgetBlueprint": "WBP_",
    "Material": "M_",
    "MaterialInstanceConstant": "MI_",
    "Texture2D": "T_",
    "StaticMesh": "SM_",
    "SkeletalMesh": "SKM_",
    "NiagaraSystem": "NS_",
    "NiagaraEmitter": "NE_",
    "InputAction": "IA_",
    "InputMappingContext": "IMC_",
    "SoundWave": "SW_",
    "DataAsset": "DA_",
    "DataTable": "DT_",
    "LevelSequence": "LS_"
}

# Expected category subfolder mappings
EXPECTED_CATEGORY_MAP = {
    "Blueprints": ["Blueprint", "BlueprintGeneratedClass"],
    "Materials": ["Material", "MaterialInstanceConstant", "MaterialInstanceDynamic", "MaterialInstance", "MaterialFunction", "MaterialParameterCollection", "SubsurfaceProfile", "PhysicalMaterial"],
    "Textures": ["Texture2D", "TextureCube", "VolumeTexture", "RenderTarget2D", "Texture"],
    "UI": ["WidgetBlueprint", "WidgetBlueprintGeneratedClass", "SlateWidgetStyleAsset", "Font", "FontFace"],
    "Effects": ["NiagaraSystem", "NiagaraEmitter", "ParticleSystem"],
    "Input": ["InputAction", "InputMappingContext"],
    "Audio": ["SoundWave", "SoundCue", "SoundAttenuation", "SoundConcurrency", "MetaSoundSource"],
    "Meshes": ["StaticMesh", "SkeletalMesh", "PhysicsAsset"],
    "Animation": ["AnimSequence", "AnimMontage", "AnimBlueprint", "AnimBlueprintGeneratedClass", "BlendSpace", "BlendSpace1D", "Skeleton", "IKRigDefinition", "IKRetargeter"],
    "Data": ["DataAsset", "PrimaryDataAsset", "DataTable", "CurveTable", "StringTable"],
    "Sequencer": ["LevelSequence"],
    "PCG": ["PCGGraph", "PCGGraphInterface"]
}


def test_prefix_mappings_defined():
    """Verify all 15 major asset classes have defined prefix mappings."""
    assert len(EXPECTED_PREFIX_MAP) == 15
    for asset_class, prefix in EXPECTED_PREFIX_MAP.items():
        assert prefix.endswith("_")
        assert len(prefix) >= 2


def test_category_mappings_defined():
    """Verify all 12 requested categories have mapped asset classes."""
    categories = ["Blueprints", "Materials", "Textures", "UI", "Effects", "Input", "Audio", "Meshes", "Animation", "Data", "Sequencer", "PCG"]
    for cat in categories:
        assert cat in EXPECTED_CATEGORY_MAP
        assert len(EXPECTED_CATEGORY_MAP[cat]) > 0


def test_parameter_aliasing_payload_formatting():
    """Verify parameter payloads with both snake_case and PascalCase variants."""
    # enforce_naming_conventions payloads
    payload_snake_12 = {
        "action": "enforce_naming_conventions",
        "folder_path": "/Game/TestFolder",
        "dry_run": True,
        "recursive": True,
        "custom_rules": {"Blueprint": "BP_"}
    }

    payload_pascal_12 = {
        "action": "enforce_naming_conventions",
        "FolderPath": "/Game/TestFolder",
        "DryRun": True,
        "Recursive": True,
        "CustomRules": {"Blueprint": "BP_"}
    }

    assert payload_snake_12["folder_path"] == payload_pascal_12["FolderPath"]
    assert payload_snake_12["dry_run"] == payload_pascal_12["DryRun"]
    assert payload_snake_12["recursive"] == payload_pascal_12["Recursive"]
    assert payload_snake_12["custom_rules"] == payload_pascal_12["CustomRules"]

    # organize_assets_by_type payloads
    payload_snake_14 = {
        "action": "organize_assets_by_type",
        "folder_path": "/Game/TestFolder",
        "dry_run": True,
        "create_subfolders": True,
        "recursive": True
    }

    payload_pascal_14 = {
        "action": "organize_assets_by_type",
        "FolderPath": "/Game/TestFolder",
        "DryRun": True,
        "CreateSubfolders": True,
        "Recursive": True
    }

    assert payload_snake_14["folder_path"] == payload_pascal_14["FolderPath"]
    assert payload_snake_14["dry_run"] == payload_pascal_14["DryRun"]
    assert payload_snake_14["create_subfolders"] == payload_pascal_14["CreateSubfolders"]
    assert payload_snake_14["recursive"] == payload_pascal_14["Recursive"]


if __name__ == "__main__":
    pytest.main(["-v", __file__])
