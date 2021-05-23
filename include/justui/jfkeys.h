//---
// JustUI.jfkeys: Row of function keys
//---

#ifndef _J_JFKEYS
#define _J_JFKEYS

#include <justui/defs.h>
#include <justui/jwidget.h>

#include <gint/display.h>

/* jfkeys: Functions keys indicating functions for the F1..F6 keys

   This widget is the typical function key bar with a slightly different
   design. There are four types of keys, with conventional guidelines:

   * MENU KEYS are used for functions that open menus or navigate between tabs
     on a same application. The name comes from their primary usage in the
     system apps. Navigation functions should be easily reversible and fairly
     failproof. Menu keys are black rectangular keys with a chipped corner.

   * ENTRY KEYS are used for catalog entries such as the leaves of PRGM's many
     nested input menus. They represent entries to be chosen from. Entry keys
     are black rectangular keys.

   * ACTION KEYS are used for generic safe and unsafe actions. Action keys are
     black round keys.

   * SPECIAL KEYS are used for special functions, such as scrolling to the next
     set of functions keys when there are several pages, important functions
     that should catch attention, or particularly unsafe actions. They are
     round white keys.

   On fx-CG 50, the keys are drawn dynamically using gint's default font, and
   specified using 6 strings that give the type and name of the keys:

   * "/NAME" for a menu key;
   * ".NAME" for an entry key;
   * "@NAME" for an action key;
   " "#NAME" for a special key.

   The names are separated by semicolons, eg. "/F1;;/F3;.F4;@F5;#F6". Several
   sets of function keys can be defined if separated by a '|' character. For
   instance, "/F1;#F2|/F1" represents a function bar where the F2
   function can be hidden by switching from level 0 to level 1.

   On fx-9860G, there is not enough space to generate keys on-the-fly, so the
   full specification is just an image. The convention for the image is to be
   128x8 pixels, with each function key (i) positioned at (x = 21*i + 2) of
   width 19. Several levels can be stacked up (n levels in an image of height
   9n-1) and selected independently. */
typedef struct {
	jwidget widget;
	int8_t level;

	#ifdef FX9860G
	bopti_image_t const *img;
	#endif

	#ifdef FXCG50
	char const *labels;
  char const *overrides[6];
	#endif

} jfkeys;

/* jfkeys_create(): Create a set of function keysed

   The arguments are obviously different on fx-9860G and fx-CG 50. If your
   application supports both, you might want to specify arguments for both
   platforms in a single call with jfkeys_create2() which will filter them out
   for you. Referencing an image unavailable on fx-CG 50 in jfkeys_create2() is
   safe since the preprocessor will remove that text. */

#ifdef FX9860G
jfkeys *jfkeys_create(bopti_image_t const *img, void *parent);
#define jfkeys_create2(img, labels, parent) jfkeys_create(img, parent)
#endif

#ifdef FXCG50
jfkeys *jfkeys_create(char const *labels, void *parent);
#define jfkeys_create2(img, labels, parent) jfkeys_create(labels, parent)
#endif

/* jfkeys_set(): Replace functions
   This will also reset the level to 0. */

#ifdef FX9860G
void jfkeys_set(jfkeys *keys, bopti_image_t const *img);
#define jfkeys_set2(keys, img, labels) jfkeys_set(keys, img)
#endif

#ifdef FXCG50
void jfkeys_set(jfkeys *keys, char const *labels);
#define jfkeys_set2(keys, img, labels) jfkeys_set(keys, labels)
#endif

/* jfkeys_level(): Return the current function key level */
int jfkeys_level(jfkeys *keys);

/* jfkeys_set_level(): Set the function key level */
void jfkeys_set_level(jfkeys *keys, int level);

/* The following functions are available only on fx-CG 50 and are no-ops on
   fx-9860 (you can't generate a good image for a tiny key). */

/* fkeys_override(): Get the override for a key */
char const *jfkeys_override(jfkeys *keys, int key);

/* fkeys_set_override(): Override the value of a single key on all levels
   This functions sets the override on the specified key, which replaces the
   label for that key on all levels. This is useful to conditionally show
   functions. */
void jfkeys_set_override(jfkeys *keys, int key, char const *override);

#endif /* _J_JFKEYS */
