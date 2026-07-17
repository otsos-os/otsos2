# Kato Language Specification (or book (: )

Kato is a pure functional data-oriented language that compiles to C. It merges Data-Oriented Design (Structure of Arrays, flat memory, cache-friendly layouts) with Haskell-like functional programming (pure functions, immutability, map/filter/fold, pattern matching). The language has no standard library. The compiler produces portable C code.

The core of Kato is the **Metadata DSL** — a full compile-time language that defines types with real value ranges, function contracts, struct layouts, memory strategies, code generation rules, and compile-time assertions. The metadata DSL is not annotation — it is the authority. The compiler obeys it.

---

## 1. Philosophy

1. **Data-First**: Data structures are designed for cache utilization. Structure of Arrays (SoA) is the default layout. Flat arrays and indices replace pointer chasing.
2. **Pure Functional**: Functions are pure by default. No side effects, no hidden mutation. Immutability is the baseline. Explicit `mut` marks the rare mutable paths.
3. **Metadata is the Language**: The metadata DSL is a compile-time language that defines every aspect of code generation. Types have value ranges. Functions have contracts. Structs have layout rules. The same source code produces radically different C depending on metadata: optimized C for speed, or hardened C for safety-critical systems.
4. **One Source, Many Targets**: The same `.kato` file can compile in speed mode (restrict pointers, auto-vectorization, no bounds checking) or safety mode (fixed arrays, runtime bounds checks, overflow checks, single exit point, no dynamic allocation). The metadata selects the mode and controls every transformation.
5. **No Runtime, No Stdlib**: The language provides no built-in functions, no I/O, no printing, no allocations. Everything is either user-defined or imported from C via interop. The compiler generates pure C with no runtime dependency.

---

## 2. Lexical Structure

### 2.1 Encoding

Source files are UTF-8.

### 2.2 Comments

```
// single line comment
```

### 2.3 Identifiers

Identifiers start with a letter or underscore, followed by letters, digits, or underscores.

### 2.4 Integer Literals

```
42          // decimal
0xFF        // hex
0b1010      // binary
1_000_000   // underscores for readability (ignored)
```

### 2.5 Float Literals

```
3.14
2.0
1.5e-10
```

### 2.6 Character Literals

```
'a'
'\n'
'\0'
```

### 2.7 String Literals

```
"hello world"
```

String literals are `char*` in C. They exist only for C interop and constant data.

### 2.8 Boolean Literals

```
true
false
```

### 2.9 Keywords

```
let mut func struct enum const
if else match where
map filter fold
in return break continue
import export
true false
void while asm
```

### 2.10 Operators and Punctuation

```
+ - * / % //       arithmetic
== != < > <= >=    comparison
&& || !            logical
& *                address-of dereference
= -> =>            assignment arrow fat-arrow
.. ...             range (exclusive) range (inclusive)
( ) { } [ ]        grouping
; : , .            statement type separator dot
```

---

## 3. The Metadata DSL

The metadata DSL is a compile-time language that controls every aspect of Kato compilation. It defines types with real constraints, function contracts, struct layouts, memory strategies, optimization directives, and compile-time logic. The compiler obeys the DSL — it is not optional documentation.

### 3.1 Structure

Metadata is declared at the top of every `.kato` file in a `!META!` block:

```
!META!
    $directive ...
    $directive ...
!END!
```

The `!META!` block is required. It must appear before any code.

### 3.2 Mode Selection

```
$mode speed       // optimize for performance
$mode safety      // optimize for correctness
```

A file cannot mix modes. If `$mode` is absent, `$mode speed` is assumed.

The mode sets defaults for all other directives. Speed mode defaults: no bounds checking, no overflow checking, `restrict` enabled, dynamic allocation allowed. Safety mode defaults: bounds checking, overflow checking, no `restrict`, no dynamic allocation, single exit point, complexity <= 10.

Every individual default can be overridden by explicit directives. For example, in speed mode you can enable overflow checking for specific functions:

```
$mode speed
$overflow checked particles_count_active
```

### 3.3 Type Definitions

Types are defined with real, machine-readable properties. The compiler uses these to generate correct C types, validate values, and emit runtime checks.

#### Integer Types

```
$define %type int32 as integer
    bits: 32
    signed: true
    from: {-2147483648}
    to: {2147483647}

$define %type int64 as integer
    bits: 64
    signed: true
    from: {-9223372036854775808}
    to: {9223372036854775807}

$define %type uint32 as integer
    bits: 32
    signed: false
    from: {0}
    to: {4294967295}

$define %type uint8 as integer
    bits: 8
    signed: false
    from: {0}
    to: {255}
```

Properties:
- `bits` — semantic width in bits (`1..64`). Standard widths (`8`, `16`, `32`, `64`) use native scalar C storage. Non-standard widths use the closest compact byte storage that can hold the value.
- `signed` — `true` or `false`
- `from` — minimum value (compile-time expression in `{}`)
- `to` — maximum value (compile-time expression in `{}`)

The compiler uses `from`/`to` to:
- Generate overflow checks in safety mode
- Validate constant values at compile time
- Emit runtime range checks when `$range_check` is enabled

If `from`/`to` are omitted, the compiler derives them from `bits` and
`signed`. For example, signed `bits: 5` means `-16..15`; unsigned
`bits: 5` means `0..31`.

Non-standard integer widths are first-class Kato integer types. The
compiler must not widen their semantic range to the backing C storage.
For storage, Kato uses the smallest byte count that can contain the
requested bit width. For example, `int34` uses 5 bytes of storage, not an
8-byte `int64_t` scalar. The C backend may represent this as a generated
small struct plus helper functions for conversion and arithmetic.

#### Float Types

```
$define %type float32 as float
    bits: 32
    encoding: ieee754
    precision: single

$define %type float64 as float
    bits: 64
    encoding: ieee754
    precision: double
```

Properties:
- `bits` — 32 or 64
- `encoding` — `ieee754` (only standard supported)
- `precision` — `single` or `double`

#### Bool Type

```
$define %type bool as boolean
    bits: 8
    true_value: {1}
    false_value: {0}
```

#### Char Type

```
$define %type char as character
    bits: 8
    signed: true
    encoding: ascii
```

#### Void Type

```
$define %type void as empty
```

#### Struct Types

```
$define %type ParticleSystem as struct
    layout: soa
    fields: {
        x: float32[ MAX_PARTICLES ],
        y: float32[ MAX_PARTICLES ],
        vx: float32[ MAX_PARTICLES ],
        vy: float32[ MAX_PARTICLES ],
        lifetime: float32[ MAX_PARTICLES ],
        active: bool[ MAX_PARTICLES ],
        count: int32
    }
    max_count_field: count
    capacity: { MAX_PARTICLES }
```

Properties:
- `layout` — `soa` (Structure of Arrays), `aos` (Array of Structures), `packed`, or `packed_bits`
- `fields` — field declarations with types and array sizes
- `max_count_field` — the field that tracks the active element count (used for bounds checking)
- `capacity` — compile-time expression for the maximum number of elements
- `align` — alignment in bytes (optional, default = natural alignment)
- `packed` — `true` or `false` (optional, default = `false`)

The `max_count_field` and `capacity` properties are used by the compiler to automatically generate bounds checks for array accesses through this struct in safety mode.

Layout is part of a struct type's identity. Two structs with the same
fields but different layouts are different types and are not implicitly
interchangeable. Conversion between layouts must be explicit.

#### Enum Types

```
$define %type Color as enum
    base: int32
    variants: {
        Red: {0},
        Green: {1},
        Blue: {2}
    }
```

Properties:
- `base` — underlying integer type
- `variants` — name to value mapping (compile-time expressions in `{}`)

#### Pointer Types

```
$define %type int32_ptr as pointer
    target: int32
    nullable: true
```

Properties:
- `target` — the pointed-to type
- `nullable` — `true` or `false` (if `false`, NULL checks are not emitted)

### 3.4 Compile-Time Expressions

Values in `{}` are compile-time expressions. The DSL supports:

```
{ 1024 }                          // integer literal
{ 3.14 }                          // float literal
{ "hello" }                       // string literal
{ MAX_PARTICLES }                 // reference to a constant
{ MAX_PARTICLES * 2 }             // arithmetic
{ MAX_PARTICLES + 64 }            // addition
{ 1 << 20 }                       // bit shift
{ if MAX_PARTICLES > 512 then 64 else 32 }  // conditional
```

