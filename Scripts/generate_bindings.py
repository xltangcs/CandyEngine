#!/usr/bin/env python3
"""
CandyEngine Python Binding Code Generator

Scans C++ headers for inline CANDY_CLASS / CANDY_PROPERTY / CANDY_ENUM
annotation macros and generates:
  - ScriptBindings.generated.inl  -- pybind11 binding code (included by PythonBindings.cpp)
                                    -> stays in the engine (Candy/Source/Runtime/Scripting/)
  - stubs/candy/__init__.pyi    -- Python type stubs for IDE autocompletion
                                    -> package-style stub under stubPath (stubs/)
  - .vscode/settings.json        -- Pyright/Pylance stub path config (repo root)

The .inl must live in the engine because it is #included by PythonBindings.cpp.
The .pyi lives under ``stubs/candy/`` as a package-style stub so that Pylance's
``python.analysis.stubPath`` (pointing at ``stubs/``) reliably resolves
``import candy`` for autocompletion.

Usage:
  python generate_bindings.py [--source <dir>] [--output <dir>] [-p <stub_dir>]

  -p, --pyi-dir <dir>   Output directory for the candy stub package
                        (default: <root>/stubs/candy)
"""

import re
import os
import sys
import json
from pathlib import Path
from collections import OrderedDict


# ============================================================================
# Configuration
# ============================================================================

# Script directory (Scripts/)
SCRIPT_DIR = Path(__file__).resolve().parent
# Project root
PROJECT_ROOT = SCRIPT_DIR.parent
# Default source directory to scan
DEFAULT_SOURCE_DIR = PROJECT_ROOT / "Candy" / "Source"
# Default output for .inl file
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "Candy" / "Source" / "Runtime" / "Scripting"
# Default output for .pyi file (package-style stub under stubPath)
DEFAULT_PYI_DIR = PROJECT_ROOT / "stubs" / "candy"

# C++ type -> Python type mapping
TYPE_MAP = {
    "float":       "float",
    "double":      "float",
    "int":         "int",
    "int32_t":     "int",
    "uint32_t":    "int",
    "bool":        "bool",
    "std::string": "str",
    "glm::vec2":   "Vec2",
    "glm::vec3":   "Vec3",
    "glm::vec4":   "Vec4",
    "glm::ivec2":  "IVec2",
    "glm::ivec3":  "IVec3",
    "glm::ivec4":  "IVec4",
}

# C++ type -> pybind11 type specifier for .def_readwrite
PYBIND11_TYPE_HINTS = {
    "std::string": "py::return_value_policy::copy",
}

# Base types that are always generated (third-party / external)
BASE_TYPES = OrderedDict([
    ("Vec2", {
        "cpp_type": "glm::vec2",
        "members": [
            ("float", "x"),
            ("float", "y"),
        ],
        "init_args": "float, float",
        "extra": ['        .def("__repr__", [](const glm::vec2& v) {',
                  '            return "Vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";',
                  '        })'],
    }),
    ("Vec3", {
        "cpp_type": "glm::vec3",
        "members": [
            ("float", "x"),
            ("float", "y"),
            ("float", "z"),
        ],
        "init_args": "float, float, float",
        "extra": ['        .def("__repr__", [](const glm::vec3& v) {',
                  '            return "Vec3(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";',
                  '        })'],
    }),
    ("Vec4", {
        "cpp_type": "glm::vec4",
        "members": [
            ("float", "r"),
            ("float", "g"),
            ("float", "b"),
            ("float", "a"),
        ],
        "init_args": "float, float, float, float",
        "extra": ['        .def("__repr__", [](const glm::vec4& v) {',
                  '            return "Vec4(" + std::to_string(v.r) + ", " + std::to_string(v.g) + ", " + std::to_string(v.b) + ", " + std::to_string(v.a) + ")";',
                  '        })'],
    }),
])


# ============================================================================
# Parsing
# ============================================================================

class StructInfo:
    """Represents a parsed C++ struct/class with its annotated members."""
    def __init__(self, name: str, cpp_type: str):
        self.name = name
        self.cpp_type = cpp_type  # e.g., "Candy::TransformComponent"
        self.members: list[tuple[str, str]] = []  # [(cpp_type, member_name), ...]

