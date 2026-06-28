# Data-Driven Procedural Style (DDPS) - C language coding style


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
