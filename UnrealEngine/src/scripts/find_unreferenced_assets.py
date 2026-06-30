import unreal

def find_unreferenced_assets(folder_path):
    """
    Scans the specified folder recursively and returns a list of assets
    that have zero external referencers (dependencies pointing to them).
    Does NOT delete assets; only reports them.
    """
    if not unreal.EditorAssetLibrary.does_directory_exist(folder_path):
        unreal.log_error(f"Directory {folder_path} does not exist.")
        return []

    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    if ar.is_loading_assets():
        unreal.log_warning("Asset Registry is still loading assets. Results might be incomplete.")

    assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True)
    unreal.log(f"Scanning {len(assets)} assets in {folder_path} for referencers...")

    unreferenced_assets = []

    # Configure dependency query options to be as thorough as possible
    options = None
    try:
        options = unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=True,
            include_hard_package_references=True,
            include_searchable_names=True,
            include_soft_management_references=True,
            include_hard_management_references=True
        )
    except AttributeError:
        # Fallback if AssetRegistryDependencyOptions is not available in this UE version
        pass

    for asset_path in assets:
        asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
        if not asset_data:
            continue

        package_name = asset_data.package_name

        try:
            if options:
                referencers = ar.get_referencers(package_name, options)
            else:
                referencers = ar.get_referencers(package_name)
        except Exception as e:
            unreal.log_error(f"Failed to query referencers for {package_name}: {e}")
            continue

        # Filter out self-references (if any)
        external_referencers = [r for r in referencers if str(r) != str(package_name)]

        if len(external_referencers) == 0:
            unreferenced_assets.append(asset_path)
            unreal.log(f"Unreferenced asset found: {asset_path}")

    unreal.log(f"Scan complete. Found {len(unreferenced_assets)} unreferenced assets.")
    return unreferenced_assets

if __name__ == "__main__":
    # Example usage:
    # find_unreferenced_assets("/Game/PathToFolder")
    pass