class EnumInfo:
    """Represents a parsed C++ enum."""
    def __init__(self, name: str, cpp_type: str, values: list[str] | None = None):
        self.name = name          # e.g., "BodyType"
        self.cpp_type = cpp_type  # e.g., "Candy::Rigidbody2DComponent::BodyType"
        self.values: list[str] = values if values is not None else []  # e.g., ["Static", "Dynamic", "Kinematic"]


# Types that cannot be safely bound via py::class_::def_readwrite (non-copyable,
# pointer, or container types). When a CANDY_PROPERTY sits above such a member we
# warn and skip it rather than generating a broken binding.
UNBINDABLE_TYPES = {
    "void*", "entt::entity", "SceneCamera", "ScriptableEntity",
    "ScriptableEntity*", "UUID",
}
UNBINDABLE_TYPE_PREFIXES = (
    "Ref<", "Scope<", "std::vector", "std::unordered_map", "std::map",
    "std::set", "std::shared_ptr", "std::unique_ptr", "std::function",
)

# Matches a named type declaration: struct Foo, class Foo, enum class Foo, enum Foo
RE_TYPE_DECL = re.compile(r'\b(struct|class|enum(?:\s+class|\s+struct)?)\s+(\w+)')
RE_CANDY_CLASS = re.compile(r'CANDY_CLASS\s*\(')
RE_CANDY_PROPERTY = re.compile(r'CANDY_PROPERTY\s*\(')
RE_CANDY_ENUM = re.compile(r'CANDY_ENUM\s*\(')


def _next_meaningful_index(lines: list[str], start: int) -> int | None:
    """Index of the next non-blank, non-`//` line, or None if end of file."""
    k = start
    while k < len(lines):
        s = lines[k].strip()
        if s == "" or s.startswith("//"):
            k += 1
            continue
        return k
    return None


def _parse_member_decl(line: str):
    """
    Parse a C++ member declaration line into (cpp_type, name).
    Returns None if the line is not a bindable member (constructor, method,
    comment, unbindable type, ...).
    """
    s = line.strip()
    s = re.sub(r'//.*', '', s)              # strip trailing comments
    if ';' in s:
        s = s[:s.index(';')]
    s = s.strip()
    s = re.sub(r'\{[^{}]*\}', '', s)        # drop brace-initializer lists
    s = s.strip()
    eq = s.find('=')                         # drop default value
    if eq != -1:
        s = s[:eq].strip()
    if not s:
        return None

    parts = s.split()
    if len(parts) < 2:
        return None
    name = parts[-1]
    type_str = ' '.join(parts[:-1])

    # Skip function-like declarations (constructors, methods, function pointers)
    if '(' in name or '(' in type_str:
        return None
    if name in {'if', 'for', 'while', 'return', 'switch', 'do', 'else',
                'struct', 'class', 'enum', 'using', 'typedef'}:
        return None
    if type_str in UNBINDABLE_TYPES or type_str.startswith(UNBINDABLE_TYPE_PREFIXES):
        print(f"  Warning: skipping CANDY_PROPERTY '{name}' - type '{type_str}' "
              f"cannot be bound via def_readwrite")
        return None

    return (type_str, name)


def _parse_enum_decl(line: str, parent: str | None):
    """
    Parse an `enum class Name { ... }` line, auto-detecting its values.
    `parent` is the enclosing struct/class name (for nested enums).
    """
    s = line.strip()
    m = RE_TYPE_DECL.search(s)
    if not m or m.group(1) not in ('enum', 'enum class', 'enum struct'):
        return None
    name = m.group(2)

    brace_start = s.find('{')
    brace_end = s.rfind('}')
    body = s[brace_start + 1:brace_end] if brace_start != -1 and brace_end != -1 else ""
    body = re.sub(r'//.*', '', body)
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.DOTALL)

    values: list[str] = []
    for part in body.split(','):
        part = part.strip()
        if not part:
            continue
        v = part.split('=')[0].strip().split()[0]
        if v and v not in values:
            values.append(v)

    cpp_type = f"{parent}::{name}" if parent else name
    return EnumInfo(name, cpp_type, values)


