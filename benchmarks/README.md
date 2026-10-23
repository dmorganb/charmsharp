# Charm# comparison benchmarks

This directory collects four **unaltered Charm++ reference implementations** for the thesis experiment. The goal is to compare the same parallel algorithms in standard Charm++ and Charm#, first using source-level metrics and later (optionally) participant implementations.

## Benchmark matrix

| Benchmark | Difficulty | Upstream example | Primary Charm++ features |
| --- | --- | --- | --- |
| [primes](primes/charmpp/) | Basic | `examples/primes` | Mainchare, ordinary chares, asynchronous entry methods, completion callbacks |
| [reduction](reduction/charmpp/) | Basic | `examples/reduction` | 1D chare array, reduction target, sum reduction |
| [prefix-sum](prefix-sum/charmpp/) | Advanced | `examples/prefix_sdag` | 1D chare array, readonly, SDAG `when`, tagged iterations |
| [jacobi2d](jacobi2d/charmpp/) | Advanced | `examples/stencil2d` | 2D chare array, halo exchange, convergence reduction |

## Provenance

All 12 files under `*/charmpp/` are copied unchanged from:

- Repository: https://github.com/charmplusplus/charm-tutorial
- Upstream revision: [`bde0b63c372dab4e07af79c193cff240d87f6a6d`](https://github.com/charmplusplus/charm-tutorial/tree/bde0b63c372dab4e07af79c193cff240d87f6a6d/examples)
- Upstream directories: `examples/primes/`, `examples/reduction/`, `examples/prefix_sdag/`, and `examples/stencil2d/`

The `jacobi2d` benchmark retains the upstream executable and source basename `stencil2d`. This is the five-point iterative stencil used for Jacobi-style relaxation, not an independently reimplemented solver.

**Keep the originals unchanged.** Any local correctness fixes or experiment-specific modifications should be separately recorded so the upstream reference stays auditable. The upstream tutorial repository did not contain a root `LICENSE` file at the revision inspected; provenance is documented here, but licensing should be confirmed before redistribution outside this repository.

## Build and run

Requires a built Charm++ toolchain. Set `CHARM_HOME` to the **platform build directory** containing `bin/charmc` (not the Charm++ source root). For example:

```sh
export CHARM_HOME=/path/to/charm/netlrts-linux-x86_64
cd benchmarks/primes/charmpp
make
./charmrun ++local +p4 ./primes 20
```

Run the other benchmarks from their respective `charmpp` directories:

```sh
# reduction: sum of the indices [0, n)
./charmrun ++local +p4 ./reduction 10
# expected: result = 45

# prefix-sum: each element checks its inclusive prefix value
./charmrun ++local +p4 ./prefix 8
# expected: every element reports OK

# jacobi2d: grid side, tile side, convergence threshold
./charmrun ++local +p4 ./stencil2d 64 8 0.01
# expected: iterative updates followed by convergence
```

The upstream Makefiles also contain `make test` targets. Some targets use `++ppn` and expect `./charmrun`; their operation may depend on your Charm++ installation and launcher configuration. These files have intentionally not been edited.

## Planned comparison layout

Each benchmark will eventually have:

- `charmpp/`: this pinned upstream implementation.
- `charmsharp/`: a semantically equivalent Charm# implementation, once the compiler supports the necessary features.
- `tests/`: shared input fixtures and automated validation for equivalent observable behavior.

Phase 4.0 adds the [Primes Charm# port](primes/charmsharp/primes.csh) and a runtime comparison test. Phase 4.1 adds the [Reduction Charm# port](reduction/charmsharp/reduction.csh) and a runtime comparison test. Prefix Sum and Jacobi2D remain unported reference inputs.

## Measurement notes

- For Halstead analysis, count the handwritten `.ci` and `.C` sources together for Charm++; exclude generated `.decl.h`/`.def.h`, Makefiles, and runtime code.
- Define the operator/operand token classification before evaluating either language, and use the same policy for every program.
- Compare only semantically equivalent programs. In particular, the prefix-sum reference already uses SDAG; it should not be compared against manual message-state machinery when attributing differences to Charm#.
- Record toolchain version, input parameters, PE count, architecture, output validation, and repeated runtime measurements.
- Program runtime is distinct from participant implementation time. They address different experimental questions.

## Current status

**Phase 4.0:** Primes is ported to Charm#; GitHub Actions builds the unchanged Charm++ reference and generated Charm# version against the same cached Charm++ v8.0.0 toolchain, runs both with 2 PEs and K=5, and compares all five ordered results. Run locally with `make charm-benchmark-primes` when `charmc` is on PATH. **Phase 4.1:** Reduction is ported using `[[charm::reductiontarget]]`; CI compiles both implementations with the same toolchain, runs at +p2 with 5 and 10 array elements, checks their expected sums, and compares their reported outputs. Run locally with `make charm-benchmark-reduction`. Runtime equivalence for Prefix Sum and Jacobi2D remains pending.
