#ifndef __STDBOOL_REPLACEMENT__H__
#define __STDBOOL_REPLACEMENT__H__

#ifdef _MSC_VER
typedef enum { __bool_must_promote_to_int = -1, false = 0, true = 1 } __bool;
#define bool __bool
#define false 0
#define true 1
#endif

#endif /* __STDBOOL_REPLACEMENT__H__ */