def parse_header(filepath: Path) -> tuple[list[StructInfo], list[EnumInfo]]:
    """
    Parse a single header for inline CANDY_CLASS / CANDY_PROPERTY / CANDY_ENUM
    annotation macros. The macros expand to nothing at compile time; they are
    read here as metadata.

    Usage in C++:

        CANDY_CLASS()
        struct TransformComponent
        {
            CANDY_PROPERTY()
            glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
            ...
        };

        struct Rigidbody2DComponent
        {
            CANDY_ENUM()
            enum class BodyType { Static = 0, Dynamic, Kinematic };
            ...
        };

    Note: the macros are real (no-op) C++ and must NOT appear inside `//`
    comments - comment lines are ignored by the parser. Brace-initialized
    members (``glm::vec4 Color{ 1.0f, ... };``) are handled correctly.
    """
    structs: list[StructInfo] = []
    enums: list[EnumInfo] = []
    try:
        content = filepath.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        try:
            content = filepath.read_text(encoding="gbk")
        except Exception:
            return structs, enums

    lines = content.splitlines(keepends=True)

    scope: list[str | None] = []   # one entry per open brace: type name or None
    pending_type: str | None = None  # type name to attach to the next '{'
    collecting: StructInfo | None = None
    collect_scope_len = 0
    await_struct = False
    collect_name: str | None = None   # struct name awaiting its opening '{'

    def enclosing_type() -> str | None:
        for n in reversed(scope):
            if n:
                return n
        return None

    i = 0
    while i < len(lines):
        raw = lines[i]
        code = raw.split('//', 1)[0]   # strip line comment
        code_s = code.strip()

        opens = code.count('{')
        closes = code.count('}')

        # detect a type declaration on this line
        type_decl = None
        m = RE_TYPE_DECL.search(code_s)
        if m and m.group(1) in ('struct', 'class', 'enum', 'enum class', 'enum struct'):
            type_decl = m.group(2)

        # --- annotation macros (real code only) -----------------------------
        if RE_CANDY_CLASS.search(code_s):
            await_struct = True
        elif collecting is not None and RE_CANDY_PROPERTY.search(code_s):
            j = _next_meaningful_index(lines, i + 1)
            if j is not None:
                mem = _parse_member_decl(lines[j])
                if mem is not None:
                    collecting.members.append(mem)
        elif RE_CANDY_ENUM.search(code_s):
            parent = enclosing_type()
            j = _next_meaningful_index(lines, i + 1)
            if j is not None:
                e = _parse_enum_decl(lines[j], parent)
                if e is not None:
                    enums.append(e)

        # attach the detected type name to the next opened brace
        if type_decl is not None:
            pending_type = type_decl
            if await_struct and m.group(1) in ('struct', 'class'):
                collect_name = type_decl
                await_struct = False

        # process open braces; start collecting when the target struct's brace
        # is pushed (handles '{' on the same line or the next line)
        for _ in range(opens):
            pushed = pending_type
            scope.append(pushed)
            pending_type = None
            if collect_name is not None and collect_name == pushed and collecting is None:
                collecting = StructInfo(collect_name, collect_name)
                collect_scope_len = len(scope)
                collect_name = None

        for _ in range(closes):
            if scope:
                scope.pop()

        # stop collecting once the struct's own brace is closed
        if collecting is not None and len(scope) < collect_scope_len:
            structs.append(collecting)
            collecting = None

        i += 1

    if collecting is not None:
        structs.append(collecting)

    return structs, enums


def scan_all_headers(source_dir: Path) -> tuple[list[StructInfo], list[EnumInfo]]:
    """Scan all .h files in source_dir recursively."""
    structs: list[StructInfo] = []
    enums: list[EnumInfo] = []

    headers = list(source_dir.rglob("*.h"))
    print(f"Scanning {len(headers)} header files in {source_dir}...")

    for h in headers:
        # Skip third-party / platform headers
        rel = str(h.relative_to(source_dir))
        if any(rel.startswith(p) for p in ["ThirdParty", "Platform", "CandyPCH"]):
            continue

        s, e = parse_header(h)
        if s:
            print(f"  {rel}: {len(s)} class(es) found")
            structs.extend(s)
        if e:
            print(f"  {rel}: {len(e)} enum(s) found")
            enums.extend(e)

    return structs, enums


# ============================================================================
# Code Generation
# ============================================================================

def cpp_to_py_type(cpp_type: str) -> str:
    """Map C++ type to Python type name for .pyi stubs."""
    cpp_type = cpp_type.strip()
    # Remove namespace prefixes for Candy types
    if cpp_type.startswith("Candy::"):
        return cpp_type[len("Candy::"):]
    # Check type map
    if cpp_type in TYPE_MAP:
        return TYPE_MAP[cpp_type]
    # Remove common namespace prefixes
    for ns in ["glm::", "std::", "Candy::"]:
        if cpp_type.startswith(ns):
            return cpp_type[len(ns):]
    return cpp_type


