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

import json
import sqlite3
import asyncio
import math
from typing import Optional
import logging
import winreg
import re
import threading
import io
from pathlib import Path
import atexit
import signal

# Logging configuration
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger("CppAstMcpServer")

# Path Configuration relative to this file
SERVER_DIR = Path(__file__).resolve().parent.parent
DB_PATH = SERVER_DIR / "ast_cache.db"
REPO_ROOT = SERVER_DIR.parent.parent

# Resolve fallback paths dynamically relative to the repo root
fallback_base = REPO_ROOT.parent / "tau-game"
if not fallback_base.exists():
    user_profile = os.environ.get("USERPROFILE", "")
    if user_profile:
        fallback_base = Path(user_profile) / "Documents" / "Unreal Projects" / "tau-game"
    else:
        fallback_base = Path("c:/Users") / os.getlogin() / "Documents" / "Unreal Projects" / "tau-game"

PROJECT_UPROJECT = fallback_base / "Tau.uproject"
COMPILE_COMMANDS_PATH = fallback_base / "compile_commands.json"
WATCH_DIR = fallback_base / "Source"

def find_unreal_engine_dir() -> Optional[Path]:
    """
    Attempts to discover the Unreal Engine installation directory dynamically using the Windows Registry.
    """
    for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
        for subkey in (r"SOFTWARE\EpicGames\Unreal Engine", r"SOFTWARE\Epic Games\Unreal Engine\Builds"):
            try:
                with winreg.OpenKey(hive, subkey) as key:
                    if "Builds" in subkey:
                        try:
                            i = 0
                            while True:
                                name, value, _ = winreg.EnumValue(key, i)
                                if value and os.path.exists(value):
                                    return Path(value)
                                i += 1
                        except WindowsError:
                            pass
                    else:
                        i = 0
                        while True:
                            ver_name = winreg.EnumKey(key, i)
                            with winreg.OpenKey(key, ver_name) as ver_key:
                                ue_dir, _ = winreg.QueryValueEx(ver_key, "InstalledDirectory")
                                if ue_dir and os.path.exists(ue_dir):
                                    return Path(ue_dir)
                            i += 1
            except WindowsError:
                pass
    # Hardcoded fallbacks
    for p in [Path(r"D:\UE_5.7"), Path(r"C:\Program Files\Epic Games\UE_5.7"), Path(r"C:\Program Files\Epic Games\UE_5.6")]:
        if p.exists():
            return p
    return None

def resolve_project_paths():
    """
    Dynamically resolves project paths upward from the current working directory,
    falling back to environment overrides or hardcoded paths.
    """
    global PROJECT_UPROJECT, COMPILE_COMMANDS_PATH, WATCH_DIR
    
    # Check env override
    env_dir = os.environ.get("GAME_PROJECT_DIR")
    if env_dir and os.path.exists(env_dir):
        base_dir = Path(env_dir)
        logger.info(f"Using game project directory from env override: {base_dir}")
    else:
        # Search upwards from CWD for a .uproject file
        cwd = Path.cwd()
        base_dir = None
        for parent in [cwd] + list(cwd.parents):
            uprojects = list(parent.glob("*.uproject"))
            if uprojects:
                base_dir = parent
                logger.info(f"Dynamically discovered game project directory: {base_dir}")
                break
        
        # If not found, check if we are inside the UE-Antigravity repo and tau-game is next to it
        if not base_dir:
            sibling_dir = REPO_ROOT.parent / "tau-game"
            if sibling_dir.exists():
                base_dir = sibling_dir
                logger.info(f"Using sibling game project directory: {base_dir}")
                
    if base_dir:
        # Resolve target paths
        uprojects = list(base_dir.glob("*.uproject"))
        PROJECT_UPROJECT = uprojects[0] if uprojects else base_dir / "Tau.uproject"
        COMPILE_COMMANDS_PATH = base_dir / "compile_commands.json"
        WATCH_DIR = base_dir / "Source"
        logger.info("Resolved paths:")
        logger.info(f"  - PROJECT_UPROJECT: {PROJECT_UPROJECT}")
        logger.info(f"  - COMPILE_COMMANDS_PATH: {COMPILE_COMMANDS_PATH}")
        logger.info(f"  - WATCH_DIR: {WATCH_DIR}")
    else:
        logger.warning(f"Could not dynamically resolve game project directory. Falling back to default: {PROJECT_UPROJECT.parent}")

# Resolve paths immediately upon module load
resolve_project_paths()

# 1. Dependency Auto-Installation
def check_and_install_dependencies():
    required = {
        'mcp': 'mcp',
        'clang': 'libclang',
        'watchdog': 'watchdog'
    }
    missing = []
    try:
        import mcp
    except ImportError:
        missing.append(required['mcp'])
    try:
        import clang.cindex
    except ImportError:
        missing.append(required['clang'])
    try:
        import watchdog
    except ImportError:
        missing.append(required['watchdog'])
        
    if missing:
        import subprocess
        logger.info(f"Missing python dependencies detected: {missing}. Installing...")
        try:
            subprocess.run(
                [sys.executable, "-m", "pip", "install", "--user"] + missing,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=True
            )
            logger.info("Successfully installed missing dependencies.")
        except Exception as e:
            logger.error(f"Failed to install dependency: {e}")

check_and_install_dependencies()

# Import the actual packages now that they are guaranteed to be installed
from mcp.server.fastmcp import FastMCP
import clang.cindex
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

# Initialize FastMCP Server
mcp = FastMCP("UE-Antigravity C++ AST & RAG Server")

# 2. Dynamic libclang.dll loader
def resolve_and_load_libclang():
    """
    Locates libclang.dll on Windows in standard installation folders,
    Unreal Engine, system PATH, or registry, and loads it into clang.cindex.
    Returns True on success, raises ImportError on failure.
    """
    if clang.cindex.Config.library_file or clang.cindex.Config.library_path:
        try:
            clang.cindex.Index.create()
            return True
        except Exception:
            pass

    candidates = []

    # 1. Environment Variable Override
    env_libclang = os.environ.get("LIBCLANG_PATH")
    if env_libclang:
        candidates.append(Path(env_libclang))
    
    env_llvm = os.environ.get("LLVM_PATH")
    if env_llvm:
        candidates.append(Path(env_llvm) / "bin" / "libclang.dll")

    # 2. Registry Lookup: Standard LLVM Installer
    for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
        for subkey in (r"SOFTWARE\LLVM", r"SOFTWARE\WOW6432Node\LLVM"):
            try:
                with winreg.OpenKey(hive, subkey) as key:
                    llvm_dir, _ = winreg.QueryValueEx(key, "LLVM")
                    if llvm_dir:
                        candidates.append(Path(llvm_dir) / "bin" / "libclang.dll")
            except WindowsError:
                pass

    # 3. Registry Lookup: Epic Games / Unreal Engine Installations
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\EpicGames\Unreal Engine") as key:
            i = 0
            while True:
                try:
                    ver_name = winreg.EnumKey(key, i)
                    with winreg.OpenKey(key, ver_name) as ver_key:
                        ue_dir, _ = winreg.QueryValueEx(ver_key, "InstalledDirectory")
                        if ue_dir:
                            iwyu_dll = Path(ue_dir) / "Engine" / "Binaries" / "ThirdParty" / "IWYU" / "libclang.dll"
                            candidates.append(iwyu_dll)
                    i += 1
                except WindowsError:
                    break
    except WindowsError:
        pass

    # 4. Typical Windows Directories (Hardcoded Fallbacks)
    fallbacks = [
        Path(r"C:\Program Files\LLVM\bin\libclang.dll"),
        Path(r"C:\Program Files (x86)\LLVM\bin\libclang.dll"),
        Path(r"D:\UE_5.7\Engine\Binaries\ThirdParty\IWYU\libclang.dll"),
    ]
    candidates.extend(fallbacks)

    # 5. Environment PATH Parsing
    path_env = os.environ.get("PATH", "")
    for part in path_env.split(os.pathsep):
        if part:
            candidates.append(Path(part) / "libclang.dll")

    # Deduplicate and check file existence
    resolved_candidates = []
    seen = set()
    for p in candidates:
        try:
            abs_p = p.resolve().absolute()
            if abs_p.name.lower() == "libclang.dll" and abs_p.is_file() and abs_p not in seen:
                seen.add(abs_p)
                resolved_candidates.append(abs_p)
        except Exception:
            pass

    # Try loading each candidate
    errors = []
    for dll_path in resolved_candidates:
        dll_dir = dll_path.parent
        cookie = None
        
        # Windows Python 3.8+: Add parent directory to DLL search paths
        if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
            try:
                cookie = os.add_dll_directory(str(dll_dir))
            except Exception:
                pass

        try:
            clang.cindex.Config.set_library_file(str(dll_path))
            clang.cindex.Index.create()
            logger.info(f"Loaded libclang.dll successfully from: {dll_path}")
            return True
        except Exception as e:
            errors.append(f"{dll_path}: {e}")
            clang.cindex.Config.set_library_file(None)
            if cookie:
                try:
                    cookie.close()
                except Exception:
                    pass

    # 6. Final Fallback: Attempt standard ctypes loading (let clang.cindex search)
    try:
        clang.cindex.Index.create()
        return True
    except Exception as e:
        errors.append(f"Standard ctypes load: {e}")

    raise ImportError(
        f"Failed to load libclang.dll. Evaluated paths:\n"
        + "\n".join(f" - {p}" for p in resolved_candidates)
        + f"\nErrors:\n"
        + "\n".join(f" - {err}" for err in errors)
    )

