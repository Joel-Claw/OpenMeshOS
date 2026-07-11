/* OpenMeshOS nRF52 time compatibility shim
   Includes the toolchain <time.h> but blocks sys/select.h to avoid
   a name clash with ed25519's static select() function in ge.c.
   Copyright 2026 Joel Claw & contributors - WTFPL v2 */
#ifndef OMS_TIME_H_SHIM
#define OMS_TIME_H_SHIM

/* Block sys/select.h from being included by the real time.h.
   ed25519's ge.c defines a static function named select(), which
   conflicts with the POSIX select() syscall declared in sys/select.h. */
#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H
#endif
#ifndef __SYS_SELECT_H
#define __SYS_SELECT_H
#endif

/* Include the real toolchain time.h (skipping our shim via #include_next) */
#include_next <time.h>

#endif /* OMS_TIME_H_SHIM */