Compile-time expression operators: `+ - * / % << >> & | ^ == != < > <= >= && || !`

### 3.5 Compile-Time Variables

```
$let CACHE_LINE = 64
$let MAX_THREADS = { CACHE_LINE * 4 }
$let BUFFER_SIZE = { if MAX_THREADS > 8 then 4096 else 1024 }
```

Compile-time variables can reference each other and use compile-time expressions. They are resolved at compile time and can be used in any `{}` expression.

### 3.6 Compile-Time Conditionals

```
$if { MAX_PARTICLES > 512 } then
    $opt vectorize
    $unroll count(8) particles_update
$else
    $unroll count(4) particles_update
$end
```

The `$if` / `$else` / `$end` block evaluates a compile-time expression and conditionally applies directives. This allows the metadata to adapt to compile-time parameters.

### 3.7 Compile-Time Assertions

```
$assert { MAX_PARTICLES > 0 }
$assert { MAX_PARTICLES <= 65536 }
$assert { CACHE_LINE == 64 || CACHE_LINE == 128 }
```

Compile-time assertions are evaluated at compile time. If an assertion fails, compilation aborts with an error.

### 3.8 Function Contracts

Functions are defined with contracts that the compiler enforces:

```
$define %func particles_update as function
    args: { sys: ParticleSystem*, dt: float32 }
    returns: void
    pure: false
    mutates: { sys }
    inline: always
    unroll: count(4)
    complexity: max(8)
    allocates: false
    recurses: false
    threadsafe: false
```

Contract properties:
- `args` — argument names and types
- `returns` — return type
- `pure` — `true` or `false` (default: `true` for non-void without mut args, `false` otherwise)
- `mutates` — list of arguments that are mutated (only valid if `pure: false`)
- `inline` — `always`, `never`, `auto` (default: `auto`)
- `unroll` — `count(N)`, `auto`, `no` (default: `auto`)
- `complexity` — `max(N)` cyclomatic complexity limit
- `allocates` — `true` or `false` (if `false`, compiler rejects any allocation in this function)
- `recurses` — `true` or `false` (if `false`, compiler rejects recursive calls)
- `threadsafe` — `true` or `false`
- `restrict` — `true` or `false` (whether to emit `restrict` on pointer params; speed mode default: `true`, safety mode: `false`)

### 3.9 Start Function

```
$define %func main as start
    args: { void }
    returns: int32
```

There must be exactly one `start` function across all files. It maps to C `main`.

### 3.10 Visibility

```
$space %export ParticleSystem, particles_update, particles_spawn
$space %internal update_position, compute_velocity
```

- `%export` — visible to other translation units (no `static` in C)
- `%internal` — file-local (`static` in C)

All functions and types must be declared in `%export` or `%internal`.

### 3.11 Constants

```
$define %const MAX_PARTICLES = { 1024 }
$define %const MAX_VELOCITY = { 100.0 }
$define %const BUFFER_SIZE = { MAX_PARTICLES * sizeof(float32) }
```

Constants are compile-time values. They can reference compile-time variables and use expressions.

### 3.12 Bounds Checking

```
$bounds none                        // no bounds checking (speed mode default)
$bounds static                      // check against compile-time array sizes
$bounds runtime                     // check against runtime count fields
$bounds static particles_update     // per-function override
```

In safety mode, `$bounds static` is the default. The compiler uses struct `capacity` and `max_count_field` from type definitions to emit bounds checks.

### 3.13 Overflow Checking

```
$overflow unchecked                 // no overflow checks (speed mode default)
$overflow checked                   // all integer arithmetic checked
$overflow checked particles_count   // per-function override
```

In safety mode, `$overflow checked` is the default. The compiler uses `from`/`to` from type definitions to emit overflow checks.

### 3.14 NULL Checking

```
$null_check never                   // no NULL checks (speed mode default)
$null_check always                  // all pointer params checked
$null_check always particles_update // per-function override
```

The compiler uses `nullable: true` from pointer type definitions. Pointers declared as `nullable: false` do not get NULL checks even in safety mode.

### 3.15 Division Checking

```
$div_check never                    // no division-by-zero checks (speed mode default)
$div_check always                   // all division/modulo checked
```

### 3.16 Initialization

```
$init none                          // no forced initialization (speed mode default)
$init zero                          // all variables zero-initialized
$init explicit                      // all variables must be explicitly initialized
```

### 3.17 Allocation Strategy

```
$alloc dynamic                      // malloc/realloc allowed (speed mode default)
$alloc static                       // all arrays fixed-size, no dynamic allocation
$alloc arena( { 65536 } )           // arena allocator with given initial size
$alloc pool( { 1024 } )             // pool allocator with given slot count
$alloc none                         // no allocation allowed at all
```

In safety mode, `$alloc static` is the default and `$alloc dynamic` is forbidden.

### 3.18 Threading

```
$thread single                      // single-threaded (safety mode default)
$thread safe                        // multi-threaded with synchronization
$thread unsafe                      // multi-threaded without synchronization
```

### 3.19 Complexity Limits

```
$complexity max(10)                 // max cyclomatic complexity per function
$stack max( { 4096 } )              // max stack size in bytes
```

### 3.20 Error Handling (Panic)

```
$panic halt                         // halt on error (safety mode default)
$panic abort                        // call abort()
$panic return                       // return a safe default value
$panic trap                         // execute a trap instruction
```

### 3.21 Layout Strategy

```
$layout soa                         // Structure of Arrays (default)
$layout aos                         // Array of Structures
```

Can also be set per-struct in the type definition.

### 3.22 Optimization Directives

```
$opt vectorize                      // enable auto-vectorization hints
$opt no_vectorize                   // disable vectorization
$opt restrict                       // use restrict on pointer params
$opt no_restrict                    // do not use restrict
$opt simd                           // allow SIMD intrinsics via C interop
$opt no_simd                        // forbid SIMD
```

These are ignored in safety mode.

### 3.23 Inlining

```
$inline always                      // always inline (static inline in C)
$inline never                       // never inline
$inline auto                        // let the C compiler decide (default)
$inline always particles_update     // per-function
```

### 3.24 Loop Unrolling

```
$unroll count(4)                    // unroll by factor of 4
$unroll auto                        // let the C compiler decide (default)
$unroll no                          // never unroll
$unroll count(8) particles_update   // per-function
```

### 3.25 C Interop Directives

```
$c_include stdio.h                  // include a C header
$c_include windows.h
$c_flag "-O3"                       // suggest C compiler flag
$c_flag "-Wall -Werror"
$c_name particles_update            // override C name for a function
$c_prefix "kato_"                   // prefix all functions
$c_no_prefix                        // do not prefix function names
$c_export my_function               // export symbol for C linkage
```

### 3.26 Direct C Emission

The DSL can directly emit C code into the output file. This is used for platform-specific code, compiler intrinsics, or headers that the metadata needs:

```
$emit c "#include <immintrin.h>"
$emit c "#pragma omp simd"
```

### 3.27 Range Check Directive

When enabled, the compiler emits runtime checks that validate integer values are within the `from`/`to` range of their type:

```
$range_check always                 // check after every assignment
$range_check never                  // no range checks
```

### 3.28 Inline Assembly Control

```
$asm allowed                    // inline assembly allowed (speed mode default)
$asm never                      // inline assembly forbidden (safety mode default)
```

When `$asm never` is set, the compiler rejects all `asm` blocks in the code body. In safety mode, `$asm never` is the default — use `$asm allowed` to explicitly enable inline assembly.

### 3.29 Full Metadata Example (Speed Mode)

