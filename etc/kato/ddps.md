# Data-Driven Procedural Style (DDPS) - C language coding style

Data-Driven Procedural Style (DDPS) is a C coding style that blends DOD (Data-Oriented Design) with Procedural Programming. It prioritizes cache-friendly memory layouts (specifically Structure of Arrays) and stateless procedural pipelines over object-oriented or pointer-heavy pointer-chasing structures.

---

## 1. philosophy

1. **Separation of Data and Logic**: Data is represented as clean, flat, passive structures. Logic is implemented as stateless procedures operating on that data.
2. **Memory-First Layout**: Design data structures for optimal cache utilization. Prefer **Structure of Arrays (SoA)** over Array of Structures (AoS) for bulk processing.
3. **Stateless Pipelines**: Functions should be pure, linear procedures that process sequential slices of memory. Minimize branching inside performance-critical loops.
4. **No Pointer Chasing**: Avoid linked lists, trees, and deep pointer hierarchies. Use flat arrays, indices, and handles.

---

## 2. Naming Conventions

* **Structs**: `PascalCase` (e.g., `ParticleSystem`).
* **Functions**: `snake_case` (e.g., `particles_update_positions`).
* **Variables and Parameters**: `snake_case` (e.g., `active_count`).
* **Constants and Macros**: `UPPER_SNAKE_CASE` (e.g., `MAX_PARTICLES`).
* **File Names**: `snake_case` matching the subsystem (e.g., `particle_system.h`).

---

## 3. Data Layout Guidelines (SoA)

When designing data structures, group related fields into parallel arrays inside a single managing structure rather than wrapping them in an individual entity structure.

### Bad (AoS)
```c
//inefficient for bulk processing
typedef struct {
    float x, y;
    float vx, vy;
    float lifetime;
    bool active;
} Particle;

typedef struct {
    Particle list[1024];
    int count;
} ParticleSystem;
```

### Good (SoA)
```c
/* !DEFINES!

$define %type float as 32 bit float
$define %type bool as 8 bit boolean
$define %type int as 32 bit signed
$define %type ParticleSystem as struct with parallel arrays for particle data

*/

/* !SPACE!

$space %export ParticleSystem

*/

//good for cache utilization and auto-vectorization
typedef struct {
	float x[1024];
	float y[1024];
	float vx[1024];
	float vy[1024];
	float lifetime[1024];
	bool  active[1024];
	int   count;
} ParticleSystem;
```

---

## 4. Function & Procedure Guidelines

1. **Minimize Scope**: Functions should only receive the specific arrays they need to read or write, rather than the entire system context.
2. **Restrict Pointer Aliasing**: Use the `restrict` keyword on pointer parameters to enable aggressive compiler optimizations and auto-vectorization.
3. **Linear Access Patterns**: Keep memory access sequential. Avoid random access indices inside high-performance loops.

### Example Function Implementation
```c
/* !DEFINES!

$define %type float as 32 bit float
$define %type bool as 8 bit boolean
$define %type int as 32 bit signed

$define %func particles_update_positions as procedure with args float*, float*, const float*, const float*, const bool*, int, float

*/

/* !SPACE!

$space %export particles_update_positions

*/

//procedure receives pointers to the arrays it actually modifies/reads
void
particles_update_positions(
	float *restrict x,
	float *restrict y,
	const float *restrict vx,
	const float *restrict vy,
	const bool *restrict active,
	int count,
	float dt
)
{
	int i;

	for (i = 0; i < count; ++i) {
		if (active[i]) {
			x[i] += vx[i] * dt;
			y[i] += vy[i] * dt;
		}
	}
}
```

---

## 5. Entity Management

To maintain cache density always keep active items packed at the beginning of the arrays

* **Allocation**: Increment the `count` and initialize fields at the new index.
* **Deallocation (Swap-and-Pop)**: Swap the deactivated element with the last active element in the arrays, then decrement the `count` to maintain a contiguous active block.

### Example: Swap-and-Pop
```c
/* !DEFINES!

$define %type int as 32 bit signed
$define %type ParticleSystem as struct with parallel arrays for particle data

$define %func particle_system_deactivate as procedure with args ParticleSystem*, int

*/

/* !SPACE!

$space %export particle_system_deactivate

*/

void
particle_system_deactivate(ParticleSystem *sys, int index)
{
	if (index < 0 || index >= sys->count) {
		return;
	}

	sys->count--;

	if (index == sys->count) {
		return;
	}

	sys->x[index]        = sys->x[sys->count];
	sys->y[index]        = sys->y[sys->count];
	sys->vx[index]       = sys->vx[sys->count];
	sys->vy[index]       = sys->vy[sys->count];
	sys->lifetime[index] = sys->lifetime[sys->count];
	sys->active[index]   = sys->active[sys->count];
}
```

---

## 6. Dynamic Memory Management

In DDPS, individual element allocation (`malloc` for a single entity or struct) is strictly forbidden. It leads to heap fragmentation and pointer chasing. Instead, use bulk allocation patterns.

