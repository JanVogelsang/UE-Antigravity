import os
import sys

# Add pywin32 dll directory to path for Python 3.8+ on Windows
dll_path = r"C:\Users\Jan\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\site-packages\pywin32_system32"
if os.path.exists(dll_path):
    os.add_dll_directory(dll_path)
else:
    # Fallback to check other potential locations
    user_profile = os.environ.get("USERPROFILE", "")
    alt_path = os.path.join(user_profile, r"AppData\Local\Packages\PythonSoftwareFoundation.Python.3.13_qbz5n2kfra8p0\LocalCache\local-packages\Python313\site-packages\pywin32_system32")
    if os.path.exists(alt_path):
        os.add_dll_directory(alt_path)

import pytest
sys.exit(pytest.main(["-v", "-s"]))
