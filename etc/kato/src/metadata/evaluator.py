import re


class CompileTimeEvalError(Exception):
    pass


class CompileTimeEvaluator:
    def __init__(self, compile_vars, dsl_vars=None):
        self.compile_vars = compile_vars
        self.dsl_vars = dsl_vars or {}

    def eval(self, expr_text):
        try:
            tokens = self._tokenize_expr(expr_text)
            if not tokens:
                return None
            parser = ExprParser(tokens, self.compile_vars, self.dsl_vars)
            return parser.parse()
        except Exception as e:
            raise CompileTimeEvalError(f"compile-time expression error: {expr_text!r}: {e}")

    def eval_int(self, expr_text):
        val = self.eval(expr_text)
        if isinstance(val, float) and val == int(val):
            return int(val)
        if isinstance(val, (int, bool)):
            return int(val)
        raise CompileTimeEvalError(f"expected integer, got {type(val).__name__}: {expr_text!r}")

    def eval_str(self, expr_text):
        val = self.eval(expr_text)
        return str(val) if val is not None else ""

    def _tokenize_expr(self, text):
        tokens = []
        i = 0
        while i < len(text):
            ch = text[i]
            if ch in " \t\r\n":
                i += 1
                continue
            if ch.isdigit():
                start = i
                while i < len(text) and (text[i].isdigit() or text[i] == "_"):
                    i += 1
                if i < len(text) and text[i] == ".":
                    i += 1
                    while i < len(text) and text[i].isdigit():
                        i += 1
                    if i < len(text) and text[i] in "eE":
                        i += 1
                        if i < len(text) and text[i] in "+-":
                            i += 1
                        while i < len(text) and text[i].isdigit():
                            i += 1
                    tokens.append(("float", float(text[start:i].replace("_", ""))))
                else:
                    tokens.append(("int", int(text[start:i].replace("_", ""))))
            elif ch == '"':
                i += 1
                start = i
                while i < len(text) and text[i] != '"':
                    i += 1
                tokens.append(("str", text[start:i]))
                i += 1
            elif ch.isalpha() or ch == "_":
                start = i
                while i < len(text) and (text[i].isalnum() or text[i] == "_"):
                    i += 1
                tokens.append(("ident", text[start:i]))
            elif ch == "$":
                i += 1
                start = i
                while i < len(text) and (text[i].isalnum() or text[i] == "_"):
                    i += 1
                tokens.append(("dsl_var", text[start:i]))
            else:
                ops = ["<<", ">>", "==", "!=", "<=", ">=", "&&", "||"]
                matched = False
                for op in ops:
                    if text[i:i+2] == op:
                        tokens.append(("op", op))
                        i += 2
                        matched = True
                        break
                if not matched:
                    tokens.append(("op", ch))
                    i += 1
        return tokens