### 1. Bulk Allocation & Arenas
* Memory must be pre-allocated in large blocks (Arenas or Pools) at startup or subsystem initialization.
* Performance-critical procedures and processing loops must never perform dynamic allocations or deallocations.

### 2. Dynamically Resizable SoA
If the maximum capacity is not known at compile time, manage all parallel arrays inside a single allocation or via synchronized reallocations.

#### Example: Dynamically Resizable SoA
```c
/* !DEFINES!

$define %type float as 32 bit float
$define %type bool as 8 bit boolean
$define %type int as 32 bit signed
$define %type DynamicParticleSystem as struct with dynamic parallel arrays

$define %func dynamic_particles_init as function with args DynamicParticleSystem*, int
$define %func dynamic_particles_reserve as function with args DynamicParticleSystem*, int
$define %func dynamic_particles_free as procedure with args DynamicParticleSystem*

*/

/* !SPACE!

$space %export DynamicParticleSystem, dynamic_particles_init, dynamic_particles_reserve, dynamic_particles_free

*/

typedef struct {
	float *x;
	float *y;
	float *vx;
	float *vy;
	float *lifetime;
	bool  *active;
	int   count;
	int   capacity;
} DynamicParticleSystem;

//initialize the entire system in bulk
bool
dynamic_particles_init(DynamicParticleSystem *sys, int initial_capacity)
{
	sys->x        = malloc(sizeof(float) * initial_capacity);
	sys->y        = malloc(sizeof(float) * initial_capacity);
	sys->vx       = malloc(sizeof(float) * initial_capacity);
	sys->vy       = malloc(sizeof(float) * initial_capacity);
	sys->lifetime = malloc(sizeof(float) * initial_capacity);
	sys->active   = malloc(sizeof(bool) * initial_capacity);

	if (!sys->x || !sys->y || !sys->vx || !sys->vy ||
	    !sys->lifetime || !sys->active) {
		free(sys->x);
		free(sys->y);
		free(sys->vx);
		free(sys->vy);
		free(sys->lifetime);
		free(sys->active);
		return (false);
	}

	sys->count    = 0;
	sys->capacity = initial_capacity;
	return (true);
}

//resize all parallel arrays
bool
dynamic_particles_reserve(DynamicParticleSystem *sys, int new_capacity)
{
	float *new_x;
	float *new_y;
	float *new_vx;
	float *new_vy;
	float *new_lifetime;
	bool  *new_active;

	if (new_capacity <= sys->capacity) {
		return (true);
	}

	new_x        = realloc(sys->x,        sizeof(float) * new_capacity);
	new_y        = realloc(sys->y,        sizeof(float) * new_capacity);
	new_vx       = realloc(sys->vx,       sizeof(float) * new_capacity);
	new_vy       = realloc(sys->vy,       sizeof(float) * new_capacity);
	new_lifetime = realloc(sys->lifetime, sizeof(float) * new_capacity);
	new_active   = realloc(sys->active,   sizeof(bool)  * new_capacity);

	if (!new_x || !new_y || !new_vx || !new_vy ||
	    !new_lifetime || !new_active) {
		if (new_x)        sys->x        = new_x;
		if (new_y)        sys->y        = new_y;
		if (new_vx)       sys->vx       = new_vx;
		if (new_vy)       sys->vy       = new_vy;
		if (new_lifetime) sys->lifetime = new_lifetime;
		if (new_active)   sys->active   = new_active;
		return (false);
	}

	sys->x        = new_x;
	sys->y        = new_y;
	sys->vx       = new_vx;
	sys->vy       = new_vy;
	sys->lifetime = new_lifetime;
	sys->active   = new_active;
	sys->capacity = new_capacity;
	return (true);
}

//free when shutting down
void
dynamic_particles_free(DynamicParticleSystem *sys)
{
	free(sys->x);
	free(sys->y);
	free(sys->vx);
	free(sys->vy);
	free(sys->lifetime);
	free(sys->active);
	sys->x        = NULL;
	sys->y        = NULL;
	sys->vx       = NULL;
	sys->vy       = NULL;
	sys->lifetime = NULL;
	sys->active   = NULL;
	sys->count    = 0;
	sys->capacity = 0;
}
```

---

## 7. Multithreading & Parallel Processing

DDPS and SoA memory layouts are exceptionally well-suited for multithreading. Because data is stored in contiguous parallel arrays and processed by stateless procedures, workloads can be easily distributed across multiple CPU cores without complex locks.

### 1. Data Partitioning (Range Splitting)
* Divide the large continuous array `[0, count)` into independent ranges (chunks) for each thread.
* **No Synchronization inside loops**: Because each thread operates on a distinct, non-overlapping index range of the parallel arrays, no mutexes, semaphores, or atomic operations are needed.
* **Avoid False Sharing**: Ensure chunk sizes are large enough (or aligned to 64-byte cache line boundaries) to prevent multiple CPU cores from writing to the same cache line.