def generate_inl(structs: list[StructInfo], enums: list[EnumInfo], output_path: Path):
    """Generate ScriptBindings.generated.inl"""
    lines = []
    lines.append("// Auto-generated by generate_bindings.py - DO NOT EDIT")
    lines.append("// Generated: " + __import__('datetime').datetime.now().isoformat())
    lines.append("")
    lines.append("// ============================================================================")
    lines.append("// Base Types (glm)")
    lines.append("// ============================================================================")
    lines.append("")

    # Generate base types (glm::vec2/3/4)
    for py_name, info in BASE_TYPES.items():
        cpp_type = info["cpp_type"]
        lines.append(f"// {py_name}")
        lines.append(f"py::class_<{cpp_type}>(m, \"{py_name}\")")
        lines.append(f"    .def(py::init<>())")
        if info.get("init_args"):
            lines.append(f"    .def(py::init<{info.get('init_args', '')}>())")
        for member_type, member_name in info["members"]:
            lines.append(f"    .def_readwrite(\"{member_name}\", &{cpp_type}::{member_name})")
        if info.get("extra"):
            for extra_line in info["extra"]:
                lines.append(extra_line)
        lines.append("    ;")
        lines.append("")

    # Generate structs
    if structs:
        lines.append("// ============================================================================")
        lines.append("// Component Structs")
        lines.append("// ============================================================================")
        lines.append("")

    for s in structs:
        cpp_type = s.cpp_type
        if not cpp_type.startswith("Candy::"):
            cpp_type = f"Candy::{cpp_type}"
        py_name = s.name

        lines.append(f"// {py_name}")
        lines.append(f"py::class_<{cpp_type}>(m, \"{py_name}\")")
        lines.append(f"    .def(py::init<>())")

        for member_type, member_name in s.members:
            lines.append(f"    .def_readwrite(\"{member_name}\", &{cpp_type}::{member_name})")

        lines.append("    ;")
        lines.append("")

    # Generate enums
    if enums:
        lines.append("// ============================================================================")
        lines.append("// Enums")
        lines.append("// ============================================================================")
        lines.append("")

    for e in enums:
        cpp_type = e.cpp_type
        if not cpp_type.startswith("Candy::"):
            cpp_type = f"Candy::{cpp_type}"
        py_name = e.name

        lines.append(f"// {py_name}")
        lines.append(f"py::enum_<{cpp_type}>(m, \"{py_name}\")")
        for val in e.values:
            lines.append(f"    .value(\"{val}\", {cpp_type}::{val})")
        lines.append("    ;")
        lines.append("")

    # Write file (write_text with default newline='' translates \n to os.linesep on Windows)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    text = "\n".join(lines)
    output_path.write_text(text, encoding="utf-8")
    print(f"Generated: {output_path}")


