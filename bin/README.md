# Binaries

GKC executables and the paths the build outputs use:

| Platform | File |
|---|---|
| Linux x86-64 | `gkc` (repository root) |
| macOS arm64 | `bin/gkc-macos-arm64` |
| Windows x86-64 | `bin/gkc-windows-x64.exe` |
| WebAssembly | `html/gkcjs.wasm` and `html/gkcjs.js` |

The macOS, Windows, and WebAssembly files are checked in. The Linux file is
produced by `compile.sh`, which links it statically at `gkc`.

Every binary takes the same command-line arguments. Run one with `-help` for
the option summary and `-version` for build information; the options themselves
are in [`../Doc/cli_reference.md`](../Doc/cli_reference.md).

## Running

```sh
./gkc Examples/steam.txt
./bin/gkc-macos-arm64 Examples/steam.txt
```

On Windows:

```text
bin\gkc-windows-x64.exe Examples\steam.txt
```

Run the commands from the repository root, so that the example paths resolve.
macOS marks a downloaded binary as quarantined; if it is refused, clear the
attribute with `xattr -d com.apple.quarantine bin/gkc-macos-arm64`.

The WebAssembly files run in a browser through
[`../html/gkc.html`](../html/gkc.html), which reads the problem from a text box:
the browser has no host filesystem, so input is not taken from it.

## Building

Run the build commands from the repository root.

| Target | Command | Tool | Output |
|---|---|---|---|
| Linux | `./compile.sh` | GCC by default; set `CC` to use another C compiler | `gkc` |
| macOS | `./compile_osx.sh` | Clang by default | `gkc` |
| Windows | `compile.bat` in an x64 MSVC command prompt | MSVC | `gkc.exe` |
| WebAssembly | `./compile_wasm.sh` | Emscripten | `gkcjs.html`, `gkcjs.js`, `gkcjs.wasm` |

The shell scripts compile the checked-in parser files under `../Builtparser/`;
Flex and Bison are not required. `compile_wasm.sh` currently expects Emscripten
at `/opt/gkcjs/emsdk/upstream/emscripten/emcc`.

The Linux makefile is an alternative:

```sh
make -f makefile
```

This form uses Flex and Bison. Use the checked-in parser files with:

```sh
make -f makefile USEBISON=false
```

The makefile does not track every header dependency. Rebuild after header
changes with `make -f makefile -B`.

The macOS and Windows builds are also defined as GitHub Actions workflows,
which build and smoke-test the binaries checked in here:

- `../.github/workflows/macos-build.yml`
- `../.github/workflows/windows-build.yml`