```
!META!
    $mode speed
    $layout soa
    $opt vectorize
    $opt restrict

    $let MAX_PARTICLES = 1024
    $let CACHE_LINE = 64

    $assert { MAX_PARTICLES > 0 }
    $assert { MAX_PARTICLES <= 65536 }

    $if { MAX_PARTICLES > 512 } then
        $unroll count(8) particles_update
    $else
        $unroll count(4) particles_update
    $end

    $define %type int32 as integer
        bits: 32
        signed: true
        from: {-2147483648}
        to: {2147483647}

    $define %type float32 as float
        bits: 32
        encoding: ieee754
        precision: single

    $define %type bool as boolean
        bits: 8
        true_value: {1}
        false_value: {0}

    $define %type ParticleSystem as struct
        layout: soa
        fields: {
            x: float32[ MAX_PARTICLES ],
            y: float32[ MAX_PARTICLES ],
            vx: float32[ MAX_PARTICLES ],
            vy: float32[ MAX_PARTICLES ],
            lifetime: float32[ MAX_PARTICLES ],
            active: bool[ MAX_PARTICLES ],
            count: int32
        }
        max_count_field: count
        capacity: { MAX_PARTICLES }

    $define %const MAX_PARTICLES = { 1024 }

    $define %func main as start
        args: { void }
        returns: int32

    $define %func particles_update as function
        args: { sys: ParticleSystem*, dt: float32 }
        returns: void
        pure: false
        mutates: { sys }
        inline: always
        unroll: count(4)
        complexity: max(8)
        allocates: false
        recurses: false

    $define %func particles_spawn as function
        args: { sys: ParticleSystem*, px: float32, py: float32, pvx: float32, pvy: float32 }
        returns: int32
        pure: false
        mutates: { sys }
        allocates: false

    $define %func particles_deactivate as procedure
        args: { sys: ParticleSystem*, index: int32 }
        returns: void
        pure: false
        mutates: { sys }

    $define %func particles_count_active as function
        args: { sys: ParticleSystem* }
        returns: int32
        pure: true
        allocates: false

    $space %export main, ParticleSystem, particles_update, particles_spawn, particles_deactivate, particles_count_active
    $space %internal update_position
!END!
```

### 3.30 Full Metadata Example (Safety Mode)

```
!META!
    $mode safety
    $layout soa
    $bounds static
    $overflow checked
    $init zero
    $null_check always
    $div_check always
    $alloc static
    $thread single
    $complexity max(10)
    $panic halt
    $range_check always

    $let SAFE_MAX_PARTICLES = 1024

    $assert { SAFE_MAX_PARTICLES > 0 }
    $assert { SAFE_MAX_PARTICLES <= 65536 }

    $define %type int32 as integer
        bits: 32
        signed: true
        from: {-2147483648}
        to: {2147483647}

    $define %type float32 as float
        bits: 32
        encoding: ieee754
        precision: single

    $define %type bool as boolean
        bits: 8
        true_value: {1}
        false_value: {0}

    $define %type SafeParticleSystem as struct
        layout: soa
        fields: {
            x: float32[ SAFE_MAX_PARTICLES ],
            y: float32[ SAFE_MAX_PARTICLES ],
            vx: float32[ SAFE_MAX_PARTICLES ],
            vy: float32[ SAFE_MAX_PARTICLES ],
            lifetime: float32[ SAFE_MAX_PARTICLES ],
            active: bool[ SAFE_MAX_PARTICLES ],
            count: int32
        }
        max_count_field: count
        capacity: { SAFE_MAX_PARTICLES }

    $define %const SAFE_MAX_PARTICLES = { 1024 }

    $define %func main as start
        args: { void }
        returns: int32

    $define %func safe_particles_update as procedure
        args: { sys: SafeParticleSystem*, dt: float32 }
        returns: void
        pure: false
        mutates: { sys }
        complexity: max(10)
        allocates: false
        recurses: false

    $define %func safe_particles_spawn as function
        args: { sys: SafeParticleSystem*, px: float32, py: float32, pvx: float32, pvy: float32, out_index: int32* }
        returns: bool
        pure: false
        mutates: { sys, out_index }
        complexity: max(10)
        allocates: false
        recurses: false

    $define %func safe_particles_deactivate as procedure
        args: { sys: SafeParticleSystem*, index: int32 }
        returns: void
        pure: false
        mutates: { sys }
        complexity: max(10)

    $define %func safe_particles_count_active as function
        args: { sys: SafeParticleSystem* }
        returns: int32
        pure: true
        complexity: max(10)

    $space %export main, SafeParticleSystem, safe_particles_update, safe_particles_spawn, safe_particles_deactivate, safe_particles_count_active
!END!
```

### 3.31 Custom Transforms — Programmer-Defined Optimizers and Safetifiers

The most powerful feature of the metadata DSL. The programmer can define custom transformation rules that control how the compiler converts AST nodes into C code. This makes the code generator pluggable — instead of hardcoded speed/safety logic, transforms are rules that the compiler interprets.

#### 3.31.1 Concept

A transform is a rule that:
1. **Matches** a specific AST construct (array access, binary op, assignment, etc.)
2. **Checks a condition** (compile-time expression, e.g. `$bounds == "static"`)
3. **Emits C code** before, after, or instead of the default code

The compiler has built-in default transforms for speed and safety modes. The programmer can override them, disable them, or add new ones.

#### 3.31.2 Syntax

```
$transform name
    match: ast_construct
    condition: { compile_time_expr }
    before: { c_code_template }
    after: { c_code_template }
    replace: { c_code_template }
```

- `match` — which AST construct to match (see 3.31.4)
- `condition` — compile-time expression, must evaluate to `true` for the transform to apply
- `before` — C code emitted before the original code
- `after` — C code emitted after the original code
- `replace` — C code that completely replaces the original

`before`, `after`, and `replace` are mutually optional. You can use `before` + `after` together to wrap code, or `replace` alone to substitute.

#### 3.31.3 Template Variables

Templates contain `{var}` placeholders that the engine fills from the matched AST node:

| Variable | Description | Available in |
|----------|-------------|-------------|
| `{original}` | The default C code for this node | all |
| `{target}` | Target of access (e.g. `sys->x`) | array_access, field_access, assignment |
| `{index}` | Index expression (e.g. `i`) | array_access |
| `{capacity}` | Array capacity from struct metadata | array_access |
| `{left}` | Left operand | binary_op |
| `{right}` | Right operand | binary_op |
| `{op}` | Operator (e.g. `+`) | binary_op |
| `{value}` | Assigned value | assignment, let_binding, return_stmt |
| `{name}` | Function or variable name | function_call, let_binding |
| `{field}` | Field name | field_access |
| `{pointer}` | Pointer expression | pointer_deref |
| `{cond}` | Condition expression | if_stmt, while_loop |
| `{args}` | Comma-separated arguments | function_call |
| `{type}` | Variable type | let_binding |
| `{panic_code}` | Panic code based on `$panic` mode | all |
| `{type_max}` | Max value of the current type | binary_op |
| `{type_min}` | Min value of the current type | binary_op |
| `{index_var}` | Loop index variable | map_loop, fold_expr |
| `{range_start}` | Range start | map_loop, fold_expr |
| `{range_end}` | Range end | map_loop, fold_expr |

Special DSL variables available in conditions:
- `$mode` — `"speed"` or `"safety"`
- `$bounds` — `"none"`, `"static"`, `"runtime"`
- `$overflow` — `"checked"`, `"unchecked"`
- `$null_check` — `"always"`, `"never"`
- `$div_check` — `"always"`, `"never"`
- `$range_check` — `"always"`, `"never"`
- `$alloc` — `"dynamic"`, `"static"`, `"none"`
- `$panic` — `"halt"`, `"abort"`, `"return"`, `"trap"`
- `$init` — `"zero"`, `"explicit"`, `"none"`

#### 3.31.4 Matchable AST Constructs

| Construct | Description | Captured Variables |
|-----------|-------------|-------------------|
| `array_access` | `arr[idx]` | `{target}`, `{index}`, `{capacity}`, `{original}` |
| `binary_op` | `a op b` | `{left}`, `{right}`, `{op}`, `{original}` |
| `assignment` | `x = y` | `{target}`, `{value}`, `{original}` |
| `function_call` | `f(args)` | `{name}`, `{args}`, `{original}` |
| `field_access` | `obj.field` | `{target}`, `{field}`, `{original}` |
| `pointer_deref` | `*ptr` | `{pointer}`, `{original}` |
| `address_of` | `&x` | `{target}`, `{original}` |
| `if_stmt` | `if cond { ... }` | `{cond}`, `{original}` |
| `while_loop` | `while cond { ... }` | `{cond}`, `{original}` |
| `map_loop` | `map (i in a..b) { ... }` | `{index_var}`, `{range_start}`, `{range_end}`, `{original}` |
| `fold_expr` | `fold (init, i in a..b) { ... }` | `{index_var}`, `{range_start}`, `{range_end}`, `{original}` |
| `return_stmt` | `return x;` | `{value}`, `{original}` |
| `let_binding` | `let x: T = y;` | `{name}`, `{type}`, `{value}`, `{original}` |