resolve_and_load_libclang()

# 3. Database connection and schema
def get_db_connection():
    conn = sqlite3.connect(DB_PATH, timeout=30.0)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON;")
    conn.execute("PRAGMA journal_mode = WAL;")
    return conn

def init_db():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    
    # Check schema migration
    if DB_PATH.exists():
        try:
            conn = sqlite3.connect(DB_PATH)
            cursor = conn.cursor()
            cursor.execute("PRAGMA table_info(symbols);")
            columns = [row[1] for row in cursor.fetchall()]
            conn.close()
            if "usr" not in columns:
                logger.info("Outdated schema detected in ast_cache.db. Deleting and recreating...")
                try:
                    os.remove(DB_PATH)
                except Exception as e:
                    logger.warning(f"Could not delete old db file: {e}")
        except Exception as e:
            logger.warning(f"Error checking database schema: {e}")
            
    conn = get_db_connection()
    cursor = conn.cursor()
    
    # 1. Files Table
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS files (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        file_path TEXT UNIQUE NOT NULL,
        last_parsed_mtime REAL NOT NULL,
        status TEXT NOT NULL DEFAULT 'parsed'
    );
    """)
    
    # 2. Symbols Table
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS symbols (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        file_id INTEGER NOT NULL,
        name TEXT NOT NULL,
        fully_qualified_name TEXT NOT NULL,
        kind TEXT NOT NULL,
        parent_symbol_id INTEGER,
        access_specifier TEXT,
        return_type TEXT,
        is_static INTEGER NOT NULL DEFAULT 0,
        is_virtual INTEGER NOT NULL DEFAULT 0,
        is_const INTEGER NOT NULL DEFAULT 0,
        is_override INTEGER NOT NULL DEFAULT 0,
        line_start INTEGER NOT NULL,
        line_end INTEGER NOT NULL,
        raw_declaration TEXT,
        usr TEXT,
        FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
        FOREIGN KEY (parent_symbol_id) REFERENCES symbols(id) ON DELETE CASCADE
    );
    """)
    
    # 3. Symbol Inheritance Table
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS symbol_inheritance (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        child_symbol_id INTEGER NOT NULL,
        parent_class_name TEXT NOT NULL,
        parent_symbol_id INTEGER,
        access_specifier TEXT NOT NULL DEFAULT 'public',
        FOREIGN KEY (child_symbol_id) REFERENCES symbols(id) ON DELETE CASCADE,
        FOREIGN KEY (parent_symbol_id) REFERENCES symbols(id) ON DELETE SET NULL
    );
    """)
    
    # 4. Method Parameters Table
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS method_parameters (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        symbol_id INTEGER NOT NULL,
        name TEXT NOT NULL,
        type TEXT NOT NULL,
        position INTEGER NOT NULL,
        default_value TEXT,
        FOREIGN KEY (symbol_id) REFERENCES symbols(id) ON DELETE CASCADE
    );
    """)
    
    # 5. Properties Table
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS properties (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        file_id INTEGER NOT NULL,
        class_symbol_id INTEGER NOT NULL,
        name TEXT NOT NULL,
        type TEXT NOT NULL,
        access_specifier TEXT NOT NULL,
        is_static INTEGER NOT NULL DEFAULT 0,
        is_mutable INTEGER NOT NULL DEFAULT 0,
        uproperty_metadata TEXT,
        line INTEGER NOT NULL,
        FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
        FOREIGN KEY (class_symbol_id) REFERENCES symbols(id) ON DELETE CASCADE
    );
    """)
    
    # 6. Function Calls Table
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS function_calls (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        caller_symbol_id INTEGER NOT NULL,
        callee_name TEXT NOT NULL,
        callee_symbol_id INTEGER,
        line INTEGER NOT NULL,
        callee_usr TEXT,
        FOREIGN KEY (caller_symbol_id) REFERENCES symbols(id) ON DELETE CASCADE,
        FOREIGN KEY (callee_symbol_id) REFERENCES symbols(id) ON DELETE SET NULL
    );
    """)
    
    # Create indexes
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_files_path ON files(file_path);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_symbols_file_id ON symbols(file_id);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_symbols_name ON symbols(name);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_symbols_fqn ON symbols(fully_qualified_name);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_symbols_parent ON symbols(parent_symbol_id);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_symbols_kind ON symbols(kind);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_inheritance_child ON symbol_inheritance(child_symbol_id);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_params_symbol_id ON method_parameters(symbol_id);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_properties_file_id ON properties(file_id);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_properties_class ON properties(class_symbol_id);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_calls_caller ON function_calls(caller_symbol_id);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_calls_callee ON function_calls(callee_symbol_id);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_symbols_usr ON symbols(usr);")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_calls_callee_usr ON function_calls(callee_usr);")
    
    conn.commit()
    conn.close()

# UPROPERTY metadata parsing
def parse_uproperty_meta(file_lines, line_idx):
    if not file_lines:
        return None
    for i in range(max(0, line_idx - 5), line_idx):
        line_text = file_lines[i].strip()
        match = re.search(r'UPROPERTY\s*\((.*?)\)', line_text)
        if match:
            meta_str = match.group(1)
            metadata = {}
            parts = re.split(r',\s*(?![^()]*\))', meta_str)
            for part in parts:
                part = part.strip()
                if not part:
                    continue
                if '=' in part:
                    try:
                        k, v = part.split('=', 1)
                        k = k.strip()
                        v = v.strip().strip('"').strip("'")
                        metadata[k] = v
                    except Exception:
                        pass
                else:
                    metadata[part] = True
            return json.dumps(metadata)
    return None

import functools
parse_lock = threading.RLock()

