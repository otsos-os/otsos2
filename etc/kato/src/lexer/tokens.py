from enum import Enum, auto
from dataclasses import dataclass


class TokenType(Enum):
    INT_LIT = auto()
    FLOAT_LIT = auto()
    CHAR_LIT = auto()
    STRING_LIT = auto()
    BOOL_LIT = auto()
    IDENT = auto()

    LET = auto()
    MUT = auto()
    FUNC = auto()
    STRUCT = auto()
    ENUM = auto()
    CONST = auto()
    IF = auto()
    ELSE = auto()
    MATCH = auto()
    WHERE = auto()
    MAP = auto()
    FILTER = auto()
    FOLD = auto()
    IN = auto()
    RETURN = auto()
    BREAK = auto()
    CONTINUE = auto()
    IMPORT = auto()
    EXPORT = auto()
    TRUE = auto()
    FALSE = auto()
    VOID = auto()
    WHILE = auto()

    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    DSLASH = auto()
    PERCENT = auto()
    EQ = auto()
    ASSIGN = auto()
    NEQ = auto()
    LT = auto()
    GT = auto()
    LTE = auto()
    GTE = auto()
    AND = auto()
    OR = auto()
    NOT = auto()
    AMP = auto()
    STAR_PTR = auto()
    ARROW = auto()
    FAT_ARROW = auto()
    DOTDOT = auto()
    DOTDOT_EQ = auto()
    LPAREN = auto()
    RPAREN = auto()
    LBRACE = auto()
    RBRACE = auto()
    LBRACKET = auto()
    RBRACKET = auto()
    SEMI = auto()
    COLON = auto()
    COMMA = auto()
    DOT = auto()

    PLUS_EQ = auto()
    MINUS_EQ = auto()
    STAR_EQ = auto()
    SLASH_EQ = auto()
    PERCENT_EQ = auto()
    PLUS_PLUS = auto()
    MINUS_MINUS = auto()
    TILDE = auto()
    PIPE = auto()
    CARET = auto()
    LSHIFT = auto()
    RSHIFT = auto()

    META_START = auto()
    META_END = auto()
    META_DIRECTIVE = auto()
    META_DOLLAR_IDENT = auto()
    META_LBRACE_EXPR = auto()

    ASM_BLOCK = auto()

    EOF = auto()


KEYWORDS = {
    "let": TokenType.LET,
    "mut": TokenType.MUT,
    "func": TokenType.FUNC,
    "struct": TokenType.STRUCT,
    "enum": TokenType.ENUM,
    "const": TokenType.CONST,
    "if": TokenType.IF,
    "else": TokenType.ELSE,
    "match": TokenType.MATCH,
    "where": TokenType.WHERE,
    "map": TokenType.MAP,
    "filter": TokenType.FILTER,
    "fold": TokenType.FOLD,
    "in": TokenType.IN,
    "return": TokenType.RETURN,
    "break": TokenType.BREAK,
    "continue": TokenType.CONTINUE,
    "import": TokenType.IMPORT,
    "export": TokenType.EXPORT,
    "true": TokenType.TRUE,
    "false": TokenType.FALSE,
    "void": TokenType.VOID,
    "while": TokenType.WHILE,
}


@dataclass
class Token:
    type: TokenType
    value: str
    line: int
    col: int

    def __repr__(self):
        return f"Token({self.type.name}, {self.value!r}, {self.line}:{self.col})"
