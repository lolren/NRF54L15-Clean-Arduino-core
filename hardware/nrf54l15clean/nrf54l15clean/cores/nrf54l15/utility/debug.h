#ifndef NRF54L15_CLEAN_DEBUG_H_
#define NRF54L15_CLEAN_DEBUG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int dbgHeapTotal(void);
int dbgHeapUsed(void);

static inline int dbgHeapFree(void)
{
    return dbgHeapTotal() - dbgHeapUsed();
}

void dbgMemInfo(void);
void dbgPrintVersion(void);
uint32_t getFreeHeapSize(void);

#ifdef __cplusplus
}
#endif

#endif  // NRF54L15_CLEAN_DEBUG_H_
