from lexer.tokens import Token, TokenType
from parser.ast import *
from util.errors import ErrorReporter


class ParseError(Exception):
    pass


class Parser:
    def __init__(self, tokens, filename="<unknown>"):
        self.tokens = [t for t in tokens if t.type not in (
            TokenType.META_START, TokenType.META_END,
            TokenType.META_DIRECTIVE, TokenType.META_DOLLAR_IDENT,
            TokenType.META_LBRACE_EXPR,
        )]
        self.pos = 0
        self.filename = filename
        self.reporter = ErrorReporter(filename)

    def peek(self, offset=0):
        idx = self.pos + offset
        if idx < len(self.tokens):
            return self.tokens[idx]
        if self.tokens:
            return self.tokens[-1]
        return Token(TokenType.EOF, "", 0, 0)

    def advance(self):
        tok = self.peek()
        if tok.type != TokenType.EOF:
            self.pos += 1
        return tok

    def check(self, ttype):
        return self.peek().type == ttype

    def match(self, *ttypes):
        if self.peek().type in ttypes:
            return self.advance()
        return None

    def expect(self, ttype, msg=None):
        tok = self.peek()
        if tok.type != ttype:
            if msg is None:
                msg = f"expected {ttype.name}, got {tok.type.name}"
            self.reporter.error("syntax", tok.line, tok.col, msg)
            raise ParseError(msg)
        return self.advance()

    def parse(self):
        program = Program()

        while not self.check(TokenType.EOF):
            tok = self.peek()
            try:
                if tok.type == TokenType.IMPORT:
                    self.advance()
                    name_tok = self.expect(TokenType.IDENT)
                    if self.check(TokenType.SEMI):
                        self.advance()
                    program.imports.append(name_tok.value)
                elif tok.type == TokenType.CONST:
                    program.consts.append(self.parse_const())
                elif tok.type == TokenType.STRUCT:
                    program.structs.append(self.parse_struct())
                elif tok.type == TokenType.ENUM:
                    program.enums.append(self.parse_enum())
                elif tok.type == TokenType.FUNC:
                    program.functions.append(self.parse_func())
                else:
                    self.advance()
            except ParseError:
                while not self.check(TokenType.EOF) and not self.check(TokenType.FUNC) \
                        and not self.check(TokenType.STRUCT) and not self.check(TokenType.ENUM) \
                        and not self.check(TokenType.CONST) and not self.check(TokenType.IMPORT):
                    self.advance()

        return program, self.reporter

    def parse_const(self):
        self.expect(TokenType.CONST)
        name_tok = self.expect(TokenType.IDENT)
        self.expect(TokenType.COLON)
        type_ref = self.parse_type_ref()
        self.expect(TokenType.ASSIGN)
        value = self.parse_expr()
        self.expect(TokenType.SEMI)
        return ConstDecl(name=name_tok.value, type_ref=type_ref, value=value,
                         line=name_tok.line)

    def parse_struct(self):
        kw = self.expect(TokenType.STRUCT)
        name = self.expect(TokenType.IDENT).value
        self.expect(TokenType.LBRACE)
        fields = []
        while not self.check(TokenType.RBRACE):
            type_ref = self.parse_type_ref()
            fname = self.expect(TokenType.IDENT)
            array_size = None
            if self.check(TokenType.LBRACKET):
                self.advance()
                array_size = self.parse_expr()
                self.expect(TokenType.RBRACKET)
            fields.append(StructField(name=fname.value, type_ref=type_ref,
                                       array_size=array_size, line=fname.line))
            if self.check(TokenType.COMMA):
                self.advance()
            elif self.check(TokenType.SEMI):
                self.advance()
            elif self.check(TokenType.RBRACE):
                break
        self.expect(TokenType.RBRACE)
        return StructDecl(name=name, fields=fields, line=kw.line)

    def parse_enum(self):
        kw = self.expect(TokenType.ENUM)
        name = self.expect(TokenType.IDENT).value
        self.expect(TokenType.LBRACE)
        variants = []
        while not self.check(TokenType.RBRACE):
            vname = self.expect(TokenType.IDENT)
            variants.append(EnumVariant(name=vname.value, line=vname.line))
            if self.check(TokenType.COMMA):
                self.advance()
            elif self.check(TokenType.RBRACE):
                break
        self.expect(TokenType.RBRACE)
        return EnumDecl(name=name, variants=variants, line=kw.line)

    def parse_func(self):
        kw = self.expect(TokenType.FUNC)
        name = self.expect(TokenType.IDENT).value
        self.expect(TokenType.LPAREN)
        params = []
        while not self.check(TokenType.RPAREN):
            is_mut = False
            if self.check(TokenType.MUT):
                self.advance()
                is_mut = True
            pname = self.expect(TokenType.IDENT)
            self.expect(TokenType.COLON)
            type_ref = self.parse_type_ref()
            params.append(Param(name=pname.value, type_ref=type_ref, is_mut=is_mut))
            if self.check(TokenType.COMMA):
                self.advance()
            elif self.check(TokenType.RPAREN):
                break
        self.expect(TokenType.RPAREN)

        return_type = None
        if self.check(TokenType.ARROW):
            self.advance()
            return_type = self.parse_type_ref()

        self.expect(TokenType.LBRACE)
        body = self.parse_block()
        self.expect(TokenType.RBRACE)

        return FuncDecl(name=name, params=params, return_type=return_type,
                        body=body, line=kw.line)

    def parse_type_ref(self):
        tok = self.peek()
        if tok.type == TokenType.VOID:
            self.advance()
            return TypeRef(name="void")

        if tok.type == TokenType.MUT:
            self.advance()
            inner = self.parse_type_ref()
            inner.is_mut = True
            inner.is_pointer = True
            return inner

        if tok.type == TokenType.FUNC:
            self.advance()
            self.expect(TokenType.LPAREN)
            param_types = []
            while not self.check(TokenType.RPAREN):
                pt = self.parse_type_ref()
                param_types.append(pt)
                if self.check(TokenType.COMMA):
                    self.advance()
                elif self.check(TokenType.RPAREN):
                    break
            self.expect(TokenType.RPAREN)
            ret_type = None
            if self.check(TokenType.ARROW):
                self.advance()
                ret_type = self.parse_type_ref()
            func_sig = "func(" + ",".join(pt.name for pt in param_types) + ")"
            if ret_type:
                func_sig += "->" + ret_type.name
            return TypeRef(name=func_sig, is_pointer=False)

        if tok.type in (TokenType.IDENT, TokenType.TRUE, TokenType.FALSE):
            name = self.advance().value
        else:
            self.reporter.error("syntax", tok.line, tok.col, f"expected type name, got {tok.type.name}")
            raise ParseError("expected type name")

        generic_args = []
        if self.check(TokenType.LT):
            generic_args = self.parse_generic_args()

        is_pointer = False
        if self.check(TokenType.STAR):
            self.advance()
            is_pointer = True

        is_array = False
        array_size = None
        if self.check(TokenType.LBRACKET):
            self.advance()
            is_array = True
            if not self.check(TokenType.RBRACKET):
                array_size = self.parse_expr()
            self.expect(TokenType.RBRACKET)

        return TypeRef(name=name, is_pointer=is_pointer, is_array=is_array,
                       array_size=array_size, generic_args=generic_args)

    def parse_generic_args(self):
        args = []
        self.expect(TokenType.LT)

        if self.check(TokenType.GT):
            self.advance()
            return args

        first = True
        while not self.check(TokenType.GT) and not self.check(TokenType.EOF):
            if not first:
                self.expect(TokenType.COMMA)
            first = False

            if self.check(TokenType.IDENT) and self.peek(1).type in (
                    TokenType.COMMA, TokenType.GT, TokenType.STAR,
                    TokenType.LT, TokenType.LBRACKET):
                args.append(self.parse_type_ref())
            elif self.check(TokenType.VOID):
                args.append(self.parse_type_ref())
            else:
                args.append(self.parse_generic_value_arg())

            if self.check(TokenType.COMMA):
                continue
            if self.check(TokenType.GT):
                break

        self.expect(TokenType.GT)
        return args

    def parse_generic_value_arg(self):
        tok = self.peek()
        if tok.type == TokenType.INT_LIT:
            self.advance()
            return IntLit(value=int(tok.value), line=tok.line)
        if tok.type == TokenType.IDENT:
            self.advance()
            return IdentExpr(name=tok.value, line=tok.line)
        self.reporter.error("syntax", tok.line, tok.col,
                            f"expected generic argument, got {tok.type.name}")
        raise ParseError("expected generic argument")

    def parse_block(self):
        stmts = []
        while not self.check(TokenType.RBRACE) and not self.check(TokenType.EOF):
            stmt = self.parse_stmt()
            if stmt is not None:
                if isinstance(stmt, list):
                    stmts.extend(stmt)
                else:
                    stmts.append(stmt)
        return stmts

    def parse_stmt(self):
        tok = self.peek()

        if tok.type == TokenType.LET:
            return self.parse_let()
        elif tok.type == TokenType.IF:
            return self.parse_if()
        elif tok.type == TokenType.WHILE:
            return self.parse_while()
        elif tok.type == TokenType.RETURN:
            return self.parse_return()
        elif tok.type == TokenType.BREAK:
            self.advance()
            self.match(TokenType.SEMI)
            return BreakStmt(line=tok.line)
        elif tok.type == TokenType.CONTINUE:
            self.advance()
            self.match(TokenType.SEMI)
            return ContinueStmt(line=tok.line)
        elif tok.type == TokenType.MATCH:
            return self.parse_match()
        elif tok.type == TokenType.MAP:
            return self.parse_map()
        elif tok.type == TokenType.FILTER:
            return self.parse_filter()
        elif tok.type == TokenType.FOLD:
            return self.parse_fold()
        elif tok.type == TokenType.ASM_BLOCK:
            self.advance()
            return self._parse_asm_block(tok)
        elif tok.type == TokenType.IDENT and tok.value == "c":
            next_tok = self.peek(1)
            if next_tok and next_tok.type == TokenType.DOT:
                return self.parse_c_stmt()
        else:
            return self.parse_expr_or_assign()

    def parse_let(self):
        kw = self.expect(TokenType.LET)
        is_mut = False
        if self.check(TokenType.MUT):
            self.advance()
            is_mut = True
        name = self.expect(TokenType.IDENT)
        type_ref = None
        if self.check(TokenType.COLON):
            self.advance()
            type_ref = self.parse_type_ref()
        value = None
        if self.check(TokenType.ASSIGN):
            self.advance()
            value = self.parse_expr()
        self.match(TokenType.SEMI)
        return LetStmt(name=name.value, type_ref=type_ref, value=value,
                       is_mut=is_mut, line=kw.line)

    def parse_if(self):
        kw = self.expect(TokenType.IF)
        cond = self.parse_expr()
        self.expect(TokenType.LBRACE)
        then_body = self.parse_block()
        self.expect(TokenType.RBRACE)

        else_body = []
        if self.check(TokenType.ELSE):
            self.advance()
            if self.check(TokenType.IF):
                nested = self.parse_if()
                else_body = [nested]
            else:
                self.expect(TokenType.LBRACE)
                else_body = self.parse_block()
                self.expect(TokenType.RBRACE)

        return IfStmt(cond=cond, then_body=then_body, else_body=else_body, line=kw.line)

    def parse_while(self):
        kw = self.expect(TokenType.WHILE)
        cond = self.parse_expr()
        self.expect(TokenType.LBRACE)
        body = self.parse_block()
        self.expect(TokenType.RBRACE)
        return WhileStmt(cond=cond, body=body, line=kw.line)

    def parse_return(self):
        kw = self.expect(TokenType.RETURN)
        value = None
        if not self.check(TokenType.SEMI) and not self.check(TokenType.RBRACE):
            value = self.parse_expr()
        self.match(TokenType.SEMI)
        return ReturnStmt(value=value, line=kw.line)

    def parse_match(self):
        kw = self.expect(TokenType.MATCH)
        subject = self.parse_expr()
        self.expect(TokenType.LBRACE)
        arms = []
        while not self.check(TokenType.RBRACE):
            pattern = self.parse_match_pattern()
            self.expect(TokenType.FAT_ARROW)

            if self.check(TokenType.LBRACE):
                self.advance()
                body = self.parse_block()
                self.expect(TokenType.RBRACE)
                arms.append(MatchArm(pattern=pattern, body=body, is_expr=False))
            else:
                expr = self.parse_expr()
                arms.append(MatchArm(pattern=pattern, body=[expr], is_expr=True))

            if self.check(TokenType.COMMA):
                self.advance()
            elif self.check(TokenType.RBRACE):
                break
        self.expect(TokenType.RBRACE)
        return MatchStmt(subject=subject, arms=arms, line=kw.line)

    def parse_match_pattern(self):
        tok = self.peek()
        if tok.type == TokenType.IDENT:
            if self.peek(1) and self.peek(1).type == TokenType.DOT:
                enum_name = tok.value
                self.advance()
                self.advance()
                variant = self.expect(TokenType.IDENT).value
                return IdentExpr(name=f"__enum_variant__{enum_name}_{variant}", line=tok.line)
            return IdentExpr(name=self.advance().value, line=tok.line)
        if tok.type in (TokenType.PLUS, TokenType.MINUS, TokenType.STAR, TokenType.SLASH):
            return CharLit(value=ord(self.advance().value[0]), line=tok.line)
        if tok.type == TokenType.CHAR_LIT:
            return CharLit(value=int(self.advance().value), line=tok.line)
        if tok.type == TokenType.INT_LIT:
            return IntLit(value=int(self.advance().value), line=tok.line)
        if tok.type == TokenType.UNDERSCORE or tok.value == "_":
            self.advance()
            return IdentExpr(name="_", line=tok.line)
        self.advance()
        return IdentExpr(name="_", line=tok.line)

    def parse_map(self):
        kw = self.expect(TokenType.MAP)
        self.expect(TokenType.LPAREN)
        index_var = self.expect(TokenType.IDENT).value
        self.expect(TokenType.IN)
        range_start = self.parse_expr()
        inclusive = self.parse_range_dotdot()
        range_end = self.parse_expr()
        self.expect(TokenType.RPAREN)

        cond = None
        if self.check(TokenType.WHERE):
            self.advance()
            cond = self.parse_expr()

        self.expect(TokenType.LBRACE)
        body = self.parse_block()
        self.expect(TokenType.RBRACE)

        return MapExpr(index_var=index_var, range_start=range_start, range_end=range_end,
                       inclusive=inclusive, cond=cond, body=body, line=kw.line)

    def parse_range_dotdot(self):
        if self.check(TokenType.DOTDOT_EQ):
            self.advance()
            return True
        self.expect(TokenType.DOTDOT)
        return False

    def parse_filter(self):
        kw = self.expect(TokenType.FILTER)
        self.expect(TokenType.LPAREN)
        index_var = self.expect(TokenType.IDENT).value
        self.expect(TokenType.IN)
        range_start = self.parse_expr()
        inclusive = self.parse_range_dotdot()
        range_end = self.parse_expr()
        self.expect(TokenType.RPAREN)

        cond = None
        if self.check(TokenType.WHERE):
            self.advance()
            cond = self.parse_expr()

        into_array = None
        into_count = None
        if self.peek() and self.peek().type == TokenType.IDENT and self.peek().value == "into":
            self.advance()
            into_array = self.expect(TokenType.IDENT).value
            self.expect(TokenType.COMMA)
            into_count = self.expect(TokenType.IDENT).value

        has_body = self.check(TokenType.LBRACE)
        body = []
        if has_body:
            self.advance()
            body = self.parse_block()
            self.expect(TokenType.RBRACE)

        self.match(TokenType.SEMI)

        return FilterExpr(index_var=index_var, range_start=range_start, range_end=range_end,
                          inclusive=inclusive, cond=cond, into_array=into_array,
                          into_count=into_count, line=kw.line)

    def parse_fold(self):
        kw = self.expect(TokenType.FOLD)
        self.expect(TokenType.LPAREN)
        init_value = self.parse_expr()
        self.expect(TokenType.COMMA)
        index_var = self.expect(TokenType.IDENT).value
        self.expect(TokenType.IN)
        range_start = self.parse_expr()
        inclusive = self.parse_range_dotdot()
        range_end = self.parse_expr()
        self.expect(TokenType.RPAREN)

        cond = None
        if self.check(TokenType.WHERE):
            self.advance()
            cond = self.parse_expr()

        self.expect(TokenType.LBRACE)
        body = self.parse_block()
        self.expect(TokenType.RBRACE)
        self.match(TokenType.SEMI)

        return FoldExpr(init_value=init_value, index_var=index_var,
                        range_start=range_start, range_end=range_end,
                        inclusive=inclusive, cond=cond, body=body, line=kw.line)

    def _parse_asm_block(self, tok):
        raw = tok.value
        inputs = []
        outputs = []
        clobbers = []
        body_lines = []

        lines = raw.split("\n")
        i = 0
        in_body = False

        while i < len(lines):
            line = lines[i]
            stripped = line.strip()

            if in_body:
                body_lines.append(line)
                i += 1
                continue

            if not stripped:
                i += 1
                continue

            matched = False
            for keyword in ("inputs", "outputs", "clobbers"):
                if stripped.startswith(keyword):
                    after = stripped[len(keyword):].lstrip()
                    if after.startswith("{"):
                        section_text = stripped
                        while "}" not in section_text and i + 1 < len(lines):
                            i += 1
                            section_text += " " + lines[i].strip()
                        brace_start = section_text.index("{") + 1
                        brace_end = section_text.rindex("}")
                        content = section_text[brace_start:brace_end].strip()
                        if keyword == "inputs":
                            inputs = self._parse_asm_operands(content)
                        elif keyword == "outputs":
                            outputs = self._parse_asm_operands(content)
                        elif keyword == "clobbers":
                            clobbers = self._parse_asm_clobbers(content)
                        matched = True
                        break

            if matched:
                i += 1
                continue

            in_body = True
            body_lines.append(line)
            i += 1

        body = "\n".join(body_lines).strip()
        return AsmBlock(code=body, inputs=inputs, outputs=outputs,
                        clobbers=clobbers, line=tok.line)

    def _parse_asm_operands(self, content):
        operands = []
        parts = []
        depth = 0
        current = []
        for ch in content:
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
            if not part or ":" not in part:
                continue
            name, constraint = part.split(":", 1)
            name = name.strip()
            constraint = constraint.strip().strip('"')
            if name and constraint:
                operands.append((name, constraint))
        return operands

    def _parse_asm_clobbers(self, content):
        clobbers = []
        parts = [p.strip() for p in content.split(",")]
        for part in parts:
            if not part:
                continue
            clobbers.append(part.strip('"'))
        return clobbers

    def parse_c_stmt(self):
        kw = self.advance()
        self.expect(TokenType.DOT)
        tok = self.peek()
        if tok.type == TokenType.IDENT and tok.value == "code":
            self.advance()
            self.expect(TokenType.LBRACE)
            depth = 1
            parts = []
            while not self.check(TokenType.EOF) and depth > 0:
                t = self.peek()
                if t.type == TokenType.LBRACE:
                    depth += 1
                    parts.append(t.value)
                    self.advance()
                elif t.type == TokenType.RBRACE:
                    depth -= 1
                    if depth == 0:
                        self.advance()
                        break
                    parts.append(t.value)
                    self.advance()
                else:
                    parts.append(t.value)
                    self.advance()
            return CCodeBlock(code=" ".join(parts), line=kw.line)
        elif tok.type == TokenType.IDENT:
            func_name = self.advance().value
            if not self.check(TokenType.LPAREN):
                self.match(TokenType.SEMI)
                return CallExpr(func_name=func_name, args=[], is_c_call=True, is_c_var_ref=True, line=kw.line)
            self.expect(TokenType.LPAREN)
            args = []
            while not self.check(TokenType.RPAREN):
                args.append(self.parse_expr())
                if self.check(TokenType.COMMA):
                    self.advance()
                elif self.check(TokenType.RPAREN):
                    break
            self.expect(TokenType.RPAREN)
            self.match(TokenType.SEMI)
            return CallExpr(func_name=func_name, args=args, is_c_call=True, line=kw.line)
        else:
            self.reporter.error("syntax", tok.line, tok.col, "expected c function name or code block")
            raise ParseError("expected c function name or code block")

    def parse_expr_or_assign(self):
        expr = self.parse_expr()

        tok = self.peek()
        if tok.type in (TokenType.ASSIGN, TokenType.PLUS_EQ, TokenType.MINUS_EQ,
                         TokenType.STAR_EQ, TokenType.SLASH_EQ, TokenType.PERCENT_EQ):
            op_map = {
                TokenType.ASSIGN: "=",
                TokenType.PLUS_EQ: "+=",
                TokenType.MINUS_EQ: "-=",
                TokenType.STAR_EQ: "*=",
                TokenType.SLASH_EQ: "/=",
                TokenType.PERCENT_EQ: "%=",
            }
            op = op_map[tok.type]
            self.advance()
            value = self.parse_expr()
            self.match(TokenType.SEMI)
            return AssignStmt(target=expr, value=value, op=op, line=tok.line)

        if tok.type in (TokenType.PLUS_PLUS, TokenType.MINUS_MINUS):
            op = "++" if tok.type == TokenType.PLUS_PLUS else "--"
            self.advance()
            self.match(TokenType.SEMI)
            return AssignStmt(target=expr, value=None, op=op, line=tok.line)

        self.match(TokenType.SEMI)
        return expr

    def parse_expr(self):
        return self.parse_match_expr_or_assign()

    def parse_match_expr_or_assign(self):
        if self.check(TokenType.MATCH):
            return self.parse_match_as_expr()
        if self.check(TokenType.IF):
            return self.parse_if_expr()
        return self.parse_ternary()

    def parse_if_expr(self):
        kw = self.expect(TokenType.IF)
        cond = self.parse_expr()
        self.expect(TokenType.LBRACE)
        then_expr = self.parse_expr()
        self.expect(TokenType.RBRACE)
        else_expr = None
        if self.check(TokenType.ELSE):
            self.advance()
            if self.check(TokenType.IF):
                else_expr = self.parse_if_expr()
            else:
                self.expect(TokenType.LBRACE)
                else_expr = self.parse_expr()
                self.expect(TokenType.RBRACE)
        return IfExpr(cond=cond, then_expr=then_expr, else_expr=else_expr, line=kw.line)

    def parse_match_as_expr(self):
        kw = self.expect(TokenType.MATCH)
        subject = self.parse_expr()
        self.expect(TokenType.LBRACE)
        arms = []
        while not self.check(TokenType.RBRACE):
            pattern = self.parse_match_pattern()
            self.expect(TokenType.FAT_ARROW)
            expr = self.parse_expr()
            arms.append(MatchExprArm(pattern=pattern, expr=expr))
            if self.check(TokenType.COMMA):
                self.advance()
            elif self.check(TokenType.RBRACE):
                break
        self.expect(TokenType.RBRACE)
        return MatchExpr(subject=subject, arms=arms, line=kw.line)

    def parse_ternary(self):
        cond = self.parse_or()
        if self.check(TokenType.IF):
            self.advance()
            then_expr = self.parse_or()
            if self.check(TokenType.ELSE):
                self.advance()
            else_expr = self.parse_ternary()
            return IfExpr(cond=cond, then_expr=then_expr, else_expr=else_expr, line=cond.line if hasattr(cond, 'line') else 0)
        return cond

    def parse_or(self):
        left = self.parse_and()
        while self.check(TokenType.OR):
            tok = self.advance()
            right = self.parse_and()
            left = BinaryOp(op="||", left=left, right=right, line=tok.line)
        return left

    def parse_and(self):
        left = self.parse_bitor()
        while self.check(TokenType.AND):
            tok = self.advance()
            right = self.parse_bitor()
            left = BinaryOp(op="&&", left=left, right=right, line=tok.line)
        return left

    def parse_bitor(self):
        left = self.parse_bitxor()
        while self.check(TokenType.PIPE):
            tok = self.advance()
            right = self.parse_bitxor()
            left = BinaryOp(op="|", left=left, right=right, line=tok.line)
        return left

    def parse_bitxor(self):
        left = self.parse_bitand()
        while self.check(TokenType.CARET):
            tok = self.advance()
            right = self.parse_bitand()
            left = BinaryOp(op="^", left=left, right=right, line=tok.line)
        return left

    def parse_bitand(self):
        left = self.parse_eq()
        while self.check(TokenType.AMP):
            tok = self.advance()
            right = self.parse_eq()
            left = BinaryOp(op="&", left=left, right=right, line=tok.line)
        return left

    def parse_eq(self):
        left = self.parse_cmp()
        while self.peek().type in (TokenType.EQ, TokenType.NEQ):
            tok = self.advance()
            right = self.parse_cmp()
            op = "==" if tok.type == TokenType.EQ else "!="
            left = BinaryOp(op=op, left=left, right=right, line=tok.line)
        return left

    def parse_cmp(self):
        left = self.parse_shift()
        while self.peek().type in (TokenType.LT, TokenType.GT, TokenType.LTE, TokenType.GTE):
            tok = self.advance()
            right = self.parse_shift()
            op_map = {TokenType.LT: "<", TokenType.GT: ">",
                      TokenType.LTE: "<=", TokenType.GTE: ">="}
            left = BinaryOp(op=op_map[tok.type], left=left, right=right, line=tok.line)
        return left

    def parse_shift(self):
        left = self.parse_add()
        while self.peek().type in (TokenType.LSHIFT, TokenType.RSHIFT):
            tok = self.advance()
            right = self.parse_add()
            op = "<<" if tok.type == TokenType.LSHIFT else ">>"
            left = BinaryOp(op=op, left=left, right=right, line=tok.line)
        return left

    def parse_add(self):
        left = self.parse_mul()
        while self.peek().type in (TokenType.PLUS, TokenType.MINUS):
            tok = self.advance()
            right = self.parse_mul()
            op = "+" if tok.type == TokenType.PLUS else "-"
            left = BinaryOp(op=op, left=left, right=right, line=tok.line)
        return left

    def parse_mul(self):
        left = self.parse_unary()
        while self.peek().type in (TokenType.STAR, TokenType.SLASH, TokenType.PERCENT, TokenType.DSLASH):
            tok = self.advance()
            right = self.parse_unary()
            op_map = {TokenType.STAR: "*", TokenType.SLASH: "/",
                      TokenType.PERCENT: "%", TokenType.DSLASH: "//"}
            left = BinaryOp(op=op_map[tok.type], left=left, right=right, line=tok.line)
        return left

    def parse_unary(self):
        tok = self.peek()
        if tok.type == TokenType.NOT:
            self.advance()
            operand = self.parse_unary()
            return UnaryOp(op="!", operand=operand, line=tok.line)
        if tok.type == TokenType.MINUS:
            self.advance()
            operand = self.parse_unary()
            return UnaryOp(op="-", operand=operand, line=tok.line)
        if tok.type == TokenType.TILDE:
            self.advance()
            operand = self.parse_unary()
            return UnaryOp(op="~", operand=operand, line=tok.line)
        if tok.type == TokenType.AMP:
            self.advance()
            operand = self.parse_unary()
            return AddrOf(operand=operand, line=tok.line)
        if tok.type == TokenType.STAR:
            self.advance()
            operand = self.parse_unary()
            return Deref(operand=operand, line=tok.line)
        if tok.type in (TokenType.PLUS_PLUS, TokenType.MINUS_MINUS):
            op = "++" if tok.type == TokenType.PLUS_PLUS else "--"
            self.advance()
            operand = self.parse_unary()
            return UnaryOp(op=op, operand=operand, line=tok.line)
        return self.parse_postfix()

    def parse_postfix(self):
        expr = self.parse_primary()

        while True:
            tok = self.peek()
            if tok.type == TokenType.DOT:
                self.advance()
                field = self.expect(TokenType.IDENT).value
                expr = FieldAccess(obj=expr, field=field, line=tok.line)
            elif tok.type == TokenType.LBRACKET:
                self.advance()
                index = self.parse_expr()
                self.expect(TokenType.RBRACKET)
                expr = ArrayAccess(array=expr, index=index, line=tok.line)
            elif tok.type in (TokenType.PLUS_PLUS, TokenType.MINUS_MINUS):
                op = "++" if tok.type == TokenType.PLUS_PLUS else "--"
                self.advance()
                expr = UnaryOp(op=f"post{op}", operand=expr, line=tok.line)
            else:
                break

        return expr

    def parse_primary(self):
        tok = self.peek()

        if tok.type == TokenType.INT_LIT:
            self.advance()
            return IntLit(value=int(tok.value), line=tok.line)

        if tok.type == TokenType.FLOAT_LIT:
            self.advance()
            raw = tok.value.rstrip("fF")
            return FloatLit(value=float(raw), line=tok.line)

        if tok.type == TokenType.CHAR_LIT:
            self.advance()
            return CharLit(value=int(tok.value), line=tok.line)

        if tok.type == TokenType.STRING_LIT:
            self.advance()
            return StringLit(value=tok.value, line=tok.line)

        if tok.type == TokenType.BOOL_LIT:
            self.advance()
            return BoolLit(value=(tok.value == "true"), line=tok.line)

        if tok.type == TokenType.LPAREN:
            self.advance()
            expr = self.parse_expr()
            self.expect(TokenType.RPAREN)
            return expr

        if tok.type == TokenType.LBRACKET:
            self.advance()
            elements = []
            while not self.check(TokenType.RBRACKET):
                elements.append(self.parse_expr())
                if self.check(TokenType.COMMA):
                    self.advance()
                elif self.check(TokenType.RBRACKET):
                    break
            self.expect(TokenType.RBRACKET)
            return ArrayLit(elements=elements, line=tok.line)

        if tok.type == TokenType.LBRACE:
            self.advance()
            if self.check(TokenType.RBRACE):
                self.advance()
                return InitList(value="0", line=tok.line)
            expr = self.parse_expr()
            if self.check(TokenType.COMMA):
                elements = [expr]
                while self.check(TokenType.COMMA):
                    self.advance()
                    if self.check(TokenType.RBRACE):
                        break
                    elements.append(self.parse_expr())
                self.expect(TokenType.RBRACE)
                return ArrayLit(elements=elements, line=tok.line)
            self.expect(TokenType.RBRACE)
            return expr

        if tok.type == TokenType.IDENT:
            self.advance()
            if tok.value == "c" and self.check(TokenType.DOT):
                self.advance()
                name_tok = self.peek()
                if name_tok and name_tok.type == TokenType.IDENT and name_tok.value == "code":
                    self.advance()
                    self.expect(TokenType.LBRACE)
                    depth = 1
                    parts = []
                    while not self.check(TokenType.EOF) and depth > 0:
                        t = self.peek()
                        if t.type == TokenType.LBRACE:
                            depth += 1
                            parts.append(t.value)
                            self.advance()
                        elif t.type == TokenType.RBRACE:
                            depth -= 1
                            if depth == 0:
                                self.advance()
                                break
                            parts.append(t.value)
                            self.advance()
                        else:
                            parts.append(t.value)
                            self.advance()
                    return CCodeBlock(code=" ".join(parts), line=tok.line)
                elif name_tok and name_tok.type == TokenType.IDENT:
                    fname = self.advance().value
                    if not self.check(TokenType.LPAREN):
                        return CallExpr(func_name=fname, args=[], is_c_call=True, is_c_var_ref=True, line=tok.line)
                    self.expect(TokenType.LPAREN)
                    args = []
                    while not self.check(TokenType.RPAREN):
                        args.append(self.parse_expr())
                        if self.check(TokenType.COMMA):
                            self.advance()
                        elif self.check(TokenType.RPAREN):
                            break
                    self.expect(TokenType.RPAREN)
                    return CallExpr(func_name=fname, args=args, is_c_call=True, line=tok.line)
            if self.check(TokenType.LPAREN):
                self.advance()
                args = []
                while not self.check(TokenType.RPAREN):
                    args.append(self.parse_expr())
                    if self.check(TokenType.COMMA):
                        self.advance()
                    elif self.check(TokenType.RPAREN):
                        break
                self.expect(TokenType.RPAREN)
                return CallExpr(func_name=tok.value, args=args, line=tok.line)
            return IdentExpr(name=tok.value, line=tok.line)

        self.reporter.error("syntax", tok.line, tok.col,
                            f"unexpected token: {tok.type.name} ({tok.value!r})")
        raise ParseError(f"unexpected token: {tok.type.name}")