`binary_op` can be further filtered by operator:
```
$transform my_add_check
    match: binary_op("+")
    ...
```

#### 3.31.5 Built-in Transforms

The compiler registers default transforms based on mode:

**Speed mode defaults:**
- `speed_array_access` — no bounds check, direct access
- `speed_binary_op` — no overflow check, direct operation
- `speed_pointer_deref` — no NULL check, direct deref

**Safety mode defaults:**
- `safety_array_access` — bounds check before every array access
- `safety_binary_op` — overflow check before every arithmetic op
- `safety_pointer_deref` — NULL check before every pointer deref
- `safety_return` — `goto cleanup` instead of `return`
- `safety_division` — div-by-zero check before `/` and `%`

#### 3.31.6 Overriding Built-in Transforms

A user-defined transform with the same name as a built-in replaces it:

```
$transform safety_array_access
    match: array_access
    condition: { $bounds == "static" }
    before: {
        if ({index} < 0 || {index} >= {capacity}) {
            abort();
        }
    }
```

This replaces the default bounds check with a custom one that calls `abort()` instead of the configured panic behavior.

#### 3.31.7 Disabling Transforms

```
$transform_disable safety_array_access
$transform_disable speed_binary_op
```

Disables a built-in transform entirely. The compiler falls back to raw C emission without any wrapping.

#### 3.31.8 Custom Optimizer Example

A programmer can write a transform that adds SIMD-style hints:

```
$transform simd_hint
    match: map_loop
    condition: { $mode == "speed" }
    before: {
        #pragma omp simd
    }
```

This emits `#pragma omp simd` before every `map` loop in speed mode.

#### 3.31.9 Custom Safetifier Example

A programmer can write a transform that logs all array accesses:

```
$transform log_array_access
    match: array_access
    condition: { $bounds == "static" }
    before: {
        if ({index} < 0 || {index} >= {capacity}) {
            fprintf(stderr, "BOUNDS VIOLATION: index=%d capacity=%d\n", {index}, {capacity});
            abort();
        }
    }
```

#### 3.31.10 Custom Overflow Check with Type Metadata

```
$transform my_overflow_check
    match: binary_op("+")
    condition: { $overflow == "checked" }
    before: {
        if ({left} > 0 && {right} > {type_max} - {left}) {
            {panic_code}
        }
        if ({left} < 0 && {right} < {type_min} - {left}) {
            {panic_code}
        }
    }
```

This uses `{type_max}` and `{type_min}` from the type's `from`/`to` metadata properties.

#### 3.31.11 Full Transform Example

```
!META!
    $mode safety
    $c_include stdio.h
    $bounds static
    $overflow checked
    $panic abort

    $transform custom_bounds_check
        match: array_access
        condition: { $bounds == "static" }
        before: {
            if ({index} < 0 || {index} >= {capacity}) {
                fprintf(stderr, "bounds violation at %s:%d: index=%d cap=%d\n", __FILE__, __LINE__, {index}, {capacity});
                abort();
            }
        }

    $transform custom_overflow_check
        match: binary_op("+")
        condition: { $overflow == "checked" }
        before: {
            if ({left} > 0 && {right} > {type_max} - {left}) {
                abort();
            }
        }

    $transform custom_null_check
        match: pointer_deref
        condition: { $null_check == "always" }
        before: {
            if ({pointer} == NULL) {
                abort();
            }
        }

    $transform_disable safety_array_access
    $transform_disable safety_binary_op
    $transform_disable safety_pointer_deref
!END!
```

This defines three custom transforms that replace the built-in safety transforms with versions that call `abort()` and log violations to stderr.

---

## 4. Types

### 4.1 Primitive Types

Primitive types are defined in the metadata DSL. The built-in defaults (used if not redefined):

| Kato Name | Kind | C Type (Speed) | C Type (Safety) | Bits |
|-----------|------|----------------|-----------------|------|
| `int` | integer | `int` | `int32_t` | 32 |
| `int64` | integer | `long long` | `int64_t` | 64 |
| `uint` | integer | `unsigned int` | `uint32_t` | 32 |
| `uint64` | integer | `unsigned long long` | `uint64_t` | 64 |
| `float` | float | `float` | `float` | 32 |
| `double` | float | `double` | `double` | 64 |
| `char` | character | `char` | `char` | 8 |
| `bool` | boolean | `bool` | `bool` | 8 |
| `void` | empty | `void` | `void` | 0 |

In safety mode, all integer types map to fixed-width `<stdint.h>` types. In speed mode, native C types are used.

Custom integer types may use any bit width from 1 to 64:

```
$define %type int34 as integer
    bits: 34
    signed: true
```

`int34` has 34-bit integer semantics. Its valid value range is not widened
to the C backing type. The C backend stores non-standard widths in compact
byte storage (`ceil(bits / 8)` bytes) and emits helper code when scalar
conversion is needed.

### 4.2 Pointer Types

```
int*        // pointer to int
float*      // pointer to float
ParticleSystem*  // pointer to struct
```

### 4.3 Array Types

```
Array<int, 1024>       // fixed-size array of 1024 ints
Array<float, MAX_SIZE> // fixed-size array with compile-time capacity
```

`Array<T, N>` is a compile-time-sized array type. `T` must be a known Kato
type and `N` must resolve to a positive compile-time integer. The C backend
emits a plain C array for now:

```c
int values[1024];
```

The older bracket form (`[int; N]`) is reserved and not the canonical
syntax.

### 4.4 User-Defined Types

Structs, enums, and type aliases are defined in the metadata DSL (see Section 3.3) and declared in the language body (see Section 5).

---

## 5. Data Structures

### 5.1 Structs

Struct declarations in the language body correspond to struct type definitions in the metadata:

```
struct ParticleSystem {
    float x[MAX_PARTICLES];
    float y[MAX_PARTICLES];
    float vx[MAX_PARTICLES];
    float vy[MAX_PARTICLES];
    float lifetime[MAX_PARTICLES];
    bool  active[MAX_PARTICLES];
    int   count;
}
```

The metadata DSL defines the layout, capacity, and bounds-checking properties. The struct declaration defines the field order and types.

### 5.2 Enums

```
enum Color {
    Red,
    Green,
    Blue,
}
```

### 5.3 Constants

```
const MAX_PARTICLES: int = 1024;
const PI: float = 3.14159;
```

Constants must also be declared in the metadata with `$define %const`.

---

## 6. Variables and Immutability

### 6.1 Immutable Variables

```
let x: int = 42;
let pi: float = 3.14;
```

Immutable variables cannot be reassigned.

### 6.2 Mutable Variables

```
let mut count: int = 0;
count = count + 1;
```

### 6.3 Type Inference

```
let x = 42;       // inferred as int
let y = 3.14;     // inferred as float
```

---

## 7. Functions

### 7.1 Function Declaration

```
func name(param: Type, param2: Type) -> ReturnType {
    // body
    final_expression
}
```

The `-> ReturnType` is **optional**. When omitted, the compiler infers the return type from the function's `$define %func` contract in the metadata DSL (`returns` property). This avoids duplicating the return type in both the metadata and the code:

```
// Metadata: $define %func main as start returns: int32
func main() {
    return 0;
}
```

Functions are pure by default. A pure function:
- Cannot modify global state
- Cannot modify its arguments unless they are marked `mut`
- Must return a value (the last expression in the body)
- Has no side effects

### 7.2 Mutable Parameters

```
func particles_update(sys: mut ParticleSystem, dt: float) -> void {
    map (i in 0..sys.count) where sys.active[i] {
        sys.x[i] = sys.x[i] + sys.vx[i] * dt;
    }
}
```

`mut` parameters are listed in the function contract's `mutates` property.

### 7.3 Function Calls

```
let result: int = add(1, 2);
particles_update(sys, 0.016);
```

### 7.4 Early Return

```
func find_first(active: [bool], count: int) -> int {
    let mut i = 0;
    while i < count {
        if active[i] {
            return i;
        }
        i = i + 1;
    }
    return -1;
}
```

