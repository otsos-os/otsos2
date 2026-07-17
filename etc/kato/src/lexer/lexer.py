from lexer.tokens import Token, TokenType, KEYWORDS
from util.errors import ErrorReporter


class Lexer:
    def __init__(self, source, filename="<unknown>"):
        self.source = source
        self.filename = filename
        self.pos = 0
        self.line = 1
        self.col = 1
        self.tokens = []
        self.reporter = ErrorReporter(filename)
        self.reporter.set_source(source)
        self.in_meta = False

    def peek(self, offset=0):
        idx = self.pos + offset
        if idx < len(self.source):
            return self.source[idx]
        return "\0"

    def advance(self):
        ch = self.source[self.pos]
        self.pos += 1
        if ch == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch

    def match(self, expected):
        if self.peek() == expected:
            self.advance()
            return True
        return False

    def add(self, ttype, value, line, col):
        self.tokens.append(Token(ttype, value, line, col))

    def tokenize(self):
        while self.pos < len(self.source):
            if self.in_meta:
                self.lex_meta()
            else:
                self.lex_normal()
        self.add(TokenType.EOF, "", self.line, self.col)
        return self.tokens, self.reporter

    def lex_normal(self):
        ch = self.peek()

        if ch in " \t\r":
            self.advance()
            return

        if ch == "\n":
            self.advance()
            return

        if ch == "/" and self.peek(1) == "/":
            while self.pos < len(self.source) and self.peek() != "\n":
                self.advance()
            return

        if ch == "!" and self.source[self.pos:self.pos + 6] == "!META!":
            line, col = self.line, self.col
            for _ in range(6):
                self.advance()
            self.add(TokenType.META_START, "!META!", line, col)
            self.in_meta = True
            return

        if ch.isdigit():
            self.lex_number()
            return

        if ch == "." and self.peek(1).isdigit():
            self.lex_number()
            return

        if ch.isalpha() or ch == "_":
            if self._match_asm_block():
                return
            self.lex_identifier()
            return

        if ch == '"':
            self.lex_string()
            return

        if ch == "'":
            self.lex_char()
            return

        self.lex_operator()

    def lex_meta(self):
        ch = self.peek()

        if ch in " \t\r":
            self.advance()
            return

        if ch == "\n":
            self.advance()
            return

        if ch == "/" and self.peek(1) == "/":
            while self.pos < len(self.source) and self.peek() != "\n":
                self.advance()
            return

        if self.source[self.pos:self.pos + 5] == "!END!":
            line, col = self.line, self.col
            for _ in range(5):
                self.advance()
            self.add(TokenType.META_END, "!END!", line, col)
            self.in_meta = False
            return

        if ch == "$":
            line, col = self.line, self.col
            self.advance()
            start = self.pos
            while self.pos < len(self.source) and (self.peek().isalpha() or self.peek() == "_"):
                self.advance()
            name = self.source[start:self.pos]
            if name:
                self.add(TokenType.META_DIRECTIVE, "$" + name, line, col)
            else:
                self.reporter.error("lexical", line, col, "expected directive name after '$'")
            return

        if ch == "%":
            line, col = self.line, self.col
            self.advance()
            start = self.pos
            while self.pos < len(self.source) and (self.peek().isalpha() or self.peek() == "_"):
                self.advance()
            name = self.source[start:self.pos]
            self.add(TokenType.META_DOLLAR_IDENT, "%" + name, line, col)
            return

        if ch == "{":
            self.lex_meta_brace_expr()
            return

        if ch.isdigit():
            self.lex_number()
            return

        if ch == '"':
            self.lex_string()
            return

        if ch.isalpha() or ch == "_":
            self.lex_meta_identifier()
            return

        self.lex_operator()

    def lex_meta_identifier(self):
        line, col = self.line, self.col
        start = self.pos
        while self.pos < len(self.source) and (self.peek().isalnum() or self.peek() == "_"):
            self.advance()
        text = self.source[start:self.pos]
        self.add(TokenType.IDENT, text, line, col)

    def lex_meta_brace_expr(self):
        line, col = self.line, self.col
        self.advance()
        depth = 1
        start = self.pos
        while self.pos < len(self.source) and depth > 0:
            if self.peek() == "{":
                depth += 1
            elif self.peek() == "}":
                depth -= 1
                if depth == 0:
                    break
            self.advance()
        expr_text = self.source[start:self.pos]
        if self.peek() == "}":
            self.advance()
        else:
            self.reporter.error("lexical", line, col, "unterminated compile-time expression")
        self.add(TokenType.META_LBRACE_EXPR, expr_text.strip(), line, col)

    def lex_number(self):
        line, col = self.line, self.col
        start = self.pos

        if self.peek() == "0" and self.peek(1) in ("x", "X"):
            self.advance()
            self.advance()
            hex_start = self.pos
            while self.pos < len(self.source) and (self.peek().isdigit() or self.peek() in "abcdefABCDEF"):
                self.advance()
            text = self.source[start:self.pos]
            self.add(TokenType.INT_LIT, str(int(text, 16)), line, col)
            return

        if self.peek() == "0" and self.peek(1) in ("b", "B"):
            self.advance()
            self.advance()
            bin_start = self.pos
            while self.pos < len(self.source) and self.peek() in "01":
                self.advance()
            text = self.source[bin_start:self.pos]
            self.add(TokenType.INT_LIT, str(int(text, 2)), line, col)
            return

        while self.pos < len(self.source) and (self.peek().isdigit() or self.peek() == "_"):
            self.advance()

        is_float = False
        if self.peek() == "." and self.peek(1) != ".":
            is_float = True
            self.advance()
            while self.pos < len(self.source) and (self.peek().isdigit() or self.peek() == "_"):
                self.advance()

        if self.peek() in ("e", "E"):
            is_float = True
            self.advance()
            if self.peek() in ("+", "-"):
                self.advance()
            while self.pos < len(self.source) and self.peek().isdigit():
                self.advance()

        if self.peek() in ("f", "F") and is_float:
            self.advance()

        text = self.source[start:self.pos].replace("_", "")
        if is_float:
            self.add(TokenType.FLOAT_LIT, text, line, col)
        else:
            self.add(TokenType.INT_LIT, text, line, col)

    def _match_asm_block(self):
        if self.source[self.pos:self.pos + 3] != "asm":
            return False
        if self.pos + 3 < len(self.source) and (self.source[self.pos + 3].isalnum() or self.source[self.pos + 3] == "_"):
            return False
        j = 3
        while self.pos + j < len(self.source) and self.source[self.pos + j] in " \t\r\n":
            j += 1
        if self.pos + j >= len(self.source) or self.source[self.pos + j] != "{":
            return False
        self.lex_asm_block()
        return True

    def lex_asm_block(self):
        line, col = self.line, self.col
        for _ in range(3):
            self.advance()
        while self.pos < len(self.source) and self.peek() in " \t\r\n":
            self.advance()
        if self.peek() == "{":
            self.advance()
        depth = 1
        start = self.pos
        while self.pos < len(self.source) and depth > 0:
            if self.peek() == "{":
                depth += 1
            elif self.peek() == "}":
                depth -= 1
                if depth == 0:
                    break
            self.advance()
        asm_code = self.source[start:self.pos]
        if self.peek() == "}":
            self.advance()
        else:
            self.reporter.error("lexical", line, col, "unterminated asm block")
        self.add(TokenType.ASM_BLOCK, asm_code, line, col)

    def lex_identifier(self):
        line, col = self.line, self.col
        start = self.pos
        while self.pos < len(self.source) and (self.peek().isalnum() or self.peek() == "_"):
            self.advance()
        text = self.source[start:self.pos]

        if text in ("true", "false"):
            self.add(TokenType.BOOL_LIT, text, line, col)
        elif text in KEYWORDS:
            self.add(KEYWORDS[text], text, line, col)
        else:
            self.add(TokenType.IDENT, text, line, col)

    def lex_string(self):
        line, col = self.line, self.col
        self.advance()
        chars = []
        while self.pos < len(self.source) and self.peek() != '"':
            if self.peek() == "\\":
                self.advance()
                esc = self.advance()
                mapping = {
                    "n": "\n", "t": "\t", "r": "\r", "0": "\0",
                    "\\": "\\", "'": "'", '"': '"', "a": "\a",
                    "b": "\b", "f": "\f", "v": "\v",
                }
                chars.append(mapping.get(esc, esc))
            else:
                chars.append(self.advance())
        if self.peek() == '"':
            self.advance()
        else:
            self.reporter.error("lexical", line, col, "unterminated string literal")
        self.add(TokenType.STRING_LIT, "".join(chars), line, col)

    def lex_char(self):
        line, col = self.line, self.col
        self.advance()
        if self.peek() == "\\":
            self.advance()
            esc = self.advance()
            mapping = {
                "n": "\n", "t": "\t", "r": "\r", "0": "\0",
                "\\": "\\", "'": "'", '"': '"', "a": "\a",
                "b": "\b", "f": "\f", "v": "\v",
            }
            val = mapping.get(esc, esc)
        else:
            val = self.advance()
        if self.peek() == "'":
            self.advance()
        else:
            self.reporter.error("lexical", line, col, "unterminated character literal")
        self.add(TokenType.CHAR_LIT, str(ord(val)), line, col)

    def lex_operator(self):
        line, col = self.line, self.col
        ch = self.peek()
        nxt = self.peek(1)

        three = ch + nxt + self.peek(2)
        two = ch + nxt

        three_char = {
            "..=": TokenType.DOTDOT_EQ,
        }
        two_char = {
            "==": TokenType.EQ,
            "!=": TokenType.NEQ,
            "<=": TokenType.LTE,
            ">=": TokenType.GTE,
            "&&": TokenType.AND,
            "||": TokenType.OR,
            "->": TokenType.ARROW,
            "=>": TokenType.FAT_ARROW,
            "..": TokenType.DOTDOT,
            "//": TokenType.DSLASH,
            "+=": TokenType.PLUS_EQ,
            "-=": TokenType.MINUS_EQ,
            "*=": TokenType.STAR_EQ,
            "/=": TokenType.SLASH_EQ,
            "%=": TokenType.PERCENT_EQ,
            "++": TokenType.PLUS_PLUS,
            "--": TokenType.MINUS_MINUS,
            "<<": TokenType.LSHIFT,
            ">>": TokenType.RSHIFT,
        }
        one_char = {
            "+": TokenType.PLUS,
            "-": TokenType.MINUS,
            "*": TokenType.STAR,
            "/": TokenType.SLASH,
            "%": TokenType.PERCENT,
            "<": TokenType.LT,
            ">": TokenType.GT,
            "!": TokenType.NOT,
            "&": TokenType.AMP,
            "(": TokenType.LPAREN,
            ")": TokenType.RPAREN,
            "{": TokenType.LBRACE,
            "}": TokenType.RBRACE,
            "[": TokenType.LBRACKET,
            "]": TokenType.RBRACKET,
            ";": TokenType.SEMI,
            ":": TokenType.COLON,
            ",": TokenType.COMMA,
            ".": TokenType.DOT,
            "~": TokenType.TILDE,
            "|": TokenType.PIPE,
            "^": TokenType.CARET,
            "=": TokenType.ASSIGN,
        }

        if three in three_char:
            self.advance(); self.advance(); self.advance()
            self.add(three_char[three], three, line, col)
        elif two in two_char:
            self.advance(); self.advance()
            self.add(two_char[two], two, line, col)
        elif ch in one_char:
            self.advance()
            self.add(one_char[ch], ch, line, col)
        else:
            self.reporter.error("lexical", line, col, f"unexpected character '{ch}'")
            self.advance()