def serialized_write(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        with parse_lock:
            return func(*args, **kwargs)
    return wrapper

# C++ File Parser - AST Extraction
def extract_ast_from_file(file_path_abs, compile_args, file_lines):
    index = clang.cindex.Index.create()
    translation_unit = index.parse(file_path_abs, args=compile_args)
    
    symbols = []
    properties = []
    inheritance = []
    parameters = []
    calls = []
    
    next_temp_id = [0]
    
    def traverse(node, parent_temp_id=None, current_function_temp_id=None):
        if node.kind != clang.cindex.CursorKind.TRANSLATION_UNIT:
            if not node.location.file or os.path.normcase(os.path.abspath(node.location.file.name)) != file_path_abs:
                return
                
        node_kind = node.kind
        temp_id = parent_temp_id
        func_temp_id = current_function_temp_id
        
        raw_decl = None
        if node.extent.start.line and node.extent.end.line:
            try:
                raw_decl = "".join(file_lines[node.extent.start.line - 1 : node.extent.end.line]).strip()
            except Exception:
                pass
        if not raw_decl:
            raw_decl = node.spelling
            
        usr = None
        try:
            usr = node.get_usr()
        except Exception:
            pass
            
        if node_kind in (clang.cindex.CursorKind.CLASS_DECL,
                         clang.cindex.CursorKind.STRUCT_DECL,
                         clang.cindex.CursorKind.ENUM_DECL,
                         clang.cindex.CursorKind.NAMESPACE,
                         clang.cindex.CursorKind.CLASS_TEMPLATE):
            kind_str = {
                clang.cindex.CursorKind.CLASS_DECL: 'class',
                clang.cindex.CursorKind.STRUCT_DECL: 'struct',
                clang.cindex.CursorKind.ENUM_DECL: 'enum',
                clang.cindex.CursorKind.NAMESPACE: 'namespace',
                clang.cindex.CursorKind.CLASS_TEMPLATE: 'class'
            }[node_kind]
            
            my_temp_id = next_temp_id[0]
            next_temp_id[0] += 1
            
            symbols.append({
                "temp_id": my_temp_id,
                "name": node.spelling,
                "kind": kind_str,
                "parent_temp_id": parent_temp_id,
                "access_specifier": node.access_specifier.name.lower() if node.access_specifier else None,
                "return_type": None,
                "is_static": 0,
                "is_virtual": 0,
                "is_const": 0,
                "is_override": 0,
                "line_start": node.extent.start.line,
                "line_end": node.extent.end.line,
                "raw_declaration": raw_decl,
                "usr": usr
            })
            temp_id = my_temp_id
            
            for child in node.get_children():
                if child.kind == clang.cindex.CursorKind.CXX_BASE_SPECIFIER:
                    base_name = child.type.spelling.replace("class ", "").replace("struct ", "").strip()
                    inheritance.append({
                        "child_temp_id": my_temp_id,
                        "parent_class_name": base_name,
                        "access_specifier": child.access_specifier.name.lower() if child.access_specifier else 'public'
                    })
                    
        elif node_kind in (clang.cindex.CursorKind.FIELD_DECL, clang.cindex.CursorKind.VAR_DECL) and parent_temp_id is not None:
            uproperty_metadata = parse_uproperty_meta(file_lines, node.location.line - 1)
            access = node.access_specifier.name.lower() if node.access_specifier else 'private'
            is_static = 1 if node_kind == clang.cindex.CursorKind.VAR_DECL else 0
            is_mutable = 1 if hasattr(node, 'is_mutable_field') and node.is_mutable_field() else 0
            
            properties.append({
                "class_temp_id": parent_temp_id,
                "name": node.spelling,
                "type": node.type.spelling,
                "access_specifier": access,
                "is_static": is_static,
                "is_mutable": is_mutable,
                "uproperty_metadata": uproperty_metadata,
                "line": node.location.line
            })
            
        elif node_kind in (clang.cindex.CursorKind.FUNCTION_DECL, clang.cindex.CursorKind.CXX_METHOD, clang.cindex.CursorKind.FUNCTION_TEMPLATE):
            kind_str = 'method' if (node_kind == clang.cindex.CursorKind.CXX_METHOD or (node_kind == clang.cindex.CursorKind.FUNCTION_TEMPLATE and parent_temp_id is not None)) else 'function'
            
            my_temp_id = next_temp_id[0]
            next_temp_id[0] += 1
            
            semantic_parent_name = None
            if parent_temp_id is None and node.semantic_parent:
                sp = node.semantic_parent
                if sp.kind in (clang.cindex.CursorKind.CLASS_DECL,
                               clang.cindex.CursorKind.STRUCT_DECL,
                               clang.cindex.CursorKind.NAMESPACE,
                               clang.cindex.CursorKind.CLASS_TEMPLATE):
                    semantic_parent_name = sp.type.spelling if sp.type.spelling else sp.spelling
                    if not semantic_parent_name:
                        semantic_parent_name = sp.spelling
            
            access = node.access_specifier.name.lower() if node.access_specifier else None
            is_static = 1 if hasattr(node, 'is_static_method') and node.is_static_method() else 0
            is_virtual = 1 if hasattr(node, 'is_virtual_method') and node.is_virtual_method() else 0
            is_const = 1 if hasattr(node, 'is_const_method') and node.is_const_method() else 0
            
            is_override = 0
            try:
                for token in node.get_tokens():
                    if token.spelling == 'override':
                        is_override = 1
                        break
            except Exception:
                pass
            if not is_override and file_lines:
                try:
                    decl_text = "".join(file_lines[node.extent.start.line - 1 : node.extent.end.line])
                    if re.search(r'\boverride\b', decl_text):
                        is_override = 1
                except Exception:
                    pass
                    
            symbols.append({
                "temp_id": my_temp_id,
                "name": node.spelling,
                "kind": kind_str,
                "parent_temp_id": parent_temp_id,
                "semantic_parent_name": semantic_parent_name,
                "access_specifier": access,
                "return_type": node.result_type.spelling,
                "is_static": is_static,
                "is_virtual": is_virtual,
                "is_const": is_const,
                "is_override": is_override,
                "line_start": node.extent.start.line,
                "line_end": node.extent.end.line,
                "raw_declaration": raw_decl,
                "usr": usr
            })
            temp_id = my_temp_id
            func_temp_id = my_temp_id
            
            param_pos = 0
            for child in node.get_children():
                if child.kind == clang.cindex.CursorKind.PARM_DECL:
                    parameters.append({
                        "symbol_temp_id": my_temp_id,
                        "name": child.spelling,
                        "type": child.type.spelling,
                        "position": param_pos
                    })
                    param_pos += 1
                    
        elif node_kind == clang.cindex.CursorKind.CALL_EXPR and current_function_temp_id is not None:
            callee_usr = None
            try:
                if node.referenced:
                    callee_usr = node.referenced.get_usr()
            except Exception:
                pass
            calls.append({
                "caller_temp_id": current_function_temp_id,
                "callee_name": node.spelling,
                "line": node.location.line,
                "callee_usr": callee_usr
            })
            
        for child in node.get_children():
            traverse(child, temp_id, func_temp_id)
            
    traverse(translation_unit.cursor)
    
    return {
        "symbols": symbols,
        "properties": properties,
        "inheritance": inheritance,
        "parameters": parameters,
        "calls": calls
    }

# C++ File Parser - DB Writer
@serialized_write
def write_ast_data_to_db(file_path_abs, mtime, ast_data):
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        cursor.execute("SELECT id FROM files WHERE file_path = ?", (file_path_abs,))
        row = cursor.fetchone()
        
        cursor.execute("BEGIN TRANSACTION;")
        
        if row:
            file_id = row["id"]
            cursor.execute("DELETE FROM symbols WHERE file_id = ?", (file_id,))
            cursor.execute("DELETE FROM properties WHERE file_id = ?", (file_id,))
            cursor.execute("UPDATE files SET last_parsed_mtime = ?, status = 'parsed' WHERE id = ?", (mtime, file_id))
        else:
            cursor.execute("INSERT INTO files (file_path, last_parsed_mtime, status) VALUES (?, ?, 'parsed')", (file_path_abs, mtime))
            file_id = cursor.lastrowid
            
        temp_to_db_id = {}
        
        for sym in ast_data["symbols"]:
            parent_temp_id = sym["parent_temp_id"]
            parent_db_id = temp_to_db_id.get(parent_temp_id) if parent_temp_id is not None else None
            
            if parent_db_id is None and sym.get("semantic_parent_name"):
                sp_name = sym["semantic_parent_name"]
                cursor.execute("SELECT id FROM symbols WHERE fully_qualified_name = ?", (sp_name,))
                sp_row = cursor.fetchone()
                if sp_row:
                    parent_db_id = sp_row["id"]
            
            fqn = sym["name"]
            if parent_db_id:
                cursor.execute("SELECT fully_qualified_name FROM symbols WHERE id = ?", (parent_db_id,))
                p_row = cursor.fetchone()
                if p_row:
                    fqn = f"{p_row['fully_qualified_name']}::{sym['name']}"
                    
            cursor.execute("""
                INSERT INTO symbols (
                    file_id, name, fully_qualified_name, kind, parent_symbol_id, 
                    access_specifier, return_type, is_static, is_virtual, is_const, 
                    is_override, line_start, line_end, raw_declaration, usr
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                file_id, sym["name"], fqn, sym["kind"], parent_db_id,
                sym["access_specifier"], sym["return_type"], sym["is_static"],
                sym["is_virtual"], sym["is_const"], sym["is_override"],
                sym["line_start"], sym["line_end"], sym["raw_declaration"], sym["usr"]
            ))
            db_id = cursor.lastrowid
            temp_to_db_id[sym["temp_id"]] = db_id
            
        for prop in ast_data["properties"]:
            class_db_id = temp_to_db_id.get(prop["class_temp_id"])
            if class_db_id:
                cursor.execute("""
                    INSERT INTO properties (
                        file_id, class_symbol_id, name, type, access_specifier, 
                        is_static, is_mutable, uproperty_metadata, line
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """, (
                    file_id, class_db_id, prop["name"], prop["type"], prop["access_specifier"],
                    prop["is_static"], prop["is_mutable"], prop["uproperty_metadata"], prop["line"]
                ))
                
        for inh in ast_data["inheritance"]:
            child_db_id = temp_to_db_id.get(inh["child_temp_id"])
            if child_db_id:
                cursor.execute("SELECT id FROM symbols WHERE name = ? OR fully_qualified_name = ?", (inh["parent_class_name"], inh["parent_class_name"]))
                base_row = cursor.fetchone()
                base_id = base_row["id"] if base_row else None
                
                cursor.execute("""
                    INSERT INTO symbol_inheritance (child_symbol_id, parent_class_name, parent_symbol_id, access_specifier)
                    VALUES (?, ?, ?, ?)
                """, (child_db_id, inh["parent_class_name"], base_id, inh["access_specifier"]))
                
        for param in ast_data["parameters"]:
            sym_db_id = temp_to_db_id.get(param["symbol_temp_id"])
            if sym_db_id:
                cursor.execute("""
                    INSERT INTO method_parameters (symbol_id, name, type, position)
                    VALUES (?, ?, ?, ?)
                """, (sym_db_id, param["name"], param["type"], param["position"]))
                
        for call in ast_data["calls"]:
            caller_db_id = temp_to_db_id.get(call["caller_temp_id"])
            if caller_db_id:
                cursor.execute("""
                    INSERT INTO function_calls (caller_symbol_id, callee_name, callee_symbol_id, line, callee_usr)
                    VALUES (?, ?, ?, ?, ?)
                """, (caller_db_id, call["callee_name"], None, call["line"], call["callee_usr"]))
                
        cursor.execute("""
            UPDATE function_calls
            SET callee_symbol_id = (
                SELECT id FROM symbols 
                WHERE usr = function_calls.callee_usr 
                LIMIT 1
            )
            WHERE callee_usr IS NOT NULL AND callee_symbol_id IS NULL;
        """)
        cursor.execute("""
            UPDATE function_calls
            SET callee_symbol_id = (
                SELECT id FROM symbols 
                WHERE name = function_calls.callee_name 
                LIMIT 1
            )
            WHERE callee_symbol_id IS NULL;
        """)
        cursor.execute("""
            UPDATE symbol_inheritance
            SET parent_symbol_id = (
                SELECT id FROM symbols
                WHERE name = symbol_inheritance.parent_class_name
                LIMIT 1
            )
            WHERE parent_symbol_id IS NULL;
        """)
        
        conn.commit()
    except Exception as e:
        conn.rollback()
        logger.error(f"Error writing AST data to DB for {file_path_abs}: {e}", exc_info=True)
        cursor.execute("INSERT OR REPLACE INTO files (file_path, last_parsed_mtime, status) VALUES (?, ?, 'failed')", (file_path_abs, mtime))
        conn.commit()
    finally:
        conn.close()

# C++ File Parser - Process Worker entry point
def worker_parse_file(file_path):
    file_path_abs = os.path.normcase(os.path.abspath(file_path))
    if not os.path.exists(file_path_abs):
        return None
        
    mtime = os.path.getmtime(file_path_abs)
    
    compile_args = ['-x', 'c++', '-std=c++17']
    if COMPILE_COMMANDS_PATH.exists():
        try:
            with open(COMPILE_COMMANDS_PATH, "r") as f:
                commands = json.load(f)
                for cmd in commands:
                    cmd_file = cmd.get("file", "")
                    if cmd_file and os.path.normcase(os.path.abspath(cmd_file)) == file_path_abs:
                        arguments = cmd.get("arguments", [])
                        if not arguments and "command" in cmd:
                            import shlex
                            arguments = shlex.split(cmd["command"])
                        if arguments:
                            filtered = []
                            skip = False
                            for arg in arguments[1:]:
                                if skip:
                                    skip = False
                                    continue
                                if arg in ('-o', '--output'):
                                    skip = True
                                    continue
                                if os.path.normcase(os.path.abspath(arg)) == file_path_abs:
                                    continue
                                filtered.append(arg)
                            if filtered:
                                compile_args = filtered
                        break
        except Exception:
            pass
            
    try:
        with open(file_path_abs, 'r', encoding='utf-8', errors='ignore') as f:
            file_lines = f.readlines()
    except Exception:
        file_lines = []
        
    try:
        ast_data = extract_ast_from_file(file_path_abs, compile_args, file_lines)
        return {
            "file_path": file_path_abs,
            "mtime": mtime,
            "ast_data": ast_data
        }
    except Exception as e:
        logger.error(f"Worker failed to parse {file_path_abs}: {e}")
        return {
            "file_path": file_path_abs,
            "mtime": mtime,
            "ast_data": None,
            "error": str(e)
        }

# C++ File Parser
@serialized_write
def parse_cpp_file(file_path: str):
    file_path_abs = os.path.normcase(os.path.abspath(file_path))
    if not os.path.exists(file_path_abs):
        return
        
    mtime = os.path.getmtime(file_path_abs)
    
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        cursor.execute("SELECT id, last_parsed_mtime FROM files WHERE file_path = ?", (file_path_abs,))
        row = cursor.fetchone()
        if row and row["last_parsed_mtime"] >= mtime:
            conn.close()
            return
    finally:
        conn.close()
        
    logger.info(f"Parsing AST for C++ file: {file_path_abs}")
    res = worker_parse_file(file_path_abs)
    if res and res.get("ast_data"):
        write_ast_data_to_db(res["file_path"], res["mtime"], res["ast_data"])

# On-demand file finder
def find_defining_file(symbol_name: str) -> Optional[str]:
    patterns = [
        re.compile(rf'\b(class|struct)\s+{symbol_name}\b'),
        re.compile(rf'\b{symbol_name}\b')
    ]
    if WATCH_DIR.exists():
        conn = get_db_connection()
        cursor = conn.cursor()
        try:
            cursor.execute("SELECT file_path FROM files")
            indexed_files = {os.path.normcase(row["file_path"]) for row in cursor.fetchall()}
        except Exception:
            indexed_files = set()
        finally:
            conn.close()
            
        unindexed_files = []
        for ext in ('.h', '.hpp'):
            for file_path in WATCH_DIR.rglob(f"*{ext}"):
                path_parts = file_path.parts
                if any(x in path_parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                    continue
                abs_path = os.path.normcase(os.path.abspath(file_path))
                if abs_path not in indexed_files:
                    unindexed_files.append(abs_path)
                    
        for abs_path in unindexed_files:
            try:
                content = Path(abs_path).read_text(encoding='utf-8', errors='ignore')
                if patterns[0].search(content) or patterns[1].search(content):
                    return abs_path
            except Exception:
                pass
                
        unindexed_cpp = []
        for file_path in WATCH_DIR.rglob("*.cpp"):
            path_parts = file_path.parts
            if any(x in path_parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                continue
            abs_path = os.path.normcase(os.path.abspath(file_path))
            if abs_path not in indexed_files:
                unindexed_cpp.append(abs_path)
                
        for abs_path in unindexed_cpp:
            try:
                content = Path(abs_path).read_text(encoding='utf-8', errors='ignore')
                if patterns[1].search(content):
                    return abs_path
            except Exception:
                pass
    return None

# Initial background indexing thread
def background_initial_indexing(force=False):
    global active_executor
    if not WATCH_DIR.exists():
        return
    logger.info(f"Starting initial index scan on {WATCH_DIR} (force={force})...")
    files_to_scan = []
    for ext in ('.h', '.cpp', '.hpp', '.inl'):
        for file_path in WATCH_DIR.rglob(f"*{ext}"):
            path_parts = file_path.parts
            if any(x in path_parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                continue
            files_to_scan.append(file_path)
            
    conn = get_db_connection()
    cursor = conn.cursor()
    
    files_to_parse = []
    for file_path in files_to_scan:
        file_path_abs = os.path.normcase(os.path.abspath(file_path))
        mtime = os.path.getmtime(file_path_abs)
        try:
            if force:
                files_to_parse.append((file_path_abs, mtime))
            else:
                cursor.execute("SELECT last_parsed_mtime FROM files WHERE file_path = ?", (file_path_abs,))
                row = cursor.fetchone()
                if not row or row[0] < mtime:
                    files_to_parse.append((file_path_abs, mtime))
        except Exception as e:
            logger.error(f"Error checking file {file_path_abs} in DB: {e}")
            
    conn.close()
    
    if not files_to_parse:
        logger.info("No files need indexing. Initial index scan complete.")
        return
        
    logger.info(f"Found {len(files_to_parse)} files that need indexing/updating. Spawning multiprocessing parser...")
    
    from concurrent.futures import ProcessPoolExecutor, as_completed
    max_workers = max(1, os.cpu_count() - 1)
    
    paths_to_parse = [item[0] for item in files_to_parse]
    
    parsed_count = 0
    active_executor = ProcessPoolExecutor(max_workers=max_workers)
    try:
        futures = {active_executor.submit(worker_parse_file, path): path for path in paths_to_parse}
        for future in as_completed(futures):
            if active_executor is None:
                logger.info("Executor has been shut down. Stopping indexing loop.")
                break
            path = futures[future]
            try:
                res = future.result()
                if res and res.get("ast_data"):
                    write_ast_data_to_db(res["file_path"], res["mtime"], res["ast_data"])
                    parsed_count += 1
                elif res and "error" in res:
                    logger.error(f"Failed to parse {path}: {res['error']}")
            except Exception as e:
                logger.error(f"Exception parsing {path} in worker process: {e}")
    finally:
        if active_executor:
            active_executor.shutdown(wait=True)
            active_executor = None
                
    logger.info(f"Initial index scan complete. Parsed and updated {parsed_count}/{len(files_to_parse)} files.")

# 4. Watchdog EventHandler
class CppSourceHandler(FileSystemEventHandler):
    def on_modified(self, event):
        if event.is_directory:
            return
        if event.src_path.endswith(('.h', '.cpp', '.hpp', '.inl')):
            path_parts = Path(event.src_path).parts
            if any(x in path_parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                return
            logger.info(f"Watchdog: file modified: {event.src_path}")
            try:
                parse_cpp_file(event.src_path)
            except Exception as e:
                logger.error(f"Watchdog error in on_modified for {event.src_path}: {e}")

    def on_created(self, event):
        if event.is_directory:
            return
        if event.src_path.endswith(('.h', '.cpp', '.hpp', '.inl')):
            path_parts = Path(event.src_path).parts
            if any(x in path_parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                return
            logger.info(f"Watchdog: file created: {event.src_path}")
            try:
                parse_cpp_file(event.src_path)
            except Exception as e:
                logger.error(f"Watchdog error in on_created for {event.src_path}: {e}")

    def on_deleted(self, event):
        if event.is_directory:
            return
        if event.src_path.endswith(('.h', '.cpp', '.hpp', '.inl')):
            file_path_abs = os.path.normcase(os.path.abspath(event.src_path))
            logger.info(f"Watchdog: file deleted: {file_path_abs}")
            try:
                with parse_lock:
                    conn = get_db_connection()
                    try:
                        conn.execute("DELETE FROM files WHERE file_path = ?", (file_path_abs,))
                        conn.commit()
                    except Exception as e:
                        logger.error(f"Error handling file deletion in DB: {e}")
                    finally:
                        conn.close()
            except Exception as e:
                logger.error(f"Watchdog error in on_deleted for {file_path_abs}: {e}")

def start_watcher(loop=None):
    observer = Observer()
    handler = CppSourceHandler()
    if WATCH_DIR.exists():
        observer.schedule(handler, path=str(WATCH_DIR), recursive=True)
        logger.info(f"Watchdog observer scheduled on C++ source directory: {WATCH_DIR}")
    else:
        logger.warning(f"Watchdog directory does not exist: {WATCH_DIR}")
        
    try:
        content_dir = PROJECT_UPROJECT.parent / "Content"
        if content_dir.exists():
            uasset_handler = UAssetHandler()
            observer.schedule(uasset_handler, path=str(content_dir), recursive=True)
            logger.info(f"Watchdog observer scheduled on Content directory: {content_dir}")
        else:
            logger.warning(f"Content directory does not exist: {content_dir}")
    except Exception as e:
        logger.error(f"Failed to schedule Content directory observer: {e}")
        
    observer.start()
    logger.info("Watchdog observers started.")
    return observer

# 5. MCP Tool Implementations
def _query_cpp_ast_sync(query: str) -> str:
    search_name = query.replace("class ", "").strip()
    
    conn = get_db_connection()
    cursor = conn.cursor()
    
    try:
        # Check cache
        if "::" in search_name:
            cursor.execute("""
                SELECT s.*, f.file_path 
                FROM symbols s
                JOIN files f ON s.file_id = f.id
                WHERE s.fully_qualified_name = ? OR s.fully_qualified_name LIKE ?
            """, (search_name, "%::" + search_name))
        else:
            cursor.execute("""
                SELECT s.*, f.file_path 
                FROM symbols s
                JOIN files f ON s.file_id = f.id
                WHERE s.name = ? OR s.fully_qualified_name = ?
            """, (search_name, search_name))
        symbol_rows = cursor.fetchall()
        
        # On-demand parsing if not cached
        if not symbol_rows:
            defining_file = find_defining_file(search_name)
            if defining_file:
                parse_cpp_file(defining_file)
                if "::" in search_name:
                    cursor.execute("""
                        SELECT s.*, f.file_path 
                        FROM symbols s
                        JOIN files f ON s.file_id = f.id
                        WHERE s.fully_qualified_name = ? OR s.fully_qualified_name LIKE ?
                    """, (search_name, "%::" + search_name))
                else:
                    cursor.execute("""
                        SELECT s.*, f.file_path 
                        FROM symbols s
                        JOIN files f ON s.file_id = f.id
                        WHERE s.name = ? OR s.fully_qualified_name = ?
                    """, (search_name, search_name))
                symbol_rows = cursor.fetchall()
                
        if not symbol_rows:
            conn.close()
            return f"Result of query_cpp_ast for {query}:\n" + json.dumps({
                "error": f"Symbol '{query}' not found in AST cache."
            }, indent=2)
            
        results = []
        for symbol in symbol_rows:
            sym_id = symbol["id"]
            sym_kind = symbol["kind"]
            
            symbol_data = {
                "name": symbol["name"],
                "fully_qualified_name": symbol["fully_qualified_name"],
                "kind": sym_kind,
                "file_path": symbol["file_path"],
                "line_start": symbol["line_start"],
                "line_end": symbol["line_end"],
                "access_specifier": symbol["access_specifier"]
            }
            
            if sym_kind in ('class', 'struct'):
                cursor.execute("""
                    SELECT parent_class_name, access_specifier 
                    FROM symbol_inheritance 
                    WHERE child_symbol_id = ?
                """, (sym_id,))
                inheritance_rows = cursor.fetchall()
                symbol_data["bases"] = [
                    {"name": r["parent_class_name"], "access": r["access_specifier"]}
                    for r in inheritance_rows
                ]
                
                cursor.execute("""
                    SELECT name, type, access_specifier, is_static, uproperty_metadata, line 
                    FROM properties 
                    WHERE class_symbol_id = ?
                """, (sym_id,))
                prop_rows = cursor.fetchall()
                symbol_data["properties"] = [
                    {
                        "name": p["name"],
                        "type": p["type"],
                        "access": p["access_specifier"],
                        "is_static": bool(p["is_static"]),
                        "uproperty": json.loads(p["uproperty_metadata"]) if p["uproperty_metadata"] else None,
                        "line": p["line"]
                    }
                    for p in prop_rows
                ]
                
                cursor.execute("""
                    SELECT id, name, return_type, is_static, is_virtual, is_const, is_override, access_specifier
                    FROM symbols 
                    WHERE parent_symbol_id = ? AND kind = 'method'
                """, (sym_id,))
                method_rows = cursor.fetchall()
                
                methods = []
                for m in method_rows:
                    m_id = m["id"]
                    cursor.execute("""
                        SELECT name, type, position 
                        FROM method_parameters 
                        WHERE symbol_id = ? 
                        ORDER BY position
                    """, (m_id,))
                    param_rows = cursor.fetchall()
                    
                    methods.append({
                        "name": m["name"],
                        "return_type": m["return_type"],
                        "access": m["access_specifier"],
                        "is_static": bool(m["is_static"]),
                        "is_virtual": bool(m["is_virtual"]),
                        "is_const": bool(m["is_const"]),
                        "is_override": bool(m["is_override"]),
                        "parameters": [{"name": p["name"], "type": p["type"]} for p in param_rows]
                    })
                symbol_data["methods"] = methods
                
            elif sym_kind == 'function':
                symbol_data["return_type"] = symbol["return_type"]
                cursor.execute("""
                    SELECT name, type, position 
                    FROM method_parameters 
                    WHERE symbol_id = ? 
                    ORDER BY position
                """, (sym_id,))
                param_rows = cursor.fetchall()
                symbol_data["parameters"] = [{"name": p["name"], "type": p["type"]} for p in param_rows]
                
            if sym_kind in ('method', 'function'):
                cursor.execute("""
                    WITH RECURSIVE transitive_calls(caller_id, callee_name, callee_id, depth) AS (
                        SELECT 
                            caller_symbol_id, 
                            callee_name, 
                            callee_symbol_id,
                            1 AS depth
                        FROM function_calls
                        WHERE caller_symbol_id = ?
                        
                        UNION
                        
                        SELECT 
                            fc.caller_symbol_id, 
                            fc.callee_name, 
                            fc.callee_symbol_id,
                            tc.depth + 1
                        FROM function_calls fc
                        JOIN transitive_calls tc ON fc.caller_symbol_id = tc.callee_id
                        WHERE tc.callee_id IS NOT NULL 
                          AND tc.depth < 8
                    )
                    SELECT DISTINCT callee_name, callee_id, MIN(depth) AS min_depth
                    FROM transitive_calls
                    GROUP BY callee_name, callee_id
                    ORDER BY min_depth;
                """, (sym_id,))
                call_rows = cursor.fetchall()
                symbol_data["transitive_callees"] = [
                    {"name": c["callee_name"], "id": c["callee_id"], "depth": c["min_depth"]}
                    for c in call_rows
                ]
                
            results.append(symbol_data)
            
        conn.close()
        return f"Result of query_cpp_ast for {query}:\n" + json.dumps(results, indent=2)
    except Exception as e:
        if conn:
            conn.close()
        logger.error(f"Error executing query_cpp_ast: {e}")
        return f"Result of query_cpp_ast for {query}:\n" + json.dumps({"error": str(e)}, indent=2)

@mcp.tool()
async def query_cpp_ast(query: str) -> str:
    """
    Query the cached C++ AST for class/symbol declarations, method signatures,
    properties, functions, and transitively called functions.
    """
    return await asyncio.to_thread(_query_cpp_ast_sync, query)

@mcp.tool()
async def generate_compile_commands() -> str:
    """
    Triggers the generation of the project's compile_commands.json using Unreal Build Tool.
    """
    ue_dir = find_unreal_engine_dir()
    if ue_dir:
        ubt_path = str(ue_dir / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool" / "UnrealBuildTool.exe")
    else:
        ubt_path = r"D:\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
        
    project_path = str(PROJECT_UPROJECT)
    output_dir = str(PROJECT_UPROJECT.parent)
    target_name = PROJECT_UPROJECT.stem + "Editor"
    
    cmd = [
        ubt_path,
        "-Mode=GenerateClangDatabase",
        f"-Project={project_path}",
        target_name,
        "Win64",
        "Development",
        f"-OutputDir={output_dir}"
    ]
    try:
        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        stdout, stderr = await process.communicate()
        stdout_str = stdout.decode('utf-8', errors='ignore')
        stderr_str = stderr.decode('utf-8', errors='ignore')
        status = "succeeded" if process.returncode == 0 else "failed"
        if status == "succeeded":
            logger.info("Compile commands generated successfully. Triggering background C++ AST re-indexing...")
            threading.Thread(target=background_initial_indexing, args=(True,), daemon=True).start()
        return f"Result of generate_compile_commands ({status}):\nSTDOUT:\n{stdout_str}\nSTDERR:\n{stderr_str}"
    except Exception as e:
        return f"Result of generate_compile_commands: failed with exception {str(e)}"

def check_and_reload_vector_db():
    try:
        from ExternalServer.src import vector_store
        ue_version = get_unreal_version()
        loaded_version = vector_store.get_loaded_version()
        if loaded_version is not None and loaded_version != ue_version:
            vector_db_dir = SERVER_DIR / f"vector_db_{ue_version}"
            logger.info(f"Detected Unreal Engine version change from {loaded_version} to {ue_version}. Re-initializing vector store at {vector_db_dir}")
            vector_store.initialize_db(str(vector_db_dir), ue_version)
    except Exception as e:
        logger.error(f"Error checking and reloading vector db: {e}")

@mcp.tool()
async def search_vector_db(query: str) -> str:
    """
    Performs semantic search queries against the local documentation vector database.
    """
    check_and_reload_vector_db()
    return await asyncio.to_thread(_search_vector_db_sync, query)

import urllib.request
import urllib.error

@mcp.tool()
async def search_similar_blueprints(query: str, n_results: int = 3) -> str:
    """
    Performs semantic search queries against the local blueprints vector database.
    Use this to find relevant Blueprint assets based on a description of their logic or purpose.
    """
    check_and_reload_vector_db()
    def _search():
        try:
            from ExternalServer.src import vector_store
            results = vector_store.search_similar_blueprints(query, n_results)
            if not results:
                return f"Result of search_similar_blueprints for '{query}': No matches found."
            return f"Result of search_similar_blueprints for '{query}':\n" + json.dumps(results, indent=2)
        except Exception as e:
            return f"Error: {e}"
    return await asyncio.to_thread(_search)

@mcp.tool()
async def format_t3d_layout(t3d_text: str) -> str:
    """
    Parse a T3D text block representing Blueprint nodes, calculate a clean,
    non-overlapping grid layout (placing execution flows left-to-right and 
    data dependencies stacked vertically to the left), and return the 
    formatted T3D text with updated NodePosX and NodePosY coordinates.
    Use this BEFORE calling inject_blueprint_nodes_t3d to beautify node layouts.
    """
    try:
        from ExternalServer.src.t3d_layout import format_layout
        return format_layout(t3d_text)
    except Exception as e:
        logger.error(f"Error in format_t3d_layout tool: {e}", exc_info=True)
        return t3d_text

# --- Blueprint Sync Helpers ---
def get_unreal_port_sync() -> int:
    env_port = os.environ.get("UNREAL_HTTP_PORT")
    if env_port:
        try:
            return int(env_port)
        except ValueError:
            pass
    return 18777

def check_ue_server_online_sync(port: int) -> bool:
    try:
        url = f"http://127.0.0.1:{port}/api/tools"
        req = urllib.request.Request(url, method="GET")
        with urllib.request.urlopen(req, timeout=2) as response:
            return response.status == 200
    except Exception:
        return False

def is_likely_blueprint(filepath: str) -> bool:
    path = Path(filepath)
    name = path.name.lower()
    if name.startswith("bp_") or name.startswith("wbp_") or name.startswith("abp_"):
        return True
    parts = [p.lower() for p in path.parts]
    if any(x in parts for x in ("blueprints", "ui", "widgets", "characters", "actors")):
        return True
    return False

def sync_single_blueprint_sync_worker(filepath: str, asset_path: str, mtime: float, port: int) -> bool:
    url = f"http://127.0.0.1:{port}/api/execute_tool"
    payload = json.dumps({"tool_name": "export_blueprint_summary", "parameters": {"asset_path": asset_path}}).encode('utf-8')
    req = urllib.request.Request(url, data=payload, headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=15) as response:
            resp_data = json.loads(response.read().decode('utf-8'))
    except Exception as e:
        logger.debug(f"Failed to fetch BSF for {asset_path} (UE server offline?): {e}")
        return False
        
    if not resp_data or not resp_data.get("bSuccess"):
        return False
        
    bsf_json_str = resp_data.get("ResultMessage", "{}")
    if not bsf_json_str:
        return False
        
    try:
        json_data = json.loads(bsf_json_str)
        from ExternalServer.src import vector_store
        return vector_store.upsert_blueprint(asset_path, json_data, mtime)
    except Exception as e:
        logger.error(f"Error parsing BSF JSON for {asset_path}: {e}")
        return False

def sync_single_blueprint_sync(filepath: str) -> bool:
    def worker():
        try:
            content_dir = PROJECT_UPROJECT.parent / "Content"
            rel_path = Path(filepath).relative_to(content_dir)
            asset_name = rel_path.with_suffix("").as_posix()
            asset_path = f"/Game/{asset_name}"
            mtime = os.path.getmtime(filepath)
            port = get_unreal_port_sync()
            sync_single_blueprint_sync_worker(filepath, asset_path, mtime, port)
        except Exception as e:
            logger.error(f"Error live-syncing blueprint {filepath}: {e}")

    threading.Thread(target=worker, daemon=True).start()
    return True

def delete_single_blueprint_sync(filepath: str) -> bool:
    def worker():
        try:
            content_dir = PROJECT_UPROJECT.parent / "Content"
            rel_path = Path(filepath).relative_to(content_dir)
            asset_name = rel_path.with_suffix("").as_posix()
            asset_path = f"/Game/{asset_name}"
            
            from ExternalServer.src import vector_store
            if vector_store._bp_collection:
                try:
                    vector_store._bp_collection.delete(where={"asset_path": asset_path})
                    logger.info(f"Successfully deleted {asset_path} from vector store.")
                except Exception as e:
                    logger.error(f"ChromaDB delete failed for {asset_path}: {e}")
        except Exception as e:
            logger.error(f"Error deleting blueprint {filepath}: {e}")

    threading.Thread(target=worker, daemon=True).start()
    return True

class UAssetHandler(FileSystemEventHandler):
    def on_modified(self, event):
        if event.is_directory:
            return
        if event.src_path.endswith('.uasset'):
            path_parts = Path(event.src_path).parts
            if any(x in path_parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                return
            if is_likely_blueprint(event.src_path):
                logger.info(f"Watchdog: Blueprint modified: {event.src_path}")
                sync_single_blueprint_sync(event.src_path)

    def on_created(self, event):
        if event.is_directory:
            return
        if event.src_path.endswith('.uasset'):
            path_parts = Path(event.src_path).parts
            if any(x in path_parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                return
            if is_likely_blueprint(event.src_path):
                logger.info(f"Watchdog: Blueprint created: {event.src_path}")
                sync_single_blueprint_sync(event.src_path)

    def on_deleted(self, event):
        if event.is_directory:
            return
        if event.src_path.endswith('.uasset'):
            path_parts = Path(event.src_path).parts
            if any(x in path_parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                return
            if is_likely_blueprint(event.src_path):
                logger.info(f"Watchdog: Blueprint deleted: {event.src_path}")
                delete_single_blueprint_sync(event.src_path)

def sync_blueprints_on_startup_sync() -> str:
    logger.info("Starting Blueprint Vector DB startup differential sync...")
    try:
        from ExternalServer.src import vector_store
        indexed = vector_store.get_indexed_blueprints()
        content_dir = PROJECT_UPROJECT.parent / "Content"
        if not content_dir.exists():
            logger.warning(f"Content directory does not exist: {content_dir}. Skipping sync.")
            return "Content directory not found."
            
        uasset_files = []
        for root, dirs, files in os.walk(content_dir):
            if any(x in Path(root).parts for x in ('Intermediate', 'Binaries', 'Saved', '.git', '.agents')):
                continue
            for f in files:
                if f.endswith(".uasset") and is_likely_blueprint(os.path.join(root, f)):
                    uasset_files.append(os.path.join(root, f))
                    
        logger.info(f"Discovered {len(uasset_files)} likely Blueprint .uasset files.")
        
        to_sync = []
        discovered_paths = set()
        
        for filepath in uasset_files:
            try:
                rel_path = Path(filepath).relative_to(content_dir)
                asset_name = rel_path.with_suffix("").as_posix()
                asset_path = f"/Game/{asset_name}"
                discovered_paths.add(asset_path)
                
                mtime = os.path.getmtime(filepath)
                
                if asset_path not in indexed:
                    to_sync.append((filepath, asset_path, mtime))
                else:
                    db_mtime = indexed[asset_path]
                    if mtime - db_mtime > 1.0:
                        to_sync.append((filepath, asset_path, mtime))
            except Exception as e:
                logger.error(f"Error checking file {filepath}: {e}")
                
        to_delete = []
        for asset_path in indexed:
            if asset_path not in discovered_paths:
                to_delete.append(asset_path)
                
        if to_delete:
            logger.info(f"Purging {len(to_delete)} deleted blueprints from vector DB...")
            if vector_store._bp_collection:
                for ap in to_delete:
                    try:
                        vector_store._bp_collection.delete(where={"asset_path": ap})
                    except Exception as e:
                        logger.error(f"Failed to delete {ap}: {e}")
            
        if to_sync:
            logger.info(f"Found {len(to_sync)} new/modified blueprints. Indexing...")
            port = get_unreal_port_sync()
            
            if not check_ue_server_online_sync(port):
                msg = "Unreal Engine HTTP server is offline. Startup sync deferred."
                logger.warning(msg)
                return msg
                
            success_count = 0
            for filepath, asset_path, mtime in to_sync:
                if sync_single_blueprint_sync_worker(filepath, asset_path, mtime, port):
                    success_count += 1
                time.sleep(0.05)
                
            msg = f"Completed. Indexed {success_count}/{len(to_sync)} blueprints."
            logger.info(msg)
            return msg
        else:
            msg = "All blueprints are up to date."
            logger.info(msg)
            return msg
    except Exception as e:
        logger.error(f"Error during startup sync: {e}")
        return f"Error: {e}"

@mcp.tool()
async def index_all_blueprints() -> str:
    """
    Extracts Blueprint Summary Format (BSF) from all Blueprints in the Unreal Engine project and indexes them into the vector database.
    Requires the Unreal Engine editor to be running with the Antigravity plugin.
    """
    check_and_reload_vector_db()
    return await asyncio.to_thread(sync_blueprints_on_startup_sync)

# 6. HybridStdin and Stdin Interceptor
# 6. Search Engine Class definition and Initialization
def get_project_uproject_version() -> Optional[str]:
    """
    Parses the discovered *.uproject file to read the 'EngineAssociation' value.
    """
    try:
        if PROJECT_UPROJECT and PROJECT_UPROJECT.exists():
            with open(PROJECT_UPROJECT, 'r', encoding='utf-8') as f:
                data = json.load(f)
                engine_association = data.get("EngineAssociation")
                if engine_association:
                    # Clean it: e.g. "5.8" or a path/GUID. We want to extract the major.minor version
                    match = re.search(r"(\d+\.\d+)", str(engine_association))
                    if match:
                        return match.group(1)
    except Exception as e:
        logger.error(f"Error parsing .uproject file {PROJECT_UPROJECT}: {e}")
    return None

def get_unreal_version() -> str:
    """
    Detects the active Unreal Engine version.
    """
    # 1. Try reading the .uproject file EngineAssociation
    uproject_ver = get_project_uproject_version()
    if uproject_ver:
        return uproject_ver

    # 2. Fall back to registry-based active engine detection
    ue_dir = find_unreal_engine_dir()
    if ue_dir:
        name = ue_dir.name
        match = re.search(r"(\d+\.\d+)", name)
        if match:
            return match.group(1)
            
    for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
        subkey = r"SOFTWARE\EpicGames\Unreal Engine"
        try:
            with winreg.OpenKey(hive, subkey) as key:
                i = 0
                while True:
                    ver_name = winreg.EnumKey(key, i)
                    if re.match(r"^\d+\.\d+$", ver_name):
                        return ver_name
                    i += 1
        except WindowsError:
            pass
            
    return "5.8"

def _search_vector_db_sync(query: str) -> str:
    try:
        from ExternalServer.src import vector_store
        results = vector_store.semantic_search(query, n_results=5)
        if not results:
            results = [{
                "title": "Search Database Status",
                "content": f"No active documents matched the query '{query}' in the vector database.",
                "similarity_score": 0.0,
                "source": "None"
            }]
        return f"Result of search_vector_db for '{query}':\n" + json.dumps(results, indent=2)
    except Exception as e:
        logger.error(f"Error in search_vector_db: {e}")
        return f"Result of search_vector_db for '{query}':\n" + json.dumps([{
            "title": "Error occurred",
            "content": str(e),
            "similarity_score": 0.0,
            "source": "SystemError"
        }], indent=2)

async def execute_manual_tool(name: str, arguments: dict) -> str:
    if name == "query_cpp_ast":
        return await query_cpp_ast(**arguments)
    elif name == "generate_compile_commands":
        return await generate_compile_commands()
    elif name == "search_vector_db":
        return await search_vector_db(**arguments)
    elif name == "search_similar_blueprints":
        return await search_similar_blueprints(**arguments)
    elif name == "index_all_blueprints":
        return await index_all_blueprints()
    else:
        raise ValueError(f"Tool {name} not found.")

observer = None
active_executor = None

def cleanup_watcher():
    global observer, active_executor
    if active_executor:
        logger.info("Shutting down active ProcessPoolExecutor...")
        try:
            active_executor.shutdown(wait=False, cancel_futures=True)
        except Exception as e:
            logger.error(f"Error shutting down active executor: {e}")
        active_executor = None
    if observer:
        logger.info("Stopping watchdog observer...")
        try:
            observer.stop()
            observer.join()
        except Exception as e:
            logger.error(f"Error stopping observer: {e}")
        observer = None
    logger.info("Cleanup completed.")

def main():
    global observer
    init_db()
    
    # Initialize version-specific vector database
    ue_version = get_unreal_version()
    vector_db_dir = SERVER_DIR / f"vector_db_{ue_version}"
    logger.info(f"Initializing vector database for Unreal Engine {ue_version} at {vector_db_dir}")
    
    try:
        from ExternalServer.src import vector_store
        vector_store.initialize_db(str(vector_db_dir), ue_version)
    except Exception as e:
        logger.error(f"Failed to initialize vector store: {e}")
        
    threading.Thread(target=background_initial_indexing, daemon=True).start()
    
    observer = start_watcher()
    
    # Start Blueprint vector DB differential sync in the background
    threading.Thread(target=sync_blueprints_on_startup_sync, daemon=True).start()
    
    # Register exit and signal hooks
    atexit.register(cleanup_watcher)
    
    def signal_handler(signum, frame):
        logger.info(f"Received signal {signum}. Shutting down...")
        cleanup_watcher()
        sys.exit(0)
        
    try:
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
    except ValueError as e:
        logger.warning(f"Could not register signal handlers: {e}")
        
    try:
        mcp.run()
    finally:
        cleanup_watcher()

if __name__ == "__main__":
    main()
