from parser.ast import *
from metadata.nodes import MetadataModule, IntegerTypeDef, StructTypeDef
from util.errors import ErrorReporter


BUILTIN_TYPES = frozenset([
    "int", "int32", "int64", "uint", "uint32", "uint64", "ulong",
    "uint8", "int8",
    "uint16", "int16", "float", "float32", "double", "float64",
    "char", "char8", "bool", "void", "rawptr",
    "Array",
])


class TypeChecker:
    def __init__(self, program, metadata, filename="<unknown>"):
        self.program = program
        self.meta = metadata
        self.filename = filename
        self.reporter = ErrorReporter(filename)
        self.scopes = [{}]
        self.func_returns = {}
        self.current_func = None
        self.in_loop = 0
        self.func_call_stack = []
        self.func_defs = {}
        self.all_func_names = set()

    def push_scope(self):
        self.scopes.append({})

    def pop_scope(self):
        self.scopes.pop()

    def declare(self, name, type_ref, is_mut=False):
        self.scopes[-1][name] = (type_ref, is_mut)

    def lookup(self, name):
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        return None

    def normalize_type_name(self, name):
        aliases = {
            "int32_t": "int32", "int": "int32",
            "uint32_t": "uint32", "uint": "uint32",
            "int64_t": "int64", "uint64_t": "uint64",
            "unsigned long": "ulong",
            "int8_t": "int8", "uint8_t": "uint8",
            "int16_t": "int16", "uint16_t": "uint16",
            "float": "float32", "double": "float64",
            "char8": "char",
        }
        return aliases.get(name, name)

    def struct_layout(self, type_name):
        td = self.meta.get_type_def(self.normalize_type_name(type_name))
        if isinstance(td, StructTypeDef):
            return td.layout
        return None

    def type_identity(self, type_ref):
        if type_ref is None:
            return ("void", False, None, ())
        if type_ref.name == "Array":
            elem = self.array_element_type(type_ref)
            cap = self.array_capacity_arg(type_ref)
            return ("Array", False, None, (self.type_identity(elem), self.capacity_key(cap)))
        name = self.normalize_type_name(type_ref.name)
        return (name, type_ref.is_pointer, self.struct_layout(name), ())

    def types_compatible(self, expected, actual):
        if expected is None or actual is None:
            return True
        if expected.name == "void" or actual.name == "void":
            return expected.name == actual.name
        if self.type_identity(expected) == self.type_identity(actual):
            return True
        if expected.name == "rawptr" and self.is_integer_type(actual.name):
            return True
        if self.normalize_type_name(expected.name) == self.normalize_type_name(actual.name):
            if self.struct_layout(expected.name) and expected.is_pointer != actual.is_pointer:
                return True
        if self.is_numeric_type(expected.name) and self.is_numeric_type(actual.name):
            return True
        return False

    def is_numeric_type(self, name):
        name = self.normalize_type_name(name)
        if name in ("float32", "float64"):
            return True
        return self.is_integer_type(name)

    def is_integer_type(self, name):
        name = self.normalize_type_name(name)
        td = self.meta.get_type_def(name)
        return isinstance(td, IntegerTypeDef) or name in {
            "int8", "uint8", "int16", "uint16", "int32", "uint32",
            "int64", "uint64", "ulong", "char",
        }

    def array_element_type(self, type_ref):
        if type_ref and type_ref.name == "Array" and type_ref.generic_args:
            arg = type_ref.generic_args[0]
            if isinstance(arg, TypeRef):
                return arg
            if isinstance(arg, IdentExpr):
                return TypeRef(name=arg.name)
        return type_ref

    def array_capacity_arg(self, type_ref):
        if not type_ref or type_ref.name != "Array" or len(type_ref.generic_args) < 2:
            return None
        arg = type_ref.generic_args[1]
        if isinstance(arg, TypeRef):
            return IdentExpr(name=arg.name)
        return arg

    def capacity_key(self, expr):
        if isinstance(expr, IntLit):
            return expr.value
        if isinstance(expr, IdentExpr):
            return expr.name
        if isinstance(expr, TypeRef):
            return expr.name
        if isinstance(expr, str):
            text = expr.strip()
            if text.lstrip("-").isdigit():
                return int(text)
            return text
        return None

    def contract_type_ref(self, type_str):
        text = type_str.strip()
        is_ptr = text.endswith("*")
        if is_ptr:
            text = text[:-1].strip()
        return TypeRef(name=text, is_pointer=is_ptr)

    def integer_range(self, type_ref):
        if type_ref is None:
            return None, None
        name = self.normalize_type_name(type_ref.name)
        td = self.meta.get_type_def(name)
        if isinstance(td, IntegerTypeDef):
            if td.from_val is not None and td.to_val is not None:
                return td.from_val, td.to_val
            bits = td.bits
            if bits < 1:
                return None, None
            if td.signed:
                return -(1 << (bits - 1)), (1 << (bits - 1)) - 1
            return 0, (1 << bits) - 1
        ranges = {
            "int8": (-128, 127), "uint8": (0, 255),
            "int16": (-32768, 32767), "uint16": (0, 65535),
            "int32": (-2147483648, 2147483647),
            "uint32": (0, 4294967295),
            "int64": (-9223372036854775808, 9223372036854775807),
            "uint64": (0, 18446744073709551615),
            "ulong": (0, 18446744073709551615),
            "char": (-128, 127),
        }
        return ranges.get(name, (None, None))

    def check_integer_literal_range(self, expr, target_type, line):
        if not isinstance(expr, IntLit):
            return
        lo, hi = self.integer_range(target_type)
        if lo is None:
            return
        if expr.value < lo or expr.value > hi:
            self.reporter.error("range", line, 1,
                                f"integer literal {expr.value} does not fit in {target_type.name} "
                                f"[{lo}, {hi}]")

    def check_type_ref(self, type_ref, line, context):
        if type_ref is None:
            return
        if type_ref.name == "Array":
            if len(type_ref.generic_args) != 2:
                self.reporter.error("type", line, 1,
                                    f"{context}: Array expects exactly 2 generic arguments: Array<T, N>")
                return
            elem = self.array_element_type(type_ref)
            self.check_type_ref(elem, line, context)
            cap = self.array_capacity_arg(type_ref)
            cap_val = self.resolve_capacity(cap)
            if cap_val is not None and cap_val <= 0:
                self.reporter.error("type", line, 1,
                                    f"{context}: Array capacity must be positive")
            return

        if type_ref.name not in self.meta.type_defs and type_ref.name not in BUILTIN_TYPES:
            self.reporter.error("type", line, 1,
                                f"{context}: unknown type '{type_ref.name}'")

    def resolve_capacity(self, expr):
        if isinstance(expr, IntLit):
            return expr.value
        if isinstance(expr, IdentExpr):
            if expr.name in self.meta.consts:
                return self.meta.consts[expr.name]
            if expr.name in self.meta.compile_vars:
                return self.meta.compile_vars[expr.name]
            return None
        if isinstance(expr, TypeRef):
            if expr.name in self.meta.consts:
                return self.meta.consts[expr.name]
            if expr.name in self.meta.compile_vars:
                return self.meta.compile_vars[expr.name]
        if isinstance(expr, str):
            text = expr.strip()
            if text.lstrip("-").isdigit():
                return int(text)
            if text in self.meta.consts:
                return self.meta.consts[text]
            if text in self.meta.compile_vars:
                return self.meta.compile_vars[text]
        return None

    def check(self):
        for f in self.program.functions:
            self.func_defs[f.name] = f
            self.all_func_names.add(f.name)

        self.check_type_defs()

        for const in self.program.consts:
            self.check_const(const)

        for struct in self.program.structs:
            self.check_struct(struct)

        for enum in self.program.enums:
            self.check_enum(enum)

        for func in self.program.functions:
            self.check_func(func)

        self.check_visibility()
        self.check_main()

        return self.reporter

    def check_type_defs(self):
        for name, td in self.meta.type_defs.items():
            if isinstance(td, IntegerTypeDef):
                if td.bits < 1 or td.bits > 64:
                    self.reporter.error("metadata", 0, 0,
                                        f"integer type '{name}' has unsupported bit width {td.bits}; "
                                        "supported carrier widths are 1..64")
                lo, hi = self.integer_range(TypeRef(name=name))
                if lo is not None and hi is not None and lo > hi:
                    self.reporter.error("metadata", 0, 0,
                                        f"integer type '{name}' has invalid range [{lo}, {hi}]")

    def check_const(self, const):
        if const.name not in self.meta.consts:
            self.reporter.error("metadata", const.line, 1,
                                f"constant '{const.name}' not declared in metadata with $define %const")
        self.check_type_ref(const.type_ref, const.line, f"constant '{const.name}'")
        self.check_integer_literal_range(const.value, const.type_ref, const.line)
        self.declare(const.name, const.type_ref, is_mut=False)

    def check_struct(self, struct):
        td = self.meta.get_type_def(struct.name)
        if td is None:
            self.reporter.error("metadata", struct.line, 1,
                                f"struct '{struct.name}' not defined in metadata")
        elif isinstance(td, StructTypeDef):
            if td.layout not in ("soa", "aos", "packed", "packed_bits"):
                self.reporter.error("metadata", struct.line, 1,
                                    f"unknown layout '{td.layout}' for struct '{struct.name}'")

        seen = set()
        for field in struct.fields:
            if field.name in seen:
                self.reporter.error("type", field.line, 1,
                                    f"duplicate field '{field.name}' in struct '{struct.name}'")
            seen.add(field.name)
            self.check_type_ref(field.type_ref, field.line,
                                f"struct field '{struct.name}.{field.name}'")

        if isinstance(td, StructTypeDef) and td.fields:
            meta_fields = {f.name: f for f in td.fields}
            for field in struct.fields:
                mf = meta_fields.get(field.name)
                if mf is None:
                    self.reporter.error("metadata", field.line, 1,
                                        f"field '{field.name}' is missing from metadata type '{struct.name}'")
                    continue
                expected = TypeRef(name=mf.type_name, is_pointer=mf.is_pointer)
                if not self.field_types_compatible(expected, mf.array_size, field):
                    self.reporter.error("metadata", field.line, 1,
                                        f"field '{struct.name}.{field.name}' type differs from metadata: "
                                        f"expected {expected}, got {field.type_ref}")

            source_fields = {f.name for f in struct.fields}
            for mf_name in meta_fields:
                if mf_name not in source_fields:
                    self.reporter.error("metadata", struct.line, 1,
                                        f"metadata field '{struct.name}.{mf_name}' is missing from source struct")

    def field_types_compatible(self, expected_type, expected_size, source_field):
        if not self.types_compatible(expected_type, self.array_element_type(source_field.type_ref)):
            return False

        source_size = source_field.array_size
        if source_field.type_ref.name == "Array":
            source_size = self.array_capacity_arg(source_field.type_ref)

        if expected_size is None and source_size is None:
            return True
        return self.resolve_capacity(expected_size) == self.resolve_capacity(source_size)

    def check_enum(self, enum):
        td = self.meta.get_type_def(enum.name)
        if td is None:
            self.reporter.error("metadata", enum.line, 1,
                                f"enum '{enum.name}' not defined in metadata")

        seen = set()
        for variant in enum.variants:
            if variant.name in seen:
                self.reporter.error("type", variant.line, 1,
                                    f"duplicate variant '{variant.name}' in enum '{enum.name}'")
            seen.add(variant.name)

    def check_func(self, func):
        contract = self.meta.get_func_contract(func.name)
        if contract is None:
            self.reporter.error("metadata", func.line, 1,
                                f"function '{func.name}' has no $define %func contract in metadata")

        self.current_func = func

        if func.return_type is None and contract:
            ret_name = contract.returns.replace("*", "").strip()
            is_ptr = "*" in contract.returns
            func.return_type = TypeRef(name=ret_name, is_pointer=is_ptr)

        if contract:
            if len(contract.args) != len(func.params):
                self.reporter.error("contract", func.line, 1,
                                    f"function '{func.name}' has {len(func.params)} parameter(s), "
                                    f"metadata contract declares {len(contract.args)}")
            for param, arg_def in zip(func.params, contract.args):
                expected = self.contract_type_ref(arg_def.type_str + ("*" if arg_def.is_pointer else ""))
                if not self.types_compatible(expected, param.type_ref):
                    self.reporter.error("contract", func.line, 1,
                                        f"parameter '{param.name}' type differs from metadata: "
                                        f"expected {expected}, got {param.type_ref}")
                if arg_def.is_mut and not param.is_mut:
                    self.reporter.error("contract", func.line, 1,
                                        f"metadata marks parameter '{param.name}' mut, source does not")

            expected_ret = self.contract_type_ref(contract.returns)
            actual_ret = func.return_type or TypeRef(name="void")
            if not self.types_compatible(expected_ret, actual_ret):
                self.reporter.error("contract", func.line, 1,
                                    f"return type differs from metadata: expected {expected_ret}, got {actual_ret}")

        self.check_type_ref(func.return_type or TypeRef(name="void"), func.line,
                            f"return type of '{func.name}'")
        self.func_returns[func.name] = func.return_type

        self.push_scope()

        for param in func.params:
            is_mut = param.is_mut
            self.declare(param.name, param.type_ref, is_mut=is_mut)
            self.check_type_ref(param.type_ref, func.line,
                                f"parameter '{param.name}'")

            if param.type_ref.name == "rawptr":
                if self.meta.directives.get("unsafe") != "rawptr":
                    self.reporter.error("unsafe", func.line, 1,
                                        f"rawptr type requires $unsafe rawptr directive in metadata")

            if contract and contract.pure == True and is_mut:
                self.reporter.error("purity", func.line, 1,
                                    f"pure function '{func.name}' cannot have mut parameter '{param.name}'")

            if self.meta.mode == "safety" and param.type_ref.name.startswith("func"):
                self.reporter.error("safety", func.line, 1,
                                    f"function pointers are forbidden in safety mode: parameter '{param.name}'")

        self.check_body(func.body)

        complexity = self.calc_complexity(func.body)
        max_complexity = None
        if contract and contract.complexity is not None:
            max_complexity = contract.complexity
        elif self.meta.directives.get("complexity"):
            try:
                max_complexity = int(self.meta.directives["complexity"])
            except (ValueError, TypeError):
                pass

        if max_complexity is not None and complexity > max_complexity:
            self.reporter.error("contract", func.line, 1,
                                f"function '{func.name}' has cyclomatic complexity {complexity}, "
                                f"exceeds max {max_complexity}")

        self.pop_scope()
        self.current_func = None

    def calc_complexity(self, body):
        complexity = 1
        for stmt in body:
            complexity += self._count_branches(stmt)
        return complexity

    def _count_branches(self, stmt):
        count = 0
        if isinstance(stmt, IfStmt):
            count += 1
            for s in stmt.then_body:
                count += self._count_branches(s)
            for s in stmt.else_body:
                count += self._count_branches(s)
        elif isinstance(stmt, WhileStmt):
            count += 1
            for s in stmt.body:
                count += self._count_branches(s)
        elif isinstance(stmt, MatchStmt):
            count += len(stmt.arms)
            for arm in stmt.arms:
                for s in arm.body:
                    count += self._count_branches(s)
        elif isinstance(stmt, MapExpr):
            if stmt.cond:
                count += 1
            for s in stmt.body:
                count += self._count_branches(s)
        elif isinstance(stmt, FoldExpr):
            if stmt.cond:
                count += 1
            for s in stmt.body:
                count += self._count_branches(s)
        elif isinstance(stmt, BinaryOp):
            if stmt.op == "&&":
                count += 1
            elif stmt.op == "||":
                count += 1
        return count

    def check_body(self, body):
        self.push_scope()
        for stmt in body:
            self.check_stmt(stmt)
        self.pop_scope()

    def check_stmt(self, stmt):
        if isinstance(stmt, LetStmt):
            self.check_let(stmt)
        elif isinstance(stmt, AssignStmt):
            self.check_assign(stmt)
        elif isinstance(stmt, IfStmt):
            self.check_if(stmt)
        elif isinstance(stmt, WhileStmt):
            self.check_while(stmt)
        elif isinstance(stmt, ReturnStmt):
            self.check_return(stmt)
        elif isinstance(stmt, BreakStmt):
            if self.meta.mode == "safety":
                self.reporter.error("safety", stmt.line, 1, "break is forbidden in safety mode")
            if self.in_loop == 0:
                self.reporter.error("syntax", stmt.line, 1, "break outside loop")
        elif isinstance(stmt, ContinueStmt):
            if self.meta.mode == "safety":
                self.reporter.error("safety", stmt.line, 1, "continue is forbidden in safety mode")
            if self.in_loop == 0:
                self.reporter.error("syntax", stmt.line, 1, "continue outside loop")
        elif isinstance(stmt, MatchStmt):
            self.check_match(stmt)
        elif isinstance(stmt, MapExpr):
            self.check_map(stmt)
        elif isinstance(stmt, FilterExpr):
            self.check_filter(stmt)
        elif isinstance(stmt, FoldExpr):
            self.check_fold(stmt)
        elif isinstance(stmt, CCodeBlock):
            alloc_mode = self.meta.directives.get("alloc", "dynamic")
            if alloc_mode in ("static", "none"):
                for kw in ("malloc", "calloc", "realloc", "free", "alloca"):
                    if kw in stmt.code:
                        self.reporter.error("contract", stmt.line, 1,
                                            f"dynamic allocation '{kw}' is forbidden in $alloc {alloc_mode} mode")
        elif isinstance(stmt, AsmBlock):
            asm_mode = self.meta.directives.get("asm", "allowed")
            if asm_mode == "never":
                self.reporter.error("safety", stmt.line, 1,
                                    "inline assembly is forbidden by $asm never")
        elif isinstance(stmt, CallExpr):
            self.check_call(stmt)
        else:
            self.check_expr(stmt)

    def check_call(self, expr):
        if expr.is_c_call:
            for arg in expr.args:
                self.check_expr(arg)
            if expr.is_c_var_ref:
                return None
            alloc_mode = self.meta.directives.get("alloc", "dynamic")
            if alloc_mode in ("static", "none"):
                for kw in ("malloc", "calloc", "realloc"):
                    if kw in expr.func_name:
                        self.reporter.error("contract", expr.line, 1,
                                            f"dynamic allocation '{expr.func_name}' is forbidden in $alloc {alloc_mode} mode")
            if expr.func_name not in self.meta.c_funcs:
                self.reporter.error("metadata", expr.line, 1,
                                    f"C function '{expr.func_name}' not declared in metadata with $c_func")
            return None

        contract = self.meta.get_func_contract(expr.func_name)
        if contract is None:
            if expr.func_name not in self.all_func_names:
                self.reporter.error("type", expr.line, 1,
                                    f"call to undefined function '{expr.func_name}'")
            return None

        if len(expr.args) != len(contract.args):
            self.reporter.error("type", expr.line, 1,
                                f"function '{expr.func_name}' expects {len(contract.args)} argument(s), "
                                f"got {len(expr.args)}")

        for idx, (arg_expr, arg_def) in enumerate(zip(expr.args, contract.args)):
            actual = self.check_expr(arg_expr)
            expected = self.contract_type_ref(arg_def.type_str + ("*" if arg_def.is_pointer else ""))
            if actual and not self.types_compatible(expected, actual):
                self.reporter.error("type", expr.line, 1,
                                    f"argument {idx + 1} of '{expr.func_name}' expects {expected}, got {actual}")
            self.check_integer_literal_range(arg_expr, expected, expr.line)

        if self.current_func:
            current_contract = self.meta.get_func_contract(self.current_func.name)
            if current_contract and current_contract.pure and contract.pure is False:
                self.reporter.error("purity", expr.line, 1,
                                    f"pure function '{self.current_func.name}' cannot call impure "
                                    f"function '{expr.func_name}'")

        if self.meta.mode == "safety" and contract.recurses == False:
            if expr.func_name in self.func_call_stack:
                self.reporter.error("safety", expr.line, 1,
                                    f"recursive call to '{expr.func_name}' is forbidden by contract recurses:false")

        if self.current_func and self.current_func.name == expr.func_name:
            fc = self.meta.get_func_contract(self.current_func.name)
            if fc and fc.recurses == False:
                self.reporter.error("contract", expr.line, 1,
                                    f"function '{expr.func_name}' has recurses:false but calls itself")

        self.func_call_stack.append(expr.func_name)
        ret_type = None
        if contract:
            ret_name = contract.returns.replace("*", "")
            ret_type = TypeRef(name=ret_name, is_pointer="*" in contract.returns)
        self.func_call_stack.pop()
        return ret_type

    def check_let(self, stmt):
        if self.lookup(stmt.name) is not None:
            if stmt.name in self.scopes[-1]:
                self.reporter.error("type", stmt.line, 1,
                                    f"variable '{stmt.name}' redeclared in same scope")
        inferred_type = self.check_expr(stmt.value)
        if stmt.type_ref is None and inferred_type:
            stmt.type_ref = inferred_type
        elif stmt.type_ref and inferred_type and self.is_zero_aggregate_init(stmt.type_ref, stmt.value):
            pass
        elif stmt.type_ref and inferred_type and not self.types_compatible(stmt.type_ref, inferred_type):
            self.reporter.error("type", stmt.line, 1,
                                f"cannot initialize '{stmt.name}' of type {stmt.type_ref} "
                                f"with value of type {inferred_type}")

        if stmt.type_ref:
            self.check_type_ref(stmt.type_ref, stmt.line, f"variable '{stmt.name}'")
            self.check_integer_literal_range(stmt.value, stmt.type_ref, stmt.line)

        if stmt.type_ref and stmt.type_ref.name == "rawptr":
            if self.meta.directives.get("unsafe") != "rawptr":
                self.reporter.error("unsafe", stmt.line, 1,
                                    f"rawptr type requires $unsafe rawptr directive in metadata")
            if inferred_type and inferred_type.name in ("int", "int32", "int64",
                    "uint", "uint32", "uint64", "ulong", "uint8", "int8",
                    "uint16", "int16"):
                pass
        self.declare(stmt.name, stmt.type_ref or TypeRef(name="int"), is_mut=stmt.is_mut)

    def is_zero_aggregate_init(self, type_ref, value):
        if isinstance(value, IntLit):
            is_zero = value.value == 0
        elif isinstance(value, FloatLit):
            is_zero = value.value == 0.0
        else:
            is_zero = False
        if not is_zero:
            return False
        if type_ref and type_ref.name == "Array":
            return True
        td = self.meta.get_type_def(type_ref.name) if type_ref else None
        return isinstance(td, StructTypeDef)

    def check_assign(self, stmt):
        target_type = self.check_expr(stmt.target)
        value_type = None
        if stmt.value is not None:
            value_type = self.check_expr(stmt.value)
            if target_type and value_type and not self.types_compatible(target_type, value_type):
                self.reporter.error("type", stmt.line, 1,
                                    f"cannot assign value of type {value_type} to {target_type}")
            self.check_integer_literal_range(stmt.value, target_type, stmt.line)

        if isinstance(stmt.target, IdentExpr):
            var_info = self.lookup(stmt.target.name)
            if var_info is None:
                self.reporter.error("type", stmt.line, 1,
                                    f"assignment to undeclared variable '{stmt.target.name}'")
            elif not var_info[1] and not (var_info[0] and var_info[0].is_mut):
                self.reporter.error("purity", stmt.line, 1,
                                    f"cannot mutate immutable variable '{stmt.target.name}'")

        if isinstance(stmt.target, FieldAccess):
            obj_info = None
            if isinstance(stmt.target.obj, IdentExpr):
                obj_info = self.lookup(stmt.target.obj.name)
            if obj_info and not obj_info[1] and not (obj_info[0] and obj_info[0].is_mut):
                self.reporter.error("purity", stmt.line, 1,
                                    f"cannot mutate field of immutable variable '{stmt.target.obj.name}'")

        if isinstance(stmt.target, ArrayAccess):
            arr_info = None
            if isinstance(stmt.target.array, IdentExpr):
                arr_info = self.lookup(stmt.target.array.name)
            if arr_info and not arr_info[1] and not (arr_info[0] and arr_info[0].is_mut):
                self.reporter.error("purity", stmt.line, 1,
                                    f"cannot mutate element of immutable array '{stmt.target.array.name}'")

    def check_if(self, stmt):
        self.check_expr(stmt.cond)
        self.check_body(stmt.then_body)
        if stmt.else_body:
            self.check_body(stmt.else_body)

    def check_while(self, stmt):
        self.check_expr(stmt.cond)
        self.in_loop += 1
        self.check_body(stmt.body)
        self.in_loop -= 1

    def check_return(self, stmt):
        expected = self.current_func.return_type if self.current_func else None
        if stmt.value is not None:
            actual = self.check_expr(stmt.value)
            if expected and not self.types_compatible(expected, actual):
                self.reporter.error("type", stmt.line, 1,
                                    f"return type mismatch: expected {expected}, got {actual}")
            self.check_integer_literal_range(stmt.value, expected, stmt.line)
        elif expected and expected.name != "void":
            self.reporter.error("type", stmt.line, 1,
                                f"return without value in function returning {expected}")

    def check_match(self, stmt):
        self.check_expr(stmt.subject)
        for arm in stmt.arms:
            self.check_body(arm.body)

    def check_map(self, stmt):
        self.check_expr(stmt.range_start)
        self.check_expr(stmt.range_end)
        self.push_scope()
        self.declare(stmt.index_var, TypeRef(name="int"), is_mut=False)
        if stmt.cond:
            self.check_expr(stmt.cond)
        for s in stmt.body:
            self.check_stmt(s)
        self.pop_scope()

    def check_filter(self, stmt):
        self.check_expr(stmt.range_start)
        self.check_expr(stmt.range_end)
        self.push_scope()
        self.declare(stmt.index_var, TypeRef(name="int"), is_mut=False)
        if stmt.cond:
            self.check_expr(stmt.cond)
        self.pop_scope()

    def check_fold(self, stmt):
        init_type = self.check_expr(stmt.init_value)
        self.check_expr(stmt.range_start)
        self.check_expr(stmt.range_end)
        self.push_scope()
        self.declare(stmt.index_var, TypeRef(name="int"), is_mut=False)
        acc_type = init_type if init_type else TypeRef(name="int")
        self.declare("acc", acc_type, is_mut=False)
        if stmt.cond:
            self.check_expr(stmt.cond)
        for s in stmt.body:
            self.check_stmt(s)
        self.pop_scope()

    def check_expr(self, expr):
        if expr is None:
            return None

        if isinstance(expr, IntLit):
            return TypeRef(name="int")
        elif isinstance(expr, FloatLit):
            return TypeRef(name="float")
        elif isinstance(expr, CharLit):
            return TypeRef(name="char")
        elif isinstance(expr, StringLit):
            return TypeRef(name="char", is_pointer=True)
        elif isinstance(expr, BoolLit):
            return TypeRef(name="bool")
        elif isinstance(expr, IdentExpr):
            info = self.lookup(expr.name)
            if info is None:
                if expr.name in self.meta.consts:
                    return TypeRef(name="int")
                self.reporter.error("type", expr.line, 1,
                                    f"undefined variable '{expr.name}'")
            return info[0] if info else None
        elif isinstance(expr, BinaryOp):
            lt = self.check_expr(expr.left)
            rt = self.check_expr(expr.right)
            if expr.op in ("==", "!=", "<", ">", "<=", ">=", "&&", "||"):
                return TypeRef(name="bool")
            if lt and rt:
                if lt.name in ("float", "float32", "double", "float64") or \
                   rt.name in ("float", "float32", "double", "float64"):
                    return TypeRef(name="float" if lt.name in ("float", "float32") or
                                   rt.name in ("float", "float32") else "double")
            return lt or rt or TypeRef(name="int")
        elif isinstance(expr, UnaryOp):
            return self.check_expr(expr.operand)
        elif isinstance(expr, CallExpr):
            return self.check_call(expr)
        elif isinstance(expr, FieldAccess):
            obj_type = self.check_expr(expr.obj)
            if obj_type:
                td = self.meta.get_type_def(obj_type.name)
                if td and hasattr(td, 'fields'):
                    for f in td.fields:
                        if f.name == expr.field:
                            return TypeRef(name=f.type_name, is_pointer=f.is_pointer)
            return None
        elif isinstance(expr, ArrayAccess):
            self.check_expr(expr.array)
            self.check_expr(expr.index)
            arr_type = self.check_expr(expr.array)
            if arr_type and arr_type.name == "Array":
                return self.array_element_type(arr_type)
            if arr_type and arr_type.is_array:
                return TypeRef(name=arr_type.name)
            if arr_type and arr_type.is_pointer:
                return TypeRef(name=arr_type.name)
            if arr_type and arr_type.name == "rawptr":
                return TypeRef(name="char")
            return None
        elif isinstance(expr, AddrOf):
            t = self.check_expr(expr.operand)
            if t:
                return TypeRef(name=t.name, is_pointer=True)
            return None
        elif isinstance(expr, Deref):
            t = self.check_expr(expr.operand)
            return t
        elif isinstance(expr, CastExpr):
            self.check_expr(expr.expr)
            self.check_type_ref(expr.type_ref, expr.line, "cast target")
            return expr.type_ref
        elif isinstance(expr, IfExpr):
            self.check_expr(expr.cond)
            t1 = self.check_expr(expr.then_expr)
            t2 = self.check_expr(expr.else_expr)
            return t1 or t2
        elif isinstance(expr, MatchExpr):
            self.check_expr(expr.subject)
            result_type = None
            for arm in expr.arms:
                t = self.check_expr(arm.expr)
                if t:
                    result_type = t
            return result_type
        elif isinstance(expr, ArrayLit):
            for e in expr.elements:
                self.check_expr(e)
            return None
        elif isinstance(expr, InitList):
            return None
        elif isinstance(expr, CCodeBlock):
            return None
        return None

    def check_visibility(self):
        all_decls = {}
        for s in self.program.structs:
            all_decls[s.name] = s.line
        for e in self.program.enums:
            all_decls[e.name] = e.line
        for f in self.program.functions:
            all_decls[f.name] = f.line

        declared = set()
        declared.update(self.meta.visibility.get("export", set()))
        declared.update(self.meta.visibility.get("internal", set()))

        for name, line in all_decls.items():
            if name not in declared:
                self.reporter.warning("visibility", line, 1,
                                      f"'{name}' not declared in $space %export or $space %internal")

    def check_main(self):
        has_main = any(f.name == "main" for f in self.program.functions)
        if not has_main and self.meta.has_main:
            self.reporter.error("metadata", 0, 0, "no 'main' function found despite $define %func main as start")
        if has_main:
            main_func = next(f for f in self.program.functions if f.name == "main")
            if main_func.params:
                self.reporter.error("metadata", main_func.line, 1,
                                    "main function must not have parameters")
