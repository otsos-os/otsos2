from parser.ast import *
from metadata.nodes import (
    MetadataModule, IntegerTypeDef, FloatTypeDef, BoolTypeDef, CharTypeDef,
    VoidTypeDef, StructTypeDef, EnumTypeDef, PointerTypeDef,
)


TYPE_SIZE_ORDER = {
    "void": 0,
    "char": 1, "bool": 1, "int8": 1, "uint8": 1,
    "int16": 2, "uint16": 2, "short": 2,
    "int": 4, "int32": 4, "uint": 4, "uint32": 4, "float": 4, "float32": 4,
    "int64": 8, "uint64": 8, "long": 8, "long long": 8, "double": 8, "float64": 8,
}


class VarDecl:
    def __init__(self, name, type_ref, is_mut=False, is_struct=False, init_value=None):
        self.name = name
        self.type_ref = type_ref
        self.is_mut = is_mut
        self.is_struct = is_struct
        self.init_value = init_value
        self.is_loop_var = False
        self.is_acc = False
        self.is_array = False
        self.array_size = None

    def size_rank(self):
        if self.type_ref is None:
            return 4
        if self.type_ref.is_pointer:
            return 100
        td_name = self.type_ref.name
        td = None
        if hasattr(self, '_meta'):
            td = self._meta.get_type_def(td_name)
        if td and hasattr(td, 'fields'):
            return 200
        return TYPE_SIZE_ORDER.get(td_name, 4)


