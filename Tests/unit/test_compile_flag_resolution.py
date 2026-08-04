"""Regression tests for UBT response-file expansion and MSVC -> Clang flag conversion.

Covers the PCH forced-include defect: UBT emits /Yu"SharedPCH.<Module>.h" alongside
/FI"SharedPCH.<Module>.h". MSVC satisfies the /FI from the prebuilt .pch, but libclang has no
.pch and parses the header textually, which aborts the whole translation unit. Dropping that
forced include (while keeping Definitions.*.h, which carries the <MODULE>_API defines) is what
lets libclang report `UMyThing` instead of the expansion of the export macro.
"""
import os
import sys
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

import UnrealEngine.ExternalServer.src.main as main


BASE_DIR = "C:/Engine/Source"


class TestIsPchHeader:
    @pytest.mark.parametrize("name", [
        "SharedPCH.UnrealEd.Project.ValApi.ValExpApi.Cpp20.h",
        "SharedPCH.Core.Cpp20.h",
        "sharedpch.engine.cpp20.h",
        "PCH.MyModule.h",
    ])
    def test_detects_pch_headers(self, name):
        assert main.is_pch_header(f"C:/Intermediate/{name}") is True

    @pytest.mark.parametrize("name", [
        "Definitions.Tau.h",
        "Definitions.h",
        "CoreMinimal.h",
        "MyActor.h",
    ])
    def test_ignores_regular_headers(self, name):
        assert main.is_pch_header(f"C:/Intermediate/{name}") is False

    def test_strips_surrounding_quotes(self):
        assert main.is_pch_header('"C:/Intermediate/SharedPCH.Core.Cpp20.h"') is True


class TestConvertMsvcToClang:
    def test_drops_pch_forced_include_keeps_definitions(self):
        args = [
            '/FI"C:/Intermediate/SharedPCH.UnrealEd.Cpp20.h"',
            '/FI"C:/Intermediate/Definitions.Tau.h"',
            '/Yu"C:/Intermediate/SharedPCH.UnrealEd.Cpp20.h"',
        ]
        out = main.convert_msvc_to_clang(args, BASE_DIR)
        forced = [out[i + 1] for i, a in enumerate(out) if a == "-include"]
        assert len(forced) == 1
        assert forced[0].endswith("Definitions.Tau.h")
        assert not any("SharedPCH" in a for a in out)

    def test_drops_yu_target_even_when_not_pch_named(self):
        """A /Yu target is consumed as a PCH regardless of its filename."""
        args = [
            '/FI"C:/Intermediate/MyProjectPrefix.h"',
            '/Yu"C:/Intermediate/MyProjectPrefix.h"',
        ]
        out = main.convert_msvc_to_clang(args, BASE_DIR)
        assert "-include" not in out

    def test_keeps_forced_include_without_matching_yu(self):
        out = main.convert_msvc_to_clang(['/FI"C:/Intermediate/Definitions.h"'], BASE_DIR)
        assert out.count("-include") == 1
        assert out[out.index("-include") + 1].endswith("Definitions.h")

    def test_separated_fi_form_is_also_filtered(self):
        """UBT can emit '/FI' and the path as two tokens."""
        args = ["/FI", "C:/Intermediate/SharedPCH.Core.Cpp20.h",
                "/FI", "C:/Intermediate/Definitions.h"]
        out = main.convert_msvc_to_clang(args, BASE_DIR)
        forced = [out[i + 1] for i, a in enumerate(out) if a == "-include"]
        assert len(forced) == 1
        assert forced[0].endswith("Definitions.h")

    def test_converts_includes_and_defines(self):
        args = ['/I"C:/Engine/Runtime/Core/Public"', '/DTAU_API=DLLEXPORT', "/std:c++20"]
        out = main.convert_msvc_to_clang(args, BASE_DIR)
        assert "-IC:/Engine/Runtime/Core/Public" in out
        assert "-DTAU_API=DLLEXPORT" in out
        assert "-std=c++20" in out

    def test_relative_paths_resolved_against_base_dir(self):
        out = main.convert_msvc_to_clang(["/IRuntime/Core/Public"], BASE_DIR)
        expected = os.path.normpath(os.path.join(BASE_DIR, "Runtime/Core/Public"))
        assert out == ["-I" + expected]

    def test_pch_filter_does_not_consume_following_flag(self):
        """Dropping a /FI must still advance past its path argument."""
        args = ["/FI", "C:/Intermediate/SharedPCH.Core.Cpp20.h", "/DFOO=1"]
        out = main.convert_msvc_to_clang(args, BASE_DIR)
        assert out == ["-DFOO=1"]


