import sys


class KatoError(Exception):
    pass


class CompileError(KatoError):
    def __init__(self, category, filename, line, col, message, source_line=None,
                 severity="error"):
        self.category = category
        self.filename = filename
        self.line = line
        self.col = col
        self.message = message
        self.source_line = source_line
        self.severity = severity
        super().__init__(self._format())

    def _format(self):
        if self.line and self.col:
            loc = f"{self.filename}:{self.line}:{self.col}"
        elif self.line:
            loc = f"{self.filename}:{self.line}"
        else:
            loc = self.filename

        header = f"{self.severity}: {loc}: {self.category}: {self.message}"
        if self.source_line and self.col > 0:
            caret = " " * max(0, self.col - 1) + "^"
            return f"{header}\n    {self.source_line}\n    {caret}"
        return header


class ErrorReporter:
    def __init__(self, filename="<unknown>"):
        self.filename = filename
        self.errors = []
        self.warnings = []
        self.source_lines = []

    def set_source(self, source):
        self.source_lines = source.split("\n")

    def get_source_line(self, line):
        if 0 <= line - 1 < len(self.source_lines):
            return self.source_lines[line - 1]
        return None

    def error(self, category, line, col, message):
        src = self.get_source_line(line)
        err = CompileError(category, self.filename, line, col, message, src)
        self.errors.append(err)
        return err

    def warning(self, category, line, col, message):
        src = self.get_source_line(line)
        w = CompileError(category, self.filename, line, col, message, src,
                         severity="warning")
        self.warnings.append(w)
        return w

    def has_errors(self):
        return len(self.errors) > 0

    def report(self):
        for w in self.warnings:
            print(str(w), file=sys.stderr)
        for e in self.errors:
            print(str(e), file=sys.stderr)
        if self.errors:
            print(f"\n{len(self.errors)} error(s), {len(self.warnings)} warning(s)", file=sys.stderr)

    def fatal(self, category, line, col, message):
        err = self.error(category, line, col, message)
        self.report()
        sys.exit(1)