class ExprParser:
    def __init__(self, tokens, compile_vars, dsl_vars):
        self.tokens = tokens
        self.pos = 0
        self.compile_vars = compile_vars
        self.dsl_vars = dsl_vars

    def peek(self):
        if self.pos < len(self.tokens):
            return self.tokens[self.pos]
        return None

    def advance(self):
        tok = self.tokens[self.pos]
        self.pos += 1
        return tok

    def parse(self):
        return self.parse_ternary()

    def parse_ternary(self):
        cond = self.parse_or()
        if self.peek() and self.peek() == ("ident", "if"):
            self.advance()
            then_val = self.parse_or()
            if self.peek() and self.peek()[0] == "ident" and self.peek()[1] == "then":
                self.advance()
            else_val = self.parse_ternary()
            if self.peek() and self.peek()[0] == "ident" and self.peek()[1] == "else":
                self.advance()
                else_val = self.parse_ternary()
            return then_val if cond else else_val
        return cond

    def parse_or(self):
        left = self.parse_and()
        while self.peek() and self.peek() == ("op", "||"):
            self.advance()
            right = self.parse_and()
            left = bool(left) or bool(right)
        return left

    def parse_and(self):
        left = self.parse_bitor()
        while self.peek() and self.peek() == ("op", "&&"):
            self.advance()
            right = self.parse_bitor()
            left = bool(left) and bool(right)
        return left

    def parse_bitor(self):
        left = self.parse_bitxor()
        while self.peek() and self.peek() == ("op", "|"):
            self.advance()
            right = self.parse_bitxor()
            left = int(left) | int(right)
        return left

    def parse_bitxor(self):
        left = self.parse_bitand()
        while self.peek() and self.peek() == ("op", "^"):
            self.advance()
            right = self.parse_bitand()
            left = int(left) ^ int(right)
        return left

    def parse_bitand(self):
        left = self.parse_eq()
        while self.peek() and self.peek() == ("op", "&"):
            self.advance()
            right = self.parse_eq()
            left = int(left) & int(right)
        return left

    def parse_eq(self):
        left = self.parse_cmp()
        while self.peek() and self.peek()[0] == "op" and self.peek()[1] in ("==", "!="):
            op = self.advance()[1]
            right = self.parse_cmp()
            if op == "==":
                left = left == right
            else:
                left = left != right
        return left

    def parse_cmp(self):
        left = self.parse_shift()
        while self.peek() and self.peek()[0] == "op" and self.peek()[1] in ("<", ">", "<=", ">="):
            op = self.advance()[1]
            right = self.parse_shift()
            if op == "<":
                left = left < right
            elif op == ">":
                left = left > right
            elif op == "<=":
                left = left <= right
            else:
                left = left >= right
        return left

    def parse_shift(self):
        left = self.parse_add()
        while self.peek() and self.peek()[0] == "op" and self.peek()[1] in ("<<", ">>"):
            op = self.advance()[1]
            right = self.parse_add()
            if op == "<<":
                left = int(left) << int(right)
            else:
                left = int(left) >> int(right)
        return left

    def parse_add(self):
        left = self.parse_mul()
        while self.peek() and self.peek()[0] == "op" and self.peek()[1] in ("+", "-"):
            op = self.advance()[1]
            right = self.parse_mul()
            if op == "+":
                left = left + right
            else:
                left = left - right
        return left

    def parse_mul(self):
        left = self.parse_unary()
        while self.peek() and self.peek()[0] == "op" and self.peek()[1] in ("*", "/", "%"):
            op = self.advance()[1]
            right = self.parse_unary()
            if op == "*":
                left = left * right
            elif op == "/":
                if right == 0:
                    raise CompileTimeEvalError("division by zero in compile-time expression")
                left = left / right if isinstance(left, float) or isinstance(right, float) else left // right
            else:
                if right == 0:
                    raise CompileTimeEvalError("modulo by zero in compile-time expression")
                left = int(left) % int(right)
        return left

    def parse_unary(self):
        tok = self.peek()
        if tok and tok[0] == "op" and tok[1] == "!":
            self.advance()
            return not self.parse_unary()
        if tok and tok[0] == "op" and tok[1] == "-":
            self.advance()
            return -self.parse_unary()
        if tok and tok[0] == "op" and tok[1] == "~":
            self.advance()
            return ~int(self.parse_unary())
        return self.parse_primary()

    def parse_primary(self):
        tok = self.peek()
        if tok is None:
            raise CompileTimeEvalError("unexpected end of expression")
        kind, val = tok
        if kind == "int":
            self.advance()
            return val
        if kind == "float":
            self.advance()
            return val
        if kind == "str":
            self.advance()
            return val
        if kind == "ident":
            self.advance()
            if val == "true":
                return True
            if val == "false":
                return False
            if val in self.compile_vars:
                return self.compile_vars[val]
            if val == "sizeof":
                if self.peek() and self.peek()[0] == "ident":
                    type_name = self.advance()[1]
                    return self._sizeof(type_name)
                raise CompileTimeEvalError("sizeof requires a type argument")
            raise CompileTimeEvalError(f"undefined compile-time variable: {val}")
        if kind == "dsl_var":
            self.advance()
            if val in self.dsl_vars:
                return self.dsl_vars[val]
            raise CompileTimeEvalError(f"undefined DSL variable: ${val}")
        if kind == "op" and val == "(":
            self.advance()
            result = self.parse()
            if self.peek() and self.peek() == ("op", ")"):
                self.advance()
            return result
        raise CompileTimeEvalError(f"unexpected token: {tok}")

    def _sizeof(self, type_name):
        sizes = {
            "int8": 1, "int16": 2, "int32": 4, "int64": 8,
            "uint8": 1, "uint16": 2, "uint32": 4, "uint64": 8,
            "int": 4, "uint": 4, "float": 4, "float32": 4,
            "double": 8, "float64": 8, "char": 1, "bool": 1,
            "void": 0,
        }
        return sizes.get(type_name, 0)
