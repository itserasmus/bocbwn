# BOCBWN - Brainfuck Optimizing Compiler Because Why Not

BOCBWN is an optimizing compiler for brainfuck, one of the first esoteric programming languages,
which despite its minimal syntax, [is turing complete](https://en.wikipedia.org/wiki/Brainfuck).

BOCBWN comes with a (work in progress) proof of correctness of all its parts*, in `proofs/`!

For usage, skip directly to [BOCBWN Usage](#bocbwn-usage)


## Brainfuck Specification

Brainfuck has 8 characters (although can still be Turing complete with just 5 of them). There is a
bi-infinite tape of integers initially set to zero (or to the input of the program), and a memory
pointer which starts at an unspecified cell. The integers are unsigned integers with wraparound, and
their size is left implementation-defined. The cell pointed to by the memory pointer is hereafter
referred to as the active cell.

- `[`: If the active cell is zero, jump to the corresponding `]`, else continue normal execution.
- `]`: If the active cell is nonzero, jump to the corresponding `[`, else continue normal execution
- `+`: Increments the active cell.
- `-`: Decrements the active cell.
- `>`: Moves the memory pointer to the right.
- `<`: Moves the memory pointer to the left.
- `,`: Takes an input from stdin to the cell, the specifics of which are implementation-defined.
- `.`: Outputs the cell to stdout, the specifics of which are also implementation-defined.

Turing completeness does not require input or output, and `-` can be replaced with `+`^(n-1), which
means that the subset `[]+><` is sufficient for Turing completeness. If we allow the tape to have a
size determined by the program but known beforehand, we can further omit `<`, meaning that `[]+>`
is, under this constraint, also sufficient for Turing completeness.



## BOCBWN Specification

Brainfuck leaves the size of the cell and the behavior of `,` and `.` as implementation-defined. In
this implementation, a cell is 8 bits, represented as a `uint8_t` or stored in byte registers. To
improve pipelining behavior on modern CPUs, in which byte (`uint8_t`) registers can cause
unnecessary false dependencies and stalling, the compiler frequently uses dword (`uint32_t`)
registers instead of byte registers. However, this usage is sufficiently constrained that the
behavior it produces is identical to if byte registers had been used. The implementation of `,` and
`.` is delegated further to the C functions getchar and putchar.

A truly bi-infinite tape is, due to unfortunate physical constraints imposed by the universe upon
us poor, innocent, human beings, impossible. The brainfuck specification recognizes this failure of
physical systems, and thus, permits a finite tape, with overflow behavior being implementation-
defined. In this implementation, overflow and underflow behavior is wraparound, with the option of
disabling wraparound for quite significant performance gains and, if you're lucky, causing a
SIGSEGV on overflow or underflow.

The transpiler uses `int32_t`s to represent indices, so if a program creates more than 2 billion
instructions, it will overflow and possibly cause the program to be parsed wrong. This behavior can
be easily changed by modifying the source code.



## BOCBWN Workings

BOCBWN has a two-stage compilation process. The first stage, the transpiler, involves converting
brainfuck code into an immediate representation (IR), which is converted to an abstract syntax tree
(AST), which is then optimized and converted back into a linear IR. The second stage, the compiler,
involves converting the linear IR into an assembly program, which can be linked to the runtime
(`bf_asm.cpp`) using any standard C++ compiler.

The transpiler applies the following steps, in this order:

- Conversion to IR: First and foremost, the operations `[]+-><,.` are converted to an IR, where
these symbols are mapped to `BRZ p`, `BRNZ p`, `ADD 1`, `ADD -1`, `MOV 1`, `MOV -1`, `IN`, and
`OUT` respectively.
- `>`, `<`, `+`, `-` Compaction: Long runs of these symbols, such as `>>>>`, are converted into a
single instruction (here, `MOV 4`).
- Conversion to an AST: the linear IR is now converted to an AST to enable further optimizations.
- TALS: This step replaces some simple loop types, such as `[-]` and `[->+>+<<]`, with more
efficient identical transformations, in this case, `SET 0` and
`PUTA, SET 0, MOV 1, ACCUMA, MOV 1, ACCUMA, MOV -2`. This step uses an auxiliary register, `A`.
- Main Pass: In the main pass, we do an abstract interpretation of the brainfuck program, where we
try to unfold constants known at runtime (such as replacing `ADD k` on a known value `n` with
`SET k+n`) to eliminate dependency chains in later passes. This has the added advantage of
requiring both fewer and cheaper instructions.
- Dead Write Elimination: Write which modify cells which are overwritten before their next use are
removed in this pass. The same applies to any regiser usage which may have been obsolete due to
constant folding.
- Strength Reduction: The final optimization pass, in which obsolete instructions like `MOV 0` are
removed, and it merges any `MOV` and `ADD` blocks which may have been formed during optimization.
- Linearization: Finally, the AST is converted back into a linear array.

The compiler applies the following steps, in this order:

- Conversion to Assembly: Every instruction is mapped to an equivalent assembly instruction or
sequence of assembly instructions



## BOCBWN Usage

BOCBWN can be compiled and used with any standard C++ compiler. It must be set up by compiling the
files to executables as shown, with the compiler of your choice.
```bash
clang++ bf_cmplr.cpp -o bf_cmplr(.exe) -O3
```

Then, to compile a brainfuck file you may use either the provided script `bfcpl` for Unix-like
environments including MSYS2.
if you are a Windows user, you may
use:
```bash
./bf_cmplr foo.bf -o foo.s
clang++ bf_asm.cpp foo.s -o foo.exe
```

to compile a brainfuck program `foo.bf` and run it with `./foo`. Four example programs, a hello
world (`hw.bf`), a multiplication program (`test.bf`), a program which prints the mandelbrot set
(`mandelbrot.bf` by [Erik Bosman](https://github.com/erikdubbelboer/brainfuck-jit)), and a text-
adventure game Lost Kingdom (`lk.bf` by
[Jon Ripley](https://jonripley.com/i-fiction/games/LostKingdomBF)) are included. You can find more
info on brainfuck at [the page on esolangs.org](https://esolangs.org/wiki/Brainfuck) and more
programs written in brainfuck at [brainfuck.org](https://brainfuck.org/).