### 2. Double Buffering (Read/Write Separation)
If an update procedure requires reading neighboring elements that might be modified by other threads:
* Never read and write to the same array simultaneously across threads.
* Maintain two instances of the arrays: `ReadState` (immutable during the frame) and `WriteState` (mutable).
* Threads read safely from `ReadState` and write to their partitioned ranges in `WriteState`.
* Swap the read/write pointers at the end of the frame/update cycle.

#### Example: Range-Partitioned Multithreaded Update
```c
/* !DEFINES!

$define %type float as 32 bit float
$define %type bool as 8 bit boolean
$define %type int as 32 bit signed
$define %type ThreadWorkUnit as struct with thread range and array pointers

$define %func particles_update_range_worker as function with args void*

*/

/* !SPACE!

$space %export ThreadWorkUnit, particles_update_range_worker

*/

typedef struct {
	float *x;
	float *y;
	float *vx;
	float *vy;
	bool  *active;
	int   start_index;
	int   end_index;
	float dt;
} ThreadWorkUnit;

//the thread procedure: processes only its assigned range of the SoA
void *
particles_update_range_worker(void *arg)
{
	float           *restrict x;
	float           *restrict y;
	const float     *restrict vx;
	const float     *restrict vy;
	const bool      *restrict active;
	ThreadWorkUnit  *work;
	float           dt;
	int             i;

	work   = (ThreadWorkUnit *)arg;
	x      = work->x;
	y      = work->y;
	vx     = work->vx;
	vy     = work->vy;
	active = work->active;
	dt     = work->dt;

	for (i = work->start_index; i < work->end_index; ++i) {
		if (active[i]) {
			x[i] += vx[i] * dt;
			y[i] += vy[i] * dt;
		}
	}

	return (NULL);
}
```

### 3. Memory-Constrained Multithreading
Double Buffering is highly efficient but doubles memory consumption, which can be prohibitive in memory-constrained environments. If memory is critical, use one of the following strategies to process data in-place safely without full duplication:

#### A. Boundary Halo Buffering (Local Scratchpads)
* Instead of duplicating the entire SoA array, allocate a tiny temporary thread-local or shared scratchpad buffer.
* Copy only the boundary/neighbor elements (halo cells) that are shared between thread range edges into the scratchpad.
* This drops memory overhead from $O(N)$ (duplicating the whole array) to $O(T)$ where $T$ is the number of threads (storing just a few elements at the slice boundaries).

#### B. Independent Phase Split (Barrier Synchronization)
Divide the updates into two distinct execution phases separated by a lightweight thread barrier:
1. **Phase 1 (Parallel Inner Update)**: Each thread processes its internal elements (which do not require reading cross-thread boundary neighbors) concurrently with zero synchronization.
2. **Phase 2 (Synchronized Boundary Update)**: Threads synchronize at a barrier, then cooperatively or sequentially update the shared boundary elements, ensuring safe reads of already updated neighboring values.

#### C. Spatial Tiling & Thread Assignment
For spatial simulations (e.g. physics engines, grids):
* Group elements into spatial grids or tiles.
* Assign threads to non-adjacent tiles (e.g., in a checkerboard pattern).
* Threads process active tiles in parallel without locks because no two active threads share a border.
* Switch active tile groups sequentially to finish the remaining boundary tiles.

---

## 8. Code Style & Formatting Rules

This section defines the rules for code syntax, layout, and style, adapted from FreeBSD's `style(9)` guidelines but tailored for modern procedural and DDPS projects.

### 1. File Headers (`!DEFINES!` & `!SPACE!`)
Every source file (`.c`, `.h`, etc.) must begin with a manifest consisting of a `/* !DEFINES! ... */` block followed by a `/* !SPACE! ... */` block. These blocks outline the main types, functions, and visibility scopes of the file to make it self-documenting.

#### Format & Rules
* **`!DEFINES!` Block**: Placed at the very beginning of the file to declare types and functions.
  * Syntax for type definitions: `$define %type <C_type> as <semantic_description>`
  * The semantic description is a short, dry explanation of the type's role or size (e.g., `$define %type int as 32 bit signed`, `$define %type ParticleSystem as struct with parallel arrays for particle data`). This is documentation only — no typedef aliases are created.
  * Syntax for function definitions:
    * For procedures (no return value): `$define %func <name> as procedure with args <arguments>`
    * For functions (with return value): `$define %func <name> as function with args <arguments>`
    * For entry points: `$define %func <name> as start with args <arguments>`
* **`!SPACE!` Block**: Placed immediately after the `!DEFINES!` block to declare function visibility scopes (which functions are internal/private and which are exported/public).
  * Syntax for internal functions: `$space %internal <function1>, <function2>, ...`
  * Syntax for exported functions: `$space %export <function3>, <function4>, ...`

#### Example:
```c
/* !DEFINES!

$define %type int as 32 bit signed
$define %type char as 8 bit signed

$define %func main as start with args int, char**
$define %func add as procedure with args int, int
$define %func sub as function with args int, int

*/

/* !SPACE!

$space %export main
$space %internal add, sub

*/
```

