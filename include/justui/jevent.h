//---
// JustUI.jevent: GUI union event
//---

#ifndef _J_EVENT
#define _J_EVENT

#include <justui/defs.h>
#include <justui/jwidget.h>

#include <gint/keyboard.h>

/* jevent: GUI event

   This type is mostly a union type that provides details on every event that
   occurs in the GUI. These are mostly widget signaling state changes,
   validations, and other GUI specifics that might require attention. Events
   can either be reported to the user by the scene (upwards event) or notify
   widgets of something occurring to them (downwards event).

   JustUI tries hard to not invert flow control and leave the user to decide
   when to produce downwards events. In a normal situation, events from
   getkey() are passed to the scene using jscene_process_key() while reading
   GUI events moving upwards with jscene_pollevent(). This way, the user can
   decide to filter their key events or even craft some.

   For the sake of convenience, a single function jscene_run() is provided that
   implements a common form of main loop, which forwards keyboard events to the
   scene and reports upwards GUI events and ignored key events.

   Event IDs can be registered with j_register_event() and (usually) exposed as
   global variables. Extensions are meaningful for custom widget types that
   need their own upwards events. */
typedef struct {
	/* Widget that emitted the event (if upwards), NULL otherwise */
	void *source;
	/* Type of event */
	uint16_t type;
	/* Reserved for future use */
	uint16_t _;

	/* Event details or data */
	union {
		/* JWIDGET_KEY events */
		key_event_t key;
	};

} jevent;

#endif /* _J_EVENT */
