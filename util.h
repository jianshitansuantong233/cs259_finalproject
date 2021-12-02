#ifndef UTIL_H
#define UTIL_H

#ifdef DEBUG_ON
#define DEBUG(block) do { block; } while (false)
#else
#define DEBUG(block)
#endif

#endif // UTIL_H