class TestParseRspFile:
    def test_expands_nested_response_files(self, tmp_path):
        shared = tmp_path / "Tau.Shared.rsp"
        shared.write_text('/I"C:/Engine/Runtime/Core/Public"\n/DWITH_EDITOR=1\n', encoding="utf-8")
        main_rsp = tmp_path / "MyFile.cpp.obj.rsp"
        main_rsp.write_text(f'@"{shared.as_posix()}"\n/std:c++20\n', encoding="utf-8")

        args = main.parse_rsp_file(str(main_rsp), BASE_DIR)
        # Quotes are consumed by the lexer, so the include arrives as a single joined token.
        assert "/IC:/Engine/Runtime/Core/Public" in args
        assert "/DWITH_EDITOR=1" in args
        assert "/std:c++20" in args
        # The @-reference itself must not survive expansion.
        assert not any(a.startswith("@") for a in args)

    def test_missing_file_returns_empty(self, tmp_path):
        assert main.parse_rsp_file(str(tmp_path / "nope.rsp"), BASE_DIR) == []

    def test_paths_with_spaces_survive_tokenisation(self, tmp_path):
        rsp = tmp_path / "spaced.rsp"
        rsp.write_text('/FI"C:/Users/me/Unreal Projects/proj/Definitions.h"\n', encoding="utf-8")
        args = main.parse_rsp_file(str(rsp), BASE_DIR)
        assert any("Unreal Projects" in a for a in args)
        out = main.convert_msvc_to_clang(args, BASE_DIR)
        forced = [out[i + 1] for i, a in enumerate(out) if a == "-include"]
        assert len(forced) == 1 and "Unreal Projects" in forced[0]


class TestEndToEndRspToClangArgs:
    def test_ubt_style_rsp_yields_usable_clang_args(self, tmp_path):
        """A realistic UBT rsp pair must produce -D/-I/-include but never a PCH forced include."""
        shared = tmp_path / "Tau.Shared.rsp"
        shared.write_text(
            '/I"C:/Engine/Runtime/Core/Public"\n'
            '/I"C:/Engine/Runtime/CoreUObject/Public"\n'
            '/DUE_BUILD_DEVELOPMENT=1\n',
            encoding="utf-8")
        defs = tmp_path / "Definitions.Tau.h"
        defs.write_text("#define TAU_API DLLEXPORT\n", encoding="utf-8")
        pch = tmp_path / "SharedPCH.UnrealEd.Cpp20.h"
        pch.write_text("// huge\n", encoding="utf-8")
        obj_rsp = tmp_path / "TauThing.cpp.obj.rsp"
        obj_rsp.write_text(
            f'@"{shared.as_posix()}"\n'
            f'/FI"{pch.as_posix()}"\n'
            f'/FI"{defs.as_posix()}"\n'
            f'/Yu"{pch.as_posix()}"\n'
            "/nologo\n/TP\n/GR-\n/std:c++20\n",
            encoding="utf-8")

        out = main.convert_msvc_to_clang(main.parse_rsp_file(str(obj_rsp), BASE_DIR), BASE_DIR)

        assert sum(1 for a in out if a.startswith("-I")) == 2
        assert "-DUE_BUILD_DEVELOPMENT=1" in out
        assert "-std=c++20" in out
        forced = [out[i + 1] for i, a in enumerate(out) if a == "-include"]
        assert len(forced) == 1 and forced[0].endswith("Definitions.Tau.h")
        assert not any("SharedPCH" in a for a in out)
