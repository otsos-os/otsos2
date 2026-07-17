from lexer.tokens import TokenType
from metadata.nodes import (
    IntegerTypeDef, FloatTypeDef, BoolTypeDef, CharTypeDef, VoidTypeDef,
    StructFieldDef, StructTypeDef, EnumVariantDef, EnumTypeDef, PointerTypeDef,
    FuncArgDef, FuncContract, ConstDef, TransformDef,
    CompileTimeLet, CompileTimeIf, CompileTimeAssert, VisibilityDecl,
    CFuncDef, MetadataModule,
)
from metadata.evaluator import CompileTimeEvaluator
from util.errors import ErrorReporter


class MetadataParser:
    def __init__(self, tokens, filename="<unknown>"):
        self.tokens = tokens
        self.pos = 0
        self.filename = filename
        self.reporter = ErrorReporter(filename)
        self.module = MetadataModule()

    def peek(self, offset=0):
        idx = self.pos + offset
        if idx < len(self.tokens):
            return self.tokens[idx]
        return None

    def advance(self):
        tok = self.tokens[self.pos]
        self.pos += 1
        return tok

    def expect(self, ttype):
        tok = self.peek()
        if tok is None or tok.type != ttype:
            self.reporter.error("metadata", tok.line if tok else 0, tok.col if tok else 0,
                                f"expected {ttype.name}, got {tok.type.name if tok else 'EOF'}")
            return None
        return self.advance()

    def parse(self):
        meta_tokens = []
        in_meta = False
        for tok in self.tokens:
            if tok.type == TokenType.META_START:
                in_meta = True
                continue
            if tok.type == TokenType.META_END:
                in_meta = False
                continue
            if in_meta:
                meta_tokens.append(tok)

        self.tokens = meta_tokens
        self.pos = 0

        while self.pos < len(self.tokens):
            tok = self.peek()
            if tok is None:
                break
            if tok.type == TokenType.META_DIRECTIVE:
                self.parse_directive()
            elif tok.type == TokenType.SEMI:
                self.advance()
            else:
                self.advance()

        self._apply_defaults()
        self._evaluate_compile_time()
        return self.module, self.reporter

    def parse_directive(self):
        tok = self.advance()
        name = tok.value[1:]

        if name == "mode":
            self.parse_mode()
        elif name == "define":
            self.parse_define()
        elif name == "let":
            self.parse_let()
        elif name == "if":
            self.parse_if()
        elif name == "assert":
            self.parse_assert()
        elif name == "space":
            self.parse_space()
        elif name == "transform":
            self.parse_transform()
        elif name == "transform_disable":
            self.parse_transform_disable()
        elif name == "c_include":
            self.parse_c_include()
        elif name == "c_flag":
            self.parse_c_flag()
        elif name == "c_name":
            self.parse_c_name()
        elif name == "c_prefix":
            self.parse_c_prefix()
        elif name == "c_no_prefix":
            self.parse_c_no_prefix()
        elif name == "c_export":
            self.parse_c_export()
        elif name == "c_func":
            self.parse_c_func()
        elif name == "emit":
            self.parse_emit()
        elif name == "import":
            self.parse_import()
        elif name in ("bounds", "overflow", "null_check", "div_check",
                       "init", "alloc", "thread", "complexity", "stack",
                       "panic", "layout", "range_check", "opt", "inline", "unroll",
                       "asm", "unsafe"):
            self.parse_simple_directive(name)
        else:
            self.reporter.error("metadata", tok.line, tok.col, f"unknown directive: ${name}")

    def parse_mode(self):
        tok = self.peek()
        if tok and tok.type == TokenType.IDENT:
            mode = self.advance().value
            if mode not in ("speed", "safety"):
                self.reporter.error("metadata", tok.line, tok.col, f"invalid mode: {mode}")
            self.module.mode = mode
            self.module.directives["mode"] = mode
        else:
            self.reporter.error("metadata", tok.line if tok else 0, tok.col if tok else 0,
                                "expected mode name after $mode")

    def parse_simple_directive(self, name):
        tok = self.peek()
        if tok and tok.type in (TokenType.IDENT, TokenType.INT_LIT, TokenType.META_LBRACE_EXPR):
            val_tok = self.advance()
            val = val_tok.value

            if tok.type == TokenType.IDENT and self.peek() and self.peek().type == TokenType.LPAREN:
                self.advance()
                inner_parts = [val, "("]
                while self.peek() and self.peek().type != TokenType.RPAREN:
                    inner_parts.append(str(self.advance().value))
                if self.peek() and self.peek().type == TokenType.RPAREN:
                    self.advance()
                    inner_parts.append(")")
                val = "".join(inner_parts)

            self.module.directives[name] = val

            opt_tok = self.peek()
            if opt_tok and opt_tok.type == TokenType.IDENT:
                self.advance()
                if name not in self.module.directives or not isinstance(self.module.directives[name], dict):
                    self.module.directives[name] = {"default": self.module.directives[name]}
                self.module.directives[name][opt_tok.value] = val

    def parse_let(self):
        name_tok = self.peek()
        if not name_tok or name_tok.type != TokenType.IDENT:
            self.reporter.error("metadata", name_tok.line if name_tok else 0,
                                name_tok.col if name_tok else 0,
                                "expected variable name after $let")
            return
        name = self.advance().value

        eq_tok = self.peek()
        if eq_tok and eq_tok.type in (TokenType.ASSIGN, TokenType.EQ):
            self.advance()
        elif eq_tok and eq_tok.type == TokenType.META_LBRACE_EXPR:
            pass
        else:
            self.reporter.error("metadata", eq_tok.line if eq_tok else 0,
                                eq_tok.col if eq_tok else 0,
                                "expected '=' or '{{}}' after $let name")
            return

        val_tok = self.peek()
        if val_tok and val_tok.type == TokenType.META_LBRACE_EXPR:
            expr_text = self.advance().value
            self.module.compile_vars[name] = ("expr", expr_text)
        elif val_tok and val_tok.type == TokenType.INT_LIT:
            self.module.compile_vars[name] = ("int", int(self.advance().value))
        elif val_tok and val_tok.type == TokenType.FLOAT_LIT:
            self.module.compile_vars[name] = ("float", float(self.advance().value))
        elif val_tok and val_tok.type == TokenType.STRING_LIT:
            self.module.compile_vars[name] = ("str", self.advance().value)
        elif val_tok and val_tok.type == TokenType.IDENT:
            ident_val = self.advance().value
            if ident_val in ("true", "false"):
                self.module.compile_vars[name] = ("bool", ident_val == "true")
            else:
                self.module.compile_vars[name] = ("ident", ident_val)
        else:
            self.reporter.error("metadata", val_tok.line if val_tok else 0,
                                val_tok.col if val_tok else 0,
                                "expected value after $let")

    def parse_if(self):
        cond_tok = self.peek()
        if not cond_tok or cond_tok.type != TokenType.META_LBRACE_EXPR:
            self.reporter.error("metadata", cond_tok.line if cond_tok else 0,
                                cond_tok.col if cond_tok else 0,
                                "expected {{}} condition after $if")
            return
        cond = self.advance().value

        then_tok = self.peek()
        if then_tok and then_tok.type == TokenType.IDENT and then_tok.value == "then":
            self.advance()

        then_branch = []
        else_branch = []

        while self.pos < len(self.tokens):
            tok = self.peek()
            if tok is None:
                break
            if tok.type == TokenType.META_DIRECTIVE:
                if tok.value == "$else":
                    self.advance()
                    while self.pos < len(self.tokens):
                        t = self.peek()
                        if t is None:
                            break
                        if t.type == TokenType.META_DIRECTIVE and t.value == "$end":
                            self.advance()
                            break
                        elif t.type == TokenType.META_DIRECTIVE:
                            self._collect_directive_into(else_branch)
                        else:
                            self.advance()
                    break
                elif tok.type == TokenType.META_DIRECTIVE and tok.value == "$end":
                    self.advance()
                    break
                else:
                    self._collect_directive_into(then_branch)
            else:
                self.advance()

        cti = CompileTimeIf(condition=cond, then_branch=then_branch, else_branch=else_branch)
        self.module.directives.setdefault("compile_ifs", []).append(cti)

    def _collect_directive_into(self, branch):
        start_pos = self.pos
        if self.peek() and self.peek().type == TokenType.META_DIRECTIVE:
            self.advance()
        depth = 0
        while self.pos < len(self.tokens):
            tok = self.peek()
            if tok is None:
                break
            if tok.type == TokenType.META_DIRECTIVE:
                if tok.value in ("$if",):
                    depth += 1
                    self.advance()
                    continue
                elif tok.value in ("$end",):
                    if depth > 0:
                        depth -= 1
                        self.advance()
                        continue
                    else:
                        break
                elif tok.value in ("$else",) and depth == 0:
                    break
            self.advance()
        end_pos = self.pos
        for i in range(start_pos, end_pos):
            if i < len(self.tokens):
                branch.append(self.tokens[i])

    def parse_assert(self):
        tok = self.peek()
        if not tok or tok.type != TokenType.META_LBRACE_EXPR:
            self.reporter.error("metadata", tok.line if tok else 0, tok.col if tok else 0,
                                "expected {{}} condition after $assert")
            return
        cond = self.advance().value
        self.module.directives.setdefault("asserts", []).append(CompileTimeAssert(condition=cond))

    def parse_space(self):
        kind_tok = self.peek()
        if not kind_tok or kind_tok.type != TokenType.META_DOLLAR_IDENT:
            self.reporter.error("metadata", kind_tok.line if kind_tok else 0,
                                kind_tok.col if kind_tok else 0,
                                "expected %export or %internal after $space")
            return
        kind = self.advance().value[1:]

        names = []
        while True:
            tok = self.peek()
            if tok is None:
                break
            if tok.type == TokenType.IDENT:
                names.append(self.advance().value)
                comma = self.peek()
                if comma and comma.type == TokenType.COMMA:
                    self.advance()
                else:
                    break
            else:
                break

        self.module.visibility.setdefault(kind, set()).update(names)

    def parse_define(self):
        kind_tok = self.peek()
        if not kind_tok or kind_tok.type != TokenType.META_DOLLAR_IDENT:
            self.reporter.error("metadata", kind_tok.line if kind_tok else 0,
                                kind_tok.col if kind_tok else 0,
                                "expected %type, %func, or %const after $define")
            return
        kind = self.advance().value[1:]

        if kind == "type":
            self.parse_type_def()
        elif kind == "func":
            self.parse_func_def()
        elif kind == "const":
            self.parse_const_def()
        else:
            self.reporter.error("metadata", kind_tok.line, kind_tok.col,
                                f"unknown define kind: %{kind}")

    def parse_type_def(self):
        name_tok = self.peek()
        if not name_tok or name_tok.type != TokenType.IDENT:
            self.reporter.error("metadata", name_tok.line if name_tok else 0,
                                name_tok.col if name_tok else 0,
                                "expected type name")
            return
        name = self.advance().value

        as_tok = self.peek()
        if as_tok and as_tok.type == TokenType.IDENT and as_tok.value == "as":
            self.advance()
        else:
            self.reporter.error("metadata", as_tok.line if as_tok else 0,
                                as_tok.col if as_tok else 0,
                                "expected 'as' after type name")
            return

        kind_tok = self.peek()
        if not kind_tok or kind_tok.type != TokenType.IDENT:
            self.reporter.error("metadata", kind_tok.line if kind_tok else 0,
                                kind_tok.col if kind_tok else 0,
                                "expected type kind (integer, float, boolean, character, empty, struct, enum, pointer)")
            return
        kind = self.advance().value

        if kind == "integer":
            self._parse_integer_type(name)
        elif kind == "float":
            self._parse_float_type(name)
        elif kind == "boolean":
            self._parse_bool_type(name)
        elif kind == "character":
            self._parse_char_type(name)
        elif kind == "empty":
            self.module.type_defs[name] = VoidTypeDef(name=name)
        elif kind == "struct":
            self._parse_struct_type(name)
        elif kind == "enum":
            self._parse_enum_type(name)
        elif kind == "pointer":
            self._parse_pointer_type(name)
        else:
            self.reporter.error("metadata", kind_tok.line, kind_tok.col,
                                f"unknown type kind: {kind}")

    def _parse_integer_type(self, name):
        td = IntegerTypeDef(name=name)
        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()
                val_tok = self.peek()
                if val_tok and val_tok.type == TokenType.INT_LIT:
                    val = int(self.advance().value)
                    if prop == "bits":
                        td.bits = val
                elif val_tok and val_tok.type == TokenType.META_LBRACE_EXPR:
                    expr = self.advance().value
                    if prop == "from":
                        td.from_val = ("expr", expr)
                    elif prop == "to":
                        td.to_val = ("expr", expr)
                elif val_tok and val_tok.type in (TokenType.TRUE, TokenType.IDENT):
                    val = self.advance().value
                    if prop == "signed":
                        td.signed = (val == "true")
                else:
                    break
            else:
                break
        self.module.type_defs[name] = td

    def _parse_float_type(self, name):
        td = FloatTypeDef(name=name)
        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()
                val_tok = self.peek()
                if val_tok and val_tok.type == TokenType.INT_LIT:
                    val = int(self.advance().value)
                    if prop == "bits":
                        td.bits = val
                elif val_tok and val_tok.type == TokenType.IDENT:
                    val = self.advance().value
                    if prop == "encoding":
                        td.encoding = val
                    elif prop == "precision":
                        td.precision = val
                else:
                    break
            else:
                break
        self.module.type_defs[name] = td

    def _parse_bool_type(self, name):
        td = BoolTypeDef(name=name)
        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()
                val_tok = self.peek()
                if val_tok and val_tok.type == TokenType.INT_LIT:
                    val = int(self.advance().value)
                    if prop == "bits":
                        td.bits = val
                elif val_tok and val_tok.type == TokenType.META_LBRACE_EXPR:
                    expr = self.advance().value
                    if prop == "true_value":
                        td.true_value = ("expr", expr)
                    elif prop == "false_value":
                        td.false_value = ("expr", expr)
                else:
                    break
            else:
                break
        self.module.type_defs[name] = td

    def _parse_char_type(self, name):
        td = CharTypeDef(name=name)
        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()
                val_tok = self.peek()
                if val_tok and val_tok.type == TokenType.INT_LIT:
                    val = int(self.advance().value)
                    if prop == "bits":
                        td.bits = val
                elif val_tok and val_tok.type == TokenType.IDENT:
                    val = self.advance().value
                    if prop == "signed":
                        td.signed = (val == "true")
                    elif prop == "encoding":
                        td.encoding = val
                else:
                    break
            else:
                break
        self.module.type_defs[name] = td

    def _parse_struct_type(self, name):
        td = StructTypeDef(name=name)
        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()

                if prop == "fields":
                    brace = self.peek()
                    if brace and brace.type == TokenType.META_LBRACE_EXPR:
                        self.advance()
                        td.fields = self._parse_struct_fields(brace.value)
                    elif brace and brace.type == TokenType.LBRACE:
                        self.advance()
                        fields_text = self._read_until_rbrace()
                        td.fields = self._parse_struct_fields(fields_text)
                elif prop == "layout":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        td.layout = self.advance().value
                elif prop == "max_count_field":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        td.max_count_field = self.advance().value
                elif prop == "capacity":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.META_LBRACE_EXPR:
                        td.capacity = ("expr", self.advance().value)
                    elif val_tok and val_tok.type == TokenType.INT_LIT:
                        td.capacity = ("int", int(self.advance().value))
                elif prop == "align":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.INT_LIT:
                        td.align = int(self.advance().value)
                elif prop == "packed":
                    val_tok = self.peek()
                    if val_tok and val_tok.type in (TokenType.TRUE, TokenType.FALSE, TokenType.IDENT):
                        td.packed = self.advance().value == "true"
                else:
                    pass
            else:
                break
        self.module.type_defs[name] = td

    def _read_until_rbrace(self):
        depth = 1
        parts = []
        while self.pos < len(self.tokens):
            tok = self.peek()
            if tok.type == TokenType.LBRACE:
                depth += 1
                parts.append(tok.value)
                self.advance()
            elif tok.type == TokenType.RBRACE:
                depth -= 1
                if depth == 0:
                    self.advance()
                    break
                parts.append(tok.value)
                self.advance()
            else:
                parts.append(tok.value)
                self.advance()
        return " ".join(parts)

    def _parse_struct_fields(self, text):
        fields = []
        parts = [p.strip() for p in text.split(",")]
        for part in parts:
            if not part:
                continue
            if ":" not in part:
                continue
            fname, ftype = part.split(":", 1)
            fname = fname.strip()
            ftype = ftype.strip()

            is_pointer = ftype.endswith("*")
            if is_pointer:
                ftype = ftype[:-1].strip()

            array_size = None
            if "[" in ftype and ftype.endswith("]"):
                base, arr = ftype.split("[", 1)
                ftype = base.strip()
                array_size = arr[:-1].strip()

            fields.append(StructFieldDef(
                name=fname,
                type_name=ftype,
                array_size=array_size,
                is_pointer=is_pointer,
            ))
        return fields

    def _parse_enum_type(self, name):
        td = EnumTypeDef(name=name)
        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()

                if prop == "base":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        td.base = self.advance().value
                elif prop == "variants":
                    brace = self.peek()
                    if brace and brace.type == TokenType.META_LBRACE_EXPR:
                        self.advance()
                        variants_text = brace.value
                    elif brace and brace.type == TokenType.LBRACE:
                        self.advance()
                        variants_text = self._read_until_rbrace()
                    else:
                        break

                    parts = [p.strip() for p in variants_text.split(",")]
                    for part in parts:
                        if not part:
                            continue
                        if ":" not in part:
                            continue
                        vname, vval = part.split(":", 1)
                        vname = vname.strip()
                        vval = vval.strip()
                        if vval.startswith("{") and vval.endswith("}"):
                            vval = vval[1:-1].strip()
                        td.variants.append(EnumVariantDef(name=vname, value=vval))
            else:
                break
        self.module.type_defs[name] = td

    def _parse_pointer_type(self, name):
        td = PointerTypeDef(name=name)
        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()
                val_tok = self.peek()
                if val_tok and val_tok.type == TokenType.IDENT:
                    val = self.advance().value
                    if prop == "target":
                        td.target = val
                    elif prop == "nullable":
                        td.nullable = (val == "true")
                else:
                    break
            else:
                break
        self.module.type_defs[name] = td

    def parse_func_def(self):
        name_tok = self.peek()
        if not name_tok or name_tok.type != TokenType.IDENT:
            self.reporter.error("metadata", name_tok.line if name_tok else 0,
                                name_tok.col if name_tok else 0,
                                "expected function name")
            return
        name = self.advance().value

        as_tok = self.peek()
        if as_tok and as_tok.type == TokenType.IDENT and as_tok.value == "as":
            self.advance()
        else:
            self.reporter.error("metadata", as_tok.line if as_tok else 0,
                                as_tok.col if as_tok else 0,
                                "expected 'as' after function name")
            return

        kind_tok = self.peek()
        if not kind_tok or kind_tok.type != TokenType.IDENT:
            self.reporter.error("metadata", kind_tok.line if kind_tok else 0,
                                kind_tok.col if kind_tok else 0,
                                "expected function kind (function, procedure, start)")
            return
        kind = self.advance().value

        fc = FuncContract(name=name, kind=kind)
        if kind == "start":
            self.module.has_main = True

        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()

                if prop == "args":
                    brace = self.peek()
                    if brace and brace.type == TokenType.META_LBRACE_EXPR:
                        self.advance()
                        fc.args = self._parse_func_args(brace.value)
                    elif brace and brace.type == TokenType.IDENT and brace.value == "void":
                        self.advance()
                        fc.args = []
                    elif brace and brace.type == TokenType.LBRACE:
                        self.advance()
                        args_text = self._read_until_rbrace()
                        fc.args = self._parse_func_args(args_text)
                elif prop == "returns":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        ret = self.advance().value
                        ptr = self.peek()
                        if ptr and ptr.type == TokenType.STAR:
                            self.advance()
                            fc.returns = ret + "*"
                        else:
                            fc.returns = ret
                elif prop == "pure":
                    val_tok = self.peek()
                    if val_tok and val_tok.type in (TokenType.TRUE, TokenType.FALSE, TokenType.IDENT):
                        fc.pure = self.advance().value == "true"
                elif prop == "mutates":
                    brace = self.peek()
                    if brace and brace.type == TokenType.META_LBRACE_EXPR:
                        self.advance()
                        fc.mutates = [a.strip() for a in brace.value.split(",") if a.strip()]
                    elif brace and brace.type == TokenType.IDENT:
                        fc.mutates.append(self.advance().value)
                        while True:
                            c = self.peek()
                            if c and c.type == TokenType.COMMA:
                                self.advance()
                                n = self.peek()
                                if n and n.type == TokenType.IDENT:
                                    fc.mutates.append(self.advance().value)
                            else:
                                break
                elif prop == "inline":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        fc.inline = self.advance().value
                elif prop == "unroll":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        val = self.advance().value
                        if val == "count":
                            paren = self.peek()
                            if paren and paren.type == TokenType.LPAREN:
                                self.advance()
                                num = self.peek()
                                if num and num.type == TokenType.INT_LIT:
                                    fc.unroll = f"count({self.advance().value})"
                            close = self.peek()
                            if close and close.type == TokenType.RPAREN:
                                self.advance()
                        else:
                            fc.unroll = val
                elif prop == "complexity":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        val = self.advance().value
                        if val == "max":
                            paren = self.peek()
                            if paren and paren.type == TokenType.LPAREN:
                                self.advance()
                                num = self.peek()
                                if num and num.type == TokenType.INT_LIT:
                                    fc.complexity = int(self.advance().value)
                            close = self.peek()
                            if close and close.type == TokenType.RPAREN:
                                self.advance()
                elif prop == "allocates":
                    val_tok = self.peek()
                    if val_tok and val_tok.type in (TokenType.TRUE, TokenType.FALSE, TokenType.IDENT):
                        fc.allocates = self.advance().value == "true"
                elif prop == "recurses":
                    val_tok = self.peek()
                    if val_tok and val_tok.type in (TokenType.TRUE, TokenType.FALSE, TokenType.IDENT):
                        fc.recurses = self.advance().value == "true"
                elif prop == "threadsafe":
                    val_tok = self.peek()
                    if val_tok and val_tok.type in (TokenType.TRUE, TokenType.FALSE, TokenType.IDENT):
                        fc.threadsafe = self.advance().value == "true"
                elif prop == "restrict":
                    val_tok = self.peek()
                    if val_tok and val_tok.type in (TokenType.TRUE, TokenType.FALSE, TokenType.IDENT):
                        fc.restrict = self.advance().value == "true"
            else:
                break

        self.module.func_contracts[name] = fc

    def _parse_func_args(self, text):
        args = []
        parts = []
        depth = 0
        current = []
        for ch in text:
            if ch == "(":
                depth += 1
                current.append(ch)
            elif ch == ")":
                depth -= 1
                current.append(ch)
            elif ch == "," and depth == 0:
                parts.append("".join(current).strip())
                current = []
            else:
                current.append(ch)
        if current:
            parts.append("".join(current).strip())

        for part in parts:
            if not part or part == "void":
                continue
            if ":" not in part:
                continue
            aname, atype = part.split(":", 1)
            aname = aname.strip()
            atype = atype.strip()

            is_mut = False
            if aname.startswith("mut "):
                is_mut = True
                aname = aname[4:].strip()

            is_pointer = atype.endswith("*")
            if is_pointer:
                atype = atype[:-1].strip()

            args.append(FuncArgDef(
                name=aname,
                type_str=atype,
                is_pointer=is_pointer,
                is_mut=is_mut,
            ))
        return args

    def parse_const_def(self):
        name_tok = self.peek()
        if not name_tok or name_tok.type != TokenType.IDENT:
            self.reporter.error("metadata", name_tok.line if name_tok else 0,
                                name_tok.col if name_tok else 0,
                                "expected constant name")
            return
        name = self.advance().value

        eq_tok = self.peek()
        if eq_tok and eq_tok.type in (TokenType.ASSIGN, TokenType.EQ):
            self.advance()
        elif eq_tok and eq_tok.type == TokenType.IDENT and eq_tok.value == "as":
            self.advance()
        else:
            self.reporter.error("metadata", eq_tok.line if eq_tok else 0,
                                eq_tok.col if eq_tok else 0,
                                "expected '=' or 'as' after $const name")
            return

        val_tok = self.peek()
        if val_tok and val_tok.type == TokenType.META_LBRACE_EXPR:
            expr = self.advance().value
            self.module.consts[name] = ("expr", expr)
        elif val_tok and val_tok.type == TokenType.INT_LIT:
            self.module.consts[name] = ("int", int(self.advance().value))
        elif val_tok and val_tok.type == TokenType.FLOAT_LIT:
            self.module.consts[name] = ("float", float(self.advance().value))
        elif val_tok and val_tok.type == TokenType.STRING_LIT:
            self.module.consts[name] = ("str", self.advance().value)
        else:
            self.reporter.error("metadata", val_tok.line if val_tok else 0,
                                val_tok.col if val_tok else 0,
                                "expected constant value")

    def parse_transform(self):
        name_tok = self.peek()
        if not name_tok or name_tok.type != TokenType.IDENT:
            self.reporter.error("metadata", name_tok.line if name_tok else 0,
                                name_tok.col if name_tok else 0,
                                "expected transform name")
            return
        name = self.advance().value
        td = TransformDef(name=name)

        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()

                if prop == "match":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        match_val = self.advance().value
                        if match_val == "binary_op":
                            paren = self.peek()
                            if paren and paren.type == TokenType.LPAREN:
                                self.advance()
                                op_tok = self.peek()
                                if op_tok and op_tok.type in (TokenType.PLUS, TokenType.MINUS,
                                                               TokenType.STAR, TokenType.SLASH,
                                                               TokenType.PERCENT, TokenType.IDENT,
                                                               TokenType.STRING_LIT):
                                    td.match_op = self.advance().value
                                close = self.peek()
                                if close and close.type == TokenType.RPAREN:
                                    self.advance()
                            td.match = "binary_op"
                        else:
                            td.match = match_val
                elif prop == "condition":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.META_LBRACE_EXPR:
                        td.condition = self.advance().value
                elif prop in ("before", "after", "replace"):
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.META_LBRACE_EXPR:
                        td.__dict__[prop] = self.advance().value
            else:
                break

        self.module.transforms.append(td)

    def parse_transform_disable(self):
        while True:
            tok = self.peek()
            if tok and tok.type == TokenType.IDENT:
                self.module.disabled_transforms.append(self.advance().value)
                c = self.peek()
                if c and c.type == TokenType.COMMA:
                    self.advance()
                else:
                    break
            else:
                break

    def parse_c_include(self):
        tok = self.peek()
        if tok and tok.type == TokenType.IDENT:
            inc = self.advance().value
            if self.peek() and self.peek().type == TokenType.DOT:
                self.advance()
                while True:
                    t = self.peek()
                    if t and t.type == TokenType.IDENT:
                        inc += "." + self.advance().value
                        if self.peek() and self.peek().type == TokenType.DOT:
                            self.advance()
                        else:
                            break
                    else:
                        break
            self.module.c_includes.append(inc)

    def parse_c_flag(self):
        tok = self.peek()
        if tok and tok.type == TokenType.STRING_LIT:
            self.module.c_flags.append(self.advance().value)

    def parse_c_name(self):
        name_tok = self.peek()
        if name_tok and name_tok.type == TokenType.IDENT:
            fname = self.advance().value
            c_tok = self.peek()
            if c_tok and c_tok.type == TokenType.IDENT:
                self.module.c_names[fname] = self.advance().value

    def parse_c_prefix(self):
        tok = self.peek()
        if tok and tok.type == TokenType.STRING_LIT:
            self.module.c_prefix = self.advance().value

    def parse_c_no_prefix(self):
        self.module.c_no_prefix = True

    def parse_c_export(self):
        tok = self.peek()
        if tok and tok.type == TokenType.IDENT:
            self.module.c_exports.append(self.advance().value)

    def parse_c_func(self):
        name_tok = self.peek()
        if not name_tok or name_tok.type != TokenType.IDENT:
            self.reporter.error("metadata", name_tok.line if name_tok else 0,
                                name_tok.col if name_tok else 0,
                                "expected C function name after $c_func")
            return
        name = self.advance().value

        as_tok = self.peek()
        if as_tok and as_tok.type == TokenType.IDENT and as_tok.value == "as":
            self.advance()
            kind_tok = self.peek()
            if kind_tok and kind_tok.type == TokenType.IDENT:
                self.advance()

        cf = CFuncDef(name=name)

        while True:
            tok = self.peek()
            if tok is None or tok.type == TokenType.META_DIRECTIVE:
                break
            if tok.type == TokenType.IDENT:
                prop = self.advance().value
                colon = self.peek()
                if colon and colon.type == TokenType.COLON:
                    self.advance()
                if prop == "header":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        cf.header = self.advance().value
                elif prop == "returns":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.IDENT:
                        ret = self.advance().value
                        star = self.peek()
                        if star and star.type == TokenType.STAR:
                            self.advance()
                            ret += "*"
                        cf.returns = ret
                elif prop == "args":
                    val_tok = self.peek()
                    if val_tok and val_tok.type == TokenType.META_LBRACE_EXPR:
                        cf.args = self.advance().value
            else:
                break

        self.module.c_funcs[name] = cf

    def parse_emit(self):
        lang_tok = self.peek()
        if lang_tok and lang_tok.type == TokenType.IDENT:
            self.advance()
            code_tok = self.peek()
            if code_tok and code_tok.type == TokenType.STRING_LIT:
                self.module.emits.append(self.advance().value)

    def parse_import(self):
        tok = self.peek()
        if tok and tok.type == TokenType.IDENT:
            self.module.imports.append(self.advance().value)

    def _apply_defaults(self):
        if self.module.mode == "speed":
            defaults = {
                "bounds": "none",
                "overflow": "unchecked",
                "null_check": "never",
                "div_check": "never",
                "init": "none",
                "alloc": "dynamic",
                "thread": "unsafe",
                "panic": "abort",
                "range_check": "never",
                "layout": "soa",
                "asm": "allowed",
            }
        else:
            defaults = {
                "bounds": "static",
                "overflow": "checked",
                "null_check": "always",
                "div_check": "always",
                "init": "zero",
                "alloc": "static",
                "thread": "single",
                "panic": "halt",
                "range_check": "always",
                "layout": "soa",
                "asm": "never",
            }

        for key, val in defaults.items():
            if key not in self.module.directives:
                self.module.directives[key] = val

        if "complexity" not in self.module.directives and self.module.mode == "safety":
            self.module.directives["complexity"] = "10"

    def _evaluate_compile_time(self):
        resolved_vars = {}
        for name, (kind, val) in self.module.compile_vars.items():
            if kind == "expr":
                evaluator = CompileTimeEvaluator(resolved_vars)
                resolved_vars[name] = evaluator.eval(val)
            elif kind == "int":
                resolved_vars[name] = val
            elif kind == "float":
                resolved_vars[name] = val
            elif kind == "str":
                resolved_vars[name] = val
            elif kind == "bool":
                resolved_vars[name] = val
            elif kind == "ident":
                if val in resolved_vars:
                    resolved_vars[name] = resolved_vars[val]
                else:
                    resolved_vars[name] = val

        self.module.compile_vars = resolved_vars

        for name, (kind, val) in list(self.module.consts.items()):
            if kind == "expr":
                evaluator = CompileTimeEvaluator(resolved_vars)
                self.module.consts[name] = evaluator.eval(val)
            elif kind == "int":
                self.module.consts[name] = val
            elif kind == "float":
                self.module.consts[name] = val
            elif kind == "str":
                self.module.consts[name] = val

        for name, td in self.module.type_defs.items():
            if isinstance(td, StructTypeDef):
                if isinstance(td.capacity, tuple):
                    kind, val = td.capacity
                    if kind == "expr":
                        evaluator = CompileTimeEvaluator(resolved_vars)
                        td.capacity = evaluator.eval_int(val)
                    elif kind == "int":
                        td.capacity = val
            elif isinstance(td, IntegerTypeDef):
                if isinstance(td.from_val, tuple):
                    kind, val = td.from_val
                    if kind == "expr":
                        evaluator = CompileTimeEvaluator(resolved_vars)
                        td.from_val = evaluator.eval_int(val)
                if isinstance(td.to_val, tuple):
                    kind, val = td.to_val
                    if kind == "expr":
                        evaluator = CompileTimeEvaluator(resolved_vars)
                        td.to_val = evaluator.eval_int(val)

        if "asserts" in self.module.directives:
            evaluator = CompileTimeEvaluator(resolved_vars)
            for assert_node in self.module.directives["asserts"]:
                try:
                    result = evaluator.eval(assert_node.condition)
                    if not result:
                        self.reporter.error("compile-time", 0, 0,
                                            f"assertion failed: {assert_node.condition}")
                except Exception as e:
                    self.reporter.error("compile-time", 0, 0,
                                        f"assertion evaluation error: {assert_node.condition}: {e}")

        if "compile_ifs" in self.module.directives:
            evaluator = CompileTimeEvaluator(resolved_vars)
            for cti in self.module.directives["compile_ifs"]:
                try:
                    result = evaluator.eval(cti.condition)
                    branch = cti.then_branch if result else cti.else_branch
                    if branch:
                        sub_parser = MetadataParser(branch, self.filename)
                        sub_parser.module = self.module
                        sub_parser.tokens = branch
                        sub_parser.pos = 0
                        while sub_parser.pos < len(sub_parser.tokens):
                            tok = sub_parser.peek()
                            if tok is None:
                                break
                            if tok.type == TokenType.META_DIRECTIVE:
                                sub_parser.parse_directive()
                            elif tok.type == TokenType.SEMI:
                                sub_parser.advance()
                            else:
                                sub_parser.advance()
                except Exception:
                    pass
