import unreal
import os

# Standard prefix mapping based on Unreal Engine 5 recommended naming conventions
PREFIXES = {
    "Blueprint": "BP_",
    "Texture2D": "T_",
    "Material": "M_",
    "MaterialInstanceConstant": "MI_",
    "MaterialInstance": "MI_",
    "StaticMesh": "SM_",
    "SkeletalMesh": "SK_",
    "NiagaraSystem": "NS_",
    "NiagaraEmitter": "NE_",
    "SoundCue": "SC_",
    "SoundWave": "S_",
    "World": "L_",
    "WidgetBlueprint": "WBP_"
}

def clean_naming_conventions(folder_path):
    """
    Scans the specified folder recursively and renames assets to match UE5 naming conventions.
    """
    if not unreal.EditorAssetLibrary.does_directory_exist(folder_path):
        unreal.log_error(f"Directory {folder_path} does not exist.")
        return

    assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True)
    unreal.log(f"Scanning {len(assets)} assets in {folder_path} for naming conventions...")

    renamed_count = 0

    for asset_path in assets:
        asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
        if not asset_data:
            continue

        # Support both older and newer UE5 asset class resolution
        asset_class = str(asset_data.asset_class_path.asset_name) if hasattr(asset_data, 'asset_class_path') else str(asset_data.asset_class)
        prefix = PREFIXES.get(asset_class)

        if not prefix:
            continue

        asset_name = str(asset_data.asset_name)

        # Skip if already correctly prefixed
        if asset_name.startswith(prefix):
            continue

        # Strip existing incorrect/case-insensitive prefixes if they exist
        temp_name = asset_name
        if asset_name.lower().startswith(prefix.lower()):
            temp_name = asset_name[len(prefix):]

        if not temp_name:
            unreal.log_warning(f"Asset name would be empty after stripping prefix: {asset_name}. Skipping.")
            continue

        new_asset_name = prefix + temp_name
        if new_asset_name == asset_name:
            continue

        parent_path = os.path.dirname(asset_path).replace("\\", "/")
        new_asset_path = f"{parent_path}/{new_asset_name}"

        # Handle naming conflicts by appending numeric suffixes
        suffix_counter = 1
        while unreal.EditorAssetLibrary.does_asset_exist(new_asset_path):
            suffix = f"_{suffix_counter:02d}"
            new_asset_path = f"{parent_path}/{new_asset_name}{suffix}"
            suffix_counter += 1

        unreal.log(f"Renaming: {asset_path} -> {new_asset_path}")
        if unreal.EditorAssetLibrary.rename_asset(asset_path, new_asset_path):
            renamed_count += 1
        else:
            unreal.log_error(f"Failed to rename: {asset_path}")

    unreal.log(f"Completed clean_naming_conventions. Renamed {renamed_count} assets.")

if __name__ == "__main__":
    # Example usage:
    # clean_naming_conventions("/Game/PathToFolder")
    pass