### 2. Indentation & Spacing
* **Indentation**: Use hard tabs for indentation (tab width of 8 or 4 columns). Align wrapped lines and inline comments with spaces.
* **Keyword Spacing**: Put a space after control keywords (`if`, `for`, `while`, `switch`, `return`). Do not put spaces between a function name and its argument list (e.g., `particles_update(sys, dt)`).
* **Binary Operators**: Surround binary operators (`=`, `+`, `-`, `*`, `/`, `<`, `>`, `==`, `&&`, `||`) with a single space. Do not add spaces around unary operators (`!`, `~`, `++`, `--`, `&`, `*`).

### 3. Brace Style (K&R Variation)
* Place the opening brace `{` on the same line as the control statement (`if`, `for`, `while`, `switch`).
* Place the opening brace `{` on a new line at the start of a function definition.
* Place the closing brace `}` on its own line, except for `else` which should be placed on the same line as the closing brace of the `if` block (e.g., `} else {`).

#### Example:
```c
/* !DEFINES!

$define %type int as 32 bit signed

$define %func compute_sum as function with args int, int

*/

/* !SPACE!

$space %internal compute_sum

*/

int
compute_sum(int a, int b)
{
	if (a > b) {
		return (a);
	} else {
		return (b);
	}
}
```

### 4. Variable Declarations
* **Block Placement**: All local variables must be declared at the beginning of the function or block. Mixing declarations with executable code (mid-block declarations) is forbidden.
* **Sorting & Ordering**: Declarations must be sorted by type size (largest to smallest, e.g., structs/pointers first, then `double`, `float`, `int`, `short`, `char`). Within the same type size, variables must be grouped by semantic purpose (e.g., position coordinates together, velocity vectors together, state flags together), and ordered logically within each group.
* **Grouping**: Group multiple variables of the same type on a single line. If a line exceeds 80 characters, repeat the type keyword on the next line.
* **Initialization**: Avoid initializing variables directly in the declaration block to prevent obfuscation. Do not use function calls or complex expressions in initializers.
* **Pointers**: Do not put any whitespace between the asterisk `*` and the variable name (e.g., `char *str;` instead of `char* str` or `char * str`).

#### Example:
```c
/* !DEFINES!

$define %type char as 8 bit signed
$define %type int as 32 bit signed
$define %type double as 64 bit float
$define %type particle as struct with fields x, y, vx, vy, lifetime

$define %func update_data as procedure with args void

*/

/* !SPACE!

$space %internal update_data

*/

void
update_data(void)
{
	struct particle system;       /* Structs first (largest) */
	int             *data;       /* Core data buffer (8 bytes) */
	char            *name;       /* Metadata: identifier (8 bytes) */
	double          timestamp;   /* Temporal data (8-byte float) */
	int             count, index; /* Iteration state (4-byte int) */

	data = get_data_ptr();       /* Assignment belongs in the code body */
}
```

### 5. Line Wrapping & Length
* Keep line length within 80 characters.
* If a line must be wrapped, indent the subsequent lines with a tab or align them with the opening parenthesis of the statement.

### 6. Return Statements
use `return (value)` instead of `return value;`

---

## 9. SAFETY: Safety-Critical Mode

**Sections 1 through 7 are nullified under this mode.** All rules about cache optimization, `restrict` qualifiers, auto-vectorization, swap-and-pop, dynamic resizable SoA, double buffering, and multithreaded range partitioning are **suspended**. They exist for the standard (fast) mode only.

This section defines an independent DDPS variant — **Safety-Critical Mode** — where the sole objective is provable correctness and maximum safety. Speed, cache utilization, and throughput are irrelevant concerns here. Every rule in this section overrides any conflicting rule from sections 1–7.

**Section 8 (Code Style & Formatting Rules) remains fully in effect.** The `!DEFINES!` / `!SPACE!` manifest blocks, naming conventions, brace style, indentation, variable declaration ordering, and return statement rules are unchanged. Only the *semantic rules* (data layout, memory management, function contracts, control flow) are replaced.


---

### 9.1 Safety-First Philosophy

1. **Correctness Over Performance**: If a construct can be proven correct under all inputs, it is acceptable. If it cannot, it is forbidden — regardless of how fast it runs.
2. **Fail-Closed**: On any error, anomaly, or unexpected condition, the system must halt or return a safe error state. Silent recovery from undefined behavior is prohibited.
3. **Provable Bounds**: Every array access, every loop, every function call, and every memory region must have statically determinable or explicitly validated bounds. "Might be safe" is not safe.
4. **No Undefined Behavior**: No construct that invokes undefined, unspecified, or implementation-defined behavior is permitted. The code must behave identically across all conforming compilers and platforms.
5. **Defensive Depth**: Every function validates its own preconditions. A caller's correctness is never assumed. Every layer is independently responsible for its own safety.

