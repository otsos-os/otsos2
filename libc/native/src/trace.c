/* !DEFINES!

$define %type int as native trace handle
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type api_trace_read as native trace read argument block

$define %func traceOpen as function with args uint32_t
$define %func traceClose as function with args int
$define %func traceRead as function with args int, api_trace_read *
$define %func traceCtl as function with args int, uint32_t, void *
$define %func traceInfo as function with args uint32_t, void *
$define %func traceLoad as function with args int, api_trace_load *
$define %func traceReadAggs as function with args int, api_trace_aggs *
$define %func traceMark as function with args uint32_t, five uint64_t

*/

/* !SPACE!

$space %export traceOpen, traceClose, traceRead, traceCtl, traceInfo
$space %export traceLoad, traceReadAggs, traceMark

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

#include <native.h>
#include <stdint.h>
#include "private.h"

int
traceOpen(uint32_t flags)
{
	return (__sysret_int(__syscall1(CALL_TRACE_OPEN, (long)flags)));
}

int
traceClose(int trace)
{
	return (__sysret_int(__syscall1(CALL_TRACE_CLOSE, (long)trace)));
}

ssize_t
traceRead(int trace, struct api_trace_read *args)
{
	return (__sysret(__syscall2(CALL_TRACE_READ, (long)trace,
	    (long)args)));
}

int
traceCtl(int trace, uint32_t op, void *arg)
{
	return (__sysret_int(__syscall3(CALL_TRACE_CTL, (long)trace,
	    (long)op, (long)arg)));
}

int
traceInfo(uint32_t op, void *arg)
{
	return (__sysret_int(__syscall2(CALL_TRACE_INFO, (long)op,
	    (long)arg)));
}

int
traceLoad(int trace, struct api_trace_load *load)
{
	return (traceCtl(trace, API_TRACE_OP_LOAD, load));
}

ssize_t
traceReadAggs(int trace, struct api_trace_aggs *args)
{
	if (args != NULL) {
		args->trace = trace;
	}
	return (__sysret(__syscall2(CALL_TRACE_INFO,
	    (long)API_TRACE_INFO_AGGS, (long)args)));
}

int
traceMark(uint32_t id, uint64_t a0, uint64_t a1, uint64_t a2,
    uint64_t a3, uint64_t a4)
{
	return (__sysret_int(__syscall6(CALL_TRACE_MARK, (long)id,
	    (long)a0, (long)a1, (long)a2, (long)a3, (long)a4)));
}
