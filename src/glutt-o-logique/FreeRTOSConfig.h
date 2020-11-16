#include "../common/Core/FreeRTOSConfig.h"

#define configCHECK_FOR_STACK_OVERFLOW	2 // Default: 2

#if defined(configGENERATE_RUN_TIME_STATS)
#undef configGENERATE_RUN_TIME_STATS
#endif
#define configGENERATE_RUN_TIME_STATS	0

#if configGENERATE_RUN_TIME_STATS
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()    vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()            vGetTimerForRunTimeStats()
#endif
