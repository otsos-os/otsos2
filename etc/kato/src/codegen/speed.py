from codegen.gen import CodeGenerator
from parser.ast import *


class SpeedCodeGen(CodeGenerator):
    def __init__(self, program, metadata, freestanding=False):
        super().__init__(program, metadata, freestanding=freestanding)

    def _emit_unroll_pragma(self):
        if not self.current_func:
            return
        contract = self.meta.get_func_contract(self.current_func.name)
        unroll = None

        unroll_dir = self.meta.directives.get("unroll")
        if isinstance(unroll_dir, dict):
            if self.current_func.name in unroll_dir:
                unroll = unroll_dir[self.current_func.name]
        elif isinstance(unroll_dir, str):
            unroll = unroll_dir

        if unroll is None and contract and contract.unroll:
            unroll = contract.unroll

        if unroll and unroll.startswith("count("):
            count = unroll.replace("count(", "").replace(")", "")
            self.lines.append(f"{self.indent()}#pragma unroll {count}")
        elif unroll == "no":
            self.lines.append(f"{self.indent()}#pragma nounroll")

    def _emit_unroll_pragma_fold(self):
        self._emit_unroll_pragma()

    def format_params(self, func):
        if func.name == "main":
            return "void"

        if not func.params:
            return "void"

        contract = self.meta.get_func_contract(func.name)
        use_restrict = True
        if contract and contract.restrict == False:
            use_restrict = False
        if self.meta.directives.get("opt", "") == "no_restrict":
            use_restrict = False

        parts = []
        for param in func.params:
            ptype = self.c_type(param.type_ref)
            if param.type_ref.is_pointer:
                if use_restrict:
                    parts.append(f"{ptype}restrict {param.name}")
                else:
                    parts.append(f"{ptype} {param.name}")
            else:
                parts.append(f"{ptype} {param.name}")
        return ", ".join(parts)

    def emit_func_decl(self, func):
        prefix = "static " if self.is_static(func.name) else ""
        inline = ""
        contract = self.meta.get_func_contract(func.name)
        if contract and contract.inline == "always":
            if self.is_static(func.name):
                inline = "inline "
            else:
                inline = "static inline "

        ret_type = self.c_type(func.return_type) if func.return_type else "void"
        params = self.format_params(func)

        if func.name == "main":
            self.lines.append(f"{ret_type}")
            self.lines.append(f"{self.func_c_name(func.name)}(void);")
        else:
            if inline:
                self.lines.append(f"{prefix}{inline}{ret_type}")
            else:
                self.lines.append(f"{prefix}{ret_type}")
            self.lines.append(f"{self.func_c_name(func.name)}({params});")

    def emit_func_body(self, func):
        self.scopes = [{}]
        for param in func.params:
            self.declare_var(param.name, param.type_ref, is_mut=param.is_mut)

        self.emit_var_decls()

        self.push_scope()

        body_len = len(func.body)
        has_implicit_return = False
        if func.return_type and func.return_type.name != "void":
            if not self.body_ends_with_return(func.body):
                last = self.get_last_expr(func.body)
                if last is not None:
                    has_implicit_return = True

        for i, stmt in enumerate(func.body):
            if has_implicit_return and i == body_len - 1:
                continue
            self.gen_stmt(stmt)
        self.pop_scope()

        self.emit_void_suppressions()

        if func.return_type and func.return_type.name != "void":
            if not self.body_ends_with_return(func.body):
                if func.name == "main":
                    self.lines.append(f"{self.indent()}return (0);")
                else:
                    last = self.get_last_expr(func.body)
                    if isinstance(last, FoldExpr):
                        self.gen_fold(last)
                        val = self.gen_expr_as_type(IdentExpr(self.last_fold_result), func.return_type)
                        self.lines.append(f"{self.indent()}return ({val});")
                    elif isinstance(last, IfStmt):
                        self.gen_if_as_return(last, func.return_type)
                    elif last is not None:
                        val = self.gen_expr_as_type(last, func.return_type)
                        self.lines.append(f"{self.indent()}return ({val});")
                    else:
                        self.lines.append(f"{self.indent()}return (0);")
        elif func.return_type is None or func.return_type.name == "void":
            if not self.body_ends_with_return(func.body):
                self.lines.append(f"{self.indent()}return;")

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
                              ArrayAccess, Deref, AddrOf, UnaryOp)):
            return last
        if isinstance(last, FoldExpr):
            return last
        if isinstance(last, IfStmt):
            return last
        return None

    def gen_return(self, stmt):
        if stmt.value is not None:
            ret_type = self.current_func.return_type if self.current_func else None
            val = self.gen_expr_as_type(stmt.value, ret_type)
            self.lines.append(f"{self.indent()}return ({val});")
        else:
            self.lines.append(f"{self.indent()}return;")

    def gen_map(self, stmt):
        start = self.gen_expr(stmt.range_start)
        end = self.gen_expr(stmt.range_end)
        if stmt.inclusive:
            end = f"({end}) + 1"

        self._emit_unroll_pragma()

        self.push_scope()
        self.declare_var(stmt.index_var, TypeRef(name="int"), is_mut=False)

        self.lines.append(f"{self.indent()}for ({stmt.index_var} = {start}; "
                          f"{stmt.index_var} < {end}; ++{stmt.index_var}) {{")
        self.indent_level += 1
        if stmt.cond:
            cond_expr = self.gen_expr(stmt.cond)
            self.lines.append(f"{self.indent()}if ({cond_expr}) {{")
            self.indent_level += 1
        for s in stmt.body:
            self.gen_stmt(s)
        if stmt.cond:
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

        self.pop_scope()
        self.indent_level -= 1
        self.lines.append(f"{self.indent()}}}")