def generate_pyi(structs: list[StructInfo], enums: list[EnumInfo], output_path: Path):
    """Generate candy.pyi Python type stub."""
    lines = []
    lines.append("# Auto-generated by generate_bindings.py - DO NOT EDIT")
    lines.append(f"# Generated: {__import__('datetime').datetime.now().isoformat()}")
    lines.append("")
    lines.append("from __future__ import annotations")
    lines.append("from typing import Any, overload")
    lines.append("")

    # Generate base types
    for py_name, info in BASE_TYPES.items():
        init_args = info.get("init_args", "")
        arg_list = [a.strip() for a in init_args.split(",") if a.strip()] if init_args else []

        lines.append(f"class {py_name}:")
        for member_type, member_name in info["members"]:
            py_type = cpp_to_py_type(member_type)
            lines.append(f"\t{member_name}: {py_type}")
        lines.append(f"\t@overload")
        lines.append(f"\tdef __init__(self) -> None: ...")
        if arg_list:
            py_args = ", ".join(f"{a.split()[-1]}: {cpp_to_py_type(a.split()[0])}" for a in arg_list)
            lines.append(f"\t@overload")
            lines.append(f"\tdef __init__(self, {py_args}) -> None: ...")
        lines.append("")

    # Generate structs
    for s in structs:
        lines.append(f"class {s.name}:")
        for member_type, member_name in s.members:
            py_type = cpp_to_py_type(member_type)
            lines.append(f"\t{member_name}: {py_type}")
        lines.append(f"\tdef __init__(self) -> None: ...")
        lines.append("")

    # Generate enums
    for e in enums:
        lines.append(f"class {e.name}:")
        for val in e.values:
            lines.append(f"\t{val}: {e.name}  # type: ignore")
        lines.append("")

    # Also add manual stubs for types defined in PythonBindings.cpp
    lines.append("# ============================================================================")
    lines.append("# Manual stubs (hand-written bindings in PythonBindings.cpp)")
    lines.append("# ============================================================================")
    lines.append("")
    lines.append("class Rigidbody2DComponent:")
    lines.append("\tType: BodyType")
    lines.append("\tFixedRotation: bool")
    lines.append("\tdef __init__(self) -> None: ...")
    lines.append("\tdef get_linear_velocity(self) -> Vec2: ...")
    lines.append("\tdef set_linear_velocity(self, vx: float, vy: float) -> None: ...")
    lines.append("\tdef apply_linear_impulse(self, ix: float, iy: float) -> None: ...")
    lines.append("")
    lines.append("class UITextBlockComponent:")
    lines.append("\tdef __init__(self) -> None: ...")
    lines.append("\tdef set_text_visible(self, key: str, visible: bool) -> None: ...")
    lines.append("\tdef set_text(self, key: str, text: str) -> None: ...")
    lines.append("")
    lines.append("class UIButtonComponent:")
    lines.append("\tdef __init__(self) -> None: ...")
    lines.append("\tdef set_button_visible(self, key: str, visible: bool) -> None: ...")
    lines.append("\tdef get_button_visible(self, key: str) -> bool: ...")
    lines.append("\tdef set_button_text(self, key: str, text: str) -> None: ...")
    lines.append("\tdef set_button_onclick(self, key: str, onclick: str) -> None: ...")
    lines.append("")
    lines.append("class Entity:")
    lines.append("\tdef get_component(self, type: str) -> Any: ...")
    lines.append("\tdef has_component(self, type: str) -> bool: ...")
    lines.append("\tdef add_component(self, type: str) -> None: ...")
    lines.append("\tdef queue_free(self) -> None: ...")
    lines.append("\tdef is_queued_for_deletion(self) -> bool: ...")
    lines.append("\tdef call_function(self, func_name: str) -> None: ...")
    lines.append("\t@property")
    lines.append("\tdef tag(self) -> str: ...")
    lines.append("\t@tag.setter")
    lines.append("\tdef tag(self, value: str) -> None: ...")
    lines.append("\t@property")
    lines.append("\tdef scene(self) -> Scene: ...")
    lines.append("")
    lines.append("class ScriptObject:")
    lines.append("\tdef __init__(self) -> None: ...")
    lines.append("\tdef on_construct(self) -> None: ...")
    lines.append("\tdef on_start(self) -> None: ...")
    lines.append("\tdef on_tick(self, ts: float) -> None: ...")
    lines.append("\tdef on_destroy(self) -> None: ...")
    lines.append("\tdef on_collision_enter(self, other: Entity) -> None: ...")
    lines.append("\tdef on_collision_exit(self, other: Entity) -> None: ...")
    lines.append("\t@property")
    lines.append("\tdef _entity(self) -> Entity: ...")
    lines.append("")
    lines.append("class Scene:")
    lines.append("\tdef find_entity_by_tag(self, tag: str) -> Entity | None: ...")
    lines.append("\tdef create_entity(self, name: str) -> Entity: ...")
    lines.append("\tdef destroy_entity(self, entity: Entity) -> None: ...")
    lines.append("\tdef queue_free(self, entity: Entity) -> None: ...")
    lines.append("\tdef is_queued_for_deletion(self, entity: Entity) -> bool: ...")
    lines.append("\tdef create_physics_body(self, entity: Entity) -> None: ...")
    lines.append("\tdef recreate_physics_body(self, entity: Entity) -> None: ...")
    lines.append("\tdef instantiate_script(self, entity: Entity) -> None: ...")
    lines.append("")
    lines.append("# Module-level functions")
    lines.append("def is_key_pressed(key: str) -> bool: ...")
    lines.append("def play_one_shot(path: str, volume: float = 1.0) -> None: ...")

    # Write file (write_text with default newline='' translates \n to os.linesep on Windows)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    text = "\n".join(lines)
    output_path.write_text(text, encoding="utf-8")
    print(f"Generated: {output_path}")


