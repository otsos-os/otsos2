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

#include <kernel/event/event.h>
#include <kernel/api/api.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/*
 * EVFILT_WRITE — returns when it is possible to write.
 *
 * For pipes: returns when there is space in the write buffer.
 * For tty: always writable (output buffer is never full in our impl).
 */

static int filt_write_attach(knote_t *kn) {
  int fd = (int)kn->ident;

  /* fd 1 (stdout) and fd 2 (stderr) are always valid — implicit. */
  if (fd == 1 || fd == 2) {
    return 0;
  }

  api_handle_t *handles = api_get_handle_table();
  if (!handles) {
    return -API_ERR_BAD_HANDLE;
  }

  if (fd < 0 || fd >= MAX_HANDLES || !handles[fd].used) {
    com1_printf("[EVFILT_WRITE] attach: bad fd %d\n", fd);
    return -API_ERR_BAD_HANDLE;
  }

  return 0;
}

static void filt_write_detach(knote_t *kn) {
  (void)kn;
}

static int filt_write_event(knote_t *kn, u32 nevents) {
  int fd = (int)kn->ident;

  /* Handle 1 (stdout) / Handle 2 (stderr) — always writable */
  if (fd == 1 || fd == 2) {
    kn->data = PIPE_BUF_SIZE;
    return 1;
  }

  api_handle_t *handles = api_get_handle_table();
  if (!handles || fd < 0 || fd >= MAX_HANDLES || !handles[fd].used) {
    return 0;
  }

  api_object_t *objects = api_get_object_table();
  int obj_idx = handles[fd].object_index;
  if (obj_idx < 0 || obj_idx >= MAX_DATA_OBJECTS || !objects[obj_idx].used) {
    return 0;
  }

  api_object_t *obj = &objects[obj_idx];

  if (obj->type == API_OBJECT_PIPE) {
    pipe_t *p = (pipe_t *)obj->pipe;
    if (!p) {
      return 0;
    }

    u32 space = PIPE_BUF_SIZE - p->size;
    if (space > 0) {
      kn->data = space;
      return 1;
    }

    /* EOF: no readers left */
    if (p->readers == 0) {
      kn->flags |= EV_EOF;
      return 1;
    }

    return 0;
  }

  /* Regular file — always writable */
  if (obj->type == API_OBJECT_FILE) {
    kn->data = PIPE_BUF_SIZE;
    return 1;
  }

  return 0;
}

static void filt_write_touch(knote_t *kn, struct kevent *kev) {
  (void)kn;
  (void)kev;
}

const filter_ops_t filter_write_ops = {
  .filter = EVFILT_WRITE,
  .name   = "write",
  .attach = filt_write_attach,
  .detach = filt_write_detach,
  .event  = filt_write_event,
  .touch  = filt_write_touch,
};
