
#ifndef __BINDER_SERVICE_P_H__
#define __BINDER_SERVICE_P_H__

#ifdef __cplusplus
extern "C"{
#endif

#include "service.h"

typedef void (*BinderCallback)(const request_t *, response_t *);

extern void BinderMainThread(BinderCallback);
extern void BinderMainBlock(BinderCallback);

#ifdef __cplusplus
}
#endif

#endif

