from dataclasses import dataclass, field
from typing import Optional, List, Any


@dataclass
class TypeRef:
    name: str
    is_pointer: bool = False
    is_array: bool = False
    array_size: Optional[Any] = None
    is_mut: bool = False
    generic_args: List[Any] = field(default_factory=list)

    def c_type(self, mode="speed"):
        base = self.name
        if base == "rawptr":
            return "volatile unsigned char *"
        if base.startswith("func("):
            if mode == "safety":
                return "void *"
            parts = base.split("->")
            param_part = parts[0][5:-1]
            ret = parts[1] if len(parts) > 1 else "void"
            ret_map = {
                "int": "int", "int32": "int", "float": "float", "float32": "float",
                "double": "double", "float64": "double", "void": "void",
                "bool": "bool", "char": "char",
            }
            ret_c = ret_map.get(ret, ret)
            if not param_part.strip():
                return f"{ret_c} (*)"
            param_types = [p.strip() for p in param_part.split(",")]
            type_map = {
                "int": "int", "int32": "int", "float": "float", "float32": "float",
                "double": "double", "float64": "double", "bool": "bool", "char": "char",
            }
            c_params = ", ".join(type_map.get(p, p) for p in param_types)
            return f"{ret_c} (*)({c_params})"
        if mode == "safety":
            type_map = {
                "int": "int32_t", "int32": "int32_t", "int64": "int64_t",
                "uint": "uint32_t", "uint32": "uint32_t", "uint64": "uint64_t",
                "uint8": "uint8_t", "int8": "int8_t",
                "uint16": "uint16_t", "int16": "int16_t",
                "float": "float", "float32": "float",
                "double": "double", "float64": "double",
                "char": "char", "char8": "char", "bool": "bool", "void": "void",
            }
            base = type_map.get(base, base)
        else:
            type_map = {
                "int32": "int", "int": "int", "int64": "long long",
                "uint": "unsigned int", "uint32": "unsigned int",
                "uint64": "unsigned long long", "uint8": "unsigned char",
                "int8": "char", "uint16": "unsigned short", "int16": "short",
                "float32": "float", "float": "float",
                "float64": "double", "double": "double",
                "char": "char", "char8": "char", "bool": "bool", "void": "void",
            }
            base = type_map.get(base, base)

        if self.is_pointer:
            return base + " *"
        return base

    def __repr__(self):
        s = self.name
        if self.is_pointer:
            s += "*"
        if self.is_array:
            s += f"[{self.array_size}]"
        if self.generic_args:
            s += "<" + ", ".join(str(arg) for arg in self.generic_args) + ">"
        if self.is_mut:
            s = "mut " + s
        return s


@dataclass
class Param:
    name: str
    type_ref: TypeRef
    is_mut: bool = False


@dataclass
class Program:
    structs: List = field(default_factory=list)
    enums: List = field(default_factory=list)
    consts: List = field(default_factory=list)
    functions: List = field(default_factory=list)
    imports: List = field(default_factory=list)


@dataclass
class StructDecl:
    name: str
    fields: List = field(default_factory=list)
    line: int = 0


@dataclass
class StructField:
    name: str
    type_ref: TypeRef
    array_size: Optional[Any] = None
    line: int = 0


@dataclass
class EnumDecl:
    name: str
    variants: List = field(default_factory=list)
    line: int = 0


@dataclass
class EnumVariant:
    name: str
    line: int = 0


@dataclass
class ConstDecl:
    name: str
    type_ref: TypeRef
    value: Any
    line: int = 0


@dataclass
class FuncDecl:
    name: str
    params: List = field(default_factory=list)
    return_type: TypeRef = None
    body: List = field(default_factory=list)
    is_mut_self: bool = False
    line: int = 0


@dataclass
class LetStmt:
    name: str
    type_ref: Optional[TypeRef]
    value: Any
    is_mut: bool = False
    line: int = 0


@dataclass
class AssignStmt:
    target: Any
    value: Any
    op: str = "="
    line: int = 0


@dataclass
class IfStmt:
    cond: Any
    then_body: List = field(default_factory=list)
    else_body: List = field(default_factory=list)
    line: int = 0


@dataclass
class WhileStmt:
    cond: Any
    body: List = field(default_factory=list)
    line: int = 0


@dataclass
class ReturnStmt:
    value: Optional[Any] = None
    line: int = 0


@dataclass
class BreakStmt:
    line: int = 0


@dataclass
class ContinueStmt:
    line: int = 0


@dataclass
class MatchStmt:
    subject: Any
    arms: List = field(default_factory=list)
    line: int = 0


@dataclass
class MatchArm:
    pattern: Any
    body: List = field(default_factory=list)
    is_expr: bool = False


@dataclass
class MapExpr:
    index_var: str
    range_start: Any
    range_end: Any
    inclusive: bool = False
    cond: Optional[Any] = None
    body: List = field(default_factory=list)
    line: int = 0


@dataclass
class FilterExpr:
    index_var: str
    range_start: Any
    range_end: Any
    inclusive: bool = False
    cond: Optional[Any] = None
    into_array: Optional[str] = None
    into_count: Optional[str] = None
    line: int = 0


@dataclass
class FoldExpr:
    init_value: Any
    index_var: str
    range_start: Any
    range_end: Any
    inclusive: bool = False
    cond: Optional[Any] = None
    body: List = field(default_factory=list)
    line: int = 0


@dataclass
class BinaryOp:
    op: str
    left: Any
    right: Any
    line: int = 0


@dataclass
class UnaryOp:
    op: str
    operand: Any
    line: int = 0


@dataclass
class IntLit:
    value: int
    line: int = 0


@dataclass
class FloatLit:
    value: float
    line: int = 0


@dataclass
class CharLit:
    value: int
    line: int = 0


@dataclass
class StringLit:
    value: str
    line: int = 0


@dataclass
class BoolLit:
    value: bool
    line: int = 0


@dataclass
class IdentExpr:
    name: str
    line: int = 0


@dataclass
class CallExpr:
    func_name: str
    args: List = field(default_factory=list)
    is_c_call: bool = False
    is_c_var_ref: bool = False
    line: int = 0


@dataclass
class FieldAccess:
    obj: Any
    field: str
    line: int = 0


@dataclass
class ArrayAccess:
    array: Any
    index: Any
    line: int = 0


@dataclass
class AddrOf:
    operand: Any
    line: int = 0


@dataclass
class Deref:
    operand: Any
    line: int = 0


@dataclass
class RangeExpr:
    start: Any
    end: Any
    inclusive: bool = False
    line: int = 0


@dataclass
class ArrayLit:
    elements: List = field(default_factory=list)
    line: int = 0


@dataclass
class InitList:
    value: Any
    line: int = 0


@dataclass
class CCodeBlock:
    code: str
    line: int = 0


@dataclass
class AsmBlock:
    code: str
    inputs: list = field(default_factory=list)
    outputs: list = field(default_factory=list)
    clobbers: list = field(default_factory=list)
    line: int = 0


@dataclass
class IfExpr:
    cond: Any
    then_expr: Any
    else_expr: Any
    line: int = 0


@dataclass
class MatchExpr:
    subject: Any
    arms: List = field(default_factory=list)
    line: int = 0


@dataclass
class MatchExprArm:
    pattern: Any
    expr: Any
