#include "Galois/Runtime/sync_structures.h"

GALOIS_SYNC_STRUCTURE_REDUCE_SET(dist_current, unsigned int);
GALOIS_SYNC_STRUCTURE_REDUCE_MIN(dist_current, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(dist_current, unsigned int);
#if __OPT_VERSION__ >= 3
GALOIS_SYNC_STRUCTURE_BITSET(dist_current);
#endif

GALOIS_SYNC_STRUCTURE_REDUCE_SET(dist_old, unsigned int);
GALOIS_SYNC_STRUCTURE_REDUCE_MIN(dist_old, unsigned int);
GALOIS_SYNC_STRUCTURE_BROADCAST(dist_old, unsigned int);
#if __OPT_VERSION__ >= 3
GALOIS_SYNC_STRUCTURE_BITSET(dist_old);
#endif
