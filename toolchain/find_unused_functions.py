#!/usr/bin/env python3
"""Report functions that lack cross-file usages using libclang."""
from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple

try:
    from clang import cindex
except ImportError as exc:  # pragma: no cover - graceful failure for missing dependency
    print("Failed to import clang.cindex: {}".format(exc), file=sys.stderr)
    print("Install libclang and ensure python bindings are on PYTHONPATH.", file=sys.stderr)
    sys.exit(2)


DEF_KINDS = {
    cindex.CursorKind.FUNCTION_DECL,
    cindex.CursorKind.CXX_METHOD,
    cindex.CursorKind.CONSTRUCTOR,
    cindex.CursorKind.DESTRUCTOR,
    cindex.CursorKind.FUNCTION_TEMPLATE,
}

REF_KINDS = {
    cindex.CursorKind.CALL_EXPR,
    cindex.CursorKind.DECL_REF_EXPR,
    cindex.CursorKind.MEMBER_REF_EXPR,
    cindex.CursorKind.OVERLOADED_DECL_REF,
}

STATIC_STORAGE = getattr(cindex.StorageClass, "STATIC", None)

PARSE_INCOMPLETE = getattr(cindex.TranslationUnit, "PARSE_INCOMPLETE", 0)
PARSE_SKIP_FUNCTION_BODIES = getattr(cindex.TranslationUnit, "PARSE_SKIP_FUNCTION_BODIES", 0)
PARSE_OPTIONS = (
    cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
    | PARSE_INCOMPLETE
    | PARSE_SKIP_FUNCTION_BODIES  # Skip function bodies for massive speedup
)

UNSUPPORTED_FLAGS = {
    "-fmerge-all-constants",
    "-MMD",
    "-MP",
}

UNSUPPORTED_PREFIXES = (
    "-mcpu=",
)

UNSUPPORTED_VALUE_FLAGS = {
    "-o",
    "-MF",
    "-MT",
    "-MQ",
    "-arch",
}

SYSTEM_INCLUDE_CACHE: Dict[Tuple[str, str], List[str]] = {}


@dataclass
class FunctionInfo:
    usr: str
    name: str
    displayname: str
    location: str
    line: int
    is_static: bool
    is_inline: bool
    references: Set[str] = field(default_factory=set)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Find functions without cross-file usages.")
    parser.add_argument(
        "--compile-commands",
        default="compile_commands.json",
        help="Path to compile_commands.json (default: %(default)s)",
    )
    parser.add_argument(
        "--root",
        default=None,
        help="Project root. Defaults to the parent of compile_commands.json.",
    )
    parser.add_argument(
        "--skip-static",
        action="store_true",
        help="Do not report functions with static storage class.",
    )
    parser.add_argument(
        "--skip-inline",
        action="store_true",
        help="Do not report inline functions or methods.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print progress information to stderr.",
    )
    parser.add_argument(
        "--jobs",
        "-j",
        type=int,
        default=1,
        help="Number of parallel parse jobs (default: 1, use 0 for CPU count)",
    )
    return parser.parse_args()


def split_command(entry: dict) -> Tuple[Optional[str], List[str]]:
    if "arguments" in entry and entry["arguments"]:
        args = list(entry["arguments"])
    elif "command" in entry:
        args = shlex.split(entry["command"])
    else:
        return None, []
    if not args:
        return None, []
    return args[0], args[1:]


def load_compile_commands(path: Path) -> List[dict]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if isinstance(data, dict) and "commands" in data:
        return data["commands"]
    if isinstance(data, list):
        return data
    raise ValueError("Unsupported compile_commands.json structure")


def determine_language(source_path: Path, args: List[str]) -> str:
    ext = source_path.suffix.lower()
    if ext == ".c":
        default = "c"
    elif ext in {".cc", ".cp", ".cxx", ".cpp", ".c++", ".hpp", ".hh", ".hxx"}:
        default = "c++"
    elif ext == ".mm":
        default = "objective-c++"
    elif ext == ".m":
        default = "objective-c"
    else:
        default = "c++"
    for index, arg in enumerate(args):
        if arg == "-x" and index + 1 < len(args):
            return args[index + 1]
    for arg in args:
        if arg.startswith("-std=") and "++" in arg:
            return "c++"
    return default


def detect_system_includes(compiler: Optional[str], language: str) -> List[str]:
    if not compiler:
        return []
    resolved = shutil.which(compiler)
    if resolved is None:
        return []
    key = (resolved, language)
    cached = SYSTEM_INCLUDE_CACHE.get(key)
    if cached is not None:
        return cached
    cmd = [resolved, "-x", language, "-E", "-", "-v"]
    try:
        proc = subprocess.run(
            cmd,
            check=False,
            capture_output=True,
            text=True,
            input="",
        )
    except OSError:
        SYSTEM_INCLUDE_CACHE[key] = []
        return []
    includes: List[str] = []
    capture = False
    for line in proc.stderr.splitlines():
        stripped = line.strip()
        if stripped == "#include <...> search starts here:":
            capture = True
            continue
        if capture:
            if stripped == "End of search list.":
                break
            if stripped:
                if "framework directory" in stripped:
                    path = stripped.replace("(framework directory)", "").strip()
                    if path:
                        includes.extend(["-F", path])
                else:
                    includes.extend(["-isystem", stripped])
    SYSTEM_INCLUDE_CACHE[key] = includes
    return includes


