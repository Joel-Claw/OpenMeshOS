/* OpenMeshOS nRF52 time compatibility shim
   Provides time_t without pulling in sys/select.h (which conflicts with ed25519)
   Copyright 2026 Joel Claw & contributors - WTFPL v2 */
#ifndef OMS_TIME_H_SHIM
#define OMS_TIME_H_SHIM
#ifdef __cplusplus
extern "C" {
#endif
typedef long time_t;
#ifdef __cplusplus
}
#endif
#endif