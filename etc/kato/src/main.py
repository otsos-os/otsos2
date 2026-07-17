#!/usr/bin/env python3
import sys
import os
import argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from compiler import Compiler


def main():
    parser = argparse.ArgumentParser(
        prog="kato",
        description="Kato language compiler - compiles .kato files to C",
    )
    parser.add_argument("input", nargs="+", help="input .kato file(s)")
    parser.add_argument("-o", "--output", help="output .c file (default: same name as first input with .c extension)")
    parser.add_argument("-I", "--import-path", action="append", default=[], help="add import search path")
    parser.add_argument("-freestand", action="store_true", help="freestanding mode: no default #include, emit extern declarations instead")

    args = parser.parse_args()

    for inp in args.input:
        if not os.path.exists(inp):
            print(f"error: file not found: {inp}", file=sys.stderr)
            sys.exit(1)

    compiler = Compiler(args.input, import_paths=args.import_path, freestanding=args.freestand)

    output_path = args.output
    if output_path is None:
        base = os.path.splitext(args.input[0])[0]
        output_path = base + ".c"

    c_code = compiler.compile()
    if c_code is None:
        sys.exit(1)

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(c_code)

    inputs_str = ", ".join(args.input)
    print(f"compiled: {inputs_str} -> {output_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
