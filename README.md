# NeoGit

A minimal, Git-emulating version control system written in C++17. It implements Git's
core three-stage workflow (Working Directory, Staging/Index, Repository) backed by
a Merkle-linked commit DAG, with on-disk objects hashed using SHA-1 (OpenSSL) and
compressed with zlib.

This is a teaching project, not a drop-in replacement for Git. The object format,
index, and command set are simplified to keep the code small and readable.

---

## Features

- `init` — create a NeoGit repository in `.neogit/`
- `hash-object` — hash a file as a blob and optionally store it
- `cat-file` — read a stored blob by its SHA-1
- `add` — stage a file (path-normalized, deduplicated in the index)
- `commit` — create a Merkle-linked commit (with `parent`, `tree`, `author`, message)
- `log` — walk the commit DAG from `HEAD` backwards
- `checkout <hash>` — restore the working tree to any commit, removing files that
  exist only in the previous tree

---

## Architecture

### Object store

All objects are stored under `.neogit/objects/xx/yyyy...` (sharded by the first
two hex characters of the SHA-1, exactly like Git). Each object file is a
zlib-compressed byte stream of:

```
<type> <size-in-bytes>\0<raw-content>
```

where `<type>` is one of `blob`, `tree`, `commit`. The SHA-1 is computed over the
full header-plus-content, not just the raw content. Decompression and header
parsing happen in `readObject()` (see `utils.cpp`).

### Three object types

| Type    | Purpose                                                                                  |
|---------|------------------------------------------------------------------------------------------|
| `blob`  | Raw file content. Identical to a zip of the bytes.                                       |
| `tree`  | Flat list of `mode hash path` lines, one per staged file, sorted by path.                |
| `commit`| Text object with `tree`, `parent` (omitted on the first commit), `author`, blank line, message. |

### Commit DAG

Each commit object links to exactly one `tree` (its root) and at most one
`parent` (the previous commit). The chain is a linear linked list per branch;
branching is not implemented. `.neogit/HEAD` stores the SHA-1 of the current
commit directly (no branch refs).

```
[commit] --tree--> [tree] --entry--> [blob]
    |
    parent
    |
[commit] --tree--> [tree] --entry--> [blob]
```

This is the same Merkle structure Git uses: tampering with any blob changes
its SHA, which changes the tree hash, which changes every descendant commit
hash. A `log` traversal walks `parent` pointers; a `checkout` walks
`commit -> tree -> blob` to restore the working tree.

### Index

`.neogit/index` is a simple text file with one line per staged entry:
`<mode> <blob-hash> <path>`. Re-adding a path replaces its previous entry. The
index is cleared after a successful commit, mirroring Git's behavior.

---

## Requirements

- A C++17 compiler (MSVC, g++, or clang++)
- **OpenSSL** (for SHA-1)
- **zlib** (for compression)
- **CMake 3.10+** (optional, for the CMake build)

| Platform | Easy install                                                 |
|----------|--------------------------------------------------------------|
| Windows  | MSYS2: `pacman -S mingw-w64-x86_64-gcc openssl zlib cmake`  |
| Ubuntu   | `sudo apt install g++ libssl-dev zlib1g-dev cmake`          |
| macOS    | `brew install openssl zlib cmake` (Xcode CLI tools for g++) |

---

## Build

### With CMake (recommended)

```bash
git clone https://github.com/Muhammed-Nady/NeoGit
cd NeoGit
mkdir build && cd build
cmake ..
cmake --build .
# binary at build/neogit (or build/neogit.exe on Windows)
```

### With g++ directly

```bash
g++ -std=c++17 -I<openssl-include> -I<zlib-include> \
    main.cpp utils.cpp repository.cpp tree.cpp commit.cpp \
    -L<lib-dir> -lssl -lcrypto -lz \
    -o neogit
```

On Windows with MSYS2, the include and lib paths are usually
`C:\msys64\mingw64\include` and `C:\msys64\mingw64\lib`.

---

## Quick start

```bash
# 1. Create a project and a NeoGit repository inside it
mkdir my-project && cd my-project
/path/to/neogit init

# 2. Create and stage a file
echo "hello world" > a.txt
neogit add a.txt

# 3. Commit
neogit commit "first commit"
# -> [abc1234] first commit

# 4. See the history
neogit log

# 5. Modify and commit again
echo "hello again" > a.txt
neogit add a.txt
neogit commit "second commit"

# 6. Roll back to the first commit
neogit checkout <first-commit-hash>
cat a.txt   # -> hello world

# 7. Roll forward
neogit checkout <second-commit-hash>
cat a.txt   # -> hello again
```

Commit hashes are the 40-character SHA-1s printed by `commit` and `log`. On
Windows PowerShell you can read the current HEAD with `Get-Content .neogit\HEAD`.

---

## Commands

```
neogit init
neogit hash-object <file>
neogit cat-file <blob-hash>
neogit add <file>
neogit commit <message>
neogit log
neogit checkout <commit-hash>
```

All file paths are stored in the index with forward slashes regardless of
platform, so `neogit add src\foo.cpp` and `neogit add src/foo.cpp` are treated
identically. On checkout, parent directories are created as needed and any
files that were tracked in the previous commit but are absent from the new
commit are removed. Untracked files in the working tree are left alone.

---

## End-to-end test

A full smoke test that exercises every command:

```bash
rm -rf .neogit
neogit init
echo "v1" > a.txt
echo "this is b" > b.txt
neogit add a.txt
neogit add b.txt
neogit commit "first commit"
# save the full hash: cat .neogit/HEAD

echo "v2" > a.txt
neogit add a.txt
neogit commit "second commit"
# save this hash too

neogit log
# -> both commits, parent-linked

neogit checkout <first-hash>
cat a.txt          # v1
cat .neogit/HEAD   # first-hash

neogit checkout <second-hash>
cat a.txt          # v2

# nested paths
mkdir src
echo "int main() {}" > src/foo.cpp
neogit add src/foo.cpp
neogit commit "added nested"
neogit checkout <first-hash>   # src/foo.cpp disappears
neogit checkout <latest-hash>  # src/foo.cpp reappears

# error cases (should print "Fatal:" not crash)
neogit cat-file 1234
neogit cat-file <a-commit-hash>   # commits are not blobs
neogit checkout badhash
```

---

## Project structure

```
.
|-- CMakeLists.txt
|-- main.cpp           CLI dispatcher
|-- utils.{h,cpp}      SHA-1, zlib, storeObject, readObject
|-- repository.{h,cpp} handleInit / handleAdd / handleCommit / handleLog / handleCheckout
|-- tree.{h,cpp}       buildTree, readTree, loadIndex, writeIndex
|-- commit.{h,cpp}     writeCommit, readCommit
|-- test.txt           sample file
|-- .gitignore         ignores .neogit/, build/, neogit(.exe), desktop.ini
```

---

## Limitations

- No branches, no branch refs, no tags. `HEAD` stores a single commit hash.
- `cat-file` only prints blobs. Trees and commits can be read by hash but
  the CLI has no pretty-printer for them yet.
- No `status`, no `diff`, no `rm`, no `reset`. Untracked files are not tracked
  separately.
- No pack files. The object store is one loose object per file, which gets
  inefficient past a few thousand objects.
- No network operations of any kind.
- The tree object is flat: every entry is a blob at a (possibly nested) path.
  Subdirectories do not become nested tree objects.

These are deliberate scope cuts, not bugs.

---

## License

Use it however you like. Attribution appreciated.
