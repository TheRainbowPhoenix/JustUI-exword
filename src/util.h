//---
// JustUI.util: Header-level utilities that cannot be exposed to users
//---

#ifndef _J_UTIL
#define _J_UTIL

#include <justui/defs.h>

/* Clamp a value between two ends. */
__attribute__((always_inline))
static inline int clamp(int value, int min, int max)
{
	/* Mark the branches as unlikely, that might help */
	if(__builtin_expect(value < min, 0)) return min;
	if(__builtin_expect(value > max, 0)) return max;
	return value;
}

/* Code point for a character input */
uint32_t keymap_translate(int key, bool shift, bool alpha);

#endif /* _J_UTIL */