In safety mode, early returns are transformed by the compiler into `goto cleanup` with a result variable.

### 7.5 Function Purity Enforcement

The compiler enforces purity based on the metadata contract:
- If `pure: true`, the function cannot have `mut` parameters (except for reading)
- If `pure: false`, the function must list all mutated arguments in `mutates`
- `void` functions must have at least one `mut` parameter

### 7.6 Higher-Order Functions

```
func apply_to_array(arr: mut [int], count: int, f: func(int) -> int) -> void {
    map (i in 0..count) {
        arr[i] = f(arr[i]);
    }
}
```

In safety mode, function pointers are forbidden — the compiler rejects higher-order functions.

---

## 8. Functional Operations

### 8.1 map

```
map (i in 0..count) {
    arr[i] = arr[i] * 2;
}
```

With a condition:

```
map (i in 0..count) where active[i] {
    x[i] = x[i] + vx[i] * dt;
}
```

### 8.2 filter

`filter` collects indices that satisfy a condition into a fixed-size output buffer. It is a **statement**, not an expression — it writes results into pre-declared variables. This avoids hidden dynamic allocation in both speed and safety modes.

```
let mut indices: Array<int, MAX_COUNT> = {0};
let mut found: int = 0;
filter (i in 0..count) where active[i] into indices, found;
// indices[0..found) now contains all i where active[i] == true
// found is the count of matching indices
```

Syntax: `filter (index in start..end) where condition into out_array, out_count;`

- `into out_array` — a mutable fixed-size array of `int` (indices). Must be declared with a compile-time capacity >= the range size.
- `out_count` — a mutable `int` variable that receives the number of matching indices.

The compiler emits a for-loop that iterates the range, evaluates the condition, and writes matching indices into `out_array[out_count]` incrementing `out_count`.

In safety mode, the compiler emits bounds checks on `out_array` writes using the array's declared capacity.

### 8.3 fold

```
let total: float = fold (0.0, i in 0..count) where active[i] {
    acc + lifetime[i]
};
```

Syntax: `fold (initial_value, index in start..end) where condition { body }`

The accumulator is `acc`. The body's last expression becomes the new `acc`.

**Accumulator type inference**: The type of `acc` is inferred from the initial value:
- `fold (0, ...)` → `acc` is `int`
- `fold (0.0, ...)` → `acc` is `float`
- `fold (0.0f, ...)` → `acc` is `float`
- If the initial value is a call to a function, `acc` inherits the function's return type

The compiler uses this inferred type for the C declaration of the accumulator variable and for overflow checks in safety mode.

### 8.4 Range Expressions

```
0..10       // exclusive: 0..9
0..=10      // inclusive: 0..10
start..end  // variable bounds
```

---

## 9. Pattern Matching

### 9.1 match

```
match op {
    '+' => { result = a + b; }
    '-' => { result = a - b; }
    _   => { result = 0; }
}
```

`_` is the wildcard/default case.

### 9.2 match as expression

```
let result: int = match op {
    '+' => a + b,
    '-' => a - b,
    _   => 0,
};
```

### 9.3 match on enums

```
let dx: int = match dir {
    Direction.Up    => 0,
    Direction.Down  => 0,
    Direction.Left  => -1,
    Direction.Right => 1,
};
```

---

## 10. Control Flow

### 10.1 if / else

```
if condition {
    // body
} else if condition2 {
    // body
} else {
    // body
}
```

### 10.2 if as expression

```
let max: int = if a > b { a } else { b };
```

### 10.3 while

```
let mut i = 0;
while i < count {
    i = i + 1;
}
```

In safety mode, `break` and `continue` are forbidden.

### 10.4 break and continue

```
break;
continue;
```

Forbidden in safety mode.

### 10.5 return

```
return value;
return;
```

In safety mode, the compiler transforms all returns into `goto cleanup` with a single exit point.

---

## 11. Operators

### 11.1 Arithmetic

| Operator | Description | Safety Mode |
|----------|-------------|-------------|
| `+` | Addition | Overflow checked |
| `-` | Subtraction | Overflow checked |
| `*` | Multiplication | Overflow checked |
| `/` | Division | Div-by-zero checked |
| `%` | Modulo | Div-by-zero checked |
| `//` | Integer division | Always returns int |

### 11.2 Comparison

`== != < > <= >=`

### 11.3 Logical

`&& || !`

### 11.4 Bitwise

`& | ^ ~ << >>`

### 11.5 Assignment

`= += -= *= /= %=`

### 11.6 Increment / Decrement

`++ --`

### 11.7 Address and Dereference

`&x` `*ptr`

### 11.8 Operator Precedence (highest to lowest)

```
1.  () [] . (postfix)
2.  ! ~ ++ -- (unary prefix)
3.  * / % //
4.  + -
5.  << >>
6.  & ^ |
7.  == != < > <= >=
8.  &&
9.  ||
10. = += -= *= /= %=
11. => (match arm)
```

---

## 12. Arrays

### 12.1 Array Declaration

```
let arr: Array<int, 1024> = {0};
let mut data: Array<float, MAX_SIZE> = {0.0};
```

Array capacity is part of the type. `Array<int, 4>` and `Array<int, 8>`
are different types. The capacity argument must be a positive compile-time
integer or compile-time constant.

### 12.2 Array Access

```
let val: int = arr[0];
arr[0] = 42;
```

### 12.3 Bounds Checking

In speed mode: no bounds checking. Array access compiles to `arr[idx]`.

In safety mode: every array access is preceded by a runtime bounds check using the `capacity` and `max_count_field` from the struct's metadata type definition:

```c
if (idx < 0 || idx >= SAFE_MAX_PARTICLES) {
    // panic
}
```

---

## 13. Compilation Modes

### 13.1 Speed Mode

Speed mode prioritizes performance:

- No bounds checking (unless `$bounds` overridden)
- No NULL pointer checks (unless `$null_check` overridden)
- No overflow checks (unless `$overflow` overridden)
- Swap-and-pop for element deallocation
- Dynamic allocation allowed
- Function pointers allowed
- Early returns allowed
- `break` and `continue` allowed
- Recursion allowed
- Inline assembly allowed

### 13.2 Safety Mode

Safety mode prioritizes correctness:

- Runtime bounds checking using type metadata
- NULL pointer checks on all nullable pointer parameters
- Integer overflow checks using type `from`/`to` ranges
- Division by zero checks
- No dynamic allocation
- No swap-and-pop (elements deactivated, not moved)
- No function pointers
- No recursion (enforced by `recurses: false` contract)
- No `break` or `continue`
- Single exit point
- All variables initialized at declaration
- Cyclomatic complexity <= 10
- Range checks on integer assignments (if `$range_check always`)
- Inline assembly forbidden (unless `$asm allowed`)

### 13.3 Mode-Specific Semantics

| Feature | Speed Mode | Safety Mode |
|---------|-----------|-------------|
| Bounds checking | Disabled | Enabled, using type metadata |
| NULL pointer check | Disabled | Enabled, every nullable pointer param |
| Overflow check | Disabled | Enabled, every arithmetic op using type ranges |
| Div-by-zero check | Disabled | Enabled, every `/` and `%` |
| Range check | Disabled | After assignments (if enabled) |
| Early return | Allowed | Single exit point enforced |
| break/continue | Allowed | Forbidden |
| Recursion | Allowed | Forbidden |
| Dynamic allocation | Allowed | Forbidden |
| Function pointers | Allowed | Forbidden |
| Inline assembly | Allowed | Forbidden (unless `$asm allowed`) |
| Complexity | Unlimited | <= 10 per function |
| Variable init | Optional | Mandatory |

---

## 14. C Interop

Kato has no standard library. All platform functionality is accessed through explicit C FFI declarations in the metadata DSL. The compiler does not implicitly know about any C function — every external function must be declared before use.

### 14.1 Including C Headers

```
!META!
    $c_include stdio.h
    $c_include stdlib.h
!END!
```

### 14.2 Declaring C Functions

Every C function called from Kato must be declared in the metadata with `$c_func`. This makes the platform dependency explicit and trackable:

```
!META!
    $c_func printf as c_function
        header: stdio.h
        returns: int
        args: { const char*, ... }

    $c_func srand as c_function
        header: stdlib.h
        returns: void
        args: { unsigned int }

    $c_func rand as c_function
        header: stdlib.h
        returns: int
        args: { void }
!END!
```

