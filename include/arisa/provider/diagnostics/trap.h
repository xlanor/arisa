#pragma once

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((noreturn, nothrow, cold)) void arisa_trap(void);

#ifdef __cplusplus
}
#endif

#define __builtin_trap() arisa_trap()
