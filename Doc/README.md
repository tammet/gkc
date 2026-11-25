# GKC documentation map

The top-level [`README.md`](../README.md) is the compact user manual and
[`Examples/README.md`](../Examples/README.md) is the tutorial. This directory
contains maintained references and implementation documentation.

## User and format references

| Document | Purpose |
| --- | --- |
| [`cli_reference.md`](cli_reference.md) | Commands, flags, limits, input dispatch, conversion, and shared-KB workflows. |
| [`strategy_reference.md`](strategy_reference.md) | Strategy JSON structure, automatic portfolios, queue/limit controls, and arithmetic-instantiation settings. |
| [`json_ld_logic.md`](json_ld_logic.md) | JSON/JSON-LD-LOGIC input syntax and logical representation. |

## Current code documentation

| Document | Purpose |
| --- | --- |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Source map and end-to-end parser, strategy, search, inference, history, and shared-memory flow. |
| [`SEARCH_AND_DERIVATION_PIPELINE.md`](SEARCH_AND_DERIVATION_PIPELINE.md) | Step-by-step given-clause loop, multi-queue selection, simplification, activation, and derived-clause pipeline. |
| [`DEVELOPMENT_GUIDE.md`](DEVELOPMENT_GUIDE.md) | Practical change patterns, test commands, and review checklists. |
| [`DATA_REPRESENTATION.md`](DATA_REPRESENTATION.md) | Encoded values, records, clauses, terms, vectors, histories, queues, and run state. |
| [`TERM_MATCHING.md`](TERM_MATCHING.md) | Equality, matching, unification, varbanks, varstack, canonical term walking, and subsumption. |
| [`SHARED_MEMORY.md`](SHARED_MEMORY.md) | Persistent KB segments, local query allocation, and read-only ownership rules. |
| [`PERFORMANCE_AND_PARALLELISM.md`](PERFORMANCE_AND_PARALLELISM.md) | `fork()` worker pool, memory arenas, clause lifetimes, hot-path efficiency rules, and profiling. |
| [`STRATEGIES_AND_HEURISTICS_DEV.md`](STRATEGIES_AND_HEURISTICS_DEV.md) | Problem analysis, portfolio scheduling, clause weighting, multi-queue selection, literal selection, ordering, and completeness rules. |
| [`ARITHMETIC_INSTANTIATION.md`](ARITHMETIC_INSTANTIATION.md) | Current bounded numeric-instantiation algorithm, gates, limits, history, memory, and tests. |

Regression inputs belong under [`Test/`](../Test/); only compact teaching
examples belong under [`Examples/`](../Examples/). The arithmetic acceptance
suite is documented in [`Test/arithmetic/README.md`](../Test/arithmetic/README.md).

Documents for GK's confidence/default reasoning and GK-only input languages
belong in `/opt/gk/Doc`, not in this directory.

Working plans and design scratchpads belong under the ignored top-level
`Plans/` directory and are not maintained documentation or Git content.