Properties:
- `header` — the C header that declares this function (must also be in `$c_include`)
- `returns` — C return type
- `args` — C argument types (variadic functions use `...` as the last type)

The compiler validates that every `c.function_name(...)` call in the code has a matching `$c_func` declaration. Calling an undeclared C function is a compile-time error.

### 14.3 Calling C Functions

```
let result: int = c.printf("Hello from C!\n");
c.exit(0);
```

`c.function_name(args)` compiles to a direct C call. The function name after `c.` must match a `$c_func` declaration name.

### 14.4 Inline C

```
c.code {
    FILE *f = fopen("data.txt", "r");
    fclose(f);
}
```

Inline C blocks are emitted verbatim into the function body. In safety mode, the compiler scans inline C for forbidden constructs (malloc, free, etc.) and rejects them if `$alloc` is `static` or `none`.

### 14.5 C Exports

Kato functions can be exported for C linkage:

```
!META!
    $c_export my_function
!END!
```

This emits an `extern` declaration in the generated C file so other C translation units can call the function.

### 14.6 Inline Assembly

Kato supports inline assembly through `asm` blocks. The assembly text is captured as raw source — Kato does not tokenize, parse, or validate assembly instructions. The text is passed through to the C compiler via GCC inline assembly.

#### Basic Form

Without operands, `asm` blocks produce basic inline assembly:

```
asm {
    movl $42, %eax
    movl %eax, result
    ret
}
```

In basic form, `%` is treated literally — no operand substitution occurs. This compiles to `__asm__ volatile(...)` with no operand or clobber sections.

#### Extended Form

When operands or clobbers are declared, the `asm` block produces GCC extended inline assembly. This allows referencing C variables from within the assembly code:

```
asm {
    inputs { c: "m" }
    clobbers { "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory" }

    mov $1, %%rax
    mov $1, %%rdi
    leaq %0, %%rsi
    mov $1, %%rdx
    syscall
}
```

This compiles to:

```c
__asm__ volatile (
    "mov $1, %%rax\n"
    "mov $1, %%rdi\n"
    "leaq %0, %%rsi\n"
    "mov $1, %%rdx\n"
    "syscall"
    :
    : "m"(c)
    : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
);
```

#### Sections

Extended `asm` blocks can declare three optional sections at the top, before the assembly body:

- **`inputs { name: "constraint", ... }`** — input operands. Each operand is a C variable available in the asm body as `%0`, `%1`, etc. (in declaration order).
- **`outputs { name: "=constraint", ... }`** — output operands. Each operand is a C variable that receives a value from the asm. Use `=` prefix for write-only constraints, `+` for read-write.
- **`clobbers { "reg", ... }`** — registers and resources modified by the asm. Use `"memory"` to indicate memory is modified.

All sections are optional. An `asm` block with no sections produces basic inline assembly (no operand substitution, `%` is literal). An `asm` block with any section present produces extended inline assembly.

#### Operand References

In extended form:
- `%0`, `%1`, `%2`, ... reference input and output operands in declaration order (outputs first, then inputs)
- `%%reg` references a literal register (e.g., `%%rax`, `%%rdi`)
- In basic form (no sections), `%` is literal — no escaping needed

#### Constraints

Common GCC constraint strings:

| Constraint | Meaning |
|------------|---------|
| `"r"` | Any general-purpose register |
| `"m"` | Memory operand |
| `"i"` | Immediate integer |
| `"=r"` | Output: write-only register |
| `"+r"` | Output: read-write register |
| `"a"` | RAX/EAX/AL register |
| `"D"` | RDI/EDI register |
| `"S"` | RSI/ESI register |
| `"d"` | RDX/EDX register |

#### Example: putchar via Syscall (x86_64 Linux)

```
func putchar(c: char) {
    asm {
        inputs { c: "m" }
        clobbers { "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory" }

        mov $1, %%rax
        mov $1, %%rdi
        leaq %0, %%rsi
        mov $1, %%rdx
        syscall
    }
}
```

This function writes a single character to stdout via the `write` syscall (syscall number 1 on x86_64 Linux). The `"m"` constraint places `c` in memory, `leaq %0, %%rsi` loads its address into RSI, and the syscall writes one byte to file descriptor 1 (stdout).

#### Metadata Directive

```
$asm allowed                    // inline assembly allowed (speed mode default)
$asm never                      // inline assembly forbidden (safety mode default)
```

In safety mode, inline assembly is forbidden by default. Use `$asm allowed` to explicitly enable it:

```
!META!
    $mode safety
    $asm allowed
!END!
```

#### Restrictions

- `asm` blocks are statements only — not expressions
- `asm` blocks do not participate in type checking
- The compiler does not validate assembly syntax — syntax errors are caught by the C compiler
- In safety mode with `$asm never`, the compiler rejects all `asm` blocks at compile time
- Assembly code is target-specific — the same Kato source may not be portable across architectures when using `asm` blocks
- The `syscall` instruction clobbers `rcx` and `r11` — always include them in `clobbers` when using `syscall`

### 14.7 Freestanding Mode

The `-freestand` compiler flag produces C code with no default `#include` directives. Instead of including standard headers, the compiler emits `typedef` and `extern` declarations for the types and functions it needs internally.

```
python3 src/main.py game.kato -freestand -o game.c
```

#### What Changes

In normal mode, the compiler always emits:

```c
#include <stdbool.h>
#include <string.h>
// safety mode also: #include <stdint.h>, #include <stddef.h>
```

In freestanding mode, these are replaced by inline type definitions and extern declarations:

```c
/* freestanding mode: no standard includes */

/* fixed-width integer types (safety mode only) */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

/* boolean type */
typedef _Bool bool;
#define true (_Bool)1
#define false (_Bool)0

/* null pointer */
#define NULL ((void*)0)

/* runtime functions — user must provide implementations */
extern void *memset(void *, int, unsigned long);
extern void abort(void);
```

#### Runtime Functions

The compiler may use the following functions internally:

| Function | When Used | Source |
|----------|-----------|--------|
| `memset` | Struct zero-initialization, safety-mode variable init | User must provide |
| `abort` | Panic mode `abort` | User must provide |
| `__builtin_trap` | Panic mode `trap` | GCC/Clang builtin, no extern needed |

The user must link against implementations of `memset` and `abort` (if panic mode is `abort`). This is similar to how Zig expects the user to provide `panic` — the compiler declares the dependency, the user satisfies it.

#### User-Declared Includes

`$c_include` directives in the metadata are still emitted in freestanding mode. If the user explicitly includes a header, it is included:

```
!META!
    $c_include stdio.h
    $c_func printf as c_function
        header: stdio.h
        returns: int
        args: { const char*, ... }
!END!
```

This produces `#include <stdio.h>` even in freestanding mode. The user is explicitly opting into that dependency.

#### Use Case

Freestanding mode is for:
- Kernel development (no libc available)
- Embedded systems (no standard library)
- Custom runtime environments (user provides all functions)
- Bootloaders and bare-metal programming

#### Example

**game.kato (freestanding):**
```
python3 src/main.py game.kato -freestand -o game.c
```

**game.c (freestanding output):**
```c
/* freestanding mode: no standard includes */

/* boolean type */
typedef _Bool bool;
#define true (_Bool)1
#define false (_Bool)0

/* null pointer */
#define NULL ((void*)0)

/* runtime functions — user must provide implementations */
extern void *memset(void *, int, unsigned long);
extern void abort(void);

void
putchar(char c);
int
main(void);

void
putchar(char c)
{
    __asm__ volatile (
        "mov $1, %%rax\n"
        ...
    );
    return;
}

int
main(void)
{
    putchar('\x68');
    ...
    return (0);
}
```

The user must compile this with their own `memset` and `abort` implementations (or use `-nostdlib` with a custom runtime).

---

## 14.5 Raw Pointers ($unsafe rawptr)

`rawptr` is a raw memory pointer type. Maps to `volatile unsigned char *` in C. Disabled by default in all modes. Enabled via `$unsafe rawptr` directive.

```
$unsafe rawptr
```

Without the directive, `rawptr` usage is a compile error.

Operations:
- Integer literal assignment: `let p: rawptr = 0xB8000;` → cast to `(volatile unsigned char *)`
- Byte indexing: `p[offset] = value;` (read/write single byte)
- Function parameters: `func f(p: rawptr) { ... }`