def generate_vscode_settings(stub_dir: Path):
    """Write Pylance / Pyright config so the stub is found.

    Pylance resolves ``import candy`` only when the stub lives under the
    configured ``stubPath`` as a package (``candy/__init__.pyi``). We point it
    at the ``stubs/`` directory.

    Two files are written:
      * ``.vscode/settings.json`` - preferred by Pylance inside VS Code
        (note: ``.vscode`` is gitignored, so this is local-only).
      * ``pyrightconfig.json`` at the project root - version-controlled and
        used by the pyright CLI / other editors.
    """
    # stubPath points at the directory that CONTAINS the stub package
    # (stub_dir is ``stubs/candy``, so the containing dir is ``stubs``).
    # Pylance then resolves ``stubs/candy/__init__.pyi`` as module ``candy``.
    rel_stub = str(stub_dir.parent.resolve().relative_to(PROJECT_ROOT).as_posix())

    # 1) .vscode/settings.json ------------------------------------------------
    vscode_dir = PROJECT_ROOT / ".vscode"
    vscode_dir.mkdir(parents=True, exist_ok=True)
    settings_path = vscode_dir / "settings.json"

    settings: dict = {}
    if settings_path.exists():
        try:
            settings = json.loads(settings_path.read_text(encoding="utf-8"))
        except Exception:
            settings = {}
    settings["python.analysis.stubPath"] = rel_stub
    settings["python.analysis.useLibraryCodeForTypes"] = False
    settings["python.analysis.typeCheckingMode"] = "basic"
    try:
        settings_path.write_text(
            json.dumps(settings, indent=4) + "\n",
            encoding="utf-8"
        )
        print(f"Generated: {settings_path}")
    except Exception as e:
        print(f"  Warning: Could not write {settings_path}: {e}")

    # 2) pyrightconfig.json at project root (version-controlled) --------------
    pyright_config = {
        "stubPath": rel_stub,
        "typeCheckingMode": "basic",
    }
    pyright_path = PROJECT_ROOT / "pyrightconfig.json"
    try:
        pyright_path.write_text(
            json.dumps(pyright_config, indent=4) + "\n",
            encoding="utf-8"
        )
        print(f"Generated: {pyright_path}")
    except Exception as e:
        print(f"  Warning: Could not write {pyright_path}: {e}")


# ============================================================================
# Main
# ============================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(description="CandyEngine Python Binding Code Generator")
    parser.add_argument("--source", default=str(DEFAULT_SOURCE_DIR),
                        help="Source directory to scan for annotated C++ headers")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT_DIR),
                        help="Output directory for ScriptBindings.generated.inl")
    parser.add_argument("-p", "--pyi-dir", default=str(DEFAULT_PYI_DIR),
                        help="Output directory for candy.pyi and pyrightconfig.json "
                             "(default: <root>/JumpGame/Content/Scripts)")
    args = parser.parse_args()

    source_dir = Path(args.source)
    output_dir = Path(args.output)
    pyi_dir = Path(args.pyi_dir)

    if not source_dir.exists():
        print(f"Error: Source directory not found: {source_dir}")
        sys.exit(1)

    # Scan
    print("=" * 60)
    print("CandyEngine Python Binding Code Generator")
    print("=" * 60)
    structs, enums = scan_all_headers(source_dir)

    print(f"\nFound {len(structs)} struct(s) and {len(enums)} enum(s):")
    for s in structs:
        members = ", ".join(m[1] for m in s.members)
        print(f"  Struct: {s.name} -> [{members}]")
    for e in enums:
        vals = ", ".join(e.values)
        print(f"  Enum:   {e.name} -> [{vals}]")

    # Generate .inl
    inl_path = output_dir / "ScriptBindings.generated.inl"
    generate_inl(structs, enums, inl_path)

    # Generate .pyi as a package-style stub (candy/__init__.pyi)
    pyi_path = pyi_dir / "__init__.pyi"
    generate_pyi(structs, enums, pyi_path)

    # Generate .vscode/settings.json pointing stubPath at the stub dir
    generate_vscode_settings(pyi_dir)

    print("\nDone!")


if __name__ == "__main__":
    main()