def prepare_args(
    compiler: Optional[str],
    args: List[str],
    source: str,
    language: str,
) -> List[str]:
    if compiler is None:
        return []
    filtered: List[str] = []
    skip_next = False
    for arg in args:
        if skip_next:
            skip_next = False
            continue
        if arg == "-c":
            continue
        if arg in UNSUPPORTED_VALUE_FLAGS:
            skip_next = True
            continue
        if arg == source:
            continue
        if arg in UNSUPPORTED_FLAGS:
            continue
        if any(arg.startswith(prefix) for prefix in UNSUPPORTED_PREFIXES):
            continue
        filtered.append(arg)
    system_args = detect_system_includes(compiler, language)
    has_explicit_language = False
    for index, arg in enumerate(filtered):
        if arg == "-x" and index + 1 < len(filtered):
            has_explicit_language = True
            break
    if has_explicit_language:
        base_args = filtered
    else:
        base_args = ["-x", language, *filtered]
    return [*base_args, *system_args]


def to_path(path_obj: Optional[object]) -> Optional[Path]:
    if path_obj is None:
        return None
    if isinstance(path_obj, Path):
        return path_obj
    name = getattr(path_obj, "name", None)
    if not name:
        return None
    return Path(name)


def is_project_file(root: Path, path_obj: Optional[object]) -> bool:
    path = to_path(path_obj)
    if path is None:
        return False
    try:
        path.resolve().relative_to(root)
        return True
    except ValueError:
        return False


def cursor_is_inline(cursor: cindex.Cursor) -> bool:
    checker = getattr(cursor, "is_inlined_function", None)
    if callable(checker):
        try:
            return bool(checker())
        except cindex.LibclangError:
            return False
    checker = getattr(cursor, "is_inline_method", None)
    if callable(checker):
        try:
            return bool(checker())
        except cindex.LibclangError:
            return False
    return False


def cursor_is_static(cursor: cindex.Cursor) -> bool:
    return STATIC_STORAGE is not None and cursor.storage_class == STATIC_STORAGE


def relpath(root: Path, path: Path) -> str:
    try:
        return str(path.resolve().relative_to(root))
    except ValueError:
        return str(path.resolve())


def visit(cursor: cindex.Cursor, root: Path, functions: Dict[str, FunctionInfo]) -> None:
    stack = [cursor]
    while stack:
        current = stack.pop()
        try:
            kind = current.kind
        except ValueError:
            # Skip cursors with unknown kinds (e.g., newer AST nodes)
            stack.extend(list(current.get_children()))
            continue
        loc_file = current.location.file
        loc_path = to_path(loc_file)
        if kind in DEF_KINDS and current.is_definition() and loc_path and is_project_file(root, loc_path):
            usr = current.get_usr()
            if usr:
                file_path = relpath(root, loc_path)
                info = functions.get(usr)
                if info is None:
                    functions[usr] = FunctionInfo(
                        usr=usr,
                        name=current.spelling or current.displayname,
                        displayname=current.displayname or current.spelling,
                        location=file_path,
                        line=current.location.line,
                        is_static=cursor_is_static(current),
                        is_inline=cursor_is_inline(current),
                    )
                else:
                    info.location = file_path
                    info.line = current.location.line
                    info.is_static = cursor_is_static(current)
                    info.is_inline = cursor_is_inline(current)
                    if not info.name:
                        info.name = current.spelling or current.displayname
                    if not info.displayname:
                        info.displayname = current.displayname or current.spelling
        elif kind in REF_KINDS:
            referenced = current.referenced
            ref_file = current.location.file
            ref_path = to_path(ref_file)
            if (
                referenced
                and referenced.get_usr()
                and ref_path
                and is_project_file(root, ref_path)
                and referenced.kind in DEF_KINDS
            ):
                usr = referenced.get_usr()
                def_file = to_path(referenced.location.file)
                if not def_file or not is_project_file(root, def_file):
                    continue
                file_path = relpath(root, ref_path)
                info = functions.get(usr)
                if info is None:
                    functions[usr] = FunctionInfo(
                        usr=usr,
                        name=referenced.spelling or referenced.displayname,
                        displayname=referenced.displayname or referenced.spelling,
                        location=relpath(root, def_file),
                        line=referenced.location.line if referenced.location else 0,
                        is_static=cursor_is_static(referenced),
                        is_inline=cursor_is_inline(referenced),
                        references={file_path},
                    )
                else:
                    info.references.add(file_path)
        stack.extend(list(current.get_children()))


