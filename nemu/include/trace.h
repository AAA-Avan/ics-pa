#ifndef __TRACE_H__
#define __TRACE_H__

#include <common.h>

void iringbuf_write(const char *log);
void iringbuf_print();
void ftrace_record(paddr_t pc, paddr_t dnpc, int type);

#endif 