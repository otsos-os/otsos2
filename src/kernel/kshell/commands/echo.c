/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/kshell/kshell.h>
#include <kernel/posix/posix.h>
#include <kernel/process.h>
#include <mlibc/mlibc.h>

static int g_kshell_is_multiboot2 = 0;

void kshell_set_boot_info(int is_multiboot2) {
  g_kshell_is_multiboot2 = is_multiboot2 ? 1 : 0;
}

static const char *process_state_name(process_state_t state) {
  switch (state) {
  case PROC_STATE_UNUSED:
    return "UNUSED";
  case PROC_STATE_EMBRYO:
    return "EMBRYO";
  case PROC_STATE_RUNNABLE:
    return "RUNNABLE";
  case PROC_STATE_RUNNING:
    return "RUNNING";
  case PROC_STATE_SLEEPING:
    return "SLEEPING";
  case PROC_STATE_ZOMBIE:
    return "ZOMBIE";
  default:
    return "UNKNOWN";
  }
}

static int parse_number(const char **p, int *ok) {
  int value = 0;
  int sign = 1;

  while (**p == ' ' || **p == '\t') {
    (*p)++;
  }

  if (**p == '-') {
    sign = -1;
    (*p)++;
  }

  if (**p < '0' || **p > '9') {
    *ok = 0;
    return 0;
  }

  while (**p >= '0' && **p <= '9') {
    value = (value * 10) + (**p - '0');
    (*p)++;
  }

  return value * sign;
}

static int parse_term(const char **p, int *ok) {
  int lhs = parse_number(p, ok);
  while (*ok) {
    int rhs = 0;
    char op = **p;

    while (**p == ' ' || **p == '\t') {
      (*p)++;
      op = **p;
    }

    if (op != '*' && op != '/') {
      return lhs;
    }

    (*p)++;
    rhs = parse_number(p, ok);
    if (!*ok) {
      return 0;
    }

    if (op == '*') {
      lhs *= rhs;
    } else {
      if (rhs == 0) {
        *ok = 0;
        return 0;
      }
      lhs /= rhs;
    }
  }
  return lhs;
}

static int parse_expr(const char *expr, int *ok) {
  const char *p = expr;
  int lhs = parse_term(&p, ok);

  while (*ok) {
    int rhs = 0;
    char op = *p;

    while (*p == ' ' || *p == '\t') {
      p++;
      op = *p;
    }

    if (op != '+' && op != '-') {
      break;
    }

    p++;
    rhs = parse_term(&p, ok);
    if (!*ok) {
      return 0;
    }

    if (op == '+') {
      lhs += rhs;
    } else {
      lhs -= rhs;
    }
  }

  while (*p == ' ' || *p == '\t') {
    p++;
  }

  if (*p != '\0') {
    *ok = 0;
    return 0;
  }

  return lhs;
}

static int print_kernel_var(const char *name) {
  if (strcmp(name, "videoM") == 0) {
    if (is_framebuffer_enabled()) {
      kshell_console_write("linear fb, address: ");
      kshell_console_write_ptr((void *)(u64)fb_get_address());
      kshell_console_write(", multiboot");
      kshell_console_write_int(g_kshell_is_multiboot2 ? 2 : 1);
      kshell_console_write("\n");
    } else {
      kshell_console_write("vga\n");
    }
    return 0;
  }

  if (strcmp(name, "dkey") == 0) {
    const char *driver = keyboard_get_driver_name();
    if (!driver) {
      driver = "none";
    }
    kshell_console_write("keyboard driver: ");
    kshell_console_write(driver);
    kshell_console_write("\n");
    return 0;
  }

  if (strcmp(name, "prun") == 0) {
    kshell_console_write("PID\tNAME\tSTATE\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
      process_t *proc = &process_table[i];
      if (proc->state != PROC_STATE_UNUSED) {
        kshell_console_write_int((int)proc->pid);
        kshell_console_write("\t");
        kshell_console_write(proc->name);
        kshell_console_write("\t");
        kshell_console_write(process_state_name(proc->state));
        kshell_console_write("\n");
      }
    }
    return 0;
  }

  if (strcmp(name, "uname") == 0) {
    struct utsname uts;
    uname_fill(&uts);
    kshell_console_write("sysname: ");
    kshell_console_write(uts.sysname);
    kshell_console_write("\n");
    kshell_console_write("nodename: ");
    kshell_console_write(uts.nodename);
    kshell_console_write("\n");
    kshell_console_write("release: ");
    kshell_console_write(uts.release);
    kshell_console_write("\n");
    kshell_console_write("version: ");
    kshell_console_write(uts.version);
    kshell_console_write("\n");
    kshell_console_write("machine: ");
    kshell_console_write(uts.machine);
    kshell_console_write("\n");
    kshell_console_write("domainname: ");
    kshell_console_write(uts.domainname);
    kshell_console_write("\n");
    return 0;
  }

  return -1;
}

int kshell_echo_command(int argc, char *argv[]) {
  char expr_buf[KSHELL_MAX_LINE];

  if (argc <= 1) {
    kshell_console_write("\n");
    return 0;
  }

  if (argv[1][0] == '&') {
    int ok = 1;
    int pos = 0;
    int value = 0;

    for (int i = 1; i < argc && pos < (KSHELL_MAX_LINE - 1); i++) {
      const char *src = argv[i];
      if (i == 1 && src[0] == '&') {
        src++;
      }

      while (*src != '\0' && pos < (KSHELL_MAX_LINE - 1)) {
        expr_buf[pos++] = *src++;
      }

      if (i + 1 < argc && pos < (KSHELL_MAX_LINE - 1)) {
        expr_buf[pos++] = ' ';
      }
    }
    expr_buf[pos] = '\0';

    value = parse_expr(expr_buf, &ok);
    if (!ok) {
      kshell_console_write("echo: invalid math expression\n");
      return -1;
    }
    kshell_console_write_int(value);
    kshell_console_write("\n");
    return 0;
  }

  if (argv[1][0] == '%' && argv[1][1] != '\0') {
    if (print_kernel_var(&argv[1][1]) != 0) {
      kshell_console_write("echo: unknown kernel variable: ");
      kshell_console_write(argv[1]);
      kshell_console_write("\n");
      return -1;
    }
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    kshell_console_write(argv[i]);
    if (i + 1 < argc) {
      kshell_console_write(" ");
    }
  }
  kshell_console_write("\n");
  return 0;
}
