# GKC

GKC is a first-order logic prover and query engine. It reads TPTP FOF/CNF,
GKC's Prolog-style notation, and JSON-LD-LOGIC. It supports equality,
arithmetic, answer variables, automatic search strategies, and reusable
shared-memory knowledge bases.

## Binaries

The binary files and build outputs use these paths:

| Platform | File |
|---|---|
| Linux x86-64 | `gkc` |
| macOS arm64 | `bin/gkc-macos-arm64` |
| Windows x86-64 | `bin/gkc-windows-x64.exe` |
| WebAssembly | `html/gkcjs.wasm` and `html/gkcjs.js` |

`compile.sh` creates the statically linked Linux file at `gkc`. The macOS,
Windows, and WebAssembly files in the table are checked in. The WebAssembly
files run in a browser through [`html/gkc.html`](html/gkc.html), which reads the
problem from a text box; the input is not taken from the host filesystem.

Examples:

```sh
./gkc Examples/steam.txt
./bin/gkc-macos-arm64 Examples/steam.txt
```

On Windows:

```text
bin\gkc-windows-x64.exe Examples\steam.txt
```

Run the selected binary with `-help` for command-line options and `-version`
for build information.

## Building

Run build commands from the repository root.

| Target | Command | Tool | Output |
|---|---|---|---|
| Linux | `./compile.sh` | GCC by default; set `CC` to use another C compiler | `gkc` |
| macOS | `./compile_osx.sh` | Clang by default | `gkc` |
| Windows | `compile.bat` in an x64 MSVC command prompt | MSVC | `gkc.exe` |
| WebAssembly | `./compile_wasm.sh` | Emscripten | `gkcjs.html`, `gkcjs.js`, `gkcjs.wasm` |

The shell scripts compile the checked-in parser files under `Builtparser/`;
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

The macOS and Windows checks are defined in:

- `.github/workflows/macos-build.yml`
- `.github/workflows/windows-build.yml`

## Documentation

| Document | Contents |
|---|---|
| [`Examples/README.md`](Examples/README.md) | Tutorial and runnable examples |
| [`Doc/cli_reference.md`](Doc/cli_reference.md) | Commands and options |
| [`Doc/strategy_reference.md`](Doc/strategy_reference.md) | Search strategy files |
| [`Doc/json_ld_logic.md`](Doc/json_ld_logic.md) | JSON-LD-LOGIC input |
| [`Doc/ARCHITECTURE.md`](Doc/ARCHITECTURE.md) | Program structure and execution flow |
| [`Doc/DEVELOPMENT_GUIDE.md`](Doc/DEVELOPMENT_GUIDE.md) | Development and testing |
| [`Doc/README.md`](Doc/README.md) | Documentation index |

## Main directories

```text
Main/          command-line entry point
Reasoner/      proof search and inference
Db/            database and shared-memory code
Parser/        parser definitions
Builtparser/   checked-in generated parsers
Printer/       logical and proof output
Examples/      examples and tutorial
Test/          tests
Doc/           user and implementation documentation
```

GKC is licensed under the GNU Affero General Public License version 3. Tanel
Tammet is the author and maintainer.
