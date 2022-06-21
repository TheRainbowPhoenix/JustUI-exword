#include <justui/jwidget.h>
#include <justui/jwidget-api.h>
#include <justui/jscene.h>
#include <justui/jevent.h>
#include "jlayout_p.h"
#include "util.h"

#include <gint/display.h>
#include <gint/std/stdlib.h>
#include <gint/std/string.h>

#define WIDGET_TYPES_MAX 32

/* Polymorphic functions for jwidget */
static jwidget_poly_render_t jwidget_poly_render;
static jwidget_poly_csize_t  jwidget_poly_csize;

/* jwidget type definition */
static jwidget_poly type_jwidget = {
	.name    = "jwidget",
	.csize   = jwidget_poly_csize,
	.layout  = NULL,
	.render  = jwidget_poly_render,
	.event   = NULL,
	.destroy = NULL,
};

/* List of registered widget types */
static jwidget_poly *widget_types[WIDGET_TYPES_MAX] = {
	&type_jwidget,
	NULL,
};

/* Events */
uint16_t JWIDGET_KEY;
uint16_t JWIDGET_FOCUS_IN;
uint16_t JWIDGET_FOCUS_OUT;

//---
// Polymorphic functions for widgets
//---

static void jwidget_poly_render(void *w0, int x, int y)
{
	J_CAST(w)
	jlayout_stack *l;

	/* If there is a stack layout, render active child */
	if((l = jlayout_get_stack(w)) && l->active >= 0) {
		jwidget *child = w->children[l->active];
		if(child->visible && !child->floating)
			jwidget_render(child, x+child->x, y+child->y);
	}
	/* Otherwise, simply render all non-floating children */
	else for(int k = 0; k < w->child_count; k++) {
		jwidget *child = w->children[k];
		if(child->visible)// && !child->floating)
			jwidget_render(child, x+child->x, y+child->y);
	}

	/* Render floating widgets over the layout */
	for(int k = 0; k < w->child_count; k++) {
		jwidget *child = w->children[k];
		if(child->visible && child->floating)
			jwidget_render(child, x+child->x, y+child->y);
	}
}

static void jwidget_poly_csize(void *w0)
{
	J_CAST(w)

	/* The content box of this children is the union of the border boxes of the
	   children. This function is called only for widgets without a layout, in
	   which case the children must have their positions set. */
	w->w = 0;
	w->h = 0;

	for(int k = 0; k < w->child_count; k++) {
		jwidget *child = w->children[k];
		if(!child->visible) continue;
		jwidget_msize(child);

		w->w = max(w->w, child->x + child->w);
		w->h = max(w->h, child->y + child->h);
	}
}

//---
// Initialization
//---

jwidget *jwidget_create(void *parent)
{
	jwidget *w = malloc(sizeof *w);
	if(!w) return NULL;

	/* Type ID 0 is for jwidget */
	jwidget_init(w, 0, parent);
	return w;
}

void jwidget_init(jwidget *w, int type, void *parent)
{
	w->parent = NULL;
	w->children = NULL;
	w->child_count = 0;
	w->child_alloc = 0;
	w->dirty = 1;
	w->visible = 1;
	w->floating = 0;

	w->type = type;
	w->geometry = NULL;

	w->x = 0;
	w->y = 0;
	w->w = 0;
	w->h = 0;
	w->min_w = 0;
	w->min_h = 0;
	w->max_w = 0x7fff;
	w->max_h = 0x7fff;

	w->layout = J_LAYOUT_NONE;
	w->stretch_x = 0;
	w->stretch_y = 0;
	w->stretch_force = 0;

	jwidget_set_parent(w, parent);
}

void jwidget_destroy(void *w0)
{
	J_CAST(w)
	jwidget_set_parent(w, NULL);

	/* Run the custom destructor */
	jwidget_poly const *poly = widget_types[w->type];
	if(poly->destroy) poly->destroy(w);

	for(int i = 0; i < w->child_count; i++) {
		/* This will prevent us from unregistering the child from w in the
		   recursive call, which saves some time in pointer manipulation */
		w->children[i]->parent = NULL;
		jwidget_destroy(w->children[i]);
	}

	if(w->children) free(w->children);
	if(w->geometry) free(w->geometry);
	free(w);
}

