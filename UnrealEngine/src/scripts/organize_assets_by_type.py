import unreal
import os

# Mapping of asset classes to their target subfolder names
CLASS_TO_FOLDER = {
    "Blueprint": "Blueprints",
    "Texture2D": "Textures",
    "Material": "Materials",
    "MaterialInstanceConstant": "Materials",
    "MaterialInstance": "Materials",
    "StaticMesh": "Meshes",
    "SkeletalMesh": "Meshes",
    "NiagaraSystem": "Effects",
    "NiagaraEmitter": "Effects",
    "SoundCue": "Audio",
    "SoundWave": "Audio",
    "World": "Maps",
    "WidgetBlueprint": "UI"
}

def organize_assets_by_type(folder_path):
    """
    Organizes assets in the given folder_path recursively into type-specific subfolders.
    Unrecognized classes are left in their current directory.
    """
    if not unreal.EditorAssetLibrary.does_directory_exist(folder_path):
        unreal.log_error(f"Directory {folder_path} does not exist.")
        return

    # Clean target folder path representation
    folder_path = folder_path.rstrip("/")

    assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True)
    unreal.log(f"Scanning {len(assets)} assets to organize in {folder_path}...")

    moved_count = 0

    for asset_path in assets:
        asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
        if not asset_data:
            continue

        # Support both older and newer UE5 asset class resolution
        asset_class = str(asset_data.asset_class_path.asset_name) if hasattr(asset_data, 'asset_class_path') else str(asset_data.asset_class)
        subfolder_name = CLASS_TO_FOLDER.get(asset_class)

        if not subfolder_name:
            unreal.log_warning(f"Leaving uncategorized asset class '{asset_class}' in place: {asset_path}")
            continue

        target_parent = f"{folder_path}/{subfolder_name}"
        current_parent = os.path.dirname(asset_path).replace("\\", "/")

        # Skip if the asset is already in the target folder (or one of its subfolders)
        if current_parent.startswith(target_parent):
            continue

        asset_name = str(asset_data.asset_name)
        new_asset_path = f"{target_parent}/{asset_name}"

        # Resolve conflicts by appending numeric suffixes
        suffix_counter = 1
        while unreal.EditorAssetLibrary.does_asset_exist(new_asset_path):
            suffix = f"_{suffix_counter:02d}"
            new_asset_path = f"{target_parent}/{asset_name}{suffix}"
            suffix_counter += 1

        unreal.log(f"Moving: {asset_path} -> {new_asset_path}")
        if unreal.EditorAssetLibrary.rename_asset(asset_path, new_asset_path):
            moved_count += 1
        else:
            unreal.log_error(f"Failed to move: {asset_path}")

    unreal.log(f"Completed organize_assets_by_type. Organized {moved_count} assets.")

if __name__ == "__main__":
    # Example usage:
    # organize_assets_by_type("/Game/PathToFolder")
    pass
