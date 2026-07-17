import sys
import os

from lexer.lexer import Lexer
from metadata.parser import MetadataParser
from metadata.nodes import MetadataModule
from parser.parser import Parser
from parser.ast import Program
from typecheck.typechecker import TypeChecker
from codegen.speed import SpeedCodeGen
from codegen.safety import SafetyCodeGen
from util.errors import ErrorReporter


class Compiler:
    def __init__(self, filenames, import_paths=None, freestanding=False):
        if isinstance(filenames, str):
            filenames = [filenames]
        self.filenames = filenames
        self.import_paths = import_paths or []
        self.freestanding = freestanding
        self.reporter = ErrorReporter(filenames[0] if filenames else "<unknown>")
        self.metadata = None
        self.program = None
        self.compiled_modules = {}
        self.import_stack = []
        self.sources = {}

    def _type_defs_equal(self, td1, td2):
        if type(td1) != type(td2):
            return False
        if isinstance(td1, MetadataModule):
            return False
        for field_name in td1.__dataclass_fields__:
            v1 = getattr(td1, field_name, None)
            v2 = getattr(td2, field_name, None)
            if v1 != v2:
                return False
        return True

    def _resolve_import(self, module_name, importing_dir):
        candidates = [
            os.path.join(importing_dir, module_name + ".kato"),
        ]
        for path in self.import_paths:
            candidates.append(os.path.join(path, module_name + ".kato"))

        for candidate in candidates:
            if os.path.exists(candidate):
                return os.path.abspath(candidate)
        return None

    def _compile_single_file(self, filename):
        with open(filename, "r", encoding="utf-8") as f:
            source = f.read()

        self.sources[os.path.abspath(filename)] = source
        file_dir = os.path.dirname(os.path.abspath(filename))

        lexer = Lexer(source, filename)
        tokens, lex_reporter = lexer.tokenize()
        if lex_reporter.has_errors():
            lex_reporter.report()
            return None, None

        meta_parser = MetadataParser(tokens, filename)
        meta_parser.reporter.set_source(source)
        metadata, meta_reporter = meta_parser.parse()
        if meta_reporter.has_errors():
            meta_reporter.report()
            return None, None

        parser = Parser(tokens, filename)
        parser.reporter.set_source(source)
        program, parse_reporter = parser.parse()
        if parse_reporter.has_errors():
            parse_reporter.report()
            return None, None

        for imp_name in metadata.imports:
            if imp_name in self.import_stack:
                self.reporter.error("metadata", 0, 0,
                                    f"import cycle detected: {' -> '.join(self.import_stack + [imp_name])}")
                return None, None

            if imp_name in self.compiled_modules:
                continue

            imp_path = self._resolve_import(imp_name, file_dir)
            if imp_path is None:
                self.reporter.error("metadata", 0, 0,
                                    f"cannot resolve import '{imp_name}' from {filename}")
                return None, None

            self.import_stack.append(imp_name)
            imp_meta, imp_prog = self._compile_single_file(imp_path)
            self.import_stack.pop()

            if imp_meta is None:
                return None, None

            self.compiled_modules[imp_name] = (imp_path, imp_meta, imp_prog)

        return metadata, program

    def _merge_metadata(self, modules):
        merged = MetadataModule()
        all_modes = set()
        for filename, meta, prog in modules:
            all_modes.add(meta.mode)
            for name, td in meta.type_defs.items():
                if name in merged.type_defs:
                    if not self._type_defs_equal(merged.type_defs[name], td):
                        self.reporter.error("metadata", 0, 0,
                                            f"conflicting type definition '{name}' across modules")
                    continue
                merged.type_defs[name] = td
            for name, fc in meta.func_contracts.items():
                if name in merged.func_contracts:
                    self.reporter.error("metadata", 0, 0,
                                        f"duplicate function contract '{name}' across modules")
                merged.func_contracts[name] = fc
            for name, val in meta.consts.items():
                if name not in merged.consts:
                    merged.consts[name] = val
            for name, val in meta.compile_vars.items():
                if name not in merged.compile_vars:
                    merged.compile_vars[name] = val
            for td in meta.transforms:
                merged.transforms.append(td)
            for name in meta.disabled_transforms:
                merged.disabled_transforms.append(name)
            for kind, names in meta.visibility.items():
                if kind not in merged.visibility:
                    merged.visibility[kind] = set()
                merged.visibility[kind].update(names)
            for inc in meta.c_includes:
                if inc not in merged.c_includes:
                    merged.c_includes.append(inc)
            for flag in meta.c_flags:
                if flag not in merged.c_flags:
                    merged.c_flags.append(flag)
            for name, cname in meta.c_names.items():
                merged.c_names[name] = cname
            for name in meta.c_exports:
                if name not in merged.c_exports:
                    merged.c_exports.append(name)
            for name, cf in meta.c_funcs.items():
                if name not in merged.c_funcs:
                    merged.c_funcs[name] = cf
            for emit in meta.emits:
                merged.emits.append(emit)
            if meta.c_prefix and not merged.c_prefix:
                merged.c_prefix = meta.c_prefix
            if meta.c_no_prefix:
                merged.c_no_prefix = True
            if meta.has_main:
                if merged.has_main:
                    self.reporter.error("metadata", 0, 0,
                                        "multiple start functions across modules")
                merged.has_main = True

        if len(all_modes) == 1:
            merged.mode = all_modes.pop()
        elif len(all_modes) > 1:
            self.reporter.error("metadata", 0, 0,
                                f"cannot mix modes across modules: {all_modes}")

        merged.directives = modules[0][1].directives

        return merged

    def _merge_programs(self, modules):
        merged = Program()
        seen_funcs = set()
        seen_structs = set()
        seen_enums = set()
        seen_consts = set()

        for filename, meta, prog in modules:
            for struct in prog.structs:
                if struct.name in seen_structs:
                    continue
                seen_structs.add(struct.name)
                merged.structs.append(struct)
            for enum in prog.enums:
                if enum.name in seen_enums:
                    continue
                seen_enums.add(enum.name)
                merged.enums.append(enum)
            for const in prog.consts:
                if const.name in seen_consts:
                    continue
                seen_consts.add(const.name)
                merged.consts.append(const)
            for func in prog.functions:
                if func.name in seen_funcs:
                    continue
                seen_funcs.add(func.name)
                merged.functions.append(func)
            for imp in prog.imports:
                if imp not in merged.imports:
                    merged.imports.append(imp)

        return merged

    def compile(self):
        modules = []

        for filename in self.filenames:
            abspath = os.path.abspath(filename)
            module_name = os.path.splitext(os.path.basename(abspath))[0]

            if module_name in self.compiled_modules:
                continue

            self.import_stack.append(module_name)
            meta, prog = self._compile_single_file(abspath)
            self.import_stack.pop()

            if meta is None:
                return None

            self.compiled_modules[module_name] = (abspath, meta, prog)
            modules.append((abspath, meta, prog))

        for imp_name, (imp_path, imp_meta, imp_prog) in self.compiled_modules.items():
            if not any(m[0] == imp_path for m in modules):
                modules.append((imp_path, imp_meta, imp_prog))

        if self.reporter.has_errors():
            self.reporter.report()
            return None

        merged_metadata = self._merge_metadata(modules)
        if self.reporter.has_errors():
            self.reporter.report()
            return None

        merged_program = self._merge_programs(modules)

        typechecker = TypeChecker(merged_program, merged_metadata, self.filenames[0])
        first_source = self.sources.get(os.path.abspath(self.filenames[0]))
        if first_source:
            typechecker.reporter.set_source(first_source)
        type_reporter = typechecker.check()
        if type_reporter.has_errors():
            type_reporter.report()
            return None

        self.metadata = merged_metadata
        self.program = merged_program

        if merged_metadata.mode == "speed":
            codegen = SpeedCodeGen(merged_program, merged_metadata, freestanding=self.freestanding)
        else:
            codegen = SafetyCodeGen(merged_program, merged_metadata, freestanding=self.freestanding)

        c_code = codegen.generate()

        type_reporter.report()

        return c_code

    def compile_to_file(self, output_path=None):
        if output_path is None:
            base = os.path.splitext(self.filenames[0])[0]
            output_path = base + ".c"

        c_code = self.compile()
        if c_code is None:
            return False

        with open(output_path, "w", encoding="utf-8") as f:
            f.write(c_code)

        return True