---

### 9.2 Memory Rules

Dynamic memory management is the single largest source of runtime faults. In Safety-Critical Mode, all dynamic allocation is forbidden.

1. **No Dynamic Allocation**: `malloc`, `calloc`, `realloc`, `free`, `alloca`, and any custom allocator are strictly forbidden. All memory must be statically allocated at compile time or reserved as fixed-size stack or global buffers.
2. **Fixed Compile-Time Bounds**: Every array, every buffer, and every collection must have a size determined at compile time by a named constant. Magic numbers in array sizes are forbidden.
3. **No Variable-Length Arrays (VLAs)**: VLAs are forbidden. They introduce runtime-determined stack consumption which cannot be statically verified.
4. **No Pointer Arithmetic**: Pointer arithmetic (`ptr + n`, `ptr++`, `ptr--`, `ptr[n]` where `ptr` is a raw pointer) is forbidden. Array indexing via named arrays or verified index variables is the only permitted form of memory access.
5. **No Pointer-to-Pointer**: Multiple levels of indirection (`T **ptr`) are forbidden. They obscure ownership and lifetime.
6. **Memory Initialization**: All static and global buffers must be zero-initialized at declaration. All stack variables must be explicitly initialized before use. Uninitialized memory reads are undefined behavior and are treated as errors.

#### Example: Static Fixed-Size SoA (Safety Mode)
```c
/* !DEFINES!

$define %type float as 32 bit float
$define %type bool as 8 bit boolean
$define %type int32_t as 32 bit signed
$define %type SafeParticleSystem as struct with fixed compile-time parallel arrays

$define %func safe_particles_update as procedure with args SafeParticleSystem*, float

*/

/* !SPACE!

$space %export SafeParticleSystem, safe_particles_update

*/

#define SAFE_MAX_PARTICLES 1024

typedef struct {
	float x[SAFE_MAX_PARTICLES];
	float y[SAFE_MAX_PARTICLES];
	float vx[SAFE_MAX_PARTICLES];
	float vy[SAFE_MAX_PARTICLES];
	float lifetime[SAFE_MAX_PARTICLES];
	bool  active[SAFE_MAX_PARTICLES];
	int32_t count;
} SafeParticleSystem;

void
safe_particles_update(SafeParticleSystem *sys, float dt)
{
	int32_t i;

	if (sys == NULL) {
		return;
	}

	if (sys->count < 0 || sys->count > SAFE_MAX_PARTICLES) {
		sys->count = 0;
		return;
	}

	for (i = 0; i < sys->count; ++i) {
		if (sys->active[i]) {
			sys->x[i] += sys->vx[i] * dt;
			sys->y[i] += sys->vy[i] * dt;
		}
	}
}
```

---

### 9.3 Data Layout Rules (Safety SoA)

The SoA layout principle from Section 3 is retained as a *structural concept* (parallel arrays inside a managing struct), but all speed-oriented justifications (cache lines, auto-vectorization, false sharing) are discarded. SoA is used because it makes bounds checking trivial and centralizes all capacity information into a single struct.

1. **Centralized Capacity**: Every SoA struct must contain a `count` field and a compile-time `MAX_*` constant. The relationship `0 <= count <= MAX_*` must be validated at every function entry that receives the struct.
2. **No Swap-and-Pop**: The swap-and-pop deallocation pattern from Section 5 is forbidden. It silently overwrites data and reorders elements, which can mask use-after-deactivation bugs. Deallocation must be performed by setting the element's `active` flag to `false` and leaving all other fields intact. Compaction is a separate, explicitly invoked, and fully validated operation.
3. **No Aliasing Assumptions**: The `restrict` keyword is forbidden. It is a performance hint that introduces undefined behavior if violated. In Safety-Critical Mode, no aliasing assumptions are ever made.
4. **Fixed-Width Types Only**: All integer types must use `<stdint.h>` fixed-width types (`int32_t`, `uint16_t`, etc.). `int`, `long`, `short`, `char` are forbidden in data structures and function signatures. `float` and `double` are permitted but must be explicitly documented as IEEE 754 32-bit or 64-bit.
5. **No Bitfields**: Bitfields have implementation-defined layout, ordering, and alignment. They are forbidden. Use explicit boolean arrays or masks with fixed-width integer types and documented bit positions.

---

### 9.4 Function Contracts

1. **Input Validation on Every Entry**: Every function must validate all pointer parameters against `NULL` and all index/count parameters against their valid range before any other operation. Validation failures must result in an early safe return or an explicit error propagation — never a silent continuation.
2. **No Assumption of Caller Correctness**: Even if a caller promises to pass valid arguments, the function must still check. Defense in depth is mandatory.
3. **Cyclomatic Complexity Limit**: No function may exceed a cyclomatic complexity of **10**. Functions exceeding this must be decomposed into smaller sub-procedures, each of which independently validates its inputs.
4. **Single Exit Point**: Every function must have exactly one `return` statement at its end. Early returns are forbidden. Use a result variable and `goto cleanup` (the only permitted use of `goto`) to consolidate all exit paths through a single cleanup block.

