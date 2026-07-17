from dataclasses import dataclass, field
from typing import Any, Optional


@dataclass
class IntegerTypeDef:
    name: str
    bits: int = 32
    signed: bool = True
    from_val: Any = None
    to_val: Any = None


@dataclass
class FloatTypeDef:
    name: str
    bits: int = 32
    encoding: str = "ieee754"
    precision: str = "single"


@dataclass
class BoolTypeDef:
    name: str
    bits: int = 8
    true_value: Any = 1
    false_value: Any = 0


@dataclass
class CharTypeDef:
    name: str
    bits: int = 8
    signed: bool = True
    encoding: str = "ascii"


@dataclass
class VoidTypeDef:
    name: str = "void"


@dataclass
class StructFieldDef:
    name: str
    type_name: str
    array_size: Optional[Any] = None
    is_pointer: bool = False


@dataclass
class StructTypeDef:
    name: str
    layout: str = "soa"
    fields: list = field(default_factory=list)
    max_count_field: Optional[str] = None
    capacity: Any = None
    align: Optional[int] = None
    packed: bool = False


@dataclass
class EnumVariantDef:
    name: str
    value: Any


@dataclass
class EnumTypeDef:
    name: str
    base: str = "int32"
    variants: list = field(default_factory=list)


@dataclass
class PointerTypeDef:
    name: str
    target: str = ""
    nullable: bool = True


@dataclass
class FuncArgDef:
    name: str
    type_str: str
    is_pointer: bool = False
    is_mut: bool = False


@dataclass
class FuncContract:
    name: str
    kind: str = "function"
    args: list = field(default_factory=list)
    returns: str = "void"
    pure: Optional[bool] = None
    mutates: list = field(default_factory=list)
    inline: str = "auto"
    unroll: str = "auto"
    complexity: Optional[int] = None
    allocates: Optional[bool] = None
    recurses: Optional[bool] = None
    threadsafe: Optional[bool] = None
    restrict: Optional[bool] = None


@dataclass
class ConstDef:
    name: str
    value: Any


@dataclass
class TransformDef:
    name: str
    match: str = ""
    match_op: Optional[str] = None
    condition: Optional[str] = None
    before: Optional[str] = None
    after: Optional[str] = None
    replace: Optional[str] = None


@dataclass
class CompileTimeLet:
    name: str
    value: Any


@dataclass
class CompileTimeIf:
    condition: str
    then_branch: list = field(default_factory=list)
    else_branch: list = field(default_factory=list)


@dataclass
class CompileTimeAssert:
    condition: str


@dataclass
class VisibilityDecl:
    kind: str
    names: list


@dataclass
class CFuncDef:
    name: str
    header: str = ""
    returns: str = "void"
    args: str = ""


@dataclass
class MetadataModule:
    mode: str = "speed"
    directives: dict = field(default_factory=dict)
    type_defs: dict = field(default_factory=dict)
    func_contracts: dict = field(default_factory=dict)
    consts: dict = field(default_factory=dict)
    compile_vars: dict = field(default_factory=dict)
    transforms: list = field(default_factory=list)
    disabled_transforms: list = field(default_factory=list)
    visibility: dict = field(default_factory=dict)
    c_includes: list = field(default_factory=list)
    c_flags: list = field(default_factory=list)
    c_names: dict = field(default_factory=dict)
    c_prefix: Optional[str] = None
    c_no_prefix: bool = False
    c_exports: list = field(default_factory=list)
    c_funcs: dict = field(default_factory=dict)
    emits: list = field(default_factory=list)
    imports: list = field(default_factory=list)
    has_main: bool = False

    def get_type_def(self, name):
        return self.type_defs.get(name)

    def get_func_contract(self, name):
        return self.func_contracts.get(name)

    def is_exported(self, name):
        return name in self.visibility.get("export", set())

    def is_internal(self, name):
        return name in self.visibility.get("internal", set())
