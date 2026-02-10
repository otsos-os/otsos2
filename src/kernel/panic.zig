// /*
//  * Copyright (c) 2026, otsos team
//  *
//  * Redistribution and use in source and binary forms, with or without
//  * modification, are permitted provided that the following conditions are met:
//  *
//  * 1. Redistributions of source code must retain the above copyright notice,
//  * this list of conditions and the following disclaimer.
//  *
//  * 2. Redistributions in binary form must reproduce the above copyright notice,
//  *    this list of conditions and the following disclaimer in the documentation
//  *    and/or other materials provided with the distribution.
//  *
//  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  * POSSIBILITY OF SUCH DAMAGE.
//  */

const c = struct {
    extern fn panic(format: [*:0]const u8, ...) callconv(.c) void;
};

pub fn panic(msg: []const u8, trace: ?*anyopaque, ret_addr: ?usize) noreturn {
    _ = trace;
    _ = ret_addr;

    var buffer: [512]u8 = undefined;
    const max_len: usize = buffer.len - 1;
    const msg_len: usize = if (msg.len < max_len) msg.len else max_len;

    var i: usize = 0;
    while (i < msg_len) : (i += 1) {
        buffer[i] = msg[i];
    }
    buffer[msg_len] = 0;

    const c_msg: [*:0]const u8 = @ptrCast(&buffer[0]);
    c.panic("%s", c_msg);

    while (true) {
        asm volatile ("hlt");
    }
}