---

## 15. Modules and Imports

Kato supports multi-file compilation through a module system. Each `.kato` file is a module. Modules import other modules to access their exported types, functions, and constants.

### 15.1 Importing Modules

```
import particle_system;
import vector_math;
```

The `import` statement appears at the top of the file, before any declarations. It is optional — only needed when the file uses exported code (functions, structs) from the imported module. Metadata-only imports (types, contracts, C interop) do not require `import` in the code body. The compiler resolves module names to `.kato` files in the same directory or in import paths specified by the compiler.

### 15.2 Metadata for Imports

```
!META!
    $import particle_system
    $import vector_math
!END!
```

The `$import` directive in the metadata declares which modules this file depends on. The `import` statement in the code body is optional — it is only needed when accessing exported code (functions, structs) from the imported module. Metadata-only imports do not require `import` in the code body.

### 15.3 Metadata-Only Modules

A `.kato` file can contain only a `!META!` block with no code. Such files serve as **metadata preludes** — shared declarations of type definitions, function contracts, `$c_func` declarations, `$c_include` directives, compile-time variables, and constants.

When a file imports a metadata-only module via `$import`, it receives:
- Type definitions (`$define %type`)
- Function contracts (`$define %func`)
- Constants (`$define %const`)
- Compile-time variables (`$let`)
- C interop declarations (`$c_include`, `$c_func`, `$c_flag`, `$c_prefix`, `$c_no_prefix`)
- Compile-time assertions (`$assert`)
- Custom transforms (`$transform`, `$transform_disable`)
- Emit directives (`$emit`)

Visibility (`$space`) is **not** inherited from metadata-only modules. Each file that implements functions must declare its own `$space %export` or `$space %internal` for those functions.

Function contracts declared in a metadata-only module are available to the importing file. The importing file does not re-declare the contract — it only declares `$space` and implements the function body. Duplicate contract declarations across modules are an error.

Example:

**prelude.kato** (metadata only):
```
!META!
    $mode speed

    $c_include stdio.h
    $c_include stdlib.h

    $c_func printf as c_function
        header: stdio.h
        returns: int
        args: { const char*, ... }

    $c_func malloc as c_function
        header: stdlib.h
        returns: void*
        args: { unsigned long }

    $define %type int32 as integer
        bits: 32
        signed: true
        from: {-2147483648}
        to: {2147483647}

    $define %func add as function
        args: { a: int32, b: int32 }
        returns: int32
        pure: true

    $define %func main as start
        args: { void }
        returns: int32
!END!
```

**math.kato:**
```
!META!
    $mode speed
    $import prelude
    $space %export add
!END!

func add(a: int, b: int) {
    a + b
}
```

**main.kato:**
```
!META!
    $mode speed
    $import prelude
    $space %export main
!END!

func main() {
    let result = add(10, 20);
    c.printf("result = %d\n", result);
    return 0;
}
```

### 15.4 Module Resolution

The compiler resolves imports in the following order:
1. Same directory as the importing file: `particle_system.kato`
2. Import paths specified via `-I` compiler flag
3. Standard library path (if configured)

Each imported module is compiled recursively: its metadata is parsed and merged, and its exported symbols become available to the importing module.

### 15.5 Visibility Across Modules

Only symbols declared in `$space %export` are visible to importing modules. Symbols in `$space %internal` are file-local and cannot be accessed from other modules.

When a module imports another, the importing module can use:
- Exported types (structs, enums) in function signatures and variable declarations
- Exported functions in calls
- Exported constants in expressions

The compiler enforces visibility at compile time: referencing an internal symbol from another module is an error.

### 15.6 Multi-File Compilation

The compiler accepts multiple input files. It compiles each file independently (lexer, parser, metadata, typecheck) and then merges the results:

1. **Metadata merge**: Type definitions, function contracts, constants, and compile-time variables from all files are merged. Duplicate definitions for the same name across files are an error, unless the name is re-exported.
2. **AST merge**: Structs, enums, constants, and functions from all files are combined into a single program.
3. **Code generation**: One `.c` file is generated per `.kato` file, or a single combined `.c` file if `-o` is specified.
4. **Start function**: Exactly one `start` function must exist across all files.

### 15.7 Import Cycle Detection

Import cycles are forbidden. If module A imports module B, and module B imports module A (directly or transitively), the compiler reports an error. Modules must form a directed acyclic graph (DAG).

### 15.8 Example: Multi-File Project

**vector_math.kato:**
```kato
!META!
    $mode speed
    $define %type float32 as float bits:32 encoding:ieee754 precision:single
    $define %func vec2_dot as function
        args: { ax: float32, ay: float32, bx: float32, by: float32 }
        returns: float32
        pure: true
    $space %export vec2_dot
!END!

func vec2_dot(ax: float, ay: float, bx: float, by: float) -> float {
    ax * bx + ay * by
}
```

**main.kato:**
```kato
!META!
    $mode speed
    $import vector_math
    $c_include stdio.h
    $c_func printf as c_function header:stdio.h returns:int args:{const char*, ...}
    $define %func main as start args:{void} returns:int
    $space %export main
!END!

import vector_math;

func main() -> int {
    let result = vec2_dot(1.0, 2.0, 3.0, 4.0);
    c.printf("dot = %f\n", result);
    return 0;
}
```

---

## 16. Complete Examples

### 16.1 Speed Mode: Particle System

```kato
!META!
    $mode speed
    $layout soa
    $opt vectorize
    $opt restrict

    $let MAX_PARTICLES = 1024

    $assert { MAX_PARTICLES > 0 }
    $assert { MAX_PARTICLES <= 65536 }

    $if { MAX_PARTICLES > 512 } then
        $unroll count(8) particles_update
    $else
        $unroll count(4) particles_update
    $end

    $define %type int32 as integer
        bits: 32
        signed: true
        from: {-2147483648}
        to: {2147483647}

    $define %type float32 as float
        bits: 32
        encoding: ieee754
        precision: single

    $define %type bool as boolean
        bits: 8
        true_value: {1}
        false_value: {0}

    $define %type ParticleSystem as struct
        layout: soa
        fields: {
            x: float32[ MAX_PARTICLES ],
            y: float32[ MAX_PARTICLES ],
            vx: float32[ MAX_PARTICLES ],
            vy: float32[ MAX_PARTICLES ],
            lifetime: float32[ MAX_PARTICLES ],
            active: bool[ MAX_PARTICLES ],
            count: int32
        }
        max_count_field: count
        capacity: { MAX_PARTICLES }

    $define %const MAX_PARTICLES = { 1024 }

    $define %func main as start
        args: { void }
        returns: int32

    $define %func particles_update as function
        args: { sys: ParticleSystem*, dt: float32 }
        returns: void
        pure: false
        mutates: { sys }
        inline: always
        unroll: count(4)
        complexity: max(8)
        allocates: false
        recurses: false

    $define %func particles_spawn as function
        args: { sys: ParticleSystem*, px: float32, py: float32, pvx: float32, pvy: float32 }
        returns: int32
        pure: false
        mutates: { sys }

    $define %func particles_deactivate as procedure
        args: { sys: ParticleSystem*, index: int32 }
        returns: void
        pure: false
        mutates: { sys }

    $define %func particles_count_active as function
        args: { sys: ParticleSystem* }
        returns: int32
        pure: true

    $space %export main, ParticleSystem, particles_update, particles_spawn, particles_deactivate, particles_count_active
    $space %internal update_position
!END!

const MAX_PARTICLES: int = 1024;

struct ParticleSystem {
    float x[MAX_PARTICLES];
    float y[MAX_PARTICLES];
    float vx[MAX_PARTICLES];
    float vy[MAX_PARTICLES];
    float lifetime[MAX_PARTICLES];
    bool  active[MAX_PARTICLES];
    int   count;
}

func update_position(pos: float, vel: float, dt: float) -> float {
    pos + vel * dt
}

func particles_update(sys: mut ParticleSystem, dt: float) -> void {
    map (i in 0..sys.count) where sys.active[i] {
        sys.x[i] = update_position(sys.x[i], sys.vx[i], dt);
        sys.y[i] = update_position(sys.y[i], sys.vy[i], dt);
        sys.lifetime[i] = sys.lifetime[i] - dt;
    }
}

func particles_spawn(sys: mut ParticleSystem, px: float, py: float, pvx: float, pvy: float) -> int {
    let idx = sys.count;
    sys.x[idx] = px;
    sys.y[idx] = py;
    sys.vx[idx] = pvx;
    sys.vy[idx] = pvy;
    sys.lifetime[idx] = 0.0;
    sys.active[idx] = true;
    sys.count = idx + 1;
    idx
}

func particles_deactivate(sys: mut ParticleSystem, index: int) -> void {
    if index < 0 || index >= sys.count {
        return;
    }
    sys.count = sys.count - 1;
    if index != sys.count {
        sys.x[index]        = sys.x[sys.count];
        sys.y[index]        = sys.y[sys.count];
        sys.vx[index]       = sys.vx[sys.count];
        sys.vy[index]       = sys.vy[sys.count];
        sys.lifetime[index] = sys.lifetime[sys.count];
        sys.active[index]   = sys.active[sys.count];
    }
}

func particles_count_active(sys: ParticleSystem) -> int {
    fold (0, i in 0..sys.count) where sys.active[i] {
        acc + 1
    }
}

func main() -> int {
    let mut sys: ParticleSystem = {0};
    let idx = particles_spawn(&sys, 0.0, 0.0, 1.0, 0.5);
    particles_update(&sys, 0.016);
    let active_count = particles_count_active(sys);
    particles_deactivate(&sys, idx);
    return 0;
}
```

