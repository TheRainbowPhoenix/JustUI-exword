#include <justui/jwidget.h>
#include <justui/jwidget-api.h>
#include <justui/jscene.h>

#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/drivers/keydev.h>
#include <gint/clock.h>
#include <gint/std/stdlib.h>
#include <gint/gint.h>
#include <gint/drivers/t6k11.h>
#include <gint/cpu.h>

/* Type identifier for jscene */
static int jscene_type_id = -1;
/* Events */
uint16_t JSCENE_NONE;
uint16_t JSCENE_PAINT;

/* Keyboard transformation for inputs in a jscene */
static int jscene_repeater(int key, GUNUSED int duration, int count)
{
	if(key != KEY_LEFT && key != KEY_RIGHT && key != KEY_UP && key != KEY_DOWN)
		return -1;

	return (count ? 40 : 400) * 1000;
}
static keydev_transform_t jscene_tr = {
	.enabled =
		KEYDEV_TR_DELAYED_SHIFT |
		KEYDEV_TR_INSTANT_SHIFT |
		KEYDEV_TR_DELAYED_ALPHA |
		KEYDEV_TR_INSTANT_ALPHA |
		KEYDEV_TR_REPEATS,
	.repeater = jscene_repeater,
};

jscene *jscene_create(int x, int y, int w, int h, void *parent)
{
	if(jscene_type_id < 0) return NULL;

	jscene *s = malloc(sizeof *s);
	if(!s) return NULL;

	jwidget_init(&s->widget, jscene_type_id, parent);
	jwidget_set_focus_policy(s, J_FOCUS_POLICY_SCOPE);
	jwidget_set_fixed_size(s, w, h);
	jlayout_set_vbox(s);

	/* The scene is where active focus originates */
	s->widget.focused = 1;
	s->widget.active_focused = 1;

	s->x = x;
	s->y = y;

	s->queue_first = 0;
	s->queue_next = 0;
	s->lost_events = 0;
	s->mainmenu = true;
	s->poweroff = true;
	s->autopaint = false;

	/* Prepare first layout/paint operation */
	s->widget.dirty = 1;

	return s;
}

jscene *jscene_create_fullscreen(void *parent)
{
	return jscene_create(0, 0, DWIDTH, DHEIGHT, parent);
}

jscene *jscene_owning(void *w0)
{
	J_CAST(w)

	while(w) {
		if(w->type == jscene_type_id) return (void *)w;
		w = w->parent;
	}
	return NULL;
}

void jscene_render(jscene *scene)
{
	jwidget_layout(scene);
	jwidget_render(scene, scene->x, scene->y);
}

//---
// Event management
//---

jevent jscene_read_event(jscene *s)
{
	cpu_atomic_start();

	if(s->queue_first == s->queue_next) {
		cpu_atomic_end();
		return (jevent){ .source = NULL, .type = JSCENE_NONE };
	}

	jevent e = s->queue[s->queue_first];
	s->queue_first = (s->queue_first + 1) % JSCENE_QUEUE_SIZE;
	cpu_atomic_end();
	return e;
}

void jscene_queue_event(jscene *s, jevent e)
{
	cpu_atomic_start();

	/* Prevent filling and overflowing the queue */
	int next = (s->queue_next + 1) % JSCENE_QUEUE_SIZE;
	if(next == s->queue_first) {
		s->lost_events++;
	}
	else {
		s->queue[s->queue_next] = e;
		s->queue_next = next;
	}

	cpu_atomic_end();
}

//---
// Keyboard focus and keyboard events
//---

void *jscene_focused_widget(jscene *s)
{
	jwidget *w = &s->widget;
	while(w->focus_scope_target)
		w = w->focus_scope_target;
	return w;
}

void jscene_set_focused_widget(jscene *s, void *w0)
{
	J_CAST(w)

	/* Unfocus only at the top level */
	if(!w) {
		jwidget_scope_set_target(s, NULL);
		return;
	}

	if(w->focus_policy == J_FOCUS_POLICY_REJECT)
		return;

	/* Check that (s) is an ancestor of (w) */
	for(jwidget *anc = w; anc != (jwidget *)s; anc = anc->parent) {
		if(anc == NULL) return;
	}

	/* Set targets in every scope along the way up */
	jwidget *scope = w;
	while(scope != NULL) {
		scope = jwidgetctx_enclosing_focus_scope(scope);
		jwidget_scope_set_target(scope, w);
		w = scope;
	}
}

