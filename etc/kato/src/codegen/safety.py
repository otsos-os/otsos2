from codegen.gen import CodeGenerator, VarDecl, TYPE_SIZE_ORDER
from metadata.nodes import IntegerTypeDef
from parser.ast import *


class SafetyCodeGen(CodeGenerator):
    def __init__(self, program, metadata, freestanding=False):
        super().__init__(program, metadata, freestanding=freestanding)
        self.need_cleanup_label = False
        self.has_result_var = False
        self.validated_counts = {}
        self.loop_validated = []
        self.known_safe_vars = {}

    def format_params(self, func):
        if func.name == "main":
            return "void"

        if not func.params:
            return "void"

        parts = []
        for param in func.params:
            ptype = self.c_type(param.type_ref)
            if param.type_ref.is_pointer:
                ptype = ptype.replace(" *", " *")
            parts.append(f"{ptype} {param.name}")
        return ", ".join(parts)

    def emit_func_decl(self, func):
        prefix = "static " if self.is_static(func.name) else ""
        ret_type = self.c_type(func.return_type) if func.return_type else "void"
        params = self.format_params(func)

        if func.name == "main":
            self.lines.append(f"{ret_type}")
            self.lines.append(f"{self.func_c_name(func.name)}(void);")
        else:
            self.lines.append(f"{prefix}{ret_type}")
            self.lines.append(f"{self.func_c_name(func.name)}({params});")

    def emit_func_body(self, func):
        self.need_cleanup_label = False
        self.has_result_var = False
        self.scopes = [{}]
        self.last_fold_result = None
        self.validated_counts = {}
        self.loop_validated = []
        self.known_safe_vars = {}

        for param in func.params:
            self.declare_var(param.name, param.type_ref, is_mut=param.is_mut)

        ret_type = self.c_type(func.return_type) if func.return_type else "void"
        has_ret = func.return_type and func.return_type.name != "void"

        if has_ret:
            self.has_result_var = True

        self.emit_safety_var_block(func)

        self.emit_safety_checks(func)

        self.indent_level = 1
        self.push_scope()

        body_len = len(func.body)
        has_implicit_return = False
        if self.has_result_var and not self.body_ends_with_return(func.body):
            last = self.get_last_expr(func.body)
            if last is not None:
                has_implicit_return = True

        for i, stmt in enumerate(func.body):
            if has_implicit_return and i == body_len - 1:
                if isinstance(stmt, IfStmt):
                    self.gen_if_as_return_safety(stmt)
                elif isinstance(stmt, FoldExpr):
                    self.gen_fold(stmt)
                    val = self.gen_expr_as_type(IdentExpr(self.last_fold_result), func.return_type)
                    self.lines.append(f"{self.indent()}_result = {val};")
                    self.lines.append(f"{self.indent()}goto cleanup;")
                    self.need_cleanup_label = True
                else:
                    val = self.gen_expr_as_type(stmt, func.return_type)
                    self.lines.append(f"{self.indent()}_result = {val};")
                    self.lines.append(f"{self.indent()}goto cleanup;")
                    self.need_cleanup_label = True
            else:
                self.gen_stmt(stmt)
        self.pop_scope()

        self.emit_void_suppressions()

        if self.has_result_var and not self.need_cleanup_label:
            last = self.get_last_expr(func.body)
            if isinstance(last, FoldExpr) and not self.body_ends_with_return(func.body):
                val = self.gen_expr_as_type(IdentExpr(self.last_fold_result), func.return_type)
                self.lines.append(f"{self.indent()}_result = {val};")

        if self.need_cleanup_label:
            self.lines.append("cleanup:")
            if self.has_result_var:
                self.lines.append(f"\treturn (_result);")
            else:
                self.lines.append(f"\treturn;")
        else:
            if self.has_result_var:
                if not self.body_ends_with_return(func.body):
                    last = self.get_last_expr(func.body)
                    if isinstance(last, FoldExpr):
                        val = self.gen_expr_as_type(IdentExpr(self.last_fold_result), func.return_type)
                        self.lines.append(f"\t_result = {val};")
                    elif last is not None and not isinstance(last, IfStmt):
                        val = self.gen_expr_as_type(last, func.return_type)
                        self.lines.append(f"\t_result = {val};")
                    self.lines.append(f"\treturn (_result);")
            elif func.return_type is None or func.return_type.name == "void":
                if not self.body_ends_with_return(func.body):
                    self.lines.append(f"\treturn;")
            else:
                if not self.body_ends_with_return(func.body):
                    self.lines.append(f"\treturn ({self.zero_value_for_type(func.return_type)});")

    def emit_safety_var_block(self, func):
        has_ret = self.has_result_var
        ret_type = self.c_type(func.return_type) if func.return_type else "void"

        all_decls = []
        array_decls = []

        if has_ret:
            all_decls.append(("_result", ret_type, False, False))

        for vd in self.var_decls:
            if vd.is_loop_var or vd.is_acc:
                continue
            vtype = self.c_type(vd.type_ref)
            if vd.is_array:
                arr_val = self.eval_array_size(vd.array_size) if vd.array_size else ""
                array_decls.append((vd.name, vtype, arr_val, vd.is_struct))
            else:
                all_decls.append((vd.name, vtype, vd.is_struct, False))

        for vd in self.var_decls:
            if vd.is_loop_var:
                vtype = self.c_type(vd.type_ref)
                all_decls.append((vd.name, vtype, False, False))

        for vd in self.var_decls:
            if vd.is_acc and vd.name.startswith("_acc"):
                vtype = self.c_type(vd.type_ref)
                all_decls.append((vd.name, vtype, False, False))
            elif vd.is_acc and vd.name == "acc":
                vtype = self.c_type(vd.type_ref)
                all_decls.append((vd.name, vtype, False, False))

        type_order = {
            "void": 0,
            "char": 1, "bool": 1, "int8_t": 1, "uint8_t": 1,
            "int16_t": 2, "uint16_t": 2,
            "int32_t": 4, "uint32_t": 4, "float": 4,
            "int64_t": 8, "uint64_t": 8, "double": 8,
        }

        def size_key(item):
            name, vtype, is_struct, is_arr = item
            if is_struct:
                return -200
            if "*" in vtype:
                return -100
            base = vtype.strip().split()[0] if vtype.strip() else "int"
            return -type_order.get(base, 4)

        all_decls.sort(key=size_key)

        groups = {}
        for name, vtype, is_struct, is_arr in all_decls:
            if vtype not in groups:
                groups[vtype] = []
            groups[vtype].append(name)

        for vtype, names in groups.items():
            pad = " " * max(1, 16 - len(vtype))
            self.lines.append(f"\t{vtype}{pad}{', '.join(names)};")

        for name, vtype, arr_val, is_struct in array_decls:
            self.lines.append(f"\t{vtype} {name}[{arr_val}];")

        self.lines.append("")

        if has_ret:
            self.lines.append(f"\t_result = {self.zero_value_for_type(func.return_type)};")

        init_mode = self.meta.directives.get("init", "zero")
        for vd in self.var_decls:
            if vd.is_loop_var or vd.is_acc:
                continue
            vtype = self.c_type(vd.type_ref)
            if vd.is_array:
                self.lines.append(f"\tmemset(&{vd.name}, 0, sizeof({vd.name}));")
            elif vd.is_struct:
                self.lines.append(f"\tmemset(&{vd.name}, 0, sizeof({vd.name}));")
            elif self.is_compact_type_ref(vd.type_ref):
                self.lines.append(f"\t{vd.name} = {self.zero_value_for_type(vd.type_ref)};")
            elif vtype in ("int32_t", "int", "int32", "bool", "char", "uint32_t", "uint", "uint8_t", "int8_t", "uint16_t", "int16_t", "int64_t", "uint64_t"):
                self.lines.append(f"\t{vd.name} = 0;")
            elif vtype in ("float", "double", "float32", "float64"):
                self.lines.append(f"\t{vd.name} = 0.0f;")
            elif "*" in vtype:
                self.lines.append(f"\t{vd.name} = NULL;")

        self.lines.append("")

    def emit_safety_checks(self, func):
        null_check = self.meta.directives.get("null_check", "always") == "always"

        for param in func.params:
            if param.type_ref.is_pointer:
                td = self.meta.get_type_def(param.type_ref.name)
                nullable = True
                if td and hasattr(td, 'nullable'):
                    nullable = td.nullable
                if nullable and null_check:
                    self.lines.append(f"\tif ({param.name} == NULL) {{")
                    self.lines.append(f"\t\tgoto cleanup;")
                    self.lines.append(f"\t}}")
                    self.need_cleanup_label = True

        for param in func.params:
            if param.type_ref.is_pointer:
                td = self.meta.get_type_def(param.type_ref.name)
                if td and hasattr(td, 'max_count_field') and td.max_count_field:
                    cap = td.capacity if td.capacity else 0
                    count_field = td.max_count_field
                    self.lines.append(f"\tif ({param.name}->{count_field} < 0 || "
                                      f"{param.name}->{count_field} > {cap}) {{")
                    self.lines.append(f"\t\tgoto cleanup;")
                    self.lines.append(f"\t}}")
                    self.need_cleanup_label = True
                    self.validated_counts[f"{param.name}->{count_field}"] = cap

        if self.need_cleanup_label:
            self.lines.append("")

    def body_ends_with_return(self, body):
        if not body:
            return False
        last = body[-1]
        return isinstance(last, ReturnStmt)

    def get_last_expr(self, body):
        if not body:
            return None
        last = body[-1]
        if isinstance(last, (BinaryOp, CallExpr, IdentExpr, IntLit, FloatLit,
                              BoolLit, CharLit, IfExpr, MatchExpr, FieldAccess,
                              ArrayAccess, FoldExpr, Deref, AddrOf, UnaryOp)):
            return last
        if isinstance(last, IfStmt):
            return last
        return None

    def gen_let(self, stmt):
        target_type = stmt.type_ref or TypeRef(name="int")
        self.declare_var(stmt.name, target_type, is_mut=stmt.is_mut)
        if stmt.value is None:
            return
        if isinstance(stmt.value, InitList):
            return
        is_struct = False
        is_array = target_type.is_array or target_type.name == "Array"
        if stmt.type_ref:
            td = self.meta.get_type_def(stmt.type_ref.name)
            if td and hasattr(td, 'fields'):
                is_struct = True
        is_zero_init = (
            (isinstance(stmt.value, IntLit) and stmt.value.value == 0) or
            (isinstance(stmt.value, FloatLit) and stmt.value.value == 0.0)
        )
        if (is_struct or is_array) and is_zero_init:
            return
        if is_struct and isinstance(stmt.value, (FloatLit, BoolLit, CharLit)):
            return
        val = self.gen_expr_as_type(stmt.value, target_type)
        self.lines.append(f"{self.indent()}{stmt.name} = {val};")
        if not stmt.is_mut:
            self._track_safe_assignment(stmt.name, stmt.value)

    def gen_return(self, stmt):
        if stmt.value is not None:
            ret_type = self.current_func.return_type if self.current_func else None
            val = self.gen_expr_as_type(stmt.value, ret_type)
            if self.has_result_var:
                self.lines.append(f"{self.indent()}_result = {val};")
                self.lines.append(f"{self.indent()}goto cleanup;")
                self.need_cleanup_label = True
            else:
                self.lines.append(f"{self.indent()}return ({val});")
        else:
            if self.need_cleanup_label:
                self.lines.append(f"{self.indent()}goto cleanup;")
            else:
                self.lines.append(f"{self.indent()}return;")

    def gen_assign(self, stmt):
        if isinstance(stmt.target, IdentExpr):
            info = self.lookup_var(stmt.target.name)
            if info and info[0] and info[0].is_pointer and info[0].is_mut:
                target = f"*{stmt.target.name}"
            else:
                target = self.gen_expr(stmt.target)
        else:
            target = self.gen_expr(stmt.target)

        if stmt.op in ("++", "--"):
            target_ref = self.infer_type_ref(stmt.target)
            if self.is_compact_type_ref(target_ref):
                op = "+" if stmt.op == "++" else "-"
                scalar = self.gen_scalar_expr(stmt.target)
                self.lines.append(f"{self.indent()}{target} = {target_ref.name}_from_i64(({scalar}) {op} 1);")
            else:
                self.lines.append(f"{self.indent()}{target}{stmt.op};")
            return
        if stmt.value is None:
            return

        target_ref = self.infer_type_ref(stmt.target)
        target_scalar = self.gen_scalar_expr(stmt.target) if self.is_compact_type_ref(target_ref) else target
        val_scalar = self.gen_scalar_expr(stmt.value)

        has_user_array_transform = self.has_user_transform("array_access")
        has_user_binary_transform = self.has_user_transform("binary_op")
        has_user_div_transform = any(
            self.has_user_transform("binary_op", op_filter=op)
            for op in ("/", "%", "//")
        )

        if isinstance(stmt.target, ArrayAccess) and not has_user_array_transform:
            self.emit_bounds_check(stmt.target)

        is_integer = self.is_integer_type_ref(target_ref)

        if isinstance(stmt.value, BinaryOp) and stmt.value.op in ("/", "%", "//") and not has_user_div_transform:
            self.emit_div_check(self.gen_scalar_expr(stmt.value.left), self.gen_scalar_expr(stmt.value.right), stmt.value.op)

        if is_integer and self.meta.directives.get("overflow", "unchecked") == "checked":
            if isinstance(stmt.value, BinaryOp) and stmt.value.op in ("+", "-", "*"):
                if not has_user_binary_transform or not self.has_user_transform("binary_op", op_filter=stmt.value.op):
                    self.emit_overflow_check(stmt.value, target_scalar, target_ref)
            if stmt.op in ("+=", "-=", "*=") and not has_user_binary_transform:
                base_op = stmt.op[0]
                self.emit_overflow_check_compound(base_op, target_scalar, val_scalar, target_ref)

        if self.is_compact_type_ref(target_ref) and stmt.op in ("+=", "-=", "*=", "/=", "%="):
            base_op = stmt.op[0]
            val = f"{target_ref.name}_from_i64(({target_scalar}) {base_op} ({val_scalar}))"
            op = "="
        else:
            val = self.gen_expr_as_type(stmt.value, target_ref)
            op = stmt.op

        original = f"{target} {op} {val};"
        template_vars = {
            "target": target,
            "value": val,
            "original": original,
        }

        replaced = self.apply_transform_replace("assignment", template_vars)
        if replaced:
            for line in replaced.split("\n"):
                self.lines.append(f"{self.indent()}{line}")
        else:
            self.apply_transform_before("assignment", template_vars)
            self.lines.append(f"{self.indent()}{original}")
            self.apply_transform_after("assignment", template_vars)

        if isinstance(stmt.target, IdentExpr) and stmt.op == "=":
            self._track_safe_assignment(stmt.target.name, stmt.value)

    def emit_range_check(self, var_name, type_name):
        range_check = self.meta.directives.get("range_check", "never")
        if range_check != "always":
            return

        type_min, type_max = self.get_type_range(type_name)
        if type_min is None:
            return

        panic = self.panic_code()
        self.lines.append(f"{self.indent()}if ({var_name} < {type_min} || {var_name} > {type_max}) {{")
        self.lines.append(f"{self.indent()}\t{panic};")
        self.lines.append(f"{self.indent()}}}")
        self.need_cleanup_label = True

    def get_expr_type(self, expr):
        if isinstance(expr, IdentExpr):
            info = self.lookup_var(expr.name)
            if info and info[0]:
                return self.c_type(info[0])
        if isinstance(expr, FieldAccess):
            if isinstance(expr.obj, IdentExpr):
                info = self.lookup_var(expr.obj.name)
                if info and info[0]:
                    td = self.meta.get_type_def(info[0].name)
                    if td and hasattr(td, 'fields'):
                        for f in td.fields:
                            if f.name == expr.field:
                                return self.c_type(TypeRef(name=f.type_name, is_pointer=f.is_pointer))
        return None

    def emit_bounds_check(self, array_access):
        if self._is_index_proven_safe(array_access):
            return

        arr_str = self.gen_expr(array_access.array)
        idx_str = self.gen_expr(array_access.index)

        cap = self.get_array_capacity(array_access.array)
        if cap:
            self.lines.append(f"{self.indent()}if ({idx_str} < 0 || {idx_str} >= {cap}) {{")
            self.lines.append(f"{self.indent()}\tgoto cleanup;")
            self.lines.append(f"{self.indent()}}}")
            self.need_cleanup_label = True

    def get_array_capacity(self, expr):
        if isinstance(expr, FieldAccess):
            if isinstance(expr.obj, IdentExpr):
                param = None
                if self.current_func:
                    for p in self.current_func.params:
                        if p.name == expr.obj.name:
                            param = p
                            break
                if param and param.type_ref.is_pointer:
                    td = self.meta.get_type_def(param.type_ref.name)
                    if td and hasattr(td, 'capacity'):
                        return td.capacity
                    if td and hasattr(td, 'max_count_field'):
                        return f"{expr.obj.name}->{td.max_count_field}"

        for const in self.program.consts:
            if isinstance(expr, IdentExpr) and expr.name == const.name:
                if const.name in self.meta.consts:
                    return str(self.meta.consts[const.name])

        return None

    def emit_overflow_check(self, binop, target, target_type_ref=None):
        left = self.gen_scalar_expr(binop.left)
        right = self.gen_scalar_expr(binop.right)
        op = binop.op

        target_type_name = target_type_ref.name if target_type_ref else self.get_expr_type_name(target)
        type_min, type_max = self.get_type_range(target_type_name)

        if type_min is None:
            type_min = -2147483648
            type_max = 2147483647

        panic = self.panic_code()

        if op == "+":
            self.lines.append(f"{self.indent()}if ({left} > 0 && {right} > {type_max} - {left}) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.lines.append(f"{self.indent()}if ({left} < 0 && {right} < {type_min} - {left}) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.need_cleanup_label = True
        elif op == "-":
            self.lines.append(f"{self.indent()}if ({left} > 0 && {right} < {type_min} + {left}) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.lines.append(f"{self.indent()}if ({left} < 0 && {right} > {type_max} + {left}) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.need_cleanup_label = True
        elif op == "*":
            self.lines.append(f"{self.indent()}if ({left} != 0 && ({right} > {type_max} / {left} || {right} < {type_min} / {left})) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.need_cleanup_label = True

    def emit_overflow_check_compound(self, base_op, target, val, target_type_ref=None):
        target_type_name = target_type_ref.name if target_type_ref else self.get_expr_type_name(target)
        type_min, type_max = self.get_type_range(target_type_name)

        if type_min is None:
            type_min = -2147483648
            type_max = 2147483647

        panic = self.panic_code()

        if base_op == "+":
            self.lines.append(f"{self.indent()}if ({target} > 0 && {val} > {type_max} - {target}) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.lines.append(f"{self.indent()}if ({target} < 0 && {val} < {type_min} - {target}) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.need_cleanup_label = True
        elif base_op == "-":
            self.lines.append(f"{self.indent()}if ({target} > 0 && {val} < {type_min} + {target}) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.lines.append(f"{self.indent()}if ({target} < 0 && {val} > {type_max} + {target}) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.need_cleanup_label = True
        elif base_op == "*":
            self.lines.append(f"{self.indent()}if ({target} != 0 && ({val} > {type_max} / {target} || {val} < {type_min} / {target})) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.need_cleanup_label = True

    def get_expr_type_name(self, expr_str):
        if isinstance(expr_str, str):
            for vd in self.var_decls:
                if vd.name == expr_str:
                    return vd.type_ref.name
        return "int32"

    def emit_div_check(self, left, right, op):
        div_check = self.meta.directives.get("div_check", "never")
        if div_check != "always":
            return

        panic = self.panic_code()
        if op in ("/", "%", "//"):
            self.lines.append(f"{self.indent()}if ({right} == 0) {{")
            self.lines.append(f"{self.indent()}\t{panic};")
            self.lines.append(f"{self.indent()}}}")
            self.need_cleanup_label = True

    def gen_map(self, stmt):
        start = self.gen_expr(stmt.range_start)
        end = self.gen_expr(stmt.range_end)
        if stmt.inclusive:
            end = f"({end}) + 1"

        self.push_scope()
        self.declare_var(stmt.index_var, TypeRef(name="int32_t"), is_mut=False)

        loop_cap = self._check_loop_validated(stmt)

        self.lines.append(f"{self.indent()}for ({stmt.index_var} = {start}; "
                          f"{stmt.index_var} < {end}; ++{stmt.index_var}) {{")
        self.indent_level += 1

        if loop_cap is None:
            cap = self.get_range_capacity(stmt.range_end)
            if cap:
                self.lines.append(f"{self.indent()}if ({stmt.index_var} < 0 || {stmt.index_var} >= {cap}) {{")
                self.lines.append(f"{self.indent()}\tgoto cleanup;")
                self.lines.append(f"{self.indent()}}}")
                self.need_cleanup_label = True

        self.loop_validated.append((stmt.index_var, loop_cap))

        if stmt.cond:
            cond_expr = self.gen_expr(stmt.cond)
            self.lines.append(f"{self.indent()}if ({cond_expr}) {{")
            self.indent_level += 1
        for s in stmt.body:
            self.gen_stmt(s)
        if stmt.cond:
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

        self.loop_validated.pop()

        self.pop_scope()
        self.indent_level -= 1
        self.lines.append(f"{self.indent()}}}")

    def get_range_capacity(self, expr):
        if isinstance(expr, FieldAccess):
            return self.get_array_capacity(expr)
        return None

    def gen_fold(self, stmt):
        init = self.gen_expr(stmt.init_value)
        start = self.gen_expr(stmt.range_start)
        end = self.gen_expr(stmt.range_end)
        if stmt.inclusive:
            end = f"({end}) + 1"

        acc_var = None
        for vd in self.var_decls:
            if vd.is_acc and vd.name.startswith("_acc"):
                acc_var = vd.name
                break
        if acc_var is None:
            acc_var = self.fresh_var("_acc")

        self.lines.append(f"{self.indent()}acc = {init};")
        self.lines.append(f"{self.indent()}{acc_var} = {init};")

        self.push_scope()
        self.declare_var(stmt.index_var, TypeRef(name="int32_t"), is_mut=False)
        acc_type_ref = self.infer_type_ref(stmt.init_value)
        self.declare_var("acc", acc_type_ref, is_mut=False)

        loop_cap = self._check_loop_validated(stmt)

        self.lines.append(f"{self.indent()}for ({stmt.index_var} = {start}; "
                          f"{stmt.index_var} < {end}; ++{stmt.index_var}) {{")
        self.indent_level += 1

        if loop_cap is None:
            cap = self.get_range_capacity(stmt.range_end)
            if cap:
                self.lines.append(f"{self.indent()}if ({stmt.index_var} < 0 || {stmt.index_var} >= {cap}) {{")
                self.lines.append(f"{self.indent()}\tgoto cleanup;")
                self.lines.append(f"{self.indent()}}}")
                self.need_cleanup_label = True

        self.loop_validated.append((stmt.index_var, loop_cap))

        self.lines.append(f"{self.indent()}acc = {acc_var};")
        if stmt.cond:
            cond_expr = self.gen_expr(stmt.cond)
            self.lines.append(f"{self.indent()}if ({cond_expr}) {{")
            self.indent_level += 1

        for s in stmt.body:
            if isinstance(s, (BinaryOp, CallExpr, IdentExpr, IntLit, FloatLit,
                               BoolLit, CharLit, IfExpr)):
                result = self.gen_expr(s)
                self.lines.append(f"{self.indent()}{acc_var} = {result};")
            else:
                self.gen_stmt(s)

        if stmt.cond:
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")
        self.indent_level -= 1
        self.lines.append(f"{self.indent()}}}")

        self.loop_validated.pop()

        self.pop_scope()
        self.last_fold_result = acc_var

    def gen_if(self, stmt):
        cond = self.gen_expr(stmt.cond)
        self.lines.append(f"{self.indent()}if ({cond}) {{")
        self.indent_level += 1
        for s in stmt.then_body:
            self.gen_stmt(s)
        self.indent_level -= 1

        if stmt.else_body:
            if len(stmt.else_body) == 1 and isinstance(stmt.else_body[0], IfStmt):
                nested = stmt.else_body[0]
                cond2 = self.gen_expr(nested.cond)
                self.lines.append(f"{self.indent()}}} else if ({cond2}) {{")
                self.indent_level += 1
                for s in nested.then_body:
                    self.gen_stmt(s)
                self.indent_level -= 1
                if nested.else_body:
                    if len(nested.else_body) == 1 and isinstance(nested.else_body[0], IfStmt):
                        self._gen_else_chain_safety(nested.else_body)
                    else:
                        self.lines.append(f"{self.indent()}}} else {{")
                        self.indent_level += 1
                        for s in nested.else_body:
                            self.gen_stmt(s)
                        self.indent_level -= 1
                        self.lines.append(f"{self.indent()}}}")
                else:
                    self.lines.append(f"{self.indent()}}}")
            else:
                self.lines.append(f"{self.indent()}}} else {{")
                self.indent_level += 1
                for s in stmt.else_body:
                    self.gen_stmt(s)
                self.indent_level -= 1
                self.lines.append(f"{self.indent()}}}")
        else:
            self.lines.append(f"{self.indent()}}}")

        self._track_post_if_validation(stmt)

    def _gen_else_chain_safety(self, else_body):
        if len(else_body) == 1 and isinstance(else_body[0], IfStmt):
            nested = else_body[0]
            cond2 = self.gen_expr(nested.cond)
            self.lines.append(f"{self.indent()}}} else if ({cond2}) {{")
            self.indent_level += 1
            for s in nested.then_body:
                self.gen_stmt(s)
            self.indent_level -= 1
            if nested.else_body:
                self._gen_else_chain_safety(nested.else_body)
            else:
                self.lines.append(f"{self.indent()}}}")
        else:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            for s in else_body:
                self.gen_stmt(s)
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

    def gen_if_as_return_safety(self, stmt):
        cond = self.gen_expr(stmt.cond)
        self.lines.append(f"{self.indent()}if ({cond}) {{")
        self.indent_level += 1
        self._emit_return_body_safety(stmt.then_body)
        self.indent_level -= 1

        if stmt.else_body:
            self._gen_else_chain_return_safety(stmt.else_body)
        else:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            ret_type = self.current_func.return_type if self.current_func else None
            self.lines.append(f"{self.indent()}_result = {self.zero_value_for_type(ret_type)};")
            self.lines.append(f"{self.indent()}goto cleanup;")
            self.need_cleanup_label = True
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

    def _gen_else_chain_return_safety(self, else_body):
        if len(else_body) == 1 and isinstance(else_body[0], IfStmt):
            nested = else_body[0]
            cond2 = self.gen_expr(nested.cond)
            self.lines.append(f"{self.indent()}}} else if ({cond2}) {{")
            self.indent_level += 1
            self._emit_return_body_safety(nested.then_body)
            self.indent_level -= 1
            if nested.else_body:
                self._gen_else_chain_return_safety(nested.else_body)
            else:
                self.lines.append(f"{self.indent()}}} else {{")
                self.indent_level += 1
                ret_type = self.current_func.return_type if self.current_func else None
                self.lines.append(f"{self.indent()}_result = {self.zero_value_for_type(ret_type)};")
                self.lines.append(f"{self.indent()}goto cleanup;")
                self.need_cleanup_label = True
                self.indent_level -= 1
                self.lines.append(f"{self.indent()}}}")
        else:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            self._emit_return_body_safety(else_body)
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

    def _emit_return_body_safety(self, body):
        ret_type = self.current_func.return_type if self.current_func else None
        if len(body) == 1:
            val = self.gen_expr_as_type(body[0], ret_type)
            self.lines.append(f"{self.indent()}_result = {val};")
            self.lines.append(f"{self.indent()}goto cleanup;")
            self.need_cleanup_label = True
        elif body:
            for s in body[:-1]:
                self.gen_stmt(s)
            val = self.gen_expr_as_type(body[-1], ret_type)
            self.lines.append(f"{self.indent()}_result = {val};")
            self.lines.append(f"{self.indent()}goto cleanup;")
            self.need_cleanup_label = True
        else:
            self.lines.append(f"{self.indent()}_result = {self.zero_value_for_type(ret_type)};")
            self.lines.append(f"{self.indent()}goto cleanup;")
            self.need_cleanup_label = True

    def gen_while(self, stmt):
        cond = self.gen_expr(stmt.cond)
        self.lines.append(f"{self.indent()}while ({cond}) {{")
        self.indent_level += 1
        for s in stmt.body:
            self.gen_stmt(s)
        self.indent_level -= 1
        self.lines.append(f"{self.indent()}}}")

    def gen_match_stmt(self, stmt):
        first = True
        has_wildcard = False
        wildcard_arm = None
        for arm in stmt.arms:
            if isinstance(arm.pattern, IdentExpr) and arm.pattern.name == "_":
                has_wildcard = True
                wildcard_arm = arm
                continue
            cond = self.gen_match_arm_cond(stmt.subject, arm.pattern)
            if first:
                self.lines.append(f"{self.indent()}if ({cond}) {{")
                first = False
            else:
                self.lines.append(f"{self.indent()}}} else if ({cond}) {{")
            self.indent_level += 1
            if len(arm.body) == 1 and isinstance(arm.body[0], (BinaryOp, CallExpr, IdentExpr,
                                                                 IntLit, FloatLit, BoolLit, CharLit,
                                                                 IfExpr, MatchExpr)) and arm.is_expr:
                if self.has_result_var:
                    ret_type = self.current_func.return_type if self.current_func else None
                    result = self.gen_expr_as_type(arm.body[0], ret_type)
                    self.lines.append(f"{self.indent()}_result = {result};")
                    self.lines.append(f"{self.indent()}goto cleanup;")
                    self.need_cleanup_label = True
                else:
                    for s in arm.body:
                        self.gen_stmt(s)
            else:
                for s in arm.body:
                    self.gen_stmt(s)
            self.indent_level -= 1
        if has_wildcard:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            if len(wildcard_arm.body) == 1 and isinstance(wildcard_arm.body[0], (BinaryOp, CallExpr, IdentExpr,
                                                                                IntLit, FloatLit, BoolLit, CharLit,
                                                                                IfExpr, MatchExpr)) and wildcard_arm.is_expr:
                if self.has_result_var:
                    ret_type = self.current_func.return_type if self.current_func else None
                    result = self.gen_expr_as_type(wildcard_arm.body[0], ret_type)
                    self.lines.append(f"{self.indent()}_result = {result};")
                    self.lines.append(f"{self.indent()}goto cleanup;")
                    self.need_cleanup_label = True
                else:
                    for s in wildcard_arm.body:
                        self.gen_stmt(s)
            else:
                for s in wildcard_arm.body:
                    self.gen_stmt(s)
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")
        else:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            self.lines.append(f"{self.indent()}/* no match */")
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

    def _check_loop_validated(self, stmt):
        range_start_nonneg = isinstance(stmt.range_start, IntLit) and stmt.range_start.value >= 0

        end_key = None
        if isinstance(stmt.range_end, FieldAccess) and isinstance(stmt.range_end.obj, IdentExpr):
            end_key = f"{stmt.range_end.obj.name}->{stmt.range_end.field}"

        if range_start_nonneg and end_key and end_key in self.validated_counts:
            return self.validated_counts[end_key]

        return None

    def _is_index_proven_safe(self, array_access):
        if not isinstance(array_access.index, IdentExpr):
            return False

        var_name = array_access.index.name
        arr_cap = self.get_array_capacity(array_access.array)
        if arr_cap is None:
            return False

        if var_name in self.known_safe_vars:
            if str(self.known_safe_vars[var_name]) == str(arr_cap):
                return True

        for loop_var, loop_cap in reversed(self.loop_validated):
            if var_name == loop_var and loop_cap is not None:
                if str(loop_cap) == str(arr_cap):
                    return True

        return False

    def _track_safe_assignment(self, var_name, value_expr):
        if value_expr is None:
            self.known_safe_vars.pop(var_name, None)
            return
        if isinstance(value_expr, FieldAccess) and isinstance(value_expr.obj, IdentExpr):
            key = f"{value_expr.obj.name}->{value_expr.field}"
            if key in self.validated_counts:
                self.known_safe_vars[var_name] = self.validated_counts[key]
                return
        self.known_safe_vars.pop(var_name, None)

    def _track_post_if_validation(self, stmt):
        if not stmt.then_body or stmt.else_body:
            return
        if not isinstance(stmt.then_body[-1], ReturnStmt):
            return

        cond = stmt.cond
        if not isinstance(cond, BinaryOp) or cond.op != "||":
            return

        var_name = None
        count_key = None

        for side in (cond.left, cond.right):
            if not isinstance(side, BinaryOp):
                continue
            if side.op == "<" and isinstance(side.left, IdentExpr) and \
               isinstance(side.right, IntLit) and side.right.value == 0:
                var_name = side.left.name
            elif side.op == ">=" and isinstance(side.left, IdentExpr) and \
                 isinstance(side.right, FieldAccess) and isinstance(side.right.obj, IdentExpr):
                count_key = f"{side.right.obj.name}->{side.right.field}"

        if var_name and count_key and count_key in self.validated_counts:
            self.known_safe_vars[var_name] = self.validated_counts[count_key]

    def infer_type(self, expr):
        if isinstance(expr, IntLit):
            return "int32_t"
        if isinstance(expr, FloatLit):
            return "float"
        if isinstance(expr, CharLit):
            return "char"
        if isinstance(expr, StringLit):
            return "char *"
        if isinstance(expr, BoolLit):
            return "bool"
        if isinstance(expr, CallExpr):
            contract = self.meta.get_func_contract(expr.func_name)
            if contract:
                ret = contract.returns
                if "*" in ret:
                    return self.c_type(TypeRef(name=ret.replace("*", ""), is_pointer=True))
                return self.c_type(TypeRef(name=ret))
        return "int32_t"