### 16.2 Safety Mode: Same Source, Different Semantics

The same particle system logic, but with safety metadata that produces radically different C:

```kato
!META!
    $mode safety
    $layout soa
    $bounds static
    $overflow checked
    $init zero
    $null_check always
    $div_check always
    $alloc static
    $thread single
    $complexity max(10)
    $panic halt
    $range_check always

    $let SAFE_MAX_PARTICLES = 1024

    $assert { SAFE_MAX_PARTICLES > 0 }
    $assert { SAFE_MAX_PARTICLES <= 65536 }

    $define %type int32 as integer
        bits: 32
        signed: true
        from: {-2147483648}
        to: {2147483647}

    $define %type float32 as float
        bits: 32
        encoding: ieee754
        precision: single

    $define %type bool as boolean
        bits: 8
        true_value: {1}
        false_value: {0}

    $define %type SafeParticleSystem as struct
        layout: soa
        fields: {
            x: float32[ SAFE_MAX_PARTICLES ],
            y: float32[ SAFE_MAX_PARTICLES ],
            vx: float32[ SAFE_MAX_PARTICLES ],
            vy: float32[ SAFE_MAX_PARTICLES ],
            lifetime: float32[ SAFE_MAX_PARTICLES ],
            active: bool[ SAFE_MAX_PARTICLES ],
            count: int32
        }
        max_count_field: count
        capacity: { SAFE_MAX_PARTICLES }

    $define %const SAFE_MAX_PARTICLES = { 1024 }

    $define %func main as start
        args: { void }
        returns: int32

    $define %func safe_particles_update as procedure
        args: { sys: SafeParticleSystem*, dt: float32 }
        returns: void
        pure: false
        mutates: { sys }
        complexity: max(10)
        allocates: false
        recurses: false

    $define %func safe_particles_spawn as function
        args: { sys: SafeParticleSystem*, px: float32, py: float32, pvx: float32, pvy: float32, out_index: int32* }
        returns: bool
        pure: false
        mutates: { sys, out_index }
        complexity: max(10)
        allocates: false
        recurses: false

    $define %func safe_particles_deactivate as procedure
        args: { sys: SafeParticleSystem*, index: int32 }
        returns: void
        pure: false
        mutates: { sys }
        complexity: max(10)

    $define %func safe_particles_count_active as function
        args: { sys: SafeParticleSystem* }
        returns: int32
        pure: true
        complexity: max(10)

    $space %export main, SafeParticleSystem, safe_particles_update, safe_particles_spawn, safe_particles_deactivate, safe_particles_count_active
!END!

const SAFE_MAX_PARTICLES: int = 1024;

struct SafeParticleSystem {
    float x[SAFE_MAX_PARTICLES];
    float y[SAFE_MAX_PARTICLES];
    float vx[SAFE_MAX_PARTICLES];
    float vy[SAFE_MAX_PARTICLES];
    float lifetime[SAFE_MAX_PARTICLES];
    bool  active[SAFE_MAX_PARTICLES];
    int   count;
}

func safe_particles_update(sys: mut SafeParticleSystem, dt: float) -> void {
    map (i in 0..sys.count) where sys.active[i] {
        sys.x[i] = sys.x[i] + sys.vx[i] * dt;
        sys.y[i] = sys.y[i] + sys.vy[i] * dt;
        sys.lifetime[i] = sys.lifetime[i] - dt;
    }
}

func safe_particles_spawn(sys: mut SafeParticleSystem, px: float, py: float, pvx: float, pvy: float, out_index: mut int) -> bool {
    if sys.count >= SAFE_MAX_PARTICLES {
        return false;
    }
    let idx = sys.count;
    sys.x[idx] = px;
    sys.y[idx] = py;
    sys.vx[idx] = pvx;
    sys.vy[idx] = pvy;
    sys.lifetime[idx] = 0.0;
    sys.active[idx] = true;
    sys.count = idx + 1;
    out_index = idx;
    true
}

func safe_particles_deactivate(sys: mut SafeParticleSystem, index: int) -> void {
    if index < 0 || index >= sys.count {
        return;
    }
    sys.active[index] = false;
}

func safe_particles_count_active(sys: SafeParticleSystem) -> int {
    fold (0, i in 0..sys.count) where sys.active[i] {
        acc + 1
    }
}

func main() -> int {
    let mut sys: SafeParticleSystem = {0};
    let mut idx: int = 0;
    let ok = safe_particles_spawn(&sys, 0.0, 0.0, 1.0, 0.5, &idx);
    safe_particles_update(&sys, 0.016);
    let active_count = safe_particles_count_active(sys);
    safe_particles_deactivate(&sys, idx);
    return 0;
}
```

---

## 17. Error Handling

### 17.1 Compile-Time Errors

```
error: particles.kato:15:8: cannot mutate immutable parameter 'sys'
    sys.x[0] = 1.0;
       ^^^
```

### 17.2 Error Categories

- **Lexical**: invalid token, unterminated string
- **Syntax**: unexpected token, missing semicolon, unbalanced braces
- **Type**: type mismatch, unknown type, incompatible operation
- **Purity**: mutating immutable variable, side effect in pure function
- **Contract**: function violates its metadata contract (pure, allocates, recurses, complexity)
- **Metadata**: invalid directive, missing required declaration, conflicting directives
- **Compile-Time**: `$assert` failure, undefined `$let` variable, invalid compile-time expression
- **Safety**: forbidden construct in safety mode (recursion, break, function pointer, dynamic alloc)
- **Visibility**: calling internal function from another module, undeclared symbol

### 17.3 Safety Mode Violations

In safety mode, the compiler rejects:

- `break` and `continue` statements
- Recursive function calls (enforced by `recurses: false`)
- Function pointer types and higher-order functions
- Dynamic allocation functions
- Variable-length arrays
- Pointer arithmetic
- `union` types
- Bitfields
- Cyclomatic complexity > 10 (enforced by `complexity: max(N)`)
- Assignment outside type `from`/`to` range (if `$range_check always`)

---

## 18. Rules and Restrictions

- Every file must begin with a `!META!` block
- Every file must declare exactly one mode (`speed` or `safety`)
- Every function and struct must be declared in `%export` or `%internal`
- Every function must have a `$define %func` contract in the metadata
- Every struct must have a `$define %type` definition in the metadata
- There must be exactly one `start` function across all files
- Functions must be declared before use (top-to-bottom within a file, or via import)
- All code must be inside functions
- Semicolons are required after statements
- Global mutable state is forbidden
- Variables cannot be redeclared in the same scope
- Struct fields cannot be duplicated within a struct
- The `main` function (declared as `start`) must not have parameters
- All `$assert` expressions must evaluate to `true` at compile time
- All `$let` variables must be defined before use in compile-time expressions
- `asm` blocks are forbidden in safety mode unless `$asm allowed` is set