//---
// Ownership
//---

void jwidget_set_parent(void *w0, void *parent0)
{
	J_CAST(w, parent)
	if(w->parent == parent) return;

	if(w->parent != NULL)
		jwidget_remove_child(w->parent, w);
	if(parent != NULL)
		jwidget_add_child(parent, w);
}

void jwidget_add_child(void *w0, void *child0)
{
	J_CAST(w, child)
	jwidget_insert_child(w, child, w->child_count);
}

void jwidget_insert_child(void *w0, void *child0, int position)
{
	J_CAST(w, child)
	if(child->parent == w) return;
	if(position < 0 || position > w->child_count) return;

	/* Don't overflow the child_count and child_alloc fields! */
	if(w->child_count >= 256 - 4) return;

	/* Make room for a new child if needed */
	if(w->child_count + 1 > w->child_alloc) {
		size_t new_size = (w->child_alloc + 4) * sizeof(jwidget *);
		jwidget **new_children = realloc(w->children, new_size);
		if(!new_children) return;

		w->children = new_children;
		w->child_alloc += 4;
	}

	/* Insert new child */
	for(int k = w->child_count; k > position; k--)
		w->children[k] = w->children[k - 1];

	w->children[position] = child;
	w->child_count++;

	/* Remove the existing parent at the last moment, in order to keep the tree
	   in a safe state if allocations fail or parameters are invalid */
	if(child->parent != NULL)
		jwidget_remove_child(child->parent, child);

	child->parent = w;

	/* Update stack layouts to keep showing the same child */
	if(w->layout == J_LAYOUT_STACK && w->layout_stack.active >= position) {
		w->layout_stack.active++;
	}
	/* Update stack layout to show the first child if none was visible yet */
	if(w->layout == J_LAYOUT_STACK && w->layout_stack.active == -1) {
		w->layout_stack.active = 0;
	}

	/* Force a later recomputation of the layout */
	w->dirty = 1;
}

void jwidget_remove_child(void *w0, void *child0)
{
	J_CAST(w, child)
	if(child->parent != w) return;

	int write = 0;
	int index = -1;

	/* Remove all occurrences of (child) from (w->children) */
	for(int read = 0; read < w->child_count; read++) {
		if(w->children[read] != child) {
			w->children[write++] = w->children[read];
		}
		else if(index < 0) {
			index = read;
		}
	}
	w->child_count = write;

	/* Remove the parent from the child */
	child->parent = NULL;

	/* Update stack layouts to not show the removed child */
	if(w->layout == J_LAYOUT_STACK && w->layout_stack.active == index) {
		w->layout_stack.active = -1;
	}

	/* Force a later recomputation of the layout */
	w->dirty = 1;
}

int jwidget_child_position(void *w0, void *child0)
{
	J_CAST(w, child)

	for(int i = 0; i < w->child_count; i++) {
		if(w->children[i] == child)
			return i;
	}

	return -1;
}

//---
// Sizing and stretching
//---

void jwidget_set_minimum_width(void *w0, int min_width)
{
	J_CAST(w)
	w->min_w = clamp(min_width, 0, 0x7fff);
	w->dirty = 1;
}

void jwidget_set_minimum_height(void *w0, int min_height)
{
	J_CAST(w)
	w->min_h = clamp(min_height, 0, 0x7fff);
	w->dirty = 1;
}

void jwidget_set_minimum_size(void *w, int min_width, int min_height)
{
	jwidget_set_minimum_width(w, min_width);
	jwidget_set_minimum_height(w, min_height);
}

void jwidget_set_maximum_width(void *w0, int max_width)
{
	J_CAST(w)
	w->max_w = clamp(max_width, 0, 0x7fff);
	w->dirty = 1;
}