class CodeGenerator:
    def __init__(self, program, metadata, freestanding=False):
        self.program = program
        self.meta = metadata
        self.freestanding = freestanding
        self.indent_level = 0
        self.lines = []
        self.current_func = None
        self.var_counter = 0
        self.label_counter = 0
        self.need_cleanup = False
        self.scopes = [{}]
        self.last_fold_result = None
        self.var_decls = []
        self.body_lines = []
        self.in_var_collect = False
        self.active_transforms = []
        self.disabled_transforms = set()
        self._init_transforms()

    def _init_transforms(self):
        self.disabled_transforms = set(self.meta.disabled_transforms)
        self.active_transforms = []
        for td in self.meta.transforms:
            if td.name not in self.disabled_transforms:
                self.active_transforms.append(td)

    def get_dsl_vars(self):
        return {
            "mode": self.meta.mode,
            "bounds": self.meta.directives.get("bounds", "none"),
            "overflow": self.meta.directives.get("overflow", "unchecked"),
            "null_check": self.meta.directives.get("null_check", "never"),
            "div_check": self.meta.directives.get("div_check", "never"),
            "range_check": self.meta.directives.get("range_check", "never"),
            "alloc": self.meta.directives.get("alloc", "dynamic"),
            "panic": self.meta.directives.get("panic", "abort"),
            "init": self.meta.directives.get("init", "none"),
        }

    def check_transform_condition(self, td):
        if td.condition is None:
            return True
        from metadata.evaluator import CompileTimeEvaluator
        dsl_vars = self.get_dsl_vars()
        evaluator = CompileTimeEvaluator({}, dsl_vars)
        try:
            result = evaluator.eval(td.condition)
            return bool(result)
        except Exception:
            return False

    def has_user_transform(self, construct_name, op_filter=None):
        for td in self.active_transforms:
            if td.match == construct_name:
                if op_filter and td.match_op and td.match_op != op_filter:
                    continue
                if self.check_transform_condition(td):
                    return True
        return False

    def apply_transform_before(self, construct_name, template_vars, op_filter=None):
        for td in self.active_transforms:
            if td.match == construct_name and td.before and self.check_transform_condition(td):
                if op_filter and td.match_op and td.match_op != op_filter:
                    continue
                code = self._fill_template(td.before, template_vars)
                for line in code.split("\n"):
                    self.lines.append(f"{self.indent()}{line}")

    def apply_transform_after(self, construct_name, template_vars, op_filter=None):
        for td in self.active_transforms:
            if td.match == construct_name and td.after and self.check_transform_condition(td):
                if op_filter and td.match_op and td.match_op != op_filter:
                    continue
                code = self._fill_template(td.after, template_vars)
                for line in code.split("\n"):
                    self.lines.append(f"{self.indent()}{line}")

    def apply_transform_replace(self, construct_name, template_vars, op_filter=None):
        for td in self.active_transforms:
            if td.match == construct_name and td.replace and self.check_transform_condition(td):
                if op_filter and td.match_op and td.match_op != op_filter:
                    continue
                return self._fill_template(td.replace, template_vars)
        return None

    def _fill_template(self, template, template_vars):
        result = template
        for key, val in template_vars.items():
            result = result.replace("{" + key + "}", str(val))
        return result

    def push_scope(self):
        self.scopes.append({})

    def pop_scope(self):
        self.scopes.pop()

    def declare_var(self, name, type_ref, is_mut=False):
        self.scopes[-1][name] = (type_ref, is_mut)

    def lookup_var(self, name):
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        return None

    def generate(self):
        self.emit_manifest_defines()
        self.emit_manifest_space()
        self.emit_includes()
        self.emit_compact_integer_types()
        self.emit_emits()
        self.emit_constants()
        self.emit_structs()
        self.emit_enums()
        self.emit_func_decls()
        self.emit_c_exports()
        self.emit_func_defs()
        return "\n".join(self.lines)

    def emit_manifest_defines(self):
        self.lines.append("/* !DEFINES!")
        self.lines.append("")
        self.lines.append(f"$mode {self.meta.mode}")
        self.lines.append("")

        for name, td in self.meta.type_defs.items():
            if isinstance(td, IntegerTypeDef):
                self.lines.append(f"$define %type {name} as {td.bits} bit {'signed' if td.signed else 'unsigned'}")
            elif isinstance(td, FloatTypeDef):
                self.lines.append(f"$define %type {name} as {td.bits} bit float")
            elif isinstance(td, BoolTypeDef):
                self.lines.append(f"$define %type {name} as {td.bits} bit boolean")
            elif isinstance(td, CharTypeDef):
                self.lines.append(f"$define %type {name} as {td.bits} bit {'signed' if td.signed else 'unsigned'}")
            elif isinstance(td, VoidTypeDef):
                self.lines.append(f"$define %type {name} as empty")
            elif isinstance(td, StructTypeDef):
                field_names = ", ".join(f.name for f in td.fields)
                self.lines.append(f"$define %type {name} as struct with fields {field_names}")
            elif isinstance(td, EnumTypeDef):
                self.lines.append(f"$define %type {name} as enum with variants {', '.join(v.name for v in td.variants)}")
            elif isinstance(td, PointerTypeDef):
                self.lines.append(f"$define %type {name} as pointer to {td.target}")

        self.lines.append("")

        for name, fc in self.meta.func_contracts.items():
            if fc.kind == "start":
                self.lines.append(f"$define %func {name} as start with args void")
            elif fc.kind == "procedure":
                arg_types = ", ".join(a.type_str + ("*" if a.is_pointer else "") for a in fc.args)
                self.lines.append(f"$define %func {name} as procedure with args {arg_types if arg_types else 'void'}")
            else:
                arg_types = ", ".join(a.type_str + ("*" if a.is_pointer else "") for a in fc.args)
                self.lines.append(f"$define %func {name} as function with args {arg_types if arg_types else 'void'}")

        self.lines.append("")
        self.lines.append("*/")
        self.lines.append("")

    def emit_manifest_space(self):
        self.lines.append("/* !SPACE!")
        self.lines.append("")

        exports = sorted(self.meta.visibility.get("export", set()))
        internals = sorted(self.meta.visibility.get("internal", set()))

        if exports:
            self.lines.append(f"$space %export {', '.join(exports)}")
        if internals:
            self.lines.append(f"$space %internal {', '.join(internals)}")

        self.lines.append("")
        self.lines.append("*/")
        self.lines.append("")

    def emit_includes(self):
        if self.freestanding:
            self.emit_freestanding_decls()
            return

        needs_stdint = self.meta.mode == "safety" or self.has_compact_integer_types()
        if needs_stdint:
            self.lines.append("#include <stdint.h>")
        if self.meta.mode == "safety":
            self.lines.append("#include <stdbool.h>")
            self.lines.append("#include <stddef.h>")
            self.lines.append("#include <string.h>")
        else:
            self.lines.append("#include <stdbool.h>")
            self.lines.append("#include <string.h>")

        for inc in sorted(set(self.meta.c_includes)):
            if "." in inc and not inc.endswith(".h"):
                self.lines.append(f'#include "{inc}"')
            else:
                self.lines.append(f"#include <{inc}>")

        self.lines.append("")

    def emit_freestanding_decls(self):
        self.lines.append("/* freestanding mode: no standard includes */")
        self.lines.append("")

        if self.meta.mode == "safety" or self.has_compact_integer_types():
            self.lines.append("/* fixed-width integer types */")
            self.lines.append("typedef __INT8_TYPE__ int8_t;")
            self.lines.append("typedef __UINT8_TYPE__ uint8_t;")
            self.lines.append("typedef __INT16_TYPE__ int16_t;")
            self.lines.append("typedef __UINT16_TYPE__ uint16_t;")
            self.lines.append("typedef __INT32_TYPE__ int32_t;")
            self.lines.append("typedef __UINT32_TYPE__ uint32_t;")
            self.lines.append("typedef __INT64_TYPE__ int64_t;")
            self.lines.append("typedef __UINT64_TYPE__ uint64_t;")
            self.lines.append("")

        self.lines.append("/* boolean type */")
        self.lines.append("typedef _Bool bool;")
        self.lines.append("#define true (_Bool)1")
        self.lines.append("#define false (_Bool)0")
        self.lines.append("")

        self.lines.append("/* null pointer */")
        self.lines.append("#define NULL ((void*)0)")
        self.lines.append("")

        self.lines.append("/* runtime functions — user must provide implementations */")
        self.lines.append("extern void *memset(void *, int, unsigned long);")

        panic = self.meta.directives.get("panic", "abort")
        if panic == "abort":
            self.lines.append("extern void abort(void);")

        self.lines.append("")

        for inc in sorted(set(self.meta.c_includes)):
            if "." in inc and not inc.endswith(".h"):
                self.lines.append(f'#include "{inc}"')
            else:
                self.lines.append(f"#include <{inc}>")

        self.lines.append("")

    def emit_emits(self):
        for emit in self.meta.emits:
            self.lines.append(emit)
        if self.meta.emits:
            self.lines.append("")

    def emit_compact_integer_types(self):
        emitted = False
        for name, td in self.meta.type_defs.items():
            if not self.is_compact_integer(td):
                continue

            byte_count = (td.bits + 7) // 8
            self.lines.append(f"typedef struct {{")
            self.lines.append(f"\tuint8_t bytes[{byte_count}];")
            self.lines.append(f"}} {name};")
            self.lines.append("")

            self.lines.append(f"static inline {name}")
            self.lines.append(f"{name}_from_i64(int64_t value)")
            self.lines.append("{")
            self.lines.append(f"\t{name} out;")
            self.lines.append("\tuint64_t raw;")
            self.lines.append("")
            self.lines.append("\traw = (uint64_t)value;")
            for i in range(byte_count):
                self.lines.append(f"\tout.bytes[{i}] = (uint8_t)(raw >> {i * 8});")
            self.lines.append("\treturn (out);")
            self.lines.append("}")
            self.lines.append("")

            self.lines.append("static inline int64_t")
            self.lines.append(f"{name}_to_i64({name} value)")
            self.lines.append("{")
            self.lines.append("\tuint64_t raw;")
            self.lines.append("\tuint64_t mask;")
            self.lines.append("")
            self.lines.append("\traw = 0;")
            for i in range(byte_count):
                self.lines.append(f"\traw |= ((uint64_t)value.bytes[{i}]) << {i * 8};")
            self.lines.append(f"\tmask = (1ULL << {td.bits}) - 1ULL;")
            self.lines.append("\traw &= mask;")
            if td.signed:
                self.lines.append(f"\tif (raw & (1ULL << {td.bits - 1})) {{")
                self.lines.append("\t\traw |= ~mask;")
                self.lines.append("\t}")
            self.lines.append("\treturn ((int64_t)raw);")
            self.lines.append("}")
            self.lines.append("")
            emitted = True

        if emitted:
            self.lines.append("")

    def emit_constants(self):
        for const in self.program.consts:
            cval = self.meta.consts.get(const.name)
            if cval is not None:
                if isinstance(cval, float):
                    if const.type_ref and const.type_ref.name in ("float", "float32"):
                        self.lines.append(f"#define {const.name} {cval}f")
                    else:
                        self.lines.append(f"#define {const.name} {cval}")
                elif isinstance(cval, str):
                    self.lines.append(f'#define {const.name} "{cval}"')
                else:
                    self.lines.append(f"#define {const.name} {cval}")
        if self.program.consts:
            self.lines.append("")

    def c_type(self, type_ref):
        if type_ref is None:
            return "void"
        if type_ref.name == "Array" and len(type_ref.generic_args) >= 1:
            elem = self.array_element_type(type_ref)
            return self.c_type(elem)

        td = self.meta.get_type_def(type_ref.name)
        if isinstance(td, IntegerTypeDef):
            if self.is_compact_integer(td):
                if type_ref.is_pointer:
                    return td.name + " *"
                return td.name
            base = self.integer_carrier_c_type(td)
            if type_ref.is_pointer:
                return base + " *"
            return base

        return type_ref.c_type(self.meta.mode)

    def is_compact_integer(self, td):
        if not isinstance(td, IntegerTypeDef):
            return False
        byte_count = (td.bits + 7) // 8
        return byte_count not in (1, 2, 4, 8)

    def is_compact_type_ref(self, type_ref):
        if type_ref is None:
            return False
        td = self.meta.get_type_def(type_ref.name)
        return self.is_compact_integer(td)

    def is_integer_type_ref(self, type_ref):
        if type_ref is None or type_ref.is_pointer:
            return False
        if isinstance(self.meta.get_type_def(type_ref.name), IntegerTypeDef):
            return True
        return type_ref.name in {
            "int", "int32", "int64", "uint", "uint32", "uint64",
            "uint8", "int8", "uint16", "int16", "char", "char8",
        }

    def compact_from_expr(self, type_ref, expr):
        if not self.is_compact_type_ref(type_ref):
            return self.gen_expr(expr)
        name = type_ref.name
        if isinstance(expr, IdentExpr):
            info = self.lookup_var(expr.name)
            if info and self.is_compact_type_ref(info[0]) and info[0].name == name:
                return expr.name
        if isinstance(expr, CallExpr):
            contract = self.meta.get_func_contract(expr.func_name)
            if contract and contract.returns.replace("*", "") == name and "*" not in contract.returns:
                return self.gen_expr(expr)
        return f"{name}_from_i64({self.gen_scalar_expr(expr)})"

    def gen_scalar_expr(self, expr):
        if isinstance(expr, IdentExpr):
            info = self.lookup_var(expr.name)
            if info and self.is_compact_type_ref(info[0]):
                return f"{info[0].name}_to_i64({expr.name})"
            return expr.name
        if isinstance(expr, CallExpr):
            result = self.gen_expr(expr)
            ret = self.infer_type_ref(expr)
            if self.is_compact_type_ref(ret):
                return f"{ret.name}_to_i64({result})"
            return result
        if isinstance(expr, FieldAccess):
            result = self.gen_expr(expr)
            ret = self.infer_type_ref(expr)
            if self.is_compact_type_ref(ret):
                return f"{ret.name}_to_i64({result})"
            return result
        if isinstance(expr, ArrayAccess):
            result = self.gen_expr(expr)
            ret = self.infer_type_ref(expr)
            if self.is_compact_type_ref(ret):
                return f"{ret.name}_to_i64({result})"
            return result
        if isinstance(expr, BinaryOp):
            left = self.gen_scalar_expr(expr.left)
            right = self.gen_scalar_expr(expr.right)
            if expr.op == "//":
                int_type = "int32_t" if self.meta.mode == "safety" else "int"
                return f"({int_type})(({left}) / ({right}))"
            return f"({left}) {expr.op} ({right})"
        if isinstance(expr, UnaryOp):
            operand = self.gen_scalar_expr(expr.operand)
            if expr.op == "post++":
                return f"({operand})++"
            if expr.op == "post--":
                return f"({operand})--"
            return f"{expr.op}({operand})"
        if isinstance(expr, IfExpr):
            cond = self.gen_expr(expr.cond)
            then_val = self.gen_scalar_expr(expr.then_expr)
            else_val = self.gen_scalar_expr(expr.else_expr) if expr.else_expr else "0"
            return f"(({cond}) ? ({then_val}) : ({else_val}))"
        if isinstance(expr, MatchExpr):
            return self.gen_match_expr(expr, scalar=True)
        return self.gen_expr(expr)

    def has_compact_integer_types(self):
        for td in self.meta.type_defs.values():
            if self.is_compact_integer(td):
                return True
        return False

    def integer_carrier_c_type(self, td):
        bits = td.bits
        if bits <= 0:
            bits = 32

        if bits <= 8:
            width = 8
        elif bits <= 16:
            width = 16
        elif bits <= 32:
            width = 32
        else:
            width = 64

        if self.meta.mode == "safety":
            prefix = "int" if td.signed else "uint"
            return f"{prefix}{width}_t"

        if width == 8:
            return "char" if td.signed else "unsigned char"
        if width == 16:
            return "short" if td.signed else "unsigned short"
        if width == 32:
            return "int" if td.signed else "unsigned int"
        return "long long" if td.signed else "unsigned long long"

    def array_element_type(self, type_ref):
        if type_ref.name == "Array" and type_ref.generic_args:
            elem = type_ref.generic_args[0]
            if isinstance(elem, TypeRef):
                return elem
            if isinstance(elem, IdentExpr):
                return TypeRef(name=elem.name)
        return type_ref

    def array_capacity_arg(self, type_ref):
        if type_ref.name != "Array" or len(type_ref.generic_args) < 2:
            return None
        size = type_ref.generic_args[1]
        if isinstance(size, TypeRef):
            return IdentExpr(name=size.name)
        return size

    def c_type_no_space(self, type_ref):
        if type_ref is None:
            return "void"
        base = self.c_type(type_ref)
        if type_ref.is_pointer:
            parts = base.rsplit(" ", 1)
            if len(parts) == 2 and parts[1] == "*":
                return parts[0] + " *"
            return base
        return base

    def func_c_name(self, name):
        if name == "main":
            return "main"
        if name in self.meta.c_names:
            return self.meta.c_names[name]
        if self.meta.c_no_prefix:
            return name
        if self.meta.c_prefix:
            return self.meta.c_prefix + name
        return name

    def is_static(self, name):
        if name in self.meta.visibility.get("internal", set()):
            return True
        if name in self.meta.visibility.get("export", set()):
            return False
        return False

    def emit_structs(self):
        for struct in self.program.structs:
            self.emit_struct(struct)
        if self.program.structs:
            self.lines.append("")

    def emit_struct(self, struct):
        self.lines.append("typedef struct {")
        for field in struct.fields:
            field_type_ref = self.array_element_type(field.type_ref)
            field_type = self.c_type(field_type_ref)
            if field.type_ref.is_pointer:
                field_type = field_type.replace(" *", " *")
            array_str = ""
            field_array_size = field.array_size or self.array_capacity_arg(field.type_ref)
            if field_array_size is not None:
                arr_val = self.eval_array_size(field_array_size)
                array_str = f"[{arr_val}]"
            self.lines.append(f"\t{field_type} {field.name}{array_str};")
        self.lines.append(f"}} {struct.name};")

    def eval_array_size(self, expr):
        if isinstance(expr, IntLit):
            return str(expr.value)
        if isinstance(expr, IdentExpr):
            if expr.name in self.meta.consts:
                return str(self.meta.consts[expr.name])
            if expr.name in self.meta.compile_vars:
                return str(self.meta.compile_vars[expr.name])
            return expr.name
        if isinstance(expr, TypeRef):
            return expr.name
        if isinstance(expr, BinaryOp):
            left = self.eval_array_size(expr.left)
            right = self.eval_array_size(expr.right)
            return f"({left} {expr.op} {right})"
        return "0"

    def emit_enums(self):
        for enum in self.program.enums:
            td = self.meta.get_type_def(enum.name)
            base_type = "int"
            if td and hasattr(td, "base"):
                base_type = td.base
            base_type = self.c_type(TypeRef(name=base_type))

            self.lines.append("typedef enum {")
            for i, variant in enumerate(enum.variants):
                val = i
                if td and i < len(td.variants):
                    val = td.variants[i].value
                self.lines.append(f"\t{enum.name}_{variant.name} = {val},")
            self.lines.append(f"}} {enum.name};")
        if self.program.enums:
            self.lines.append("")

    def emit_func_decls(self):
        for func in self.program.functions:
            self.emit_func_decl(func)
        if self.program.functions:
            self.lines.append("")

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

    def emit_func_defs(self):
        for func in self.program.functions:
            self.emit_func_def(func)

    def emit_func_def(self, func):
        self.current_func = func
        self.var_counter = 0
        self.need_cleanup = False
        self.var_decls = []
        self.last_fold_result = None

        self.emit_unroll_pragmas(func)

        ret_type = self.c_type(func.return_type) if func.return_type else "void"
        params = self.format_params(func)

        if func.name == "main":
            self.lines.append(f"{ret_type}")
            self.lines.append(f"{self.func_c_name(func.name)}(void)")
        else:
            prefix = "static " if self.is_static(func.name) else ""
            inline = ""
            contract = self.meta.get_func_contract(func.name)
            if contract and contract.inline == "always":
                if self.is_static(func.name):
                    inline = "inline "
                else:
                    inline = "static inline "
            if inline:
                self.lines.append(f"{prefix}{inline}{ret_type}")
            else:
                self.lines.append(f"{prefix}{ret_type}")
            self.lines.append(f"{self.func_c_name(func.name)}({params})")

        self.lines.append("{")

        self.scopes = [{}]
        for param in func.params:
            self.declare_var(param.name, param.type_ref, is_mut=param.is_mut)

        self.collect_var_decls(func)

        self.indent_level = 1
        self.emit_func_body(func)

        self.indent_level = 0
        self.lines.append("}")
        self.lines.append("")

        self.current_func = None

    def collect_var_decls(self, func):
        self.in_var_collect = True
        self._collect_from_body(func.body)
        self.in_var_collect = False
        self.var_decls.sort(key=lambda d: -d.size_rank())

    def _collect_from_body(self, body):
        for stmt in body:
            self._collect_from_stmt(stmt)

    def _collect_from_stmt(self, stmt):
        if isinstance(stmt, LetStmt):
            vtype = stmt.type_ref
            if vtype is None:
                vtype = TypeRef(name="int")
            is_struct = False
            td = self.meta.get_type_def(vtype.name)
            if td and hasattr(td, 'fields'):
                is_struct = True
            vd = VarDecl(stmt.name, vtype, stmt.is_mut, is_struct, stmt.value)
            if vtype.is_array:
                vd.is_array = True
                vd.array_size = vtype.array_size
            elif vtype.name == "Array":
                array_size = self.array_capacity_arg(vtype)
                if array_size is not None:
                    vd.is_array = True
                    vd.array_size = array_size
            self.var_decls.append(vd)
        elif isinstance(stmt, IfStmt):
            self._collect_from_body(stmt.then_body)
            self._collect_from_body(stmt.else_body)
        elif isinstance(stmt, WhileStmt):
            self._collect_from_body(stmt.body)
        elif isinstance(stmt, MapExpr):
            vd = VarDecl(stmt.index_var, TypeRef(name="int"), False, False)
            vd.is_loop_var = True
            self.var_decls.append(vd)
            self._collect_from_body(stmt.body)
        elif isinstance(stmt, FoldExpr):
            vd = VarDecl(stmt.index_var, TypeRef(name="int"), False, False)
            vd.is_loop_var = True
            self.var_decls.append(vd)
            acc_type = self.infer_type_ref(stmt.init_value)
            acc_vd = VarDecl("acc", acc_type, False, False)
            acc_vd.is_acc = True
            self.var_decls.append(acc_vd)
            acc_var = self.fresh_var("_acc")
            acc_result_vd = VarDecl(acc_var, acc_type, True, False)
            acc_result_vd.is_acc = True
            self.var_decls.append(acc_result_vd)
            self._collect_from_body(stmt.body)

    def emit_var_decls(self):
        groups = {}
        array_decls = []
        for vd in self.var_decls:
            if vd.is_loop_var or vd.is_acc:
                continue
            if vd.is_array:
                array_decls.append(vd)
                continue
            vtype = self.c_type(vd.type_ref)
            if vtype not in groups:
                groups[vtype] = []
            groups[vtype].append(vd.name)

        for vtype, names in groups.items():
            line = f"\t{vtype}"
            pad = " " * max(1, 16 - len(vtype))
            if len(names) == 1:
                self.lines.append(f"\t{vtype}{pad}{names[0]};")
            else:
                self.lines.append(f"\t{vtype}{pad}{', '.join(names)};")

        for vd in array_decls:
            vtype = self.c_type(vd.type_ref)
            arr_val = self.eval_array_size(vd.array_size) if vd.array_size else ""
            self.lines.append(f"\t{vtype} {vd.name}[{arr_val}];")

        for vd in self.var_decls:
            if vd.is_loop_var:
                vtype = self.c_type(vd.type_ref)
                self.lines.append(f"\t{vtype} {vd.name};")
            elif vd.is_acc and vd.name == "acc":
                vtype = self.c_type(vd.type_ref)
                self.lines.append(f"\t{vtype} {vd.name};")
            elif vd.is_acc and vd.name.startswith("_acc"):
                vtype = self.c_type(vd.type_ref)
                self.lines.append(f"\t{vtype} {vd.name};")

        if self.var_decls:
            self.lines.append("")

    def emit_void_suppressions(self):
        suppressions = []
        for vd in self.var_decls:
            if vd.is_loop_var or vd.is_acc:
                continue
            suppressions.append(vd.name)
        if suppressions:
            self.lines.append(f"{self.indent()}/* suppress unused warnings */")
            for name in suppressions:
                self.lines.append(f"{self.indent()}(void){name};")

    def emit_func_body(self, func):
        raise NotImplementedError("subclass must implement emit_func_body")

    def indent(self):
        return "\t" * self.indent_level

    def fresh_var(self, prefix="_tmp"):
        self.var_counter += 1
        return f"{prefix}{self.var_counter}"

    def fresh_label(self, prefix="L"):
        self.label_counter += 1
        return f"{prefix}{self.label_counter}"

    def gen_expr(self, expr):
        if isinstance(expr, IntLit):
            return str(expr.value)
        elif isinstance(expr, FloatLit):
            if expr.value == int(expr.value) and abs(expr.value) < 1e15:
                return f"{int(expr.value)}.0f"
            return f"{expr.value}f"
        elif isinstance(expr, CharLit):
            return f"'\\x{expr.value:02x}'"
        elif isinstance(expr, StringLit):
            escaped = expr.value.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r").replace("\0", "\\0")
            return f'"{escaped}"'
        elif isinstance(expr, BoolLit):
            return "true" if expr.value else "false"
        elif isinstance(expr, IdentExpr):
            return expr.name
        elif isinstance(expr, BinaryOp):
            left = self.gen_scalar_expr(expr.left)
            right = self.gen_scalar_expr(expr.right)
            if expr.op == "//":
                int_type = "int32_t" if self.meta.mode == "safety" else "int"
                original = f"({int_type})(({left}) / ({right}))"
            else:
                original = f"({left}) {expr.op} ({right})"

            type_max, type_min = None, None
            inferred_type = self.infer_type_ref(expr)
            range_info = self.get_type_range(inferred_type.name if inferred_type else "int32")
            if range_info[0] is not None:
                type_min, type_max = range_info

            template_vars = {
                "left": left,
                "right": right,
                "op": expr.op,
                "original": original,
                "type_max": type_max if type_max else 2147483647,
                "type_min": type_min if type_min is not None else -2147483648,
            }

            replaced = self.apply_transform_replace("binary_op", template_vars, op_filter=expr.op)
            if replaced:
                return replaced

            self.apply_transform_before("binary_op", template_vars, op_filter=expr.op)
            result = original
            self.apply_transform_after("binary_op", template_vars, op_filter=expr.op)
            return result
        elif isinstance(expr, UnaryOp):
            operand = self.gen_expr(expr.operand)
            if expr.op == "post++":
                return f"({operand})++"
            elif expr.op == "post--":
                return f"({operand})--"
            return f"{expr.op}({operand})"
        elif isinstance(expr, CallExpr):
            if expr.is_c_var_ref:
                return expr.func_name
            args = ", ".join(self.gen_call_arg(expr, idx, a)
                             for idx, a in enumerate(expr.args))
            if expr.is_c_call:
                return f"{expr.func_name}({args})"
            return f"{self.func_c_name(expr.func_name)}({args})"
        elif isinstance(expr, FieldAccess):
            obj = self.gen_field_obj(expr.obj)
            if self.is_pointer_expr(expr.obj):
                original = f"{obj}->{expr.field}"
            else:
                original = f"{obj}.{expr.field}"

            template_vars = {
                "target": obj,
                "field": expr.field,
                "original": original,
            }

            replaced = self.apply_transform_replace("field_access", template_vars)
            if replaced:
                return replaced

            self.apply_transform_before("field_access", template_vars)
            return original
        elif isinstance(expr, ArrayAccess):
            arr = self.gen_expr(expr.array)
            idx = self.gen_expr(expr.index)
            original = f"{arr}[{idx}]"

            cap = None
            if isinstance(expr.array, FieldAccess) and isinstance(expr.array.obj, IdentExpr):
                for p in self.current_func.params if self.current_func else []:
                    if p.name == expr.array.obj.name:
                        td = self.meta.get_type_def(p.type_ref.name)
                        if td and hasattr(td, 'capacity'):
                            cap = td.capacity
                        break

            template_vars = {
                "target": arr,
                "index": idx,
                "capacity": cap if cap else 0,
                "original": original,
            }

            replaced = self.apply_transform_replace("array_access", template_vars)
            if replaced:
                return replaced

            self.apply_transform_before("array_access", template_vars)
            result = original
            self.apply_transform_after("array_access", template_vars)
            return result
        elif isinstance(expr, AddrOf):
            return f"&({self.gen_expr(expr.operand)})"
        elif isinstance(expr, Deref):
            operand = self.gen_expr(expr.operand)
            original = f"*({operand})"

            template_vars = {
                "pointer": operand,
                "original": original,
            }

            replaced = self.apply_transform_replace("pointer_deref", template_vars)
            if replaced:
                return replaced

            self.apply_transform_before("pointer_deref", template_vars)
            return original
        elif isinstance(expr, IfExpr):
            cond = self.gen_expr(expr.cond)
            then_val = self.gen_expr(expr.then_expr)
            else_val = self.gen_expr(expr.else_expr) if expr.else_expr else "0"
            return f"(({cond}) ? ({then_val}) : ({else_val}))"
        elif isinstance(expr, InitList):
            return "{0}"
        elif isinstance(expr, ArrayLit):
            return "{" + ", ".join(self.gen_expr(e) for e in expr.elements) + "}"
        elif isinstance(expr, CCodeBlock):
            return expr.code
        elif isinstance(expr, MatchExpr):
            return self.gen_match_expr(expr)
        return "0"

    def gen_call_arg(self, call, idx, arg):
        if call.is_c_call:
            return self.gen_expr(arg)
        contract = self.meta.get_func_contract(call.func_name)
        if not contract or idx >= len(contract.args):
            return self.gen_expr(arg)
        arg_def = contract.args[idx]
        expected = TypeRef(name=arg_def.type_str, is_pointer=arg_def.is_pointer)
        return self.gen_expr_as_type(arg, expected)

    def gen_expr_as_type(self, expr, type_ref):
        if self.is_compact_type_ref(type_ref):
            return self.compact_from_expr(type_ref, expr)
        expr_type = self.infer_type_ref(expr)
        if self.is_compact_type_ref(expr_type):
            return self.gen_scalar_expr(expr)
        return self.gen_expr(expr)

    def zero_value_for_type(self, type_ref):
        if self.is_compact_type_ref(type_ref):
            return f"{type_ref.name}_from_i64(0)"
        return "0"

    def gen_field_obj(self, expr):
        if isinstance(expr, IdentExpr):
            return expr.name
        return self.gen_expr(expr)

    def is_pointer_expr(self, expr):
        if isinstance(expr, IdentExpr):
            info = self.lookup_var(expr.name)
            if info and info[0] and info[0].is_pointer:
                return True
        return False

    def gen_match_expr(self, expr, scalar=False):
        parts = []
        wildcard_idx = None
        for i, arm in enumerate(expr.arms):
            if isinstance(arm.pattern, IdentExpr) and arm.pattern.name == "_":
                wildcard_idx = i
                continue
            cond = self.gen_match_arm_cond(expr.subject, arm.pattern)
            val = self.gen_scalar_expr(arm.expr) if scalar else self.gen_expr(arm.expr)
            if not parts:
                parts.append(f"(({cond}) ? ({val})")
            else:
                parts.append(f" : ({cond}) ? ({val})")
        if wildcard_idx is not None:
            val = self.gen_scalar_expr(expr.arms[wildcard_idx].expr) if scalar else self.gen_expr(expr.arms[wildcard_idx].expr)
            parts.append(f" : ({val}))")
        else:
            parts.append(" : (0))")
        return " ".join(parts)

    def gen_match_arm_cond(self, subject, pattern):
        subj = self.gen_expr(subject)
        if isinstance(pattern, IdentExpr):
            if pattern.name == "_":
                return "1"
            if pattern.name.startswith("__enum_variant__"):
                variant = pattern.name.replace("__enum_variant__", "")
                return f"{subj} == {variant}"
            return f"{subj} == {self.gen_expr(pattern)}"
        if isinstance(pattern, CharLit):
            return f"{subj} == {self.gen_expr(pattern)}"
        if isinstance(pattern, IntLit):
            return f"{subj} == {self.gen_expr(pattern)}"
        return "1"

    def gen_match_stmt(self, stmt):
        first = True
        has_wildcard = False
        wildcard_body = None
        for arm in stmt.arms:
            if isinstance(arm.pattern, IdentExpr) and arm.pattern.name == "_":
                has_wildcard = True
                wildcard_body = arm.body
                continue
            cond = self.gen_match_arm_cond(stmt.subject, arm.pattern)
            if first:
                self.lines.append(f"{self.indent()}if ({cond}) {{")
                first = False
            else:
                self.lines.append(f"{self.indent()}}} else if ({cond}) {{")
            self.indent_level += 1
            for s in arm.body:
                self.gen_stmt(s)
            self.indent_level -= 1
        if has_wildcard:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            for s in wildcard_body:
                self.gen_stmt(s)
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")
        else:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            self.lines.append(f"{self.indent()}/* no match */")
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

    def gen_stmt_common(self, stmt):
        if isinstance(stmt, LetStmt):
            self.gen_let(stmt)
        elif isinstance(stmt, AssignStmt):
            self.gen_assign(stmt)
        elif isinstance(stmt, IfStmt):
            self.gen_if(stmt)
        elif isinstance(stmt, WhileStmt):
            self.gen_while(stmt)
        elif isinstance(stmt, ReturnStmt):
            self.gen_return(stmt)
        elif isinstance(stmt, BreakStmt):
            self.lines.append(f"{self.indent()}break;")
        elif isinstance(stmt, ContinueStmt):
            self.lines.append(f"{self.indent()}continue;")
        elif isinstance(stmt, MatchStmt):
            self.gen_match_stmt(stmt)
        elif isinstance(stmt, MapExpr):
            self.gen_map(stmt)
        elif isinstance(stmt, FilterExpr):
            self.gen_filter(stmt)
        elif isinstance(stmt, FoldExpr):
            self.gen_fold(stmt)
        elif isinstance(stmt, CCodeBlock):
            self.lines.append(stmt.code)
        elif isinstance(stmt, AsmBlock):
            self.gen_asm(stmt)
        elif isinstance(stmt, CallExpr):
            self.lines.append(f"{self.indent()}{self.gen_expr(stmt)};")
        else:
            self.lines.append(f"{self.indent()}/* unhandled statement: {type(stmt).__name__} */")

    def gen_let(self, stmt):
        self.declare_var(stmt.name, stmt.type_ref or TypeRef(name="int"), is_mut=stmt.is_mut)
        if stmt.value is None:
            return
        is_struct = False
        is_array = False
        if stmt.type_ref:
            td = self.meta.get_type_def(stmt.type_ref.name)
            if td and hasattr(td, 'fields'):
                is_struct = True
            is_array = stmt.type_ref.is_array or stmt.type_ref.name == "Array"
        if isinstance(stmt.value, InitList):
            if is_struct or is_array:
                self.lines.append(f"{self.indent()}memset(&{stmt.name}, 0, sizeof({stmt.name}));")
            return
        is_zero_init = (
            (isinstance(stmt.value, IntLit) and stmt.value.value == 0) or
            (isinstance(stmt.value, FloatLit) and stmt.value.value == 0.0)
        )
        if (is_struct or is_array) and is_zero_init:
            self.lines.append(f"{self.indent()}memset(&{stmt.name}, 0, sizeof({stmt.name}));")
            return
        target_type = stmt.type_ref
        val = self.gen_expr_as_type(stmt.value, target_type)
        if stmt.type_ref and stmt.type_ref.name == "rawptr":
            if isinstance(stmt.value, IntLit) or \
               (isinstance(stmt.value, IdentExpr) and stmt.value.name in self.meta.consts):
                val = f"((volatile unsigned char *){val})"
        if is_struct:
            self.lines.append(f"{self.indent()}{stmt.name} = {{{val}}};")
        else:
            self.lines.append(f"{self.indent()}{stmt.name} = {val};")

    def infer_type(self, expr):
        if isinstance(expr, IntLit):
            return self.c_type(TypeRef(name="int"))
        if isinstance(expr, FloatLit):
            return self.c_type(TypeRef(name="float"))
        if isinstance(expr, CharLit):
            return self.c_type(TypeRef(name="char"))
        if isinstance(expr, StringLit):
            return "char *"
        if isinstance(expr, BoolLit):
            return self.c_type(TypeRef(name="bool"))
        if isinstance(expr, CallExpr):
            contract = self.meta.get_func_contract(expr.func_name)
            if contract:
                ret = contract.returns
                if "*" in ret:
                    return self.c_type(TypeRef(name=ret.replace("*", ""), is_pointer=True))
                return self.c_type(TypeRef(name=ret))
        if isinstance(expr, IdentExpr):
            info = self.lookup_var(expr.name)
            if info and info[0]:
                return self.c_type(info[0])
        if isinstance(expr, BinaryOp):
            lt = self.infer_type(expr.left)
            return lt
        if isinstance(expr, FieldAccess):
            field_type = self.infer_type_ref(expr)
            if field_type:
                return self.c_type(field_type)
            return self.c_type(TypeRef(name="int"))
        if isinstance(expr, ArrayAccess):
            item_type = self.infer_type_ref(expr)
            if item_type:
                return self.c_type(item_type)
            return self.c_type(TypeRef(name="int"))
        return self.c_type(TypeRef(name="int"))

    def infer_type_ref(self, expr):
        if isinstance(expr, IntLit):
            return TypeRef(name="int")
        if isinstance(expr, FloatLit):
            return TypeRef(name="float")
        if isinstance(expr, BoolLit):
            return TypeRef(name="bool")
        if isinstance(expr, CharLit):
            return TypeRef(name="char")
        if isinstance(expr, CallExpr):
            contract = self.meta.get_func_contract(expr.func_name)
            if contract:
                ret = contract.returns
                return TypeRef(name=ret.replace("*", ""), is_pointer="*" in ret)
        if isinstance(expr, IdentExpr):
            info = self.lookup_var(expr.name)
            if info and info[0]:
                return info[0]
        if isinstance(expr, BinaryOp):
            lt = self.infer_type_ref(expr.left)
            rt = self.infer_type_ref(expr.right)
            if lt and rt:
                if lt.name in ("float", "float32", "double", "float64") or \
                   rt.name in ("float", "float32", "double", "float64"):
                    if lt.name in ("float", "float32") or rt.name in ("float", "float32"):
                        return TypeRef(name="float")
                    return TypeRef(name="double")
            return lt or rt or TypeRef(name="int")
        if isinstance(expr, FieldAccess):
            obj_type = self.infer_type_ref(expr.obj)
            field_type = self.get_struct_field_type(obj_type, expr.field)
            if field_type:
                return field_type
            return TypeRef(name="int")
        if isinstance(expr, IfExpr):
            then_type = self.infer_type_ref(expr.then_expr)
            else_type = self.infer_type_ref(expr.else_expr) if expr.else_expr else None
            return then_type or else_type or TypeRef(name="int")
        if isinstance(expr, MatchExpr):
            result_type = None
            for arm in expr.arms:
                arm_type = self.infer_type_ref(arm.expr)
                if arm_type:
                    result_type = arm_type
            return result_type or TypeRef(name="int")
        if isinstance(expr, ArrayAccess):
            array_type = self.infer_type_ref(expr.array)
            if array_type:
                if array_type.name == "Array":
                    return self.array_element_type(array_type)
                if array_type.is_array:
                    return TypeRef(name=array_type.name, is_pointer=array_type.is_pointer)
            return TypeRef(name="int")
        return TypeRef(name="int")

    def get_struct_field_type(self, obj_type, field_name):
        if obj_type is None:
            return None
        struct_name = obj_type.name

        for struct in self.program.structs:
            if struct.name != struct_name:
                continue
            for field in struct.fields:
                if field.name == field_name:
                    return field.type_ref

        td = self.meta.get_type_def(struct_name)
        if td and hasattr(td, 'fields'):
            for f in td.fields:
                if f.name == field_name:
                    return TypeRef(name=f.type_name, is_pointer=f.is_pointer)
        return None

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
            target_type = self.infer_type_ref(stmt.target)
            if self.is_compact_type_ref(target_type):
                op = "+" if stmt.op == "++" else "-"
                scalar = self.gen_scalar_expr(stmt.target)
                self.lines.append(f"{self.indent()}{target} = {target_type.name}_from_i64(({scalar}) {op} 1);")
            else:
                self.lines.append(f"{self.indent()}{target}{stmt.op};")
            return
        if stmt.value is None:
            return
        target_type = None
        if isinstance(stmt.target, IdentExpr):
            t_info = self.lookup_var(stmt.target.name)
            if t_info:
                target_type = t_info[0]
        elif isinstance(stmt.target, FieldAccess):
            target_type = self.infer_type_ref(stmt.target)
        elif isinstance(stmt.target, ArrayAccess):
            target_type = self.infer_type_ref(stmt.target)
        if self.is_compact_type_ref(target_type) and stmt.op in ("+=", "-=", "*=", "/=", "%="):
            scalar_target = self.gen_scalar_expr(stmt.target)
            scalar_val = self.gen_scalar_expr(stmt.value)
            base_op = stmt.op[0]
            val = f"{target_type.name}_from_i64(({scalar_target}) {base_op} ({scalar_val}))"
            op = "="
        else:
            val = self.gen_expr_as_type(stmt.value, target_type)
            op = stmt.op
        if isinstance(stmt.target, IdentExpr):
            t_info = self.lookup_var(stmt.target.name)
            if t_info and t_info[0] and t_info[0].name == "rawptr":
                if isinstance(stmt.value, IntLit) or \
                   (isinstance(stmt.value, IdentExpr) and stmt.value.name in self.meta.consts):
                    val = f"((volatile unsigned char *){val})"
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
            return

        self.apply_transform_before("assignment", template_vars)
        self.lines.append(f"{self.indent()}{original}")
        self.apply_transform_after("assignment", template_vars)

    def gen_if(self, stmt):
        cond = self.gen_expr(stmt.cond)
        self.lines.append(f"{self.indent()}if ({cond}) {{")
        self.indent_level += 1
        for s in stmt.then_body:
            self.gen_stmt(s)
        self.indent_level -= 1

        if stmt.else_body:
            self._gen_else_chain(stmt.else_body)
        else:
            self.lines.append(f"{self.indent()}}}")

    def _gen_else_chain(self, else_body):
        if len(else_body) == 1 and isinstance(else_body[0], IfStmt):
            nested = else_body[0]
            cond2 = self.gen_expr(nested.cond)
            self.lines.append(f"{self.indent()}}} else if ({cond2}) {{")
            self.indent_level += 1
            for s in nested.then_body:
                self.gen_stmt(s)
            self.indent_level -= 1
            if nested.else_body:
                self._gen_else_chain(nested.else_body)
            else:
                self.lines.append(f"{self.indent()}}}")
        else:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            for s in else_body:
                self.gen_stmt(s)
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

    def gen_while(self, stmt):
        cond = self.gen_expr(stmt.cond)
        self.lines.append(f"{self.indent()}while ({cond}) {{")
        self.indent_level += 1
        for s in stmt.body:
            self.gen_stmt(s)
        self.indent_level -= 1
        self.lines.append(f"{self.indent()}}}")

    def gen_map(self, stmt):
        start = self.gen_expr(stmt.range_start)
        end = self.gen_expr(stmt.range_end)
        if stmt.inclusive:
            end = f"({end}) + 1"

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

    def gen_filter(self, stmt):
        start = self.gen_expr(stmt.range_start)
        end = self.gen_expr(stmt.range_end)
        if stmt.inclusive:
            end = f"({end}) + 1"

        if stmt.into_array and stmt.into_count:
            out_array = stmt.into_array
            out_count = stmt.into_count
        else:
            count_var = self.fresh_var("_filter_count")
            result_var = self.fresh_var("_filter_result")
            idx_type = self.c_type(TypeRef(name="int"))

            self.lines.append(f"{self.indent()}{idx_type} {count_var} = 0;")
            self.lines.append(f"{self.indent()}int {result_var}[{end} - {start}];")

            out_array = result_var
            out_count = count_var

        self.push_scope()
        self.declare_var(stmt.index_var, TypeRef(name="int"), is_mut=False)

        self.lines.append(f"{self.indent()}for ({stmt.index_var} = {start}; "
                          f"{stmt.index_var} < {end}; ++{stmt.index_var}) {{")
        self.indent_level += 1

        if stmt.cond:
            cond_expr = self.gen_expr(stmt.cond)
            self.lines.append(f"{self.indent()}if ({cond_expr}) {{")
            self.indent_level += 1
            self.lines.append(f"{self.indent()}{out_array}[{out_count}] = {stmt.index_var};")
            self.lines.append(f"{self.indent()}{out_count}++;")
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")
        else:
            self.lines.append(f"{self.indent()}{out_array}[{out_count}] = {stmt.index_var};")
            self.lines.append(f"{self.indent()}{out_count}++;")

        self.indent_level -= 1
        self.lines.append(f"{self.indent()}}}")

        self.pop_scope()
        self.last_filter_result = out_array
        self.last_filter_count = out_count

    def gen_fold(self, stmt):
        init = self.gen_expr(stmt.init_value)
        start = self.gen_expr(stmt.range_start)
        end = self.gen_expr(stmt.range_end)
        if stmt.inclusive:
            end = f"({end}) + 1"

        acc_type = self.infer_type(stmt.init_value)
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
        self.declare_var(stmt.index_var, TypeRef(name="int"), is_mut=False)
        acc_type_ref = self.infer_type_ref(stmt.init_value)
        self.declare_var("acc", acc_type_ref, is_mut=False)

        self._emit_unroll_pragma_fold()

        self.lines.append(f"{self.indent()}for ({stmt.index_var} = {start}; "
                          f"{stmt.index_var} < {end}; ++{stmt.index_var}) {{")
        self.indent_level += 1

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

        self.pop_scope()
        self.last_fold_result = acc_var

    def gen_stmt(self, stmt):
        self.gen_stmt_common(stmt)

    def panic_code(self):
        mode = self.meta.directives.get("panic", "abort")
        if mode == "abort":
            return "abort()"
        elif mode == "halt":
            return "while (1) {}"
        elif mode == "trap":
            return "__builtin_trap()"
        elif mode == "return":
            return "return (0)"
        else:
            return "abort()"

    def emit_panic(self, indent=None):
        if indent is None:
            indent = self.indent()
        panic = self.panic_code()
        self.lines.append(f"{indent}{panic};")

    def get_type_range(self, type_name):
        td = self.meta.get_type_def(type_name)
        if not td and type_name in ("int", "int32_t"):
            td = self.meta.get_type_def("int32")
        if not td and type_name in ("int64", "int64_t"):
            td = self.meta.get_type_def("int64")
        if not td and type_name in ("uint", "uint32_t"):
            td = self.meta.get_type_def("uint32")
        if not td and type_name in ("uint64", "uint64_t"):
            td = self.meta.get_type_def("uint64")
        if td and isinstance(td, IntegerTypeDef):
            from_val = td.from_val
            to_val = td.to_val
            if from_val is None or to_val is None:
                if td.bits > 0:
                    if td.signed:
                        return -(1 << (td.bits - 1)), (1 << (td.bits - 1)) - 1
                    return 0, (1 << td.bits) - 1
                return None, None
            if isinstance(from_val, tuple):
                from_val = from_val[1]
            if isinstance(to_val, tuple):
                to_val = to_val[1]
            try:
                return int(from_val), int(to_val)
            except (ValueError, TypeError):
                pass
        if type_name in ("int32", "int", "int32_t"):
            return -2147483648, 2147483647
        if type_name in ("int64", "int64_t"):
            return -9223372036854775808, 9223372036854775807
        if type_name in ("uint32", "uint", "uint32_t"):
            return 0, 4294967295
        if type_name in ("uint64", "uint64_t"):
            return 0, 18446744073709551615
        if type_name in ("uint8", "uint8_t"):
            return 0, 255
        if type_name in ("int8", "int8_t"):
            return -128, 127
        if type_name in ("uint16", "uint16_t"):
            return 0, 65535
        if type_name in ("int16", "int16_t"):
            return -32768, 32767
        if type_name == "char":
            return -128, 127
        return None, None

    def emit_c_exports(self):
        for name in self.meta.c_exports:
            func = None
            for f in self.program.functions:
                if f.name == name:
                    func = f
                    break
            if func:
                ret_type = self.c_type(func.return_type) if func.return_type else "void"
                params = self.format_params(func)
                self.lines.append(f"extern {ret_type} {self.func_c_name(name)}({params});")
        if self.meta.c_exports:
            self.lines.append("")

    def emit_unroll_pragmas(self, func):
        pass

    def _emit_unroll_pragma_fold(self):
        pass

    def gen_return(self, stmt):
        if stmt.value is not None:
            ret_type = self.current_func.return_type if self.current_func else None
            val = self.gen_expr_as_type(stmt.value, ret_type)
            self.lines.append(f"{self.indent()}return ({val});")
        else:
            self.lines.append(f"{self.indent()}return;")

    def gen_asm(self, stmt):
        lines = stmt.code.strip().split("\n")
        asm_lines = []
        for line in lines:
            stripped = line.strip()
            if stripped:
                escaped = stripped.replace("\\", "\\\\").replace("\"", "\\\"")
                asm_lines.append(f'"{escaped}\\n"')

        has_extended = bool(stmt.inputs or stmt.outputs or stmt.clobbers)

        if not has_extended:
            self.lines.append(f"{self.indent()}__asm__ volatile (")
            for al in asm_lines:
                self.lines.append(f"{self.indent()}\t{al}")
            self.lines.append(f"{self.indent()});")
            return

        self.lines.append(f"{self.indent()}__asm__ volatile (")
        for al in asm_lines:
            self.lines.append(f"{self.indent()}\t{al}")

        if stmt.outputs:
            out_parts = [f'"{constraint}"({name})' for name, constraint in stmt.outputs]
            self.lines.append(f"{self.indent()} : {', '.join(out_parts)}")
        else:
            self.lines.append(f"{self.indent()} :")

        if stmt.inputs:
            in_parts = [f'"{constraint}"({name})' for name, constraint in stmt.inputs]
            self.lines.append(f"{self.indent()} : {', '.join(in_parts)}")
        else:
            self.lines.append(f"{self.indent()} :")

        if stmt.clobbers:
            clob_parts = [f'"{c}"' for c in stmt.clobbers]
            self.lines.append(f"{self.indent()} : {', '.join(clob_parts)}")

        self.lines.append(f"{self.indent()});")

    def gen_if_as_return(self, stmt, ret_type):
        cond = self.gen_expr(stmt.cond)
        self.lines.append(f"{self.indent()}if ({cond}) {{")
        self.indent_level += 1
        self._emit_return_body(stmt.then_body, ret_type)
        self.indent_level -= 1

        if stmt.else_body:
            self._gen_else_chain_return(stmt.else_body, ret_type)
        else:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            self.lines.append(f"{self.indent()}return ({self.zero_value_for_type(ret_type)});")
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

    def _gen_else_chain_return(self, else_body, ret_type):
        if len(else_body) == 1 and isinstance(else_body[0], IfStmt):
            nested = else_body[0]
            cond2 = self.gen_expr(nested.cond)
            self.lines.append(f"{self.indent()}}} else if ({cond2}) {{")
            self.indent_level += 1
            self._emit_return_body(nested.then_body, ret_type)
            self.indent_level -= 1
            if nested.else_body:
                self._gen_else_chain_return(nested.else_body, ret_type)
            else:
                self.lines.append(f"{self.indent()}}} else {{")
                self.indent_level += 1
                self.lines.append(f"{self.indent()}return ({self.zero_value_for_type(ret_type)});")
                self.indent_level -= 1
                self.lines.append(f"{self.indent()}}}")
        else:
            self.lines.append(f"{self.indent()}}} else {{")
            self.indent_level += 1
            self._emit_return_body(else_body, ret_type)
            self.indent_level -= 1
            self.lines.append(f"{self.indent()}}}")

    def _emit_return_body(self, body, ret_type):
        if len(body) == 1:
            val = self.gen_expr_as_type(body[0], ret_type)
            self.lines.append(f"{self.indent()}return ({val});")
        elif body:
            for s in body[:-1]:
                self.gen_stmt(s)
            val = self.gen_expr_as_type(body[-1], ret_type)
            self.lines.append(f"{self.indent()}return ({val});")
        else:
            self.lines.append(f"{self.indent()}return ({self.zero_value_for_type(ret_type)});")
