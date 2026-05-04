# BOCBWN Proof

## 1. Foreword

In the following markdown, I will present a proof that BOCBWN's optimizer is valid. This proof proves only the validity
of each transformation applied by the file `src/bf_optimize.cpp`. Their implementations, while trivially following from
the intent, are not proved. This proof does not touch on the parsing and IR compilation process, only on the IR
optimization process. In other words, that every transformation after `treeify` abd before `linearize` is valid.


## 2. Notation

Due to this proof being written in markdown, many mathematical symbols cannot be easily typed. Therefore, the following
notation is used.
=>, <=>                 implies, bidirectional implies
A U B, A n B            set union and intersection
!p or ~p                negation of p
a & b, a | b            logical and and logical or of a and b
N                       the set of natural numbers including 0, also representing the first transfinite ordinal omega
a in S, a !in S         membership and non-membership
a != b, a >= b, a <= b  not equal, greater than or equal, less than or equal
:, |                    such that
fa, te, st              for all, there exists, such that
A.x, A[x]               for a set A of the form {(a,b): P(a,b)}, A.x refers to the unique b:(a,b) in A.
A[x->y]                 changing A[x] to y in a function, or A[x->y] = {(a, b) in A: a != x} U {(x, y)}
T[a:b]                  a sub-tuple of a tuple T, for 1<=a<=b+1<=|T|+1, T[a:b] = {(k, T[a+k-1]): 1<=k<=b-a+1}
{1..n}                  denotes the set {i: i in N, 1 <= i <= n}
(1..n)                  denotes the tuple (i)\_{i=1}^n
set(K)                  for a "type" K, set(K) denotes the set of all valid K
void                    a special symbol used to represent an invalid or undefined state

In many cases, we have numbers which can take a value of "infinity". We use the first transfinite ordinal, denoted by N,
to denote the value of infinity. Thus, n = N means that n is "infinity". For comparison, we use the standard definition
under which i < N for all i in N.
Instead of using the more common definition of tuples (a_1, a_2) = {a_1, {a_1, a_2}}, and (a_1, a_2, a_3) =
(a_1, (a_2, a_3)), we define a tuple as a function, so (a_1, a_2, ... a_n) = {(i, a_i): 1 <= i <= n} for both n in N and
n = N. This also allows use to use |x| to get the size of a tuple.
We use the notation T+e = T U (|T|+1, e) to define addition to a tuple.



## 3. Definitions

First, we informally define how brainfuck program runs. A brainfuck program consists of the characters []+-><., whose
meaning I shall not define here, operating on a bi-infinite tape of unisgned, wrapping integers indexed by a memory
pointer, which is an integer. These integers can be treated as elements of Z/nZ, where n is the number around which they
wrap. BOCBWN uses n=256, however, these proofs work adequately with any n >= 2.


### 3.1. State Model

Now, we define what possible states a brainfuck program can be in. A brainfuck program, under this model, consists of
a tape, a memory pointer, a standard input, a standard output, and a register collection. We define each of these as
follows.

1. tape (t): This is a bi-infinite tape of unisgned, wrapping integers. This can be modelled as a function t:Z->Z/nZ
2. pointer (p): This is an integer, and can be modelled as p in Z
3. stdin (i): This is either a finite or infinite tuple, modelled as i:{1..|i|}->Z/nZ or i:Z->Z/nZ for a finite
    or infinite tuple respectively.
4. stdout (o): This is a finite tuple, modelled as o:{1..|o|}->Z/nZ.
5. register (r): Currently, r consists of only one register, A. Thus, r is modelled as r:{A}->(Z/nZ U {void}).

We now define a VState, V as a tuple
    V = (tape, pointer, stdin, stdout, register)
And convenient accesses V.t = V[1], V.p = V[2], V.i = V[3], V.o = V[4], and V.r = V[5]. Then, we define
    VState = set(V)

We also define two new types of states, a finished state, and an invalid state. The finished state allows for us to
easily work with programs like `+[].`, which run forever, and the invalid state is merely for uniformity of some
operations.
We define a halted state by information about the tape, the pointer, stdout, and the registers. In a halted state, we
slightly change these definitions as follows:

1. tape (t): modelled as Z->(Z/nZ U {void}), to allow representation of programs like `+[++]`, where cells may not have
    a defined final state
2. pointer (p): modelled as p in Z U {void}, to allow representation of programs like `>+<+[>>+<]`, where the pointer
    may not have a defined final state
3. stdout (o): Here, we allow infinite tuples as well, again modelled as o:{1..|o|}->Z/nZ, to allow programs with
    infinite output, like `+[.]`

