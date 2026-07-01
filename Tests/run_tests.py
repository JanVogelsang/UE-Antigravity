import os
import sys

# Add pywin32 dll directory to path for Python 3.8+ on Windows
if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
    import site
    site_dirs = site.getsitepackages()
    if hasattr(site, "getusersitepackages"):
        site_dirs.append(site.getusersitepackages())
    for s_dir in site_dirs:
        pywin_path = os.path.join(s_dir, "pywin32_system32")
        if os.path.exists(pywin_path):
            try:
                os.add_dll_directory(pywin_path)
            except Exception:
                pass

import pytest
sys.exit(pytest.main(["-v", "-s"] + sys.argv[1:]))