def parse_single_tu(args_tuple: Tuple[Path, Path, str, List[str], int, int]) -> Dict[str, FunctionInfo]:
    """Parse a single translation unit and return function information."""
    source_path, root, language, args, idx, total = args_tuple
    index = cindex.Index.create()
    functions: Dict[str, FunctionInfo] = {}
    
    try:
        tu = index.parse(
            str(source_path),
            args=args,
            options=PARSE_OPTIONS,
        )
    except cindex.TranslationUnitLoadError as exc:
        print("Failed to parse {}: {}".format(source_path, exc), file=sys.stderr)
        return functions
    
    visit(tu.cursor, root, functions)
    return functions


def analyze(root: Path, commands: Iterable[dict], verbose: bool, jobs: int) -> Dict[str, FunctionInfo]:
    processed: Set[Tuple[Path, str]] = set()
    
    # Build list of unique translation units to parse
    tu_list: List[Tuple[Path, str, List[str]]] = []
    for entry in commands:
        source = entry.get("file")
        if not source:
            continue
        source_path = Path(source)
        compiler, raw_args = split_command(entry)
        if compiler is None:
            continue
        language = determine_language(source_path, raw_args)
        visit_key = (source_path.resolve(), language)
        if visit_key in processed:
            continue
        processed.add(visit_key)
        args = prepare_args(compiler, raw_args, source, language)
        tu_list.append((source_path, language, args))
    
    total = len(tu_list)
    if verbose:
        print("Parsing {} unique translation units using {} worker(s)...".format(total, jobs), file=sys.stderr)
    
    functions: Dict[str, FunctionInfo] = {}
    
    if jobs == 1:
        # Sequential processing
        index = cindex.Index.create()
        for idx, (source_path, language, args) in enumerate(tu_list, 1):
            if verbose:
                print("[{}/{}] Parsing {}".format(idx, total, source_path), file=sys.stderr)
            try:
                tu = index.parse(
                    str(source_path),
                    args=args,
                    options=PARSE_OPTIONS,
                )
            except cindex.TranslationUnitLoadError as exc:
                print("Failed to parse {}: {}".format(source_path, exc), file=sys.stderr)
                continue
            visit(tu.cursor, root, functions)
    else:
        # Parallel processing
        parse_args = [
            (source_path, root, language, args, idx, total)
            for idx, (source_path, language, args) in enumerate(tu_list, 1)
        ]
        
        with ProcessPoolExecutor(max_workers=jobs) as executor:
            futures = {executor.submit(parse_single_tu, arg): arg for arg in parse_args}
            
            for future in as_completed(futures):
                arg = futures[future]
                source_path = arg[0]
                idx = arg[4]
                
                if verbose:
                    print("[{}/{}] Completed parsing {}".format(idx, total, source_path), file=sys.stderr)
                
                try:
                    result = future.result()
                    # Merge results
                    for usr, info in result.items():
                        if usr in functions:
                            # Merge references
                            functions[usr].references.update(info.references)
                            # Update definition info if this one has better data
                            if info.location and not functions[usr].location:
                                functions[usr].location = info.location
                                functions[usr].line = info.line
                        else:
                            functions[usr] = info
                except Exception as exc:
                    print("Exception while parsing {}: {}".format(source_path, exc), file=sys.stderr)
    
    return functions


def main() -> None:
    args = parse_args()
    compile_commands_path = Path(args.compile_commands).resolve()
    if not compile_commands_path.exists():
        print("Missing compile_commands.json at {}".format(compile_commands_path), file=sys.stderr)
        sys.exit(1)
    project_root = Path(args.root).resolve() if args.root else compile_commands_path.parent
    commands = load_compile_commands(compile_commands_path)
    
    jobs = args.jobs
    if jobs == 0:
        jobs = os.cpu_count() or 1
    
    functions = analyze(project_root, commands, args.verbose, jobs)
    unused: List[FunctionInfo] = []
    for info in functions.values():
        if args.skip_static and info.is_static:
            continue
        if args.skip_inline and info.is_inline:
            continue
        if not info.references:
            unused.append(info)
            continue
        external_refs = {ref for ref in info.references if ref != info.location}
        if not external_refs:
            unused.append(info)
    if not unused:
        print("No functions without cross-file usage detected.")
        return
    unused.sort(key=lambda item: (item.location, item.line, item.name))
    print("Functions lacking cross-file usages:\n")
    for info in unused:
        explanation = "no references recorded" if not info.references else "only referenced in defining file"
        print(
            "- {} ({}:{}) -> {}".format(
                info.displayname,
                info.location,
                info.line,
                explanation,
            )
        )
    print("\nTotal functions analyzed: {}".format(len(functions)))
    print("Flagged functions: {}".format(len(unused)))


if __name__ == "__main__":
    main()