void jwidget_set_maximum_height(void *w0, int max_height)
{
	J_CAST(w)
	w->max_h = clamp(max_height, 0, 0x7fff);
	w->dirty = 1;
}

void jwidget_set_maximum_size(void *w, int max_width, int max_height)
{
	jwidget_set_maximum_width(w, max_width);
	jwidget_set_maximum_height(w, max_height);
}

void jwidget_set_fixed_width(void *w, int width)
{
	jwidget_set_minimum_width(w, width);
	jwidget_set_maximum_width(w, width);
}

void jwidget_set_fixed_height(void *w, int height)
{
	jwidget_set_minimum_height(w, height);
	jwidget_set_maximum_height(w, height);
}

void jwidget_set_fixed_size(void *w, int width, int height)
{
	jwidget_set_minimum_size(w, width, height);
	jwidget_set_maximum_size(w, width, height);
}

void jwidget_set_stretch(void *w0, int stretch_x, int stretch_y, bool force)
{
	J_CAST(w)
	w->stretch_x = clamp(stretch_x, 0, 15);
	w->stretch_y = clamp(stretch_y, 0, 15);
	w->stretch_force = force;
	w->dirty = 1;
}

//---
// Geometry
//---

static jwidget_geometry default_geometry = {
	.margins  = { 0, 0, 0, 0 },
	.borders  = { 0, 0, 0, 0 },
	.paddings = { 0, 0, 0, 0 },
	.border_color = C_NONE,
	.border_style = J_BORDER_NONE,
	.background_color = C_NONE,
};

jwidget_geometry const *jwidget_geometry_r(void *w0)
{
	J_CAST(w)
	return (w->geometry == NULL) ? &default_geometry : w->geometry;
}

jwidget_geometry *jwidget_geometry_rw(void *w0)
{
	J_CAST(w)

	/* Duplicate default geometry as a copy-on-write tactic to save memory */
	if(w->geometry == NULL) {
		w->geometry = malloc(sizeof *w->geometry);
		if(!w->geometry) return NULL;
		*w->geometry = default_geometry;
	}

	/* Assume layout will need to be recomputed */
	w->dirty = 1;

	return w->geometry;
}

void jwidget_set_border(void *w, jwidget_border_style s, int width, int color)
{
	jwidget_geometry *g = jwidget_geometry_rw(w);

	g->border_style = s;
	g->border_color = color;
	for(int i = 0; i < 4; i++) g->borders[i] = width;
}

void jwidget_set_padding(void *w, int top, int right, int bottom, int left)
{
	jwidget_geometry *g = jwidget_geometry_rw(w);
	g->padding.top    = top;
	g->padding.right  = right;
	g->padding.bottom = bottom;
	g->padding.left   = left;
}

void jwidget_set_margin(void *w, int top, int right, int bottom, int left)
{
	jwidget_geometry *g = jwidget_geometry_rw(w);
	g->margin.top    = top;
	g->margin.right  = right;
	g->margin.bottom = bottom;
	g->margin.left   = left;
}

void jwidget_set_background(void *w, int color)
{
	jwidget_geometry *g = jwidget_geometry_rw(w);
	g->background_color = color;
}

//---
// Layout
//---

void jlayout_set_manual(void *w0)
{
	J_CAST(w)
	w->layout = J_LAYOUT_NONE;
	w->dirty = 1;
}

void jwidget_msize(void *w0)
{
	J_CAST(w)
	int t = w->layout;

	/* Size of contents */
	if(t == J_LAYOUT_NONE) {
		jwidget_poly const *poly = widget_types[w->type];
		if(poly->csize) poly->csize(w);
	}

	else if(t == J_LAYOUT_HBOX || t == J_LAYOUT_VBOX)
		jlayout_box_csize(w);

	else if(t == J_LAYOUT_STACK)
		jlayout_stack_csize(w);

	else if(t == J_LAYOUT_GRID)
		jlayout_grid_csize(w);

	/* Add the size of the geometry */
	jwidget_geometry const *g = jwidget_geometry_r(w);

	w->w += g->margin.left   + g->border.left   + g->padding.left;
	w->w += g->margin.right  + g->border.right  + g->padding.right;
	w->h += g->margin.top    + g->border.top    + g->padding.top;
	w->h += g->margin.bottom + g->border.bottom + g->padding.bottom;

	w->dirty = 1;
}

