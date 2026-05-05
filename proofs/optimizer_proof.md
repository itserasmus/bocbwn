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
A sub B, A !sub B       improper subset
!p or ~p                negation of p
a & b, a | b            logical and and logical or of a and b
N                       the set of natural numbers including 0, also representing the first transfinite ordinal omega
W, Z+                   the set of whole numbers, N / {0}
a in S, a !in S         membership and non-membership
a != b, a >= b, a <= b  not equal, greater than or equal, less than or equal
:                       such that
fa, te, !te, st         for all, there exists, there does not exist, such that
bwoc, wlog              by way of contradiction, without loss of generality
A.a, A[a]               for a function A of the form {(a,b): P(a,b)}, A.a refers to the unique b:(a,b) in A.
A[a->x]                 changing A[a] to x in a function, or A[a->x] = {(m, v) in A: m != a} U {(a, x)}
T[a:b]                  a sub-tuple of a tuple T, for 1<=a<=b+1<=|T|+1, T[a:b] = {(k, T[a+k-1]): 1<=k<=b-a+1}
{1..n}                  denotes the set {i: i in N, 1 <= i <= n}
(1..n)                  denotes the tuple {(i, i): i in N, 1 <= i <= n}
set(K)                  for a "type" K, set(K) denotes the set of all valid K
void                    a special symbol used to represent an invalid or undefined state

In many cases, we have numbers which can take a value of "infinity". We use the first transfinite ordinal, denoted by N,
to denote the value of infinity. Thus, n = N means that n is "infinity". For comparison, we use the standard definition
under which i < N for all i in N.

Instead of using the more common definition of tuples (a_1, a_2) = {a_1, {a_1, a_2}}, and (a_1, a_2, a_3) =
(a_1, (a_2, a_3)), we define a tuple as a function, so (a_1, a_2, ... a_n) = {(i, a_i): 1 <= i <= n} for both n in N and
n = N. This also allows use to use |x| to get the size of a tuple.

*to avoid circularity, since in set theory functions require tuples, we define the 2-tuple using the standard set-
theoretic definition (a_1, a_2) = {a_1, {a_1, a_2}} within functions and within functions only, to avoid the problem of
|(a, a)| = 1 under the set-theoretic definition.

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
3. stdin (i): This is either a finite or infinite tuple, modelled as i:{1..|i|}->Z/nZ.
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
    !(P)(inv)           = (inv)
    !(P)S               = S_N                                           if te N st S_N.t[S_N.p] = 0
                        = (fin, L(S.t,S,P), L(S.p,S,P), L(S.o,S,P))     otherwise

where L is a sort of limit operator for t, p, and o, defined as
    L(S.t,S,P)  = {(i, c): i in Z; if te N st fa n>N, S_n.t[i] = S_N.t[i], c = S_N.t[i], else c = void}
    L(S.p,S,P)  = if te N st fa n>N, S_n.p = S_N.p, S_N.p, else void
    L(S.o,S,P)  = if te N st fa n>N, S_n.o = S_N.o, S_N.o, else
                    {(i, c): i in Z; c = S_N.o[i] st fa n>N, S_n.o[i] = S_N.o[i]}

For defining L(S.o,S,P), we include no void state. Informally, this is valid since fa (P), S, S.o[i] = (P)S.o[i] fa
1 <= i <= |S.o|, or all modifications to stdout are appending, not rewriting, in nature

*more formally, since the (P) and !(P) operators' definitions are both mutually-referencing and self-referencing, one
could define (P) and !(P) for P in InS_i, and use those definitions to define (P) and !(P) for P in InS_{i+1}, since
the definitions are recursive, but they recurse "one level lower" in the InS tree.


## 4. Lemmas

Here, we shall prove useful lemmas about our model


### 4.1. Lemmas on the assign and read operators

The assign operator is defined on single-valued functions A as
    A[a->x] = {(m, v) in A: m != a} U {(a, x)}
This operator always creates a single-valued function, and thus, can be chained.
The read operator is defined on single-valued functions A with a in the domain of A as
    A[a]    = x: (a, x) in A
and is unique.
where the domain of A is simply
    dom(A) = {a: te x st (a, x) in A}.

Since our operators are only defined for certain A, and a, we may safely assume that A and a satisfy the properties
    A is composed of ordered pairs:         A = {(m, v) in A: true}
    A is a single-valued function:          fa (m, v), (n, u) in A, m=n => v=u


