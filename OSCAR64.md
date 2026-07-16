# Oscar64 — Learnings

Notes from exploring the [Oscar64](https://github.com/drmortalwombat/oscar64) C/C++
cross-compiler that targets the 6502 family (C64, C128, PET, VIC-20, Plus/4, NES,
Atari 8-bit, Commander X16, Mega 65). Source tree was copied from `~/temp/c64/oscar64`
into `./oscar64`.

## 1. What it is, in one paragraph

Oscar64 is a native (not LLVM, not CC65) C/C++ compiler whose whole reason to
exist is "fast, dense 6502 code from C". The author expected 6502 to be a bad C
target — small register file, no stack-relative addressing, 256-byte hardware
stack, no native 16-bit ops — and tried a bytecode interpreter first. The
interpreter won on size but lost badly on speed, so they switched to native code
generation and discovered that careful analysis actually beats the interpreter on
both axes. The C64 hits 442 dhrystone V2.2 iterations/sec at `-O3`, which is
the kind of number that makes you look twice. The compiler is self-contained
(no external assembler / linker / CRT), supports disk overlays and banked
cartridges for big projects, and has been used to ship multiple real games.

## 2. Repository layout

```
oscar64/
├── oscar64/              # The compiler itself (C++17, ~50 .cpp/.h files)
│   ├── oscar64.cpp       # CLI driver / main()
│   ├── Compiler.{h,cpp}  # Orchestrator: ParseSource() → GenerateCode() → WriteOutputFile()
│   ├── Preprocessor.{h,cpp}        # Macros, #embed, #for, #repeat
│   ├── Scanner.{h,cpp}             # Tokenizer
│   ├── Parser.{h,cpp}              # C99 + big chunks of C++
│   ├── Declaration.{h,cpp}         # AST / symbol tables
│   ├── InterCode.{h,cpp}           # Target-independent IR (the "IC_*" ops)
│   ├── InterCodeGenerator.{h,cpp}  # AST → IR
│   ├── GlobalAnalyzer.{h,cpp}      # Whole-program analysis
│   ├── GlobalOptimizer.{h,cpp}     # Whole-program opts (inlining, const-prop, dead code)
│   ├── Constexpr.{h,cpp}           # Compile-time evaluation
│   ├── ByteCodeGenerator.{h,cpp}   # Optional bytecode backend (-bc)
│   ├── NativeCodeGenerator.{h,cpp} # 6502 native backend (the hot path)
│   ├── NativeCodeOutliner.{h,cpp}  # -Oo: extract repeated code sequences
│   ├── Assembler.{h,cpp}           # 6502 assembler + macros
│   ├── Linker.{h,cpp}              # Region/section/object layout
│   ├── DiskImage.{h,cpp}           # .d64 writer
│   ├── Emulator.{h,cpp}            # Built-in 6502 emulator for -e/-ep
│   ├── Compression.{h,cpp}         # LZO + RLE for #embed
│   ├── Disassembler.{h,cpp}        # For .lbl / .asm output
│   └── ...
├── include/              # Runtime headers + stdlib (what compiled programs #include)
│   ├── crt.c, crt.h      # Target-specific startup (linked unless -rt=)
│   ├── oscar.c, oscar.h  # oscar_expand_lzo / oscar_expand_rle
│   ├── c64/              # VIC, CIA, SID, kernalio, rasterirq, memmap, sprites…
│   ├── c128/, vic20/, plus4/, nes/, cx16/, opp/, audio/
│   ├── gfx/              # bitmap, bitmapmc, vector3d, blob…
│   ├── new/              # C++ new/delete
│   └── stdio.h, stdlib.h, string.h, math.h, conio.h, …
├── samples/              # ~100 worked-example programs
│   ├── stdio/, kernalio/, memmap/, hires/, hiresmc/, particles/, fractals/,
│   ├── rasterirq/, scrolling/, sprites/, games/
│   └── resources/        # .bin / .ctm / .spd assets used by samples
├── autotest/             # Self-test harness
├── CMakeLists.txt        # C++17, globs oscar64/*.cpp
├── make/                 # Makefile-based build (no CMake needed)
├── oscar64.sln / .vcxproj
└── oscar64.md            # The full reference manual (also in README)
```

Build: `make -C make compiler` (uses the makefile). Or `cmake -B build && cmake --build build` — both produce a single `oscar64` binary. Windows is supported via the included `.sln`.

## 3. Compilation pipeline

From `oscar64.cpp` (the driver) the lifecycle is short and explicit:

```cpp
Compiler* compiler = new Compiler();
compiler->ParseSource();       // lex + parse + AST for every translation unit
compiler->GenerateCode();      // analyze, optimize, code-gen, link
compiler->WriteOutputFile(...);// emit .prg / .crt / .bin (+ .map / .asm / .int / .lbl)
```

Inside `Compiler::ParseSource()` (`Compiler.cpp:57`):

1. Choose zero-page register layout from `-tm` (the 6502 has no general-purpose
   regs, so the compiler dedicates a fixed ZP window per target — see
   `BC_REG_*` symbols).
2. Loop: pop a pending `CompilationUnit` from the queue, open the file via
   `Preprocessor`, feed it to `Scanner` → `Parser`. `#pragma compile("foo.c")`
   enqueues additional units, which is how the "no makefile" build model works:
   `#include <stdio.h>` literally drags `stdio.c` into the build.

Inside `Compiler::GenerateCode()` (`Compiler.cpp:550`):

1. Verify the runtime startup (`crt.c`) is present, otherwise error.
2. Materialize default memory regions for the chosen target machine — e.g. for
   a C64 PRG: `zeropage` at `0xf7..0xff` (or `0x80..0xff` with `-xz`), `startup`
   at `0x801..0x880` (native) or `0x801..0x900` (bytecode), and a `main` region
   that holds `code`, `data`, `bss`, `heap`, `stack`.
3. Build the vtables (`BuildVTables`), check `operator new` overrides, finish
   deferred template expansion.
4. Per-procedure: `CompileProcedure()` → `InterCodeGenerator` lowers the AST to
   IC, then for `-n` (native, the default) `NativeCodeGenerator` emits 6502,
   otherwise `ByteCodeGenerator` produces interpreter ops.
5. `NativeCodeOutliner` (when `-Oo`) collapses repeated code sequences.
6. `GlobalOptimizer` runs across the whole program: inlining, constant
   propagation, dead-code removal, value-range narrowing, constant-parameter
   folding, recursion/function-pointer detection, etc.
7. `Linker` lays out objects into regions, produces `.prg` (or `.crt`/`.bin`),
   plus `.map`, `.asm`, `.int`, `.lbl`, and (with `-g`) `.dbj` JSON debug info.

The key takeaway: there is no separate `cc1` / `ld` / `as` — it is one
whole-program compiler. The "linker" is a pass that knows every function and
chooses what to emit.

## 4. How it beats the 6502

The reference manual is upfront about the tricks. The four big ones:

- **Software stack in zero page.** Locals and parameters live in ZP, not on the
  256-byte hardware stack. The C64 default reserves `0xf7..0xff` for this; with
  `-xz` it expands to `0x80..0xff` (and you lose the `STOP/RESTORE` BASIC return).
- **Static call-graph stack.** Because the compiler sees the whole program, it
  analyses the call graph and gives every function a fixed ZP frame offset
  instead of pushing/popping. This is why *recursion* and *function pointers*
  are explicitly called out in the manual as "you will pay for this" — both
  defeat the static analysis and force real stack usage.
- **Zero page as register file.** Globals can be opted into ZP with
  `__zeropage`; the compiler also auto-places small globals into ZP under `-Oz`.
  With `-xz` the wider ZP window gives you ~128 bytes of pseudo-registers.
- **Value-range analysis.** 16-bit ops on the 6502 are *much* more expensive
  than 8-bit. The compiler narrows arithmetic to 8 bits whenever it can prove
  the value fits. Pointers and globals are harder to narrow (they could change
  behind the compiler's back) so the manual hammers the "use 8-bit locals, mark
  things `const`, prefer `unsigned`" advice.

The classic 6502 limitation "no indirect-with-offset" is worked around with
`__striped` arrays — instead of `LHLHLHLH…` you get `LLLLLLLLHHHHHHHH` so
each element can be reached with `abs,X` instead of needing a multiply.

## 5. The C/C++ language surface

C99 is supported in full. C++ mode is selected with `-pp` or a `.cpp` file. The
listed supported C++ features (from `oscar64.md`) are surprisingly complete:

namespaces, references, member functions, ctors/dtors, operator overloading,
single inheritance, `const` methods, `new`/`delete`/`new[]`/`delete[]`, virtual
functions, `string`/`iostream`, default params, templates, vector/array/list,
lambdas, `auto`, range-for, `constexpr`, parameter packs. Not a current
standard, but plenty for a 6502.

## 6. 6502-specific extensions (the fun stuff)

These are the bits that make Oscar feel like "C for the C64" rather than
"hosted C squeezed onto a 6502":

- **`#embed`** — the preprocessor can read a binary file, optionally compress
  it (`lzo` / `rle`), slice an offset+length, and emit 8-bit or 16-bit words.
  It also understands Charpad `.ctm` (chars/tiles/map/attr1/attr2) and
  Spritepad `.spd` (sprites/tiles). This is the standard way to ship charset
  and sprite data in a sample.
- **`#pragma compile("file.c")`** — pulls another translation unit into the
  build. The whole "no makefile" build model rests on this: `#include <stdio.h>`
  ends up `#pragma compile("stdio.c")`-ing the stdio implementation.
- **Storage-class qualifiers:** `__native` (force native codegen for a fn),
  `__zeropage` (place a global in ZP), `__noinline`, `__forceinline`, `__export`
  (force a symbol to be emitted even if unreferenced), `__striped` (the
  layout trick above), `__memmap` (a stronger memory fence than `volatile`,
  used for things like bank registers that re-map the address space).
- **Pragmas:** `region(name,start,end,flags,bank,{sections})` and
  `section(name,…)` reshape the memory map. `align(var, 8)`, `heapsize()`,
  `stacksize()`, `overlay(name, bank)`, `unroll(n|full|page)`, `optimize(…)`,
  `reference(name)`, `intrinsic(name)`, `message(...)`, `charmap(char,code,…)`,
  `callinline()`, `warning(disable:num,…)`.
- **PETSCII and screen-code literals:** `p"hello"` / `P"hello"` makes a
  PETSCII string, `s"hello"` / `S"hello"` makes screen codes. `-psci` flips
  the default for unprefixed strings. `iocharmap(IOCHM_PETSCII_2)` also
  swaps the font.
- **Inline assembler with full C interop:** `__asm { lda c ; bne w1 ; … }`,
  where `c` is a local var (ZP register) and `accu`/`addr`/`sp`/`fp` are the
  fixed return-value and frame-pointer slots. `Type::Member` (e.g. `lda #P::ns`)
  generates a struct member offset. `__interrupt` saves ZP scratch regs; `__hwinterrupt`
  also saves A/X/Y and exits with `rti`. A top-level `__asm name { … }` defines
  a pure-asm routine. The optimizer (`-Oa`, on at `-O2+`) will rewrite your
  inline asm.
- **Hypotheticals:** `__assume(x < 10)`, `__assume(p != nullptr)`,
  `__assume(false)` for `default:` arms — these feed the value-range analysis.
- **Preprocessor loops:** `#assign name expr` and `#repeat … #until cond` let
  you generate unrolled code (e.g. 25 absolute stores to fill a screen
  column). `#for(i, n) text` is the one-liner form.

## 7. Memory model and program layout

The "Region / Section / Object" three-level linker is worth understanding because
almost every advanced sample uses it:

- **Region** = a physical chunk of memory (or a cartridge bank).
  `#pragma region(main, 0x0a00, 0xa000, , , {code, data, bss, heap, stack})`
  reshapes the default map. Flags and an optional bank slot exist for
  cartridge layouts.
- **Section** = a logical bucket (`code`, `data`, `bss`, `heap`, `stack`,
  `zeropage`, or a user-named one). `#pragma section(name, 0, startSym, endSym)`
  declares it; the start/end symbols are emitted so asm can refer to them
  (this is how `crt.c` exposes `StackStart`, `StackEnd`, `BSSStart`, `BSSEnd`,
  `CodeStart`, `CodeEnd`).
- **Object** = one actual blob: a function, a `const` array, a BSS variable,
  a pad, etc. The linker walks the dependency graph from `main` (via the
  startup code in `crt.c`) and only includes reachable objects, which is why
  the whole stdio family can live in a single `.c` without exploding the
  output.

For a default C64 PRG the layout is:
- `0x0801–0x0880` (native) or `0x0801–0x0900` (bytecode) — startup + BASIC stub.
- `0x0900–0x0a00` — bytecode jump table (only when not all native).
- `0x0a00–0xa000` — main, with `code`, `data`, `bss`, `heap`, `stack` sections.

For cartridges, `easyflash` (`-tf=crt`) expands bank 0 into common memory at
startup; generic `crt8`/`crt16` use the `rom` region from bank 0 and put
`bss`/`stack`/`heap` in `main`. The `__bankof(expr)` operator gives you the
bank id of a symbol, so dynamic code can `mmap_set` to the right bank before
calling.

For larger programs the `#pragma overlay(name, bank)` mechanism turns each
cartridge bank into a `.prg` file inside the `.d64` image, and `krnio_load`
brings it in on demand — that's how the games fit.

## 8. Targets and output formats

`-tm` picks the machine: c64, c128, c128b, c128e, plus4, vic20 (+3/+8/+16/+24),
pet (8k/16k/32k), nes (nrom, nrom_h, nrom_v, mmc1, mmc3), atari, x16, mega65.
`-tf` picks the file: `prg`, `crt`, `crt8`, `crt16`, `bin`. `-d64=foo.d64`
builds a disk image and `-f=path` / `-fz=path` add (optionally compressed)
resource files to it; `oscar_expand_lzo` decompresses at runtime, and
`krnio_read_lzo` decompresses on the fly during a kernal read.

## 9. Built-in emulator

`-e` runs the program in a built-in 6502 emulator, `-ep` profiles it. This is
the iteration loop: write C, compile, run, no need to launch VICE for the
common case. (VICE is still the right choice when you need cycle-exact raster
behaviour — see `-moncommands` for loading the `.lbl` symbol file.)

## 10. Useful artifacts the compiler writes

Beyond the main binary, you get (all explained in `oscar64.md`):

- `.map` — regions, sections, objects, objects-by-size. The first place to
  look when your `.prg` is too big.
- `.asm` — full 6502 listing with source references when `-g` is on. This is
  what you cross-reference from inside the VICE monitor.
- `.int` — the intermediate code dump.
- `.lbl` — `al address .symbol` lines for VICE's monitor.
- `.dbj` — JSON with `memory`, `variables`, `functions`, `types`. Combined
  with [Modern VICE PDB Monitor](https://github.com/MihaMarkic/modern-vice-pdb-monitor)
  this gives source-level debugging. The manual recommends `-n -g -O0` for
  the best debug experience.
- `.csz` — every source line annotated with its start address and emitted
  byte count. Great for finding the one loop that ate all your memory.

## 11. Optimization levels (cheat sheet)

```
-O0  no optimization
-O / -O1  default
-O2  + auto-inline, inline-asm optimizer, constant-parameter folding
-O3  + auto-place globals in ZP, outliner
-Os  size
-Oi  auto-inline (sub-flag of O2/O3)
-Oa  inline-asm optimizer (sub-flag of O2/O3)
-Oz  globals → zero page (sub-flag of O3)
-Op  constant-parameter folding
-Oo  outliner (extract repeated sequences)
-Ox  pointer-arith page-boundary awareness
-OM  self-modifying code (saves bytes when SMC is cheap)
```

The mantra in the manual: "Avoid recursion. Avoid function pointers. Be aware
of aliasing. Prefer unsigned. Stick to 8 bits. Prefer enums over #defines. Mark
const. Use `__assume`." Each of those maps directly to a 6502 pain point.

## 12. A minimal end-to-end

From `samples/stdio/helloworld.c` (9 lines, the whole file):

```c
#include <stdio.h>

int main(void)
{
    putchar(14);              // clear screen
    printf(p"Hello World\n"); // p"" → PETSCII string
    return 0;
}
```

Build: `oscar64 helloworld.c` → `helloworld.prg`. Run with `x64 helloworld.prg`
or `oscar64 helloworld.c -e`. The C runtime comes from `include/crt.c` and
`include/stdio.c`, both pulled in automatically by `#include <stdio.h>` (which
in turn `#pragma compile("stdio.c")`s the stdio implementation, which
`#pragma compile("conio.c")`s conio, which is how the whole tree stitches
together with no makefile).

A more interesting example, `samples/hires/lines.c`, shows the full
hardware-programming idiom: `#include <c64/memmap.h>` to bank out BASIC,
`#include <c64/vic.h>` to flip into hires mode, `#include <gfx/bitmap.h>` for
`bm_line`, and the `Bitmap` / `ClipRect` types. Note `char *Hires = (char *)0xe000;`
— absolute addressing is the normal way to reach video memory.

## 13. Things I want to remember when I come back

- It is one binary. No assembler, no linker, no CRT to wire up. `crt.c` is
  pulled in by default; `-rt=foo.c` swaps it, `-rt=` removes it.
- Source files are pulled in by `#pragma compile("x.c")`, not by listing them
  on the command line. The only files on the command line are the user's root
  translation units (and they probably just `#include` headers that do the
  pragma work).
- The CLI is `-option` style. `-i=path` adds an include, `-ii=path` *replaces*
  the default include, `-tf=…`/`-tm=…` set format/machine, `-dNAME` defines a
  macro, `-O3` is the speed preset, `-e` runs the built-in emulator, `-g`
  emits `.dbj` and source lines in `.asm`.
- Native code is the default (`-n` is implicit now). `-bc` opts into bytecode.
- "Region / Section / Object" is the mental model for the linker. Almost every
  non-trivial memory layout in the samples is a `#pragma region(…)`.
- The two escape hatches when the optimizer is wrong: `__assume(cond)` and
  `#pragma optimize(noasm)` (or `__asm volatile { … }` for a single statement).
- Read `oscar64.md` in the repo root — it is the real reference manual and is
  far more thorough than what fits here.

---

# Part II — Programming the C64 with Oscar64

This section combines the Oscar64 architecture notes above with the C64
hardware reference in `../PROG_C64.md` and `../docs/c64/`. The goal is a
practical "how do I write C for the C64 with this compiler and not
shoot myself in the foot" guide.

## 14. The C64 memory model in Oscar64

### 14.1 The default region layout (no `region`/`section` pragmas)

The runtime `crt.c` materializes these regions for a C64 PRG (the
`Compiler::GenerateCode` switch in `Compiler.cpp:550-650` is the
authoritative source):

| Region | Address | Contents |
|--------|---------|----------|
| `startup` | $0801-$0880 (native) | BASIC stub, KERNAL trampoline, init code, `main()` |
| `main` | $0900-$A000 | `code`, `data`, `bss`, `heap`, `stack` sections |

This gives you 38 KB of usable RAM at $0900-$A000 once the KERNAL ISR
trampoline is set up. (For a fresh C64 that's *less* than BASIC sees,
but in exchange you have the entire 38 KB contiguous, no ROM reloads,
and a known startup state.)

Inside the `main` region, the default stack size is 1 KB and the default
heap size is 1 KB. Both are tweakable with `#pragma stacksize(N)` and
`#pragma heapsize(N)`; setting `heapsize(0)` is mandatory if you don't
use `malloc`.

### 14.2 The zeropage software-stack window

Oscar64 dedicates a fixed zero-page window per target machine to its
software stack (parameters, locals, temporaries). For the C64 with
`-O3` the default window is `0xF7..0xFF` (9 bytes: frame pointer, stack
pointer, two scratch pairs, plus a return-address register). With
`-xz` you get `0x80..0xFF` (128 bytes — much more room for locals, but
the KERNAL's `STOP/RESTORE` warm-start vector at $02/$03 stops working,
which is why the Oscar64 docs call it "extended zero page usage").

This means: by default, you cannot freely use $F7-$FF. The Oscar64
runtime uses them, and so does its `mmap_set` trampoline. If you write
inline asm that touches those addresses, you will collide. If you want
more zero-page scratch you have three options:
1. Compile with `-xz` to widen the window.
2. Use `__zeropage` on globals (compiler places them in `$80..$F7` if
   `-Oz` is on, otherwise wherever it fits).
3. Mark the global `__zeropage int x;` and use the same name in
   inline asm — Oscar64 will route both to the same ZP slot.

### 14.3 The bank-switching model

The C64's `mmap_set()` API in `c64/memmap.h` writes to `$01` (the
6510 port) to change which ROMs and I/O are visible. The trampoline
that `mmap_set` installs at $EA31 (the KERNAL IRQ vector location) is
key: when an IRQ fires, the trampoline briefly restores the default
memory configuration, calls the real KERNAL ISR, and then re-applies
your mapping. Without that trampoline, the KERNAL's IRQ handler
(which is at $EA31 in KERNAL ROM) would see whatever banking you've
set up, which will crash because the KERNAL isn't actually there.

Practical implications:
- If you write your own raster IRQ, end with `JMP $EA31` (which goes
  through the trampoline) **not** `JMP kernal_isr` directly, unless
  you have re-implemented the IRQ save/restore yourself.
- `mmap_trampoline()` is called once at startup and must complete
  successfully before you change the memory configuration. It
  allocates 2 bytes somewhere safe (usually in the stack area).

### 14.4 What goes where, end to end

| Memory | Used by | Why |
|--------|---------|-----|
| $00-$01 | 6510 port (always) | Read/write = port bits, not RAM |
| $02-$8F | KERNAL/BASIC scratch | Don't touch in your C code |
| $90-$A2 | KERNAL state (jiffy clock, I/O status) | Use `kernal_*` functions, don't POKE |
| $C0-$CF | KERNAL screen editor | Use conio.h, don't POKE |
| $F7-$FF | Oscar64 ZP software stack | Don't touch in inline asm |
| $0100-$01FF | 6502 hardware stack | Don't store persistent C data here |
| $0400-$07FF | Screen RAM (40×25) | Use `conio.h` for text I/O |
| $0801-$0880 | Startup (your code starts here) | Don't try to call into it |
| $0900-$A000 | Your code, data, bss, heap, stack | Where the linker puts your stuff |
| $A000-$BFFF | BASIC ROM (default) or your RAM (`-O3` banks it out) | Bank-switch with `mmap_set` |
| $C000-$CFFF | Always free for your code | Bank-switch with `mmap_set` |
| $D000-$DFFF | I/O + Color RAM (CHAR ROM if CHAREN=0) | Use the `c64/*.h` headers |
| $D800-$DBFF | Color RAM (low 4 bits only) | `screen.color[i] = c;` style helpers |
| $E000-$FFFF | KERNAL ROM (default) or your RAM | Bank-switch with `mmap_set` |

## 15. How Oscar64 helps — and what it doesn't

This is the section that says "you used to have to do this in assembly,
now you can do it in C, but you still have to know about the hardware
because the compiler won't do the unsafe thing for you."

### 15.1 The wins

- **Replacing hand-rolled 6502 ASM.** Things like the dhrystone
  benchmark at 442 iterations/sec (cited in the Oscar64 README) used
  to require an expert ASM programmer. With Oscar64 you can express
  the same logic in C, write `__assume` hints, and let the optimizer
  produce near-optimal code. The oscar64 manual has a worked example
  where a `Plot()` function written in C and marked `__native` ends
  up exactly as good as hand-written ASM.

- **No more "this is a global, so it's worse" pessimization.** Static
  call-graph analysis means Oscar64 gives every function a fixed ZP
  frame offset for its parameters and locals. You can pass ints to
  functions and Oscar64 will try to use absolute addressing when it
  can prove the value is in the ZP-resident static frame.

- **Striped arrays for the 6502 no-indirect-with-offset problem.** The
  `__striped` qualifier is the killer feature for hires graphics. A
  320-byte bitmap with `__striped` becomes `LLLLLLLLHHHHHHHH` so the
  hot inner loop compiles to `LDA $xxxx,Y` instead of needing a
  multiply.

- **PETSCII/screen-code literals.** `printf(p"Hello\n")` and the
  `p""` / `s""` prefixes, plus `iocharmap(IOCHM_PETSCII_2)`, mean you
  don't have to write a translation table for every string. The
  compiler emits the correct byte sequence for the active character
  set at compile time.

- **Inline asm with full C interop.** When you do need to drop to
  ASM, you can write `__asm { lda c ; bne w1 ; … }` and the
  compiler will substitute `c` with its ZP location, set up the
  right addressing mode, and let the optimizer clean up afterwards
  (when `-Oa` is on, which it is for `-O2+`).

- **Compile-time `#embed` for binary data.** No more `FOR I=0 TO 62:
  READ B: POKE 12288+I,B: NEXT I` in BASIC. The `#embed` directive
  pulls in a `.bin`, `.ctm`, or `.spd` file at compile time, with
  optional LZO or RLE compression, and emits the bytes as a `const`
  array you can reference directly.

- **`#pragma compile("foo.c")` means no makefile.** The entire stdio,
  conio, gfx, and c64/ families of headers pull in their
  implementations this way. You write one `main.c`, `#include
  <stdio.h>`, and the compiler drags in `stdio.c` and `conio.c` and
  `crt.c` automatically.

- **Whole-program dead-code elimination.** `printf("hello\n")`
  pulls in only the code path that formats a string and calls
  `CHROUT`. The floating-point formatter, the file I/O, the scanf
  family — all of it disappears unless you actually use it. With
  `-dNOFLOAT` you can also strip float support from printf entirely.

- **Automatic zero-page placement under `-Oz`.** Globals and small
  arrays that the optimizer can prove are hot get moved into ZP at
  `-O3 + -Oz`. You don't have to manually mark every variable; the
  compiler does it for you.

- **Disk overlays via `-tf=d64`.** Banked code becomes a `.prg` per
  bank inside the disk image. You can write your C code as if it
  were all linked, and the compiler figures out which bank each
  function goes into.

- **The built-in emulator (`-e`, `-ep`).** Edit, compile, run, watch
  the screen. No need to launch VICE for the iteration loop. (VICE
  is still the right choice for cycle-exact raster work.)

### 15.2 The "you still need to know the hardware" cases

- **The badline penalty.** A `for` loop that runs across a badline
  boundary will take 40 extra cycles on that one line. The compiler
  doesn't know about badlines. If you have a tight loop that needs
  to be cycle-stable, put it all inside one raster line (a "stable
  raster" loop, commonly on line 311 on PAL) or you'll see glitches.
  Workaround: use a raster IRQ to start the work right after a known
  stable line.

- **VIC bank bits.** The VIC only has a 14-bit address bus. The
  other 2 bits come from CIA 2 PRA. Oscar64's `screen` / `bitmap`
  helpers manage this for you, but if you write inline asm that
  reads video memory at a literal address, you have to make sure
  the right bank is selected. The right thing is to set up a
  `char * const Hires = (char *)0xe000;` in C — Oscar64 will emit
  the right bank-switch dance for you the first time you read or
  write through that pointer (or at startup, depending on the
  helper).

- **Color RAM upper nibble.** The compiler can't help you if you
  `*((char *)0xd800) = 0xFF;` and expect the high nibble to be 0.
  It won't be. Use `screen.color[i] = c;` or `POKE_COLOR(0xd800, i,
  c)` style helpers instead.

- **Sprite pointers need 64-byte alignment.** Oscar64's `Sprite` type
  in `c64/sprites.h` handles this for you (it pads the structure to
  64 bytes), but if you declare `char my_sprite[63]` and `POKE 2040,
  (int)my_sprite/64;`, you'll crash if `my_sprite` isn't aligned.
  Use the helpers.

- **PETSCII vs ASCII.** Writing `"Hello\n"` in C gives you ASCII
  bytes 48, 65, 6C, 6C, 6F, 0A. The C64's character ROM has `H` at
  PETSCII 8, not 65, and uses CR (0x0D) instead of LF (0x0A). Use
  the `p""` prefix in Oscar64 or the compiler flag `-psci`, or your
  text will look like garbage.

- **KERNAL save/restore around the trampoline.** If you call into
  the KERNAL directly (e.g. `JSR $FFD2` to print a character), the
  trampoline will fix up the memory configuration for you. If you
  jump to a KERNAL function in the middle (e.g. setting up your own
  routine that ends with `JMP $EA81`), the KERNAL will read its own
  data at $EA31 and find your trampoline instead — which is fine if
  the trampoline is installed, but if you've done a `mmap_set` that
  banks out KERNAL, you'll crash. The fix is to call
  `mmap_set(MMAP_ROM)` first, *then* do your JMP.

- **Cycle-counting is your job.** Oscar64 will tell you the cycle
  count of the code it generated (look at the `.asm` file), but it
  won't tell you that the work happens during a badline and is
  therefore going to be 40 cycles slower. If you're trying to fit
  code in a specific raster window, you still need to read the
  Christian Bauer VIC article.

- **The CIA 1 IRQ is the system's IRQ.** If you set up a raster IRQ
  and forget to mask CIA 1's timer A in `$DC0D`, you'll get a
  nested IRQ every jiffy (1/60 sec) and your raster code will
  glitch. The Oscar64 `rasterirq.h` library handles this for you;
  the pattern is always:
  ```c
  SEI;
  POKE(0xDC0D, 0x7F);  // mask CIA 1 IRQs
  POKE(0xDD0D, 0x7F);  // mask CIA 2 IRQs
  POKE(0xD01A, 0x01);  // enable VIC raster IRQ
  CLI;
  ```

- **Self-modifying code is sometimes required.** The 6502 has no
  indirect-with-offset addressing for arrays, so a "load a byte from
  a C array" can take more cycles than a "load a byte from a known
  constant address". Sometimes the right answer is `__asm { lda
  my_const+1 }` after the compiler has put `my_const` in a known
  place. Oscar64 supports this with `#pragma optimize(asm)` to
  allow the optimizer to clean it up, or `__asm volatile { … }` if
  you want it untouched.

## 16. Oscar64 patterns for the C64 (a tour)

### 16.1 Hires frame setup, the idiomatic way

```c
#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>

char Color[1000];
char Hires[8000];
Bitmap Screen;

int main(void) {
    mmap_trampoline();
    mmap_set(MMAP_RAM);              // bank out BASIC + KERNAL → 38 KB free
    mmap_set(MMAP_NO_ROM);           // also bank out CHAR ROM at $D000

    memset(Color, 1, 1000);          // white on black
    memset(Hires, 0, 8000);          // clear bitmap
    vic_setmode(VICM_HIRES, Color, Hires);
    vic.color_border = VCOL_WHITE;
    bm_init(&Screen, Hires, 40, 25);

    // ... draw stuff ...
    getch();

    mmap_set(MMAP_ROM);              // restore for clean exit
    vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);
    return 0;
}
```

Compare to the equivalent in raw ASM:
- Set `$01` to `$34` (bank out BASIC+KERNAL+CHAREN).
- Wait for a stable raster line.
- Write `$D011` for ECM=BMM=0, DEN=1, RSEL=1.
- Write `$D016` for MCM=0, CSEL=1.
- Write `$D018` to set the screen memory and bitmap base.
- Set CIA 2 PRA bits 0-1 to the right VIC bank.
- Set up VIC bank by writing to `$DD00`.
- Clear 8 KB of bitmap and 1 KB of color RAM.
- ... draw ...
- Reverse everything.

The C version is 5 lines of setup. The ASM version is 30+.

### 16.2 Sprites the right way

```c
#include <c64/sprites.h>

// Sprite data is 64 bytes (63 + 1 pad) per sprite
const char MySprite[64] = { 0, 126, 0, 3, 255, 192, /* ... */ };

int main(void) {
    // ... init ...
    spr_init(SPRITES_BASE);                  // sets up sprite pointers
    spr_set(0, MySprite, 100, 100, VCOL_WHITE);
    spr_show(0, true);
    // ...
}
```

In `c64/sprites.h`, `spr_set` does the math (`(int)MySprite >> 6` for
the pointer, separate X/Y writes, color register write). The
`Sprite[8]` and `SpritePointer[8]` types handle the "where do I put
sprite data" question (must be 64-byte aligned in the current VIC
bank; the helper reserves a 512-byte region and aligns each sprite
inside it).

### 16.3 Raster IRQ, the right way

```c
#include <c64/rasterirq.h>

RIRQCode screenRIRQ;

void __interrupt screen_handler(void) {
    // ... do per-line work ...
    rasterirq_next(&screenRIRQ, line + 8);  // schedule next IRQ
}

int main(void) {
    // ...
    rasterirq_init();
    screenRIRQ.handler = screen_handler;
    rasterirq_setup(&screenRIRQ, 100, 0);   // line 100, no change
    rasterirq_enable(&screenRIRQ);
    rasterirq_sort();                       // sort all active RIRQs
    // ... main loop ...
}
```

This wraps all the `SEI/CLI/POKE $D01A/$0314/$0315` dance and also
installs a KERNAL trampoline that preserves the jiffy clock and
keyboard scan. Without `rasterirq.h` you'd write 30+ lines of
assembly and 2 separate hand-managed chains of ISRs.

### 16.4 The KERNAL ISR trick

If you want to keep the KERNAL's jiffy clock, keyboard scan, and
cursor blink going while running your own raster code, end your ISR
with `JMP $EA31` (not $EA81). The KERNAL ISR at $EA31 will run after
yours and do the standard work. The Oscar64 `rasterirq.h` library
uses this approach for all its "system-friendly" RIRQs.

If you want the IRQ to be as fast as possible and don't care about
the jiffy clock, end with `JMP $EA81` (the register-restore stub).
The KERNAL still restores A/X/Y and RTIs, but doesn't do the
service work.

If you want the IRQ to do absolutely nothing but return (e.g. a
timer-based music player that runs as fast as possible), you can
write your own `RTI` and skip the KERNAL entirely. But you must
preserve the stack layout (PC_hi, PC_lo, P, A, X, Y from top to
bottom) so the RTI returns to the right place.

### 16.5 The "what about the 6510 port?" question

The C64's bank switching is just `*((char *)0x01) = value;`. The
Oscar64 `mmap_set` API wraps this but also installs the trampoline.
If you need to do it yourself (rare), the pattern is:

```c
__asm {
    sei
    lda #0x34
    sta $01        ; bank out BASIC + KERNAL
    jsr $02        ; call user code (at $02-$03 trampoline slot)
    lda #0x37
    sta $01        ; restore default
    cli
}
```

The `jsr $02` is the key. It's not a real "jsr"; it's a trampoline
slot that the user can install code at. The mmap library uses this
to call user code with the ROMs banked out.

### 16.6 Using `__assume` for the C64

The Oscar64 optimizer is value-range-aware. It will use 8-bit
operations whenever it can prove the value fits in 8 bits. The
classic case on the C64 is VIC register writes: $D011, $D016, $D018
all only need the low 5-8 bits, so an `int` value with the high bits
clobbered is wasteful.

```c
void set_scroll(char x, char y) {
    // x is 0-7, y is 0-7 — known at the call site
    __assume(x < 8);
    __assume(y < 8);
    *((char *)0xD016) = (*((char *)0xD016) & 0xF8) | x;
    *((char *)0xD011) = (*((char *)0xD011) & 0xF8) | y;
}
```

The `__assume`s tell the compiler that `x` and `y` fit in 3 bits,
so it can AND with 7 instead of $F8 and then OR, saving 4 cycles
per call. For a function called in a raster IRQ this matters.

### 16.7 `__striped` for the bitmap

The classic demo-scene optimization. Without `__striped`, accessing
`bitmap[y][x]` (a `char [200][40]`) needs a multiply-by-40 to get the
byte offset. With `__striped`, the compiler splits the array into
separate per-row arrays that are 40 bytes wide each, and uses `LDA
$xxxx,X` (no multiply) to read them.

```c
__striped char hires[8000];       // interleaved as 8 1000-byte stripes
                                  // wait, this isn't quite right; see below
```

Actually for `Bitmap` the trick is more subtle. The Oscar64
`gfx/bitmap.h` library uses `__striped` internally to get good code
for the per-pixel access. The `bm_line` and `bm_fill` functions are
written in a way that lets the compiler emit very tight code for the
common case of "plot a pixel in a 320×200 bitmap".

## 17. C64-specific optimization notes

### 17.1 Use `unsigned char` for VIC coordinates

A sprite X position is 0-344 (with bit 8 in $D010). A sprite Y
position is 0-255. Color register values are 0-15. Background color
values are 0-15. The Oscar64 optimizer will use 8-bit operations
when it can prove the value fits in 8 bits; if you use `int` for a
"the value is always 0-15" variable, the optimizer may not be able
to prove it, and you'll get 16-bit code for a 4-bit value.

### 17.2 Use `const` aggressively

A `const` global is read-only, so the optimizer can keep it in the
char ROM or in a register, not in RAM. A `const` array of 256
bytes (like a color lookup table) can be in the char ROM at no cost.
A non-const array of 256 bytes costs 256 bytes of RAM.

### 17.3 Use `enum` instead of `#define` for register values

```c
enum VICMode {
    VICM_TEXT      = 0x00,
    VICM_HIRES     = 0x20,
    VICM_MULTI     = 0x10,
    VICM_EXTENDED  = 0x40,
    VICM_BITMAP    = 0x20,
    // ...
};
```

vs

```c
#define VICM_HIRES 0x20
```

The `enum` version lets the optimizer prove that `mode` is a small
value, so the write to $D011 can be an 8-bit operation. The
`#define` version is a preprocessor substitution, so the optimizer
sees `int mode = 0x20;` and can't narrow it.

### 17.4 Use `__forceinline` for tiny helpers in hot paths

If you have a `bm_pixel()` function that's called millions of times
per frame, mark it `__forceinline` (or at least `inline`). The
`__striped` access inside will then compile to a single `LDA
$xxxx,X` instruction in the caller's body.

### 17.5 Use `__noinline` for cold paths

Conversely, if a function is only called in a startup path or an
error path, mark it `__noinline`. The optimizer will keep it
out-of-line, saving code size in the hot paths.

### 17.6 `__hwinterrupt` for ISRs

```c
__hwinterrupt void my_isr(void) {
    // saves A/X/Y, your code, then RTI
    vic.color_border ^= 1;
}
```

`__hwinterrupt` saves the CPU registers for you, runs your code,
then exits with `RTI`. This is the right annotation for an ISR that
takes over the IRQ line completely (e.g. a music player that
disables the KERNAL ISR).

`__interrupt` saves just the zero-page registers; you'd use this
for a function that calls back into C and needs the ZP frame to be
preserved but doesn't touch the CPU registers.

For most C64 work, `__hwinterrupt` is what you want. The Oscar64
runtime's `rasterirq.h` uses it.

### 17.7 `#pragma optimize(noasm)` for fragile inline asm

If you have an inline asm snippet that you're absolutely sure is
cycle-perfect and you don't want the optimizer to touch it, wrap
it in `#pragma optimize(noasm) ... #pragma optimize(asm)` or use
`__asm volatile { ... }`. The Oscar64 `-Oa` flag (on by default at
`-O2+`) will rewrite your inline asm — usually correctly, but
occasionally in surprising ways.

## 18. The "common beginner mistakes" list

These are mistakes that the C64 + Oscar64 combination makes easy and
that cost you a frame or a crash.

1. **Forgetting `mmap_trampoline()`.** Symptom: works in the
   emulator, crashes on real hardware. Cause: the KERNAL ISR doesn't
   see the KERNAL ROM and tries to execute garbage.

2. **Forgetting to mask CIA 1 in a raster IRQ.** Symptom: works
   most of the time, but every 1/60 sec the IRQ fires twice
   (once for your raster, once for the jiffy clock) and the second
   one is the KERNAL ISR. Cause: the CIA 1 timer A IRQ is enabled
   by default. Fix: `POKE(0xDC0D, 0x7F)` at the start of your IRQ
   setup.

3. **Reading from `*((char *)0xD800)` and expecting a meaningful
   upper nibble.** Symptom: weird color values. Cause: only the
   lower 4 bits are connected.

4. **Writing `printf("Hello\n")` and getting garbage.** Symptom:
   text doesn't display correctly. Cause: PETSCII vs ASCII. Fix:
   use `p""` prefix or `-psci` flag.

5. **Setting a sprite pointer to an unaligned address.** Symptom:
   the sprite shows as garbage or as the wrong sprite. Cause: the
   pointer value is the pattern address divided by 64, so the
   pattern must be 64-byte aligned. Fix: use the `Sprite` /
   `spr_set` helpers from `c64/sprites.h`.

6. **Calling `mmap_set` from inside an ISR.** Symptom: crash.
   Cause: `mmap_set` modifies $01, and if the KERNAL trampoline
   isn't installed yet, or if you re-enter the trampoline, things
   go wrong. Fix: do all your mmap changes at startup, not in an
   ISR.

7. **Forgetting to restore the IRQ vector.** Symptom: when your
   program exits, the machine hangs or crashes. Cause: the IRQ
   vector at $0314 still points to your ISR, which is no longer
   there. Fix: `POKE(0x0314, 0x31); POKE(0x0315, 0xEA);` before
   exit (the default KERNAL vector).

8. **Using `char` and expecting it to be 8 bits unsigned.** It's
   not — C `char` is signed by default on the C64 with the
   `signed char` type and `unsigned char` for unsigned. For VIC
   color values, sprite coordinates, and bitmap data, use
   `unsigned char` explicitly. The `oscar64` `byte` typedef is
   `unsigned char`.

9. **Forgetting `-rt=` when you want a no-runtime build.** The
   default `crt.c` includes BASIC + KERNAL. If you want a
   self-contained 8 KB binary that boots without the KERNAL,
   pass `-rt=my_crt.c` (or `-rt=` for no runtime at all and you
   supply every byte yourself).

10. **Using `malloc` without checking the heap size.** Default
    heap is 1 KB. If you allocate more than that, you corrupt
    the stack. Fix: `#pragma heapsize(N)` to set the heap, or
    use a memory pool you manage yourself.

## 19. The mental checklist before you ship

For a C64 release, the things I want to verify:

- [ ] Does it boot on real hardware? (Emulators are forgiving; the
      real chip is not.)
- [ ] Does the raster IRQ work at NTSC frame rate too? (Many C64
      demos were PAL-only. PAL has 312 lines, NTSC has 263.)
- [ ] Does it survive RESTORE? (RUN/STOP+RESTORE should return to
      BASIC, not hang.)
- [ ] Does it survive a disk change? (The serial bus IRQ will fire
      when the drive door is toggled.)
- [ ] Does the .prg file size match what the user can load? (A
      stock C64 with a stock 1541 can load 202 blocks max; 1
      block = 254 bytes, so ~50 KB is the limit before you need
      a fast loader or multiple disk sides.)
- [ ] Does the code size fit in $0900-$A000? (38 KB is the
      default, but your code + data + bss + heap + stack all have
      to fit.)
- [ ] If you used `mmap_set(MMAP_RAM)`, did you `mmap_set(MMAP_ROM)`
      before exiting? (Otherwise the next program loaded sees a
      weird memory state.)
- [ ] If you wrote your own ISR, did you ack the IRQ in `$D019`?
      (Otherwise the IRQ fires forever.)
- [ ] Did you mask the CIAs you don't use? (An unmasked CIA 1
      timer A will fire every jiffy, even if you don't care.)
- [ ] Are the sprites' 64-byte alignment OK? (Use the helpers,
      or check with `assert(((int)my_sprite & 63) == 0);`)
- [ ] Are the color values 0-15? (The low 4 bits only. The high
      4 bits of $D021-$D02E are unused.)
- [ ] Does the code work with -O0, -O1, -O2, and -O3? (Each level
      may surface a different bug; -O0 hides aliasing problems
      that -O3 exploits.)
- [ ] If you used `__assume`, is it true? (A wrong `__assume` is a
      silent bug. Test it at -O0 too.)

The Oscar64 compiler gives you an enormous productivity boost over raw
assembly for the C64 — you can write 10x as much code in the same
time, and most of it will be near-optimal out of the box. The cost
is that you have to understand the hardware enough to write the
right C, and the Oscar64 runtime headers and pragmas give you most
of what you need. The remaining 10% — the cycle-perfect raster
tricks, the badline-aware work, the disk-overlayed banked code —
still requires reading the Christian Bauer VIC article and the
comprehensive reference in `../PROG_C64.md`.
</content>
</invoke>