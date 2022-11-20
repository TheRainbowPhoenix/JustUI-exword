//---
// JustUI.jframe: Scrolling frame holding a widget
//---

#ifndef _J_JFRAME
#define _J_JFRAME

#include <justui/defs.h>
#include <justui/jwidget.h>

/* jframe: Scrolling frame holding a widget

   This widget is used to implement scrolling widgets. It has a single child,
   which is displayed fully if it's smaller than the frame, or partially (with
   scrollbars) otherwise.

   The child widget has horizontal and vertical alignments, which specify its
   position within the frame when smaller than the frame. Its position when
   larger than the frame is determined by the scrolling offsets, which can be
   manipulated manually or left for the frame to control with arrow keys.

   Scrollbars can be set to either render on top of the framed widget, or
   occupy dedicated space. */
typedef struct {
    jwidget widget;

    /* Horizontal and vertical alignment for the child */
    jalign halign, valign;
    /* Force scrollbars even if the child is smaller than the frame */
    bool scrollbars_always_visible;
    /* Scrollbars render on top of the child widget */
    bool floating_scrollbars;
    /* Scrolling can be handled by the frame itself, with arrow keys */
    bool keyboard_control;
    /* Force matching the width and/or height of the child widget */
    bool match_width, match_height;
    /* Scrollbar width, in pixels */
    uint8_t scrollbar_width;
    /* If floating_scrollbars == false, spacing between scrollbars and child */
    uint8_t scrollbar_spacing;

    /* Whether scrollbars are shown */
    bool scrollbar_x, scrollbar_y;
    /* Current scroll offsets */
    int16_t scroll_x, scroll_y;
    /* Maximum scroll offsets for the current size of the child widget */
    int16_t max_scroll_x, max_scroll_y;

} jframe;

/* jframe_create(): Create a new frame

   The frame's inner widget is always its first child. It can be specified by
   jwidget_set_parent() or by creating the child with the frame as a parent
   directy. More children can be added, but they will not be rendered. */
jframe *jframe_create(void *parent);

/* Trivial properties */
void jframe_set_align(jframe *j, jalign halign, jalign valign);
void jframe_set_scrollbars_always_visible(jframe *j, bool always_visible);
void jframe_set_floating_scrollbars(jframe *j, bool floating_scrollbars);
void jframe_set_keyboard_control(jframe *j, bool keyboard_control);
void jframe_set_match_size(jframe *j, bool match_width, bool match_height);

#endif /* _J_JFRAME */