#### Example: Validated Function with Single Exit
```c
/* !DEFINES!

$define %type float as 32 bit float
$define %type int32_t as 32 bit signed
$define %type SafeParticleSystem as struct with fixed compile-time parallel arrays

$define %func safe_particle_deactivate as procedure with args SafeParticleSystem*, int32_t

*/

/* !SPACE!

$space %export safe_particle_deactivate

*/

#define SAFE_MAX_PARTICLES 1024

void
safe_particle_deactivate(SafeParticleSystem *sys, int32_t index)
{
	bool ok;

	ok = true;

	if (sys == NULL) {
		ok = false;
		goto cleanup;
	}

	if (index < 0 || index >= sys->count) {
		ok = false;
		goto cleanup;
	}

	if (sys->count < 0 || sys->count > SAFE_MAX_PARTICLES) {
		ok = false;
		goto cleanup;
	}

	sys->active[index] = false;

cleanup:
	return;
}
```

---

### 9.5 Forbidden Constructs

The following constructs are unconditionally forbidden in Safety-Critical Mode. No exceptions, no `#ifdef`, no "just this once."

| Forbidden | Reason |
|---|---|
| `malloc`, `calloc`, `realloc`, `free`, `alloca` | Dynamic allocation introduces unprovable lifetime, fragmentation, and OOM conditions. |
| `restrict` | Alias-based undefined behavior if violated. |
| Variable-Length Arrays (VLA) | Runtime stack size, unprovable bounds. |
| Pointer arithmetic (`ptr+n`, `ptr++`) | Bypasses bounds checking, enables out-of-bounds access. |
| `goto` (except single-exit cleanup) | Unstructured control flow, except the one sanctioned cleanup pattern. |
| Recursion (direct or indirect) | Unbounded stack depth, unprovable termination. |
| `union` | Type confusion, undefined behavior on active member mismatch. |
| Function pointers | Indirect calls defeat static analysis and control-flow verification. |
| Bitfields | Implementation-defined layout and ordering. |
| `setjmp` / `longjmp` | Stack corruption, skipped destructors, undefined behavior. |
| Inline assembly | Non-portable, unverifiable by compiler. |
| `typedef` that hides a pointer (`typedef struct { ... } *Handle`) | Obscures ownership and indirection level. |
| Flexible array members (`T arr[]` in struct) | Runtime-determined size, incompatible with fixed bounds. |
| Comma operator in expressions | Obscures evaluation order and side effects. |
| Ternary operator nesting (`a ? b : c ? d : e`) | Unreadable, error-prone. Each ternary must be standalone. |
| Assignment inside `if` / `while` condition (`if ((x = foo()) != 0)`) | Conflates assignment with test, classic bug source. |
| Implicit integer conversions / promotions | Silent truncation or sign extension. All conversions must be explicit via cast. |
| `switch` without `default` | Unhandled cases are silent failures. Every `switch` must have a `default` that handles the unexpected. |
| `switch` fall-through without comment | Unintended fall-through is a defect source. Every fall-through must be explicitly commented with `/* FALLTHROUGH */`. |
| Macro that evaluates arguments more than once | Double-evaluation side effects (e.g., `#define MAX(a,b) ((a)>(b)?(a):(b))`). Use inline functions or `static const` instead. |
| `#undef` of compiler-defined macros | Alters compiler semantics, breaks static analysis. |
| Multiple side effects in a single expression (`arr[i++] = arr[--j]`) | Undefined evaluation order. |

---

### 9.6 Mandatory Runtime Checks

Every function must perform the following checks at runtime before proceeding. These are not optional. A function that skips any of these is non-conforming.

1. **NULL Pointer Check**: Every pointer parameter must be checked against `NULL` before dereferencing. No exceptions, including "the caller guarantees it."
2. **Array Bounds Check**: Every array index must be validated against `[0, capacity)` before access. The capacity must come from a named constant or a struct field — never a raw number.
3. **Integer Overflow Check**: Every arithmetic operation on signed integers that could overflow (`a + b`, `a * b`, `a - b`) must be preceded by an explicit overflow check. Signed overflow is undefined behavior in C. Use checked arithmetic patterns.
4. **Division by Zero Check**: Every division and modulo operation must verify the divisor is non-zero before the operation.
5. **Count Integrity Check**: Every function receiving a struct with a `count` field must validate `0 <= count <= MAX_*` on entry.
6. **Return Value Check**: Every function call that returns a status or error code must have its return value checked. Discarding return values is forbidden.

