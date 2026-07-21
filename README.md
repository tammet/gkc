# GKC

GKC is a first-order logic prover and query engine. It reads TPTP FOF/CNF,
GKC's Prolog-style notation, and JSON-LD-LOGIC. It supports equality,
arithmetic, answer variables, automatic search strategies, and reusable
shared-memory knowledge bases.

## Running GKC

Prebuilt binaries for macOS, Windows, and WebAssembly are in `bin/` and
`html/`; the Linux binary is built by `./compile.sh` at `gkc`:

```sh
./gkc Examples/steam.txt
```

Run the binary with `-help` for the command-line options. Every platform takes
the same arguments; [`bin/README.md`](bin/README.md) covers the other
platforms, the browser build, and how each one is compiled.

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
bin/           prebuilt binaries and their build notes
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