bool jwidget_layout_dirty(void *w0)
{
	J_CAST(w)
	if(w->dirty) return true;

	for(int k = 0; k < w->child_count; k++) {
		if(!w->children[k]->visible) continue;
		if(jwidget_layout_dirty(w->children[k])) return true;
	}

	return false;
}

bool jwidget_visible(void *w0)
{
	J_CAST(w)
	return w->visible;
}

void jwidget_set_visible(void *w0, bool visible)
{
	J_CAST(w)
	if(w->visible == visible) return;

	w->visible = (visible != 0);
	if(w->parent) w->parent->dirty = 1;
}

bool jwidget_needs_update(void *w0)
{
	J_CAST(w)
	if(w->update) return true;

	jlayout_stack *l = jlayout_get_stack(w);

	/* Ignore invisible children, because their update bits are not reset after
	   a repaint, resulting in infinite REPAINT events */
	for(int k = 0; k < w->child_count; k++) {
		if(l && l->active != k) continue;
		if(!w->children[k]->visible) continue;
		if(jwidget_needs_update(w->children[k])) return true;
	}

	return false;
}

/* Apply layout recursively on this widget and its children */
static void jwidget_layout_apply(void *w0)
{
	J_CAST(w)
	if(!w->visible) return;
	int t = w->layout;

	if(t == J_LAYOUT_NONE) {
		jwidget_poly const *poly = widget_types[w->type];
		if(poly->layout) poly->layout(w);
	}
	else if(t == J_LAYOUT_HBOX || t == J_LAYOUT_VBOX) {
		jlayout_box_apply(w);
	}
	else if(t == J_LAYOUT_STACK) {
		jlayout_stack_apply(w);
	}
	else if(t == J_LAYOUT_GRID) {
		jlayout_grid_apply(w);
	}

	/* The layout is now up-to-date and will not be recomputed until a widget
	   in the hierarchy requires it */
	w->dirty = 0;

	for(int k = 0; k < w->child_count; k++)
		jwidget_layout_apply(w->children[k]);
}

void jwidget_layout(void *root0)
{
	J_CAST(root)
	if(!jwidget_layout_dirty(root)) return;

	/* Phase 1: Compute the natural margin size of every widget (bottom-up) */
	jwidget_msize(root);

	/* Decide on the size of the root; first make sure it's acceptable */
	root->w = clamp(root->w, root->min_w, root->max_w);
	root->h = clamp(root->h, root->min_h, root->max_h);
	/* Now if there is stretch and a maximum size, stretch */
	if(root->stretch_x && root->max_w != 0x7fff)
		root->w = root->max_w;
	if(root->stretch_y && root->max_h != 0x7fff)
		root->h = root->max_h;

	/* Phase 2: Distribute space recursively (top-down) */
	jwidget_layout_apply(root);
}

int jwidget_content_width(void *w0)
{
	J_CAST(w)
	jwidget_geometry const *g = jwidget_geometry_r(w);

	return w->w - g->margin.left  - g->border.left  - g->padding.left
	            - g->margin.right - g->border.right - g->padding.right;
}

int jwidget_content_height(void *w0)
{
	J_CAST(w)
	jwidget_geometry const *g = jwidget_geometry_r(w);

	return w->h - g->margin.top    - g->border.top    - g->padding.top
	            - g->margin.bottom - g->border.bottom - g->padding.bottom;
}

int jwidget_full_width(void *w0)
{
	J_CAST(w)
	return w->w;
}

int jwidget_full_height(void *w0)
{
	J_CAST(w)
	return w->h;
}

bool jwidget_floating(void *w0)
{
	J_CAST(w)
	return w->floating;
}