#### Example: Checked Arithmetic
```c
/* !DEFINES!

$define %type int32_t as 32 bit signed
$define %type bool as 8 bit boolean

$define %func safe_add as function with args int32_t, int32_t, int32_t*

*/

/* !SPACE!

$space %export safe_add

*/

#include <stdint.h>

#define INT32_MAX_SAFE 2147483647

bool
safe_add(int32_t a, int32_t b, int32_t *result)
{
	bool ok;

	ok = true;

	if (result == NULL) {
		ok = false;
		goto cleanup;
	}

	if (a > 0 && b > INT32_MAX_SAFE - a) {
		*result = 0;
		ok = false;
		goto cleanup;
	}

	if (a < 0 && b < (-INT32_MAX_SAFE - 1) - a) {
		*result = 0;
		ok = false;
		goto cleanup;
	}

	*result = a + b;

cleanup:
	return (ok);
}
```

---

### 9.7 Control Flow Rules

1. **No Recursion**: All loops must be iterative (`for`, `while`). Recursive function calls — direct or indirect — are forbidden. Recursion makes stack depth unbounded and termination unprovable.
2. **Bounded Loops**: Every `for` loop must have a statically determinable upper bound or a runtime-validated upper bound sourced from a checked `count` field. Infinite loops (`for (;;)`, `while (1)`) are forbidden except in a single top-level main loop of a firmware/embedded system, which must be explicitly documented.
3. **No `break` in Loops**: `break` inside loop bodies is forbidden. It creates hidden exit paths that defeat loop-bound analysis. Use a loop condition variable instead.
4. **No `continue` in Loops**: `continue` is forbidden for the same reason. Restructure the loop body to avoid it.
5. **Single Loop Exit**: Every loop must have exactly one termination condition, stated in the loop header (`for` init/condition or `while` condition). No side exits.
6. **Loop Counter Scope**: Loop counter variables must be declared in the function's variable block (per Section 8.4), not inside the `for` statement. `for (int32_t i = 0; ...)` is forbidden.
7. **No Early Return**: See Section 9.4 rule 4. All exits go through the single cleanup label.

#### Example: Safe Iteration Pattern
```c
/* !DEFINES!

$define %type float as 32 bit float
$define %type int32_t as 32 bit signed
$define %type SafeParticleSystem as struct with fixed compile-time parallel arrays

$define %func safe_particles_lifetime_step as procedure with args SafeParticleSystem*, float

*/

/* !SPACE!

$space %internal safe_particles_lifetime_step

*/

#define SAFE_MAX_PARTICLES 1024

void
safe_particles_lifetime_step(SafeParticleSystem *sys, float dt)
{
	int32_t i;
	bool  ok;

	i  = 0;
	ok = true;

	if (sys == NULL) {
		ok = false;
		goto cleanup;
	}

	if (sys->count < 0 || sys->count > SAFE_MAX_PARTICLES) {
		ok = false;
		goto cleanup;
	}

	for (i = 0; i < sys->count; ++i) {
		if (sys->active[i]) {
			sys->lifetime[i] -= dt;
		}
	}

cleanup:
	return;
}
```

---

### 9.8 Error Handling Protocol

1. **Explicit Error Propagation**: Functions that can fail must return a `bool` or an explicit error code (`enum`). The caller must check this value. Discarding it is a violation.
2. **No Silent Failure**: A function that detects an error must either return an error indication or, in systems without error return channels, set a globally accessible error flag and abort the current operation. Silent recovery is forbidden.
3. **No `assert` in Production**: `assert()` is a debug-only mechanism that is typically compiled out in release builds. It must never be used for runtime safety checks. All checks must use real `if` guards with real error handling.
4. **Fail-Closed Default**: When a function encounters an unexpected state that it cannot safely resolve, it must leave the system in the most conservative possible state (zeroed outputs, error return, no mutations) rather than attempting a best-effort recovery.
5. **No Error Masking**: A function that calls another function which returns an error must propagate that error. Swallowing errors (checking the return value but taking no action) is forbidden.

#### Example: Error Propagation Chain
```c
/* !DEFINES!

$define %type int32_t as 32 bit signed
$define %type bool as 8 bit boolean
$define %type SafeParticleSystem as struct with fixed compile-time parallel arrays

$define %func safe_particles_spawn as function with args SafeParticleSystem*, float, float, float, float, int32_t*

*/

/* !SPACE!

$space %export safe_particles_spawn

*/

#define SAFE_MAX_PARTICLES 1024

bool
safe_particles_spawn(
	SafeParticleSystem *sys,
	float x, float y,
	float vx, float vy,
	int32_t *out_index
)
{
	bool    ok;
	int32_t idx;

	ok  = true;
	idx = 0;

	if (sys == NULL) {
		ok = false;
		goto cleanup;
	}

	if (out_index == NULL) {
		ok = false;
		goto cleanup;
	}

	if (sys->count < 0 || sys->count >= SAFE_MAX_PARTICLES) {
		*out_index = -1;
		ok = false;
		goto cleanup;
	}

	idx = sys->count;

	sys->x[idx]        = x;
	sys->y[idx]        = y;
	sys->vx[idx]       = vx;
	sys->vy[idx]       = vy;
	sys->lifetime[idx] = 0.0f;
	sys->active[idx]   = true;
	sys->count         = idx + 1;

	*out_index = idx;

cleanup:
	return (ok);
}
```