void jscene_show_and_focus(jscene *scene, void *w0)
{
	J_CAST(w)

	/* Ensure the widget is visible */
	jwidget_set_visible(w, true);

	/* Force stacked layouts all the wat up to [scene] to show [w] */
	jwidget *current = w;
	jwidget *parent = w->parent;

	while(parent != (jwidget *)scene) {
		jlayout_stack *stack = jlayout_get_stack(parent);
		if(stack) {
			int pos = jwidget_child_position(parent, current);
			if(stack->active != pos) {
				stack->active = pos;
				parent->update = true;
			}
		}

		current = parent;
		parent = current->parent;
	}

	/* Give the widget focus */
	jscene_set_focused_widget(scene, w);
}

bool jscene_process_key_event(jscene *scene, key_event_t event)
{
	jwidget *candidate = jscene_focused_widget(scene);
	jevent e = { .type = JWIDGET_KEY, .key = event };

	while(candidate) {
		if(jwidget_event(candidate, e))
			return true;
		candidate = jwidgetctx_enclosing_focus_scope(candidate);
	}

	return false;
}

bool jscene_process_event(GUNUSED jscene *scene, jevent event)
{
	if(!event.source) return false;
	jwidget *candidate = ((jwidget *)event.source)->parent;

	while(candidate) {
		if(jwidget_event(candidate, event)) return true;
		candidate = candidate->parent;
	}

	return false;
}

void jscene_set_mainmenu(jscene *scene, bool mainmenu)
{
	scene->mainmenu = mainmenu;
}

void jscene_set_poweroff(jscene *scene, bool poweroff)
{
	scene->poweroff = poweroff;
}

void jscene_set_autopaint(jscene *scene, bool autopaint)
{
	scene->autopaint = autopaint;
}

jevent jscene_run(jscene *s)
{
	keydev_t *d = keydev_std();
	keydev_transform_t tr0 = keydev_transform(d);
	keydev_set_transform(d, jscene_tr);

	jevent e;
	while(1) {
		/* Create repaint events (also handle relayout if needed) */
		if(jwidget_layout_dirty(s) || jwidget_needs_update(s)) {
			jscene_queue_event(s, (jevent){ .type = JSCENE_PAINT });
		}

		/* Queued GUI events */
		e = jscene_read_event(s);
		if(e.type == JSCENE_PAINT && s->autopaint) {
			jscene_render(s);
			dupdate();
			continue;
		}
		if(e.type != JSCENE_NONE && !jscene_process_event(s, e)) break;

		/* Queued keyboard events */
		key_event_t k = keydev_read(d, false, NULL);

		if(k.type == KEYEV_DOWN && k.key == KEY_MENU && !k.shift && !k.alpha) {
			if(s->mainmenu) {
				gint_osmenu();
				jscene_queue_event(s, (jevent){ .type = JSCENE_PAINT });
				continue;
			}
		}
		if(k.type == KEYEV_DOWN && k.key == KEY_ACON && k.shift && !k.alpha) {
			if(s->poweroff) {
				gint_poweroff(true);
				jscene_queue_event(s, (jevent){ .type = JSCENE_PAINT });
				continue;
			}
		}
		// TODO: Temporarily disabled to allow build-fxg3a, will put back when
		// gint's generic video interface is available
/*		#ifdef FX9860G
		if(k.type == KEYEV_DOWN && k.key == KEY_OPTN && k.shift && !k.alpha) {
			t6k11_backlight(-1);
			continue;
		}
		#endif */

		getkey_feature_t feat = getkey_feature_function();
		if((k.type == KEYEV_DOWN || k.type == KEYEV_HOLD) && feat && feat(k))
			continue;

		if(k.type != KEYEV_NONE && !jscene_process_key_event(s, k)) {
			e.type = JWIDGET_KEY;
			e.key = k;
			break;
		}

		// TODO: Should only sleep when out of events!
		sleep();
	}

	keydev_set_transform(d, tr0);
	return e;
}

/* jscene type definition */
static jwidget_poly type_jscene = {
	.name    = "jscene",
	.csize   = NULL,
	.layout  = NULL,
	.render  = NULL,
	.event   = NULL,
	.destroy = NULL,
};

__attribute__((constructor))
static void j_register_jscene(void)
{
	jscene_type_id = j_register_widget(&type_jscene);
	JSCENE_NONE = j_register_event();
	JSCENE_PAINT = j_register_event();
}
