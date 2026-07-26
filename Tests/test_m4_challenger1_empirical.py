import pytest
import re

# Python mirror of C++ GetPrefixForClass
PREFIX_MAP = {
    "Blueprint": "BP_",
    "BlueprintGeneratedClass": "BP_",
    "WidgetBlueprint": "WBP_",
    "WidgetBlueprintGeneratedClass": "WBP_",
    "AnimBlueprint": "ABP_",
    "AnimBlueprintGeneratedClass": "ABP_",
    "Material": "M_",
    "MaterialInstanceConstant": "MI_",
    "MaterialInstanceDynamic": "MI_",
    "MaterialInstance": "MI_",
    "MaterialFunction": "MF_",
    "MaterialParameterCollection": "MPC_",
    "Texture2D": "T_",
    "TextureCube": "T_",
    "VolumeTexture": "T_",
    "Texture": "T_",
    "RenderTarget2D": "RT_",
    "StaticMesh": "SM_",
    "SkeletalMesh": "SKM_",
    "PhysicsAsset": "PHYS_",
    "NiagaraSystem": "NS_",
    "NiagaraEmitter": "NE_",
    "ParticleSystem": "PS_",
    "InputAction": "IA_",
    "InputMappingContext": "IMC_",
    "SoundWave": "SW_",
    "SoundCue": "SC_",
    "SoundAttenuation": "SA_",
    "SoundConcurrency": "SCN_",
    "MetaSoundSource": "MS_",
    "DataAsset": "DA_",
    "PrimaryDataAsset": "DA_",
    "DataTable": "DT_",
    "CurveTable": "CT_",
    "StringTable": "ST_",
    "LevelSequence": "LS_",
    "AnimSequence": "A_",
    "AnimMontage": "AM_",
    "BlendSpace": "BS_",
    "BlendSpace1D": "BS_",
    "Skeleton": "SK_",
    "IKRigDefinition": "IKR_",
    "IKRetargeter": "IKRT_",
    "BehaviorTree": "BT_",
    "BlackboardData": "BB_",
    "PCGGraph": "PCG_",
    "PCGGraphInterface": "PCG_",
    "World": "L_",
    "Level": "L_",
    "SubsurfaceProfile": "SP_",
    "PhysicalMaterial": "PM_"
}

LEGACY_PREFIXES = [
    "bp_", "Bp_", "BP_",
    "wbp_", "Wbp_", "WBP_",
    "abp_", "Abp_", "ABP_",
    "m_", "M_", "Mat_", "mat_", "MAT_", "Material_",
    "mi_", "MI_", "Mi_",
    "t_", "T_", "Tex_", "tex_", "TEX_", "Texture_",
    "sm_", "SM_", "Sm_", "Mesh_", "SMesh_", "StaticMesh_",
    "skm_", "SKM_", "Skm_", "SKMesh_", "SkeletalMesh_",
    "ns_", "NS_", "Ns_",
    "ne_", "NE_", "Ne_",
    "ia_", "IA_", "Ia_",
    "imc_", "IMC_", "Imc_",
    "sw_", "SW_", "Sw_",
    "sc_", "SC_", "Sc_",
    "da_", "DA_", "Da_",
    "dt_", "DT_", "Dt_",
    "ls_", "LS_", "Ls_"
]

def cpp_strip_legacy_prefix(in_asset_name: str, expected_prefix: str) -> str:
    name = in_asset_name

    # Check case-insensitive start with expected_prefix
    if name.lower().startswith(expected_prefix.lower()):
        name = name[len(expected_prefix):]
    else:
        for legacy in LEGACY_PREFIXES:
            if name.lower().startswith(legacy.lower()):
                name = name[len(legacy):]
                break

    while name.startswith("_"):
        name = name[1:]

    if not name:
        name = in_asset_name

    return name

def cpp_normalize_folder_path(folder_path: str) -> str:
    if not folder_path.startswith("/Game"):
        if folder_path.startswith("Content/"):
            folder_path = "/Game/" + folder_path[8:]
        elif folder_path == "Content":
            folder_path = "/Game"
        elif not folder_path.startswith("/"):
            folder_path = "/Game/" + folder_path

    # Normalize trailing slash
    if folder_path.endswith("/") and len(folder_path) > 1:
        folder_path = folder_path.rstrip("/")
    return folder_path

def test_folder_path_normalization():
    assert cpp_normalize_folder_path("/Game/UI") == "/Game/UI"
    assert cpp_normalize_folder_path("Content/UI") == "/Game/UI"
    assert cpp_normalize_folder_path("Content") == "/Game"
    assert cpp_normalize_folder_path("UI/Weapons/") == "/Game/UI/Weapons"
    assert cpp_normalize_folder_path("/Game/Folder/") == "/Game/Folder"

def test_prefix_stripping_and_renaming():
    # Test case 1: Already compliant exact case
    cleaned = cpp_strip_legacy_prefix("BP_PlayerCharacter", "BP_")
    assert "BP_" + cleaned == "BP_PlayerCharacter"

    # Test case 2: Wrong case prefix
    cleaned = cpp_strip_legacy_prefix("bp_PlayerCharacter", "BP_")
    assert "BP_" + cleaned == "BP_PlayerCharacter"

    # Test case 3: Legacy prefix "Mat_" for Material
    cleaned = cpp_strip_legacy_prefix("Mat_BrickWall", "M_")
    assert "M_" + cleaned == "M_BrickWall"

    # Test case 4: No prefix
    cleaned = cpp_strip_legacy_prefix("BrickWall", "M_")
    assert "M_" + cleaned == "M_BrickWall"

def test_organize_skip_subfolders():
    target_package_path = "/Game/MyFolder/Textures"
    
    # Asset inside target subfolder
    current_package_path_1 = "/Game/MyFolder/Textures"
    assert current_package_path_1.lower() == target_package_path.lower() or current_package_path_1.lower().startswith((target_package_path + "/").lower())

    # Asset inside sub-directory of target subfolder
    current_package_path_2 = "/Game/MyFolder/Textures/Character"
    assert current_package_path_2.lower() == target_package_path.lower() or current_package_path_2.lower().startswith((target_package_path + "/").lower())

    # Asset outside target subfolder
    current_package_path_3 = "/Game/MyFolder"
    assert not (current_package_path_3.lower() == target_package_path.lower() or current_package_path_3.lower().startswith((target_package_path + "/").lower()))

if __name__ == "__main__":
    pytest.main(["-v", __file__])