void jwidget_set_floating(void *w0, bool floating)
{
	J_CAST(w)
	if(w->floating == floating) return;

	w->floating = (floating != 0);
	if(w->parent) w->parent->dirty = 1;
}

//---
// Rendering
//---

void jwidget_render(void *w0, int x, int y)
{
	J_CAST(w)
	if(!w->visible) return;

	/* Render widget border */
	jwidget_geometry const *g = jwidget_geometry_r(w);
	jdirs b = g->border;
	int color = g->border_color;

	int cw = jwidget_content_width(w);
	int ch = jwidget_content_height(w);

	int x1 = x + g->margin.left;
	int y1 = y + g->margin.top;
	int x2 = x1 + b.left + g->padding.left + cw + g->padding.right;
	int y2 = y1 + b.top  + g->padding.top  + ch + g->padding.bottom;

	if(g->border_style == J_BORDER_NONE || color == C_NONE) {
	}
	else if(g->border_style == J_BORDER_SOLID) {
		if(b.top > 0)
			drect(x1, y1, x2 + b.right - 1, y1 + b.top - 1, color);
		if(b.bottom > 0)
			drect(x1, y2, x2 + b.right - 1, y2 + b.bottom - 1, color);

		if(b.left > 0)
			drect(x1, y1 + b.top, x1 + b.left - 1, y2 - 1, color);
		if(b.right > 0)
			drect(x2, y1 + b.top, x2 + b.right - 1, y2 - 1, color);
	}
	/* TODO: jwidget_render(): More border types */

	if(g->background_color != C_NONE) {
		drect(x1 + b.left, y1 + b.top, x2-1, y2-1, g->background_color);
	}

	/* Call the polymorphic render function at the top-left content point */
	x += g->margin.left + b.left + g->padding.left;
	y += g->margin.top  + b.top  + g->padding.top;

	jwidget_poly const *poly = widget_types[w->type];
	if(poly->render) poly->render(w, x, y);

	w->update = 0;
}

//---
// Event management
//---

bool jwidget_event(void *w0, jevent e)
{
	J_CAST(w)
	jwidget_poly const *poly = widget_types[w->type];
	if(poly->event) return poly->event(w, e);
	return false;
}

void jwidget_emit(void *w0, jevent e)
{
	J_CAST(w)
	if(!w) return;

	if(e.source == NULL) e.source = w;

	if(!strcmp(jwidget_type(w), "jscene")) {
		jscene_queue_event((jscene *)w, e);
	}
	else {
		jwidget_emit(w->parent, e);
	}
}

//---
// Extension API
//---

char const *jwidget_type(void *w0)
{
	J_CAST(w)
	return widget_types[w->type]->name;
}

int j_register_widget(jwidget_poly *poly, char const *inherits)
{
	/* Resolve inheritance */
	if(inherits) {
		jwidget_poly const *base = NULL;

		for(int i = 0; i < WIDGET_TYPES_MAX && !base; i++) {
			if(widget_types[i] && !strcmp(widget_types[i]->name, inherits))
				base = widget_types[i];
		}

		if(!base) return -1;

		if(!poly->csize)   poly->csize   = base->csize;
		if(!poly->layout)  poly->layout  = base->layout;
		if(!poly->render)  poly->render  = base->render;
		if(!poly->event)   poly->event   = base->event;
		if(!poly->destroy) poly->destroy = base->destroy;
	}

	for(int i = 0; i < WIDGET_TYPES_MAX; i++) {
		if(widget_types[i] == NULL) {
			widget_types[i] = poly;
			return i;
		}
	}
	return -1;
}

int j_register_event(void)
{
	static int event_id = 0;
	event_id++;
	return event_id;
}

__attribute__((constructor(1000)))
static void j_register_jwidget(void)
{
	JWIDGET_KEY = j_register_event();
	JWIDGET_FOCUS_IN = j_register_event();
	JWIDGET_FOCUS_OUT = j_register_event();
}
