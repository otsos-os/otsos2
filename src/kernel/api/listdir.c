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

#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/api/api.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

int api_fs_listdir(const char *path, struct api_dirent *buf, u32 max_entries) {
  vnode_t *vn;
  u32 i;
  int ret;
  char name[32];
  int type;
  int j;
  char curpath[256];
  const char *resolve_path;

  if (!buf || max_entries == 0) {
    return -API_ERR_BAD_VALUE;
  }
  if (!is_user_address(buf, max_entries * sizeof(struct api_dirent))) {
    return -API_ERR_BAD_ADDR;
  }

  if (path) {
    if (!is_user_address(path, 1)) {
      return -API_ERR_BAD_ADDR;
    }
  }

  /*
   * Determine the path to list.  An empty or NULL path means the
   * current directory, which ChainFS tracks internally.  We resolve
   * it to an absolute path so that VFS can handle it uniformly.
   */
  if (path == NULL || path[0] == '\0') {
    ret = vfs_getcwd(curpath, sizeof(curpath));
    if (ret != 0) {
      return ret;
    }
    resolve_path = curpath;
  } else {
    resolve_path = path;
  }

  ret = vfs_resolve(resolve_path, &vn);
  if (ret != 0) {
    return ret;
  }
  if (vn == NULL) {
    return -API_ERR_NOT_FOUND;
  }

  if (vn->type != VDIR) {
    vnode_release(vn);
    return -API_ERR_NOT_DIR;
  }

  for (i = 0; i < max_entries; i++) {
    type = 0;
    ret = vnode_readdir(vn, i, name, &type);
    if (ret < 0) {
      vnode_release(vn);
      return ret;
    }
    if (ret == 0) {
      break;
    }

    memset(buf[i].name, 0, sizeof(buf[i].name));
    for (j = 0; j < 31 && name[j] != '\0'; j++) {
      buf[i].name[j] = name[j];
    }
    buf[i].name[j] = '\0';
    buf[i].type = (u8)type;
    buf[i].pad[0] = 0;
    buf[i].pad[1] = 0;
    buf[i].pad[2] = 0;
  }

  vnode_release(vn);
  return (int)i;
}