1. Assignment creates a single-valued function
    ( fa (b, p), (c, q) in A, b=c => p=q ) => ( fa (m, k), (n, l) in A[a->x], m=n => k=l )
Proof:
    assume bwoc that te A, a, x, m, n, k, l st (m, k), (n, l) in A[a->x], m = n & k != l.
    Case I: m = n = a
        then, (a, k), (a, l) in A[a->x]
        However, (a, k) in A[a->x]
            => (a, k) in {(m, v) in A: m != a} U {(a, x)}
            => (a, k) in {(a, x)} or (a, k) in {(m, v) in A: m != a}
            => (a, k) = (a, x) or (a, k) in {(m, v) in A: m != a}
        But (a, k) in {(m, v) in A: m != a} => a != a, which is false, so
            (a, k) in A[a->x] => (a, k) = (a, x) => k = x
        Similarly, l = x => l = k, which contradicts our assumption
    Case II: m = n != a
        then, (m, k), (m, l) in A[a->x]
            => (m, k) in {(m, v) in A: m != a} U {(a, x)}
            => (m, k) in {(a, x)} or (m, k) in {(m, v) in A: m != a}
            => (m, k) = (a, x) or (m, k) in {(m, v) in A: m != a}
        But (m, k) = (a, x) => m = a which contradicts our assumption, thus, (m, k) in A[a->x]
            => (m, k) in {(m, v) in A: m != a}
            => (m, k) in A
        similarly, (m, l) in A, but fa (b, p), (c, q) in A, b=c => p=q, and since (m, k), (m, l) in A and m = m
        => k = l, which contradicts our assumption
    Thus, our assumption cannot hold, and our lemma is true


2. Reading is unique
    (a, m) in A => A[a] = m
Proof:
    Let (a, m) in A, and A[a] = n. Then, (a, n) in A, but since a is single-valued, this means m = n.
    

3. Double assignment is equivalent to the second assignment
    A[a->x][a->y]   = A[a->y]
Proof:
    A[a->x][a->y]   = ( {(m, v) in A: m != a} U {(a, x)} )[a->y]
                    = {(m, v) in {(m, v) in A: m != a} U {(a, x)}: m != a} U {(a, y)}
                    = {(m, v) in {(m, v) in A: m != a}: m != a} U {(m, v) in {(a, x)}: m != a} U {(a, y)}
    by distribution of set comprehension over union. However, X = {(m, v) in {(a, x)}: m != a} = {}, since (m, v) in X
    => (m, v) = (a, x) => m = a & m != a, which is false, therefore,
    A[a->x][a->y]   = {(m, v) in {(m, v) in A: m != a}: m != a} U {(a, y)}
                    = {(m, v) in A: m != a} U {(a, y)}
                    = A[a->y]


4. Reading after an assignment gives the assigned value
    A[a->P(A[a])][a]    = P(A[a])
Proof:
    A[a->P(A[a])][a]    = ( {(m, v) in A: m != a} U {(a, P(A[a]))} )[a]
                        = b: (a, b) in ( {(m, v) in A: m != a} U {(a, P(A[a]))} )
    However, (a, P(A[a])) in {(a, P(A[a]))}, ergo (a, P(A[a])) in ( X U {(a, P(A[a]))}). Thus,
    A[a->P(A[a])][a]    = b: (a, b) = (a, P(A[a]))
                        = P(A[a])


5. Writing an element to itself leaves it unchanged
    A[a->A[a]]  = A
Proof:
    since A[a] exists, a in dom(A) => (a, A[a]) in A
    by partitioning A on the predicate m != a,
        A           = {(m, v) in A: m != a} U {(m, v) in A: m = a}

    but (m, v), (a, A[a]) in A & m = a => v = A[a], therefore
        {(m, v) in A: m = a} = {(a, A[a])}

    thus,
        A           = {(m, v) in A: m != a} U {(a, A[a])}
    and this is equal to
        A[a->A[a]]  = {(m, v) in A: m != a} U {(a, A[a])}
    =>  A[a->A[a]]  = A


6. Double assignment using the cell can be merged
    A[a->P(A[a])][a->Q(A[a->P(A[a])][a])] = A[a->Q(P(A[a]))]