---

### 9.9 Preprocessor & Macro Rules

1. **No Function-Like Macros**: Function-like macros (`#define MAX(a,b) ...`) are forbidden. They bypass type checking, double-evaluate arguments, and obscure control flow. Use `static inline` functions or `static const` variables instead.
2. **Object-Like Macros Only for Constants**: `#define` is permitted only for named constants (`#define SAFE_MAX_PARTICLES 1024`). These must be `UPPER_SNAKE_CASE` per Section 2.
3. **No `#undef`**: Redefining or undefining macros is forbidden.
4. **No Conditional Compilation in Logic**: `#ifdef` / `#ifndef` may only be used for header guards and platform-specific type selections at the top of a file. Interleaving `#ifdef` inside function bodies to toggle logic paths is forbidden — it creates untested code paths.
5. **No Stringification or Token Pasting**: `#` and `##` preprocessor operators are forbidden.
6. **`#include` Ordering**: System headers first (`<stdint.h>`, `<stdbool.h>`), then project headers, each group alphabetically sorted. No duplicate includes.

---

### 9.10 Scope & Visibility Rules

1. **No Global Mutable State**: Global variables with mutable state are forbidden. `static const` globals and compile-time constant tables are permitted. All mutable state must live inside structs passed as function parameters.
2. **`static` for Internal Functions**: Functions not exported from their translation unit must be declared `static`. The `!SPACE!` manifest's `%internal` declaration maps to the `static` keyword in C.
3. **No `extern` Declarations in Headers**: Headers declare types and `static inline` functions only. External function declarations belong in the corresponding `.c` file's `!SPACE!` block and are linked at build time.
4. **Minimize Variable Scope**: Variables should be scoped as tightly as possible. If a variable is only used inside a conditional block, it should be declared at the function level (per Section 8.4) but its usage must be confined to that block. The compiler will warn about unused variables — this is intentional and acceptable.

---

### 9.11 Concurrency Rules

1. **No Multithreading**: The multithreading patterns from Section 7 (range partitioning, double buffering, halo buffering) are suspended. Safety-Critical Mode targets single-threaded execution. If concurrency is required, it must be implemented at the system level via a deterministic scheduler with cooperative scheduling, not preemptive threads.
2. **No Atomics**: `stdatomic.h`, `_Atomic`, and compiler builtins (`__sync_*`, `__atomic_*`) are forbidden. They introduce memory ordering complexity that is extremely difficult to verify.
3. **No Volatile for Synchronization**: `volatile` does not provide thread safety. It is permitted only for memory-mapped hardware registers in embedded/firmware contexts, and must be documented as such.
4. **No Signals**: Signal handlers (`signal()`, `sigaction()`) are forbidden. They execute in an interruptible context where most operations are undefined. Hardware interrupts in embedded systems must be handled by a dedicated, minimal interrupt service routine that only sets a flag consumed by the main loop.

---

### 9.12 Verification & Compliance

1. **Static Analysis Mandatory**: All Safety-Critical Mode code must pass static analysis with zero warnings. Acceptable tools include `cppcheck --enable=all`, `clang-tidy` with safety checkers, and `Coverity`. Any warning is a defect.
2. **Compiler Warnings as Errors**: Compilation must use `-Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -Wformat=2 -Wundef`. Zero warnings, zero exceptions.
3. **No Compiler-Specific Extensions**: No `__attribute__`, no `__builtin_*`, no `#pragma` (except `#pragma once` for header guards). Only ISO C11 standard language.
4. **Full Branch Coverage**: Every `if`, `switch case`, and `default` branch must be exercised by tests. MC/DC (Modified Condition/Decision Coverage) is the target metric.
5. **No Dead Code**: Functions, variables, and macros that are defined but never called must be removed. Dead code accumulates and becomes a maintenance hazard.
6. **Independent Review**: All Safety-Critical Mode code must be reviewed by a second developer who did not write it. The reviewer must verify every rule in this section.

---

### 9.13 Mode Selection

A DDPS project declares which mode it operates in at the file or module level. A single project may contain both modes — safety-critical modules (e.g., payment processing, authentication, state machines controlling money flow) use Section 9, while performance-critical modules (e.g., data processing pipelines, UI rendering) use Sections 1–7.

**The declaration is made in the `!DEFINES!` block:**

```c
/* !DEFINES!

$mode safety

$define %type int32_t as 32 bit signed
$define %type SafeTransactionState as struct with fixed arrays for transaction data

$define %func safe_transaction_validate as function with args SafeTransactionState*

*/

/* !SPACE!

$space %export safe_transaction_validate

*/
```

The `$mode safety` directive indicates that all rules in Section 9 are in effect and all rules from Sections 1–7 are suspended for this file. If `$mode` is absent or `$mode standard` is specified, Sections 1–7 apply and Section 9 does not.

**A file may not mix modes.** Every translation unit is entirely one mode or the other.
