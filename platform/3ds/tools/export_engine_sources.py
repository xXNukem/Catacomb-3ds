#!/usr/bin/env python3
"""Create a source-only snapshot of catacomb-3ds for offline native-engine integration.

Runs locally without Codex, network or game data. Includes uncommitted source
files so a patch applied by `unzip` is visible in the snapshot.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import sys
import zipfile

ALLOWED_ROOTS = {"src", "include", "tests", "platform", "cmake", "tools"}
ALLOWED_EXTS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
    ".s", ".asm", ".inc", ".inl", ".ipp", ".py", ".cmake", ".mk",
    ".txt", ".md", ".json", ".toml", ".yaml", ".yml", ".def", ".in",
    ".patch", ".diff", ".ld", ".specs", ".sh", ".bat",
}
ALLOWED_FILENAMES = {
    "makefile", "gnumakefile", "cmakelists.txt", "license", "license.txt",
    "copying", "copying.txt", ".gitmodules", ".clang-format", ".editorconfig",
}
SKIP_DIRS = {
    ".git", ".svn", ".hg", "build", "build-3ds", "build-debug", "build-release",
    "debug", "release", "dist", "out", "output", "games", "game", "romfs",
    "sdmc", "save", "saves", "saveslots", "downloads", "__pycache__", ".venv",
    "venv", "node_modules", ".cache", ".idea", ".vscode", ".pytest_cache",
}
BLOCKED_EXTS = {
    ".exe", ".com", ".c3d", ".abs", ".arm", ".apc", ".lab", ".gam",
    ".rar", ".zip", ".7z", ".iso", ".img", ".rom", ".3dsx", ".cia",
    ".3ds", ".elf", ".o", ".a", ".dylib", ".dll", ".so", ".sav",
    ".env", ".pem", ".key", ".p12", ".pfx",
}
MAX_FILE_BYTES = 4 * 1024 * 1024
MAX_TOTAL_BYTES = 100 * 1024 * 1024


def find_root(cwd: Path) -> Path:
    for test in (cwd, *cwd.parents):
        if (test / "platform" / "3ds" / "Makefile").is_file() and (test / "src").is_dir():
            return test
    raise RuntimeError("Run from the catacomb-3ds repository root (or one of its subdirectories).")


def allowed(rel: Path) -> bool:
    parts = rel.parts
    if not parts or parts[0] not in ALLOWED_ROOTS:
        return False
    if parts[0] == "platform" and (len(parts) < 2 or parts[1] != "3ds"):
        return False
    if any(p.lower() in SKIP_DIRS or p.startswith(".") for p in parts[:-1]):
        return False
    name = parts[-1]
    lower = name.lower()
    if lower in {".env", "id_rsa", "id_ed25519", "credentials.json", "secrets.json"}:
        return False
    suffix = rel.suffix.lower()
    if suffix in BLOCKED_EXTS:
        return False
    return lower in ALLOWED_FILENAMES or suffix in ALLOWED_EXTS


def gather(root: Path):
    candidates = []
    total = 0
    for sub in ("src", "include", "tests", "platform/3ds", "cmake", "tools"):
        base = root / sub
        if not base.is_dir():
            continue
        for folder, dirs, files in os.walk(base, followlinks=False):
            dirs[:] = sorted(d for d in dirs if d.lower() not in SKIP_DIRS and not d.startswith("."))
            for name in sorted(files):
                path = Path(folder) / name
                if path.is_symlink() or not path.is_file():
                    continue
                rel = path.relative_to(root)
                if not allowed(rel):
                    continue
                size = path.stat().st_size
                if size > MAX_FILE_BYTES:
                    raise RuntimeError(f"Source file too large, check manually: {rel} ({size} bytes)")
                total += size
                if total > MAX_TOTAL_BYTES:
                    raise RuntimeError("Source snapshot exceeded 100 MiB; please inspect before sharing.")
                candidates.append((rel, path))
    for name in ("CMakeLists.txt", "Makefile", "LICENSE", "LICENSE.txt", "COPYING", ".gitmodules"):
        path = root / name
        if path.is_file() and not path.is_symlink():
            candidates.append((Path(name), path))
    return sorted(set(candidates))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", default="catacomb-3ds-engine-source-snapshot.zip",
                    help="Path to generated ZIP (relative to repository root by default)")
    ap.add_argument("--dry-run", action="store_true", help="Show file count only")
    args = ap.parse_args()
    try:
        root = find_root(Path.cwd().resolve())
        paths = gather(root)
        if len(paths) < 25:
            raise RuntimeError("Too few source files: check that this is the complete repository.")
        expected = [
            "src/id91_11/cat_all/cat3d/c3_main.c",
            "src/id91_11/cat_all/cat3d/c3_draw.c",
            "src/id91_11/cat_all/c3_trace.c",
            "platform/3ds/source/main.cpp",
        ]
        missing = [p for p in expected if not (root / p).is_file()]
        if missing:
            raise RuntimeError("Missing required original sources: " + ", ".join(missing))
        print(f"Source files: {len(paths)}")
        print(f"Source bytes: {sum(p.stat().st_size for _, p in paths):,}")
        print("Expected original engine files: OK")
        print("Commercial game data: excluded by directory/extension allowlists")
        if args.dry_run:
            return 0
        dest = Path(args.output)
        if not dest.is_absolute():
            dest = root / dest
        dest = dest.resolve()
        if dest == Path(__file__).resolve():
            raise RuntimeError("Output would overwrite this script")
        with zipfile.ZipFile(dest, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for rel, src in paths:
                archive.write(src, rel.as_posix())
        digest = hashlib.sha256(dest.read_bytes()).hexdigest()
        print(f"Created: {dest}")
        print(f"ZIP size: {dest.stat().st_size:,} bytes")
        print(f"SHA-256: {digest}")
        print("\nUpload this ZIP to the conversation. No Codex required.")
        print("NOTE: Review the ZIP before sharing if you put personal information in source comments.")
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