Proof:
    using Lemma 4.1.4, we simplify the expression to
    A[a->P(A[a])][a->Q(P(A[a]))]
    but by lemma 4.1.3, this is a double assignment, so it equals
    A[a->Q(P(A[a]))]


7. Reads are unaffected by unrelated writes
    fa a!=b, A[a->x][b] = A[b]
Proof:
    A[a->x][b]  = ( {(m, v) in A: m != a} U {(a,x)} )[b]
                = c: (b, c) in ( {(m, v) in A: m != a} U {(a, x)} )
                = c: (b, c) in {(m, v) in A: m != a} | (b, c) in {(a, x)}
                = c: (b, c) in {(m, v) in A: m != a} | (b, c) = (a, x)
    b != a => (b, c) != (a, x), but (b, c) != (a, A[a]), so
    A[a->x][b]  = c: (b, c) in {(m, v) in A: m != a}
    (b, c) in {(m, v) in A: m != a} => (b, c) in A, which gives us A[b] = c, ie
    A[a->x][b]  = A[b]


8. Unrelated reads are commutative
    fa a != b, A[a->x][b->y] = A[b->y][a->x]
Proof
    A[a->x][b->y]       = ( {(m, v) in A: m != a} U {(a, x)} )[b->y]
        = {(m, v) in ( {(m, v) in A: m != a} U {(a, x)} ): m != b} U {(b, y)}
        = {(m, v) in {(m, v) in A: m != a}: m != b} U {(m, v) in {(a, x)}: m != b} U {(b, y)}
        = {(m, v) in A: m != a & m != b} U {(a, x)} U {(b, y)}
    Similarly,
    A[b->y][a->x]       = {(m, v) in A: m != a & m != b} U {(a, x)} U {(b, y)}
    => A[a->x][b->y]    = A[b->y][a->x]


9. Equality is extensional
    A = B <=> dom(A) = dom(B) & fa a in dom(A), A[a] = B[a]
Proof:
    forward:
    by congruence of equality,
    A = B => dom(A) = dom(B) & fa a in dom(A), A[a] = B[a]

    reverse:
    bwoc, assume A != B => wlog, te (a, x) in A st (a, x) !in B
    dom(A) = dom(B) => te y st (a, y) in B
        => B[a] = y
    but A[a] = x => y = x => (a, x) in B


10. Domain expands after assignment
    dom(A[a->x]) = dom(A) U {a}
Proof:
    dom(A[a->x])    = dom({(m, v) in A: m != a} U {(a, x)})
        = {k: te w st (k, w) in ( {(m, v) in A: m != a} U {(a, x)} )}
        = {k: te w st (k, w) in {(m, v) in A: m != a} | (k, w) in {(a, x)}}
        = {k: te w st (k, w) in {(m, v) in A: m != a}} U {k: (k, w) in {(a, x)}}
        = {k: te w st (k, w) in A & k != a} U {a}
        = ( {k: te w st (k, w) in A} / {k: te w st (k, w) in A & k = a} ) U {a}
        = ( {k: te w st (k, w) in A} / {k: te w st (k, w) in ( A U {(a, x)} ) & k = a} ) U {a}
        = ( dom(A) / {a} ) U {a}
        = dom(A)
        = dom(A) U {a}


### 4.2. Lemmas on the State Model

Now, we prove lemmas on the State Model introduced in section 3.1.

First, we rigorously define the states

VState = {(t,p,i,o,r): t:Z->Z/nZ & p in Z & i:(1..x)->Z/nZ & o:(1..y)->Z/nZ & r:{A}->Z/nZ, x in N U {N}, y in N}
TState = {(fin,t,p,o): t:Z->(Z/nZ U {void}) & p in (Z U {void}) & o:(1..y)->(Z/nZ U {void}), y in N U {N}}
IState = {(inv)}

State = VState U TState U IState

for the remainder of this markdown, we will, usually explicitly but possibly implicitly use S, V, and T to represent
arbitrary members of State, VState, and TState.


1. The state partition is disjoint
    VState n TState = VState n IState = TState n IState = {}
Proof:
    x in VState & x in TState => (1,fin) !in x & (1,fin) in x
    x in VState & x in IState => (1,inv) !in x & (1,inv) in x
    x in TState & x in IState => (1,inv) !in x & (1,inv) in x


2. VStates are entirely deter