We now define a TState, T, as a tuple
    T = (fin, tape, pointer, stdout)
and convenient accesses T.t = T[2], T.p = T[3], T.o = T[4]. Then, we define
    TState = set(T)

finally, we define an invalid state I = (inv), and
    IState = {I}

We have thus defined all the possible states a brainfuck program can be in, and with their powers combined, we define
    State = VState U TState U IState
and VState, TState, and IState are trivially disjoint via checking membership of fin and inv.


### 3.2. Operators on State

We define programs as total operators on State. First, we define the trivial operator, NOP, with syntax as follows. The
letter S denotes a State, V denotes a VState, T denotes a TState, and (inv) denotes the IState
    (NOP)S = S

We define an intrinsic, the set of which is denoted as In as the following set.
    In = { (MOV k), (ADD k), (OUT), (IN), (SET k), (OUTC k), (PUTA), (ACCUMA), (MACMA k), (RKILL), (NOP), (HLT) }

We will define each of the intrinsics as follows. Let (Int) denote any element of the set In.
(Int)T                  = T
(Int)(inv)              = (inv)
and for VStates, denoted as the expanded (t,p,i,o,r),
(MOV k)(t,p,i,o,r)      = (t,p+k,i,o,r)
(ADD k)(t,p,i,o,r)      = (t[p->t[p]+k],p,i,o,r)
(OUT)(t,p,i,o,r)        = (t,p,i,o+t[p],r)
(IN)(t,p,i,o,r)         = (t[p->i[1]],p,i[2:|i|],o,r) if |i| > 0, (fin,t,p,o) otherwise
(SET k)(t,p,i,o,r)      = (t[p->k],p,i,o,r)
(OUTC k)(t,p,i,o,r)     = (t,p,i,o+k,r)
(PUTA)(t,p,i,o,r)       = (t,p,i,o,r[A->t[p]])
(ACCUMA)(t,p,i,o,r)     = (t[p->t[p]+r.A],p,i,o,r) if r.A != void, (inv) otherwise
(MACMA k)(t,p,i,o,r)    = (t[p->t[p]+k*r.A],p,i,o,r) if r.A != void, (inv) otherwise
(RKILL)(t,p,i,o,r)      = (t,p,i,o,r[A->void])
(HLT)(t,p,i,o,r)        = (fin,t,p,o)

With all intrinsics defined, we define an instruction set. An instruction set P is defined as a well-founded finite
tuple of programs, intrinsics, and loops. Given a set of intrinsics In, we define the set InS of all
instruction sets as follows
    InS_0       = In
    InS_{i+1}   = InS_i U {(a_1, a_2,...a_n): n in N & a_j in InS_i for all 1<=j<=n }
                    U {{loop, (a_1, a_2,...a_n)}: n in N & a_j in InS_i for all 1<=j<=n }
    InS         = InS_0 U InS_1 U InS_2 U ...

From an instruction set P, we define a program (P) recursively as follows. 
    (P) = P if P in In, else
    (P) = (P[2:|P|])(P[1]) if |P| > 1, (P[1]) otherwise

and define Prog = set((P)). We define (!(...)) later

Since a program is well-founded and finite, this recursion terminates and yeilds a unique definition for every program.
We also introduce the notation (P_1,P_2,...P_n) or (P_1;P_2;...P_n) as follows
    (P_1,P_2,...P_n) = (P_n)(P_{n-1})(...)(P_2)(P_1)

Since operator composition is associative, grouping is meaningless. Finally, we define equality as
    fa (P), (Q) in Prog; (P) = (Q) <=> fa S in State, (P)S = (Q)S

Finally, we define a loop. A loop, L, is also defined from instruction sets as a pair {loop, P} and its corresponding
operator, (L), alternatively denoted as !(P), is defined as follows.
    S_0                 = S
    S_{i+1}             = (P)S_i
    !(P)T               = T
    !(P)(int)           = (int)

    !(P)S               = S                         if S.t[S.p] = 0
                        = !(P)(P)S                  if there exists N st S_N.t[S_N.p] = 0
                        = (fin, L(S.t,S,P), L(S.p,S,P), L(S.o,S,P))     otherwise

where L is a sort of limit operator for t, p, and o, defined as
    L(S.t,S,P)  = {(i, c): i in Z; if te N st fa n>N, S_n.t[i] = S_N.t[i], c = S_N.t[i], else c = void}
    L(S.p,S,P)  = if te N st fa n>N, S_n.p = S_N.p, S_N.p, else void
    L(S.o,S,P)  = {(i, c): i in Z; c = S_N.o[i] st fa n>N, S_n.o[i] = S_N.o[i]}



