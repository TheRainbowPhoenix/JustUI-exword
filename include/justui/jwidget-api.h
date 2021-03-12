//---
// JustUI.jwidget-API: API for subclassed widget types
//---

#ifndef _J_JWIDGET_API
#define _J_JWIDGET_API

#include <justui/defs.h>
#include <justui/jevent.h>
#include <gint/keyboard.h>

//---
// Polymorphic operations on widgets
//
// The following operations are widget-type-dependent. Derived widget types
// must provide their implementation through a jwidget_poly structure. The
// supplied functions will then be called at different points during the
// lifetime of the derived widgets.
//---

/* jwidget_poly_csize_t: Determine the natural content size of a widget

   This function is called during layout. It should set the natural size of the
   content box of the widget in (w->w) and (w->h). If the widget has a layout,
   this function will not be called; instead, the layout will determine the
   natural content size. Thus, this function is mostly useful for content
   widgets and not useful for containers.

   Implementations of this function should use jwidget_msize() on the children
   to position their margin box within the widget's content box. The size set
   by this function needs not be in the minimum/maximum range of the widget. */
typedef void jwidget_poly_csize_t(void *w);

/* jwidget_poly_layout_t: Layout a widget after its size has been set

   This function is called during the second phase of the layout process, if
   the widget has no layout. The margin-box size allocated to the widget has
   been set in (w->w) and (w->h); the widget must now position its contents and
   children. If the widget has a layout, the layout's specialized function is
   called instead of this one. */
typedef void jwidget_poly_layout_t(void *w);

/* jwidget_poly_render_t: Render a widget

   This function is called during rendering after the widget's geometry is
   drawn. (x,y) are the coordinates of the content box. This function must
   render widget-specific visuals; there is no clipping, so the widget should
   honor its width and height.

   This function should call jwidget_render() for all children that need to be
   rendered. jwidget_render() handles the geometry and takes as parameters the
   coordinates of the margin box, so it can be called as:

     jwidget_render(child, x + child->x, y + child->y).

   It will draw the geometry and call the polymorphic renderer of the child at
   its content-box coordinates. Normally you can ignore geometry altogether. */
typedef void jwidget_poly_render_t(void *w, int x, int y);

/* jwidget_poly_event_t: Handle an event

   This function is called when an event is targeted at a widget, including for
   key events. This function is somewhat of a catch-all function for dynamic
   occurrences. The widget should either accept the event, do something with
   it, and return true, or refuse the event, do nothing and return false. This
   is influences events that propagate, such as key events. */
typedef bool jwidget_poly_event_t(void *w, jevent e);

/* jwidget_poly_destroy_t: Destroy a widget's specific resources

   This function must destroy the widget-specific resources. It is called by
   jwidget_destroy(), which follows it by freeing the widget's standard data
   and destroying the children. This function can be NULL if there are no
   widget-specific resources to free. */
typedef void jwidget_poly_destroy_t(void *w);

/* jwidget_poly: Polymorphic interface for a widget type */
typedef struct {
	/* Type name, used for display and inheritance */
	char const *name;
	/* Polymorphic functions */
	jwidget_poly_csize_t   *csize;
	jwidget_poly_layout_t  *layout;
	jwidget_poly_render_t  *render;
	jwidget_poly_event_t   *event;
	jwidget_poly_destroy_t *destroy;

} jwidget_poly;

//---
// Widget registration
//---

/* j_register_widget(): Register a new widget type

   This function returns a new widget type ID to pass to jwidget_init() when
   creating widgets of the custom type. Returns -1 if registration fails. The
   polymorphic structure must outlive all widgets of the custom type.

   If (inherits) is non-NULL, the new type will inherit from the widget type
   with the provided name. All NULL functions in (poly) will be replaced by the
   parent type's functions. This mechanism only works if widgets are declared
   in inheritance order, which is normally enforced by constructor priority. */
int j_register_widget(jwidget_poly *poly, char const *inherits);

/* j_register_event(): Register a new event type

   This function returns a new ID to set in the (type) field of jevent objects.
   The ID is unique, and can be trusted to be always valid (unless you register
   more than 64k events in which case you asked for the trouble). */
int j_register_event(void);

//---
// Helpers to implement custom widget types
//---

/* jwidget_init(): Initialize a widget

   This function should be called in the constructor for the subclassed widget
   type, preferably as soon as possible. It initializes common widget
   attributes, sets the widget type, and declares the parent.

   A subclassed widget type must have a jwidget as a first member (so that the
   address of any instance of the subclassed widget is a valid pointer to
   jwidget), and that jwidget must be initialized with jwidget_init().

   @w       Widget to initialize
   @type    Type ID, as returned by j_register_widget()
   @parent  Parent, same as in jwidget_create() */
void jwidget_init(jwidget *w, int type, void *parent);

/* jwidget_msize(): Compute and apply the natural size of a widget's margin-box

   This function computes the widget's natural margin-box size. It determines
   the natural content size with the csize() function of either the layout or
   the widget type, then adds the geometry.

   The margin-box size is stored in the (w) and (h) attributes of the widget.
   This function should only be called during the first phase of the layout,
   to implement subclassed csize() functions. Usually, the parent will
   implement a customn csize() function by combining the position and msize()
   of its children. */
void jwidget_msize(void *w);

/* jwidget_emit(): Emit an upwards event from this widget

   This function walks up the tree until it finds a jscene that can store the
   event. If there is no jscene in the tree, the event is ignored. The (source)
   field can be omitted and will be set to the widget's address by default. */
void jwidget_emit(void *w, jevent e);

/* jwidget_event(): Send a downwards event to a widget

   This function calls the polymorphic event function of the targeted widget to
   notify it of the specified event. */
bool jwidget_event(void *w, jevent e);

#endif /* _J_JWIDGET_API */
