# Project 2 — Compiler Register Allocation

Tool that maps program variables to physical registers using graph coloring heuristics, operating on a 3-address instruction intermediate representation (IR).

---

## Build

```bash
make
```

Requires `g++` with C++17 support. Produces the `myProg` executable.

```bash
make clean   # remove compiled files
```

---

## Running the Program

The program supports two execution modes.

### Batch Mode

```bash
./myProg -b <ranges_file> <config_file> <output_file>
```

**Example:**
```bash
./myProg -b examples/ranges_basic.txt examples/config_basic.txt output.txt
```

Runs the full pipeline non-interactively. Error messages go to `stderr`, the result is written to `<output_file>`.

---

### Interactive Mode

```bash
./myProg
```

Launches a menu-driven interface:

```
=== Register Allocator ===
  1) Load ranges file
  2) Load config file
  3) Show webs
  4) Show interference graph
  5) Run allocation
  6) Save output to file
  0) Exit
```

**Typical session:**

```
Choice: 1
Ranges file path: examples/ranges_basic.txt
Loaded 3 web(s).

Choice: 2
Config file path: examples/config_basic.txt
Config loaded: 2 register(s), algorithm=basic

Choice: 3        ← inspect the webs built from the ranges
Choice: 4        ← inspect the interference graph
Choice: 5        ← run the allocation and print result
Choice: 6        ← save result to a file
Choice: 0        ← exit
```

> Options 3, 4, 5 require ranges to be loaded first (option 1).
> Option 5 also requires config to be loaded (option 2).
> Option 6 requires a result to exist (option 5 run first).

---

## Input File Formats

### Ranges File

One live range per line. Comments start with `#`.

```
# variable_name: definition+, intermediate_lines..., last_use-
sum: 7+,8,9,10-
i:   1+,2,3,4,5,6-
i:   9+,10,11,12-
```

- The line marked `+` is where the variable is **defined** (born).
- The line marked `-` is the **last use** (dies).
- All other lines are intermediate — the variable is alive but neither born nor dying.
- Multiple entries for the same variable are allowed. If their line ranges overlap, they are automatically **merged into one web**.

---

### Config File

```
# number of physical registers available
registers: 2

# allocation algorithm
algorithm: basic
```

**Available algorithms:**

| Value | Description |
|---|---|
| `basic` | Pure graph coloring. Fails hard if any web cannot be colored. |
| `spilling, K` | Allows up to K webs to be spilled to memory. |
| `splitting, K` | Splits up to K webs at their midpoint to reduce interference. |
| `free` | DSATUR-based coloring — colors the most constrained web first. |

---

## Output File Format

```
webs: 3
web0: 1+,2,3,4,5,6-
web1: 9+,10,11,12-
web2: 7+,8,9,10-
registers: 2
r0: web0
r0: web2
r1: web1
```

- Each `web` line shows the full live range(s) that form the web.
- Multiple webs on the same register (e.g., `r0: web0` and `r0: web2`) means those webs do not interfere — they are alive at different times.
- Webs that cannot be assigned a register are spilled to memory:

```
M: web3
```

- If allocation fails completely (no web can be colored), the output is:

```
registers: 0
M: web0
M: web1
...
```

---

## Example Files

The `examples/` directory contains ready-to-use input files:

| File | Description |
|---|---|
| `ranges_basic.txt` | Two variables, non-overlapping ranges |
| `ranges_tight.txt` | Four mutually interfering webs (4-clique) |
| `ranges_overlap.txt` | Overlapping ranges that merge into one web |
| `config_basic.txt` | 2 registers, basic algorithm |
| `config_basic_fail.txt` | 2 registers, basic — fails on 4-clique |
| `config_spilling.txt` | 1 register, spilling with K=1 |
| `config_splitting.txt` | 2 registers, splitting with K=2 |
| `config_free.txt` | 2 registers, DSATUR |

**Quick test:**
```bash
# Should succeed
./myProg -b examples/ranges_basic.txt examples/config_basic.txt out.txt && cat out.txt

# Should spill 1 web to memory
./myProg -b examples/ranges_basic.txt examples/config_spilling.txt out.txt && cat out.txt

# Should fail (4-clique needs 4 registers, only 2 available)
./myProg -b examples/ranges_tight.txt examples/config_basic_fail.txt out.txt 2>&1 && cat out.txt
```

---

## Project Structure

```
Project 2/
├── main.cpp          — entry point, CLI and interactive menu
├── Parser.h/.cpp     — parses ranges and config input files
├── Graph.h/.cpp      — Web construction, interference graph
├── Allocator.h/.cpp  — register allocation algorithms
├── Makefile
└── examples/         — sample input files
```

---

## Algorithms — Quick Reference

### Basic
Greedy graph coloring using Chaitin-Briggs simplification. Removes nodes with degree < N onto a stack, then colors them in reverse order using the lowest available register. Fails if any node cannot be simplified without spilling.

### Spilling (K)
Same as basic, but when all remaining nodes have degree ≥ N, the program selects the best candidate to spill to memory (heuristic: `degree × range_length`) and continues. Allows up to K such spills.

### Splitting (K)
Attempts basic coloring; if it fails, splits the most problematic web at its median line into two independent halves, rebuilds the interference graph, and retries. Repeats up to K times.

### Free (DSATUR)
At each step, colors the uncolored web with the highest **saturation** (number of distinct colors already present among its neighbors). Ties broken by degree. Typically produces better colorings than basic greedy ordering.
