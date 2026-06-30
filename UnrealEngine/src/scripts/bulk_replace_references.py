import unreal

def bulk_replace_references(source_path, target_path):
    """
    Consolidates assets by replacing all references to the source asset
    with references to the target asset, then deletes the source asset.
    """
    if not unreal.EditorAssetLibrary.does_asset_exist(source_path):
        unreal.log_error(f"Source asset '{source_path}' does not exist.")
        return False

    if not unreal.EditorAssetLibrary.does_asset_exist(target_path):
        unreal.log_error(f"Target asset '{target_path}' does not exist.")
        return False

    if source_path == target_path:
        unreal.log_warning("Source and target assets are the same. Skipping consolidation.")
        return True

    unreal.log(f"Loading source asset: {source_path}")
    source_asset = unreal.EditorAssetLibrary.load_asset(source_path)
    if not source_asset:
        unreal.log_error(f"Failed to load source asset: {source_path}")
        return False

    unreal.log(f"Loading target asset: {target_path}")
    target_asset = unreal.EditorAssetLibrary.load_asset(target_path)
    if not target_asset:
        unreal.log_error(f"Failed to load target asset: {target_path}")
        return False

    unreal.log(f"Consolidating references from '{source_path}' to '{target_path}'...")
    try:
        # consolidate_assets replaces all references and deletes the assets in the list
        success = unreal.EditorAssetLibrary.consolidate_assets(target_asset, [source_asset])
        if success:
            unreal.log("Consolidation and reference replacement completed successfully.")
            return True
        else:
            unreal.log_error("Consolidate assets operation failed.")
            return False
    except Exception as e:
        unreal.log_error(f"An error occurred during reference replacement: {e}")
        return False

if __name__ == "__main__":
    # Example usage:
    # bulk_replace_references("/Game/OldMaterial", "/Game/NewMaterial")
    pass
