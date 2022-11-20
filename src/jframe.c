#include <justui/jwidget.h>
#include <justui/jwidget-api.h>
#include <justui/jframe.h>
#include "util.h"

#include <gint/display.h>

#include <stdlib.h>

/* Type identifier for jframe */
static int jframe_type_id = -1;

jframe *jframe_create(void *parent)
{
    if(jframe_type_id < 0)
        return NULL;


    jframe *f = malloc(sizeof *f);
    if(!f)
        return NULL;

    jwidget_init(&f->widget, jframe_type_id, parent);
    jwidget_set_clipped(f, true);

    f->halign = J_ALIGN_CENTER;
    f->valign = J_ALIGN_MIDDLE;
    f->scrollbars_always_visible = false;
    f->floating_scrollbars = false;
    f->keyboard_control = false;
    f->match_width = false;
    f->match_height = false;

#ifdef FX9860G
    f->scrollbar_width = 1;
    f->scrollbar_spacing = 1;
#else
    f->scrollbar_width = 2;
    f->scrollbar_spacing = 2;
#endif

    f->scroll_x = 0;
    f->scroll_y = 0;
    f->max_scroll_x = 0;
    f->max_scroll_y = 0;

    return f;
}

static jwidget *frame_child(jframe *j)
{
    if(j->widget.child_count == 0)
        return NULL;
    return j->widget.children[0];
}

static int frame_scrollbar_space_size(jframe *f)
{
    if(f->floating_scrollbars)
        return 0;
    return f->scrollbar_width + f->scrollbar_spacing;
}

static void shake(jframe *f)
{
    f->scroll_x = clamp(f->scroll_x, 0, f->max_scroll_x);
    f->scroll_y = clamp(f->scroll_y, 0, f->max_scroll_y);
}

/* Start position of a block of size B, aligned as specified, inside a space of
   size S; returned as a value in [0..s). */
static int aligned_start_within(int B, int S, jalign align)
{
    if(align == J_ALIGN_LEFT || align == J_ALIGN_TOP)
        return 0;
    else if(align == J_ALIGN_RIGHT || align == J_ALIGN_BOTTOM)
        return S - B;
    else
        return (S - B) / 2;
}

//---
// Getters and setters
//---

void jframe_set_align(jframe *j, jalign halign, jalign valign)
{
    if(j->halign == halign && j->valign == valign)
        return;

    j->halign = halign;
    j->valign = valign;
    j->widget.update = 1;
}

void jframe_set_scrollbars_always_visible(jframe *j, bool always_visible)
{
    if(j->scrollbars_always_visible == always_visible)
        return;

    j->scrollbars_always_visible = always_visible;
    j->widget.dirty = 1;
}

void jframe_set_floating_scrollbars(jframe *j, bool floating_scrollbars)
{
    if(j->floating_scrollbars == floating_scrollbars)
        return;

    j->floating_scrollbars = floating_scrollbars;
    j->widget.dirty = 1;
}

void jframe_set_keyboard_control(jframe *j, bool keyboard_control)
{
    j->keyboard_control = keyboard_control;
}

//---
// Polymorphic widget operations
//---

static void jframe_poly_csize(void *f0)
{
    jframe *f = f0;
    jwidget *w = &f->widget;
    jwidget *child = frame_child(f);

    w->w = w->h = 16;

    if(child) {
        jwidget_msize(child);
        if(f->match_width)
            w->w = child->w;
        if(f->match_height)
            w->h = child->h;
    }

    int frame_sss = frame_scrollbar_space_size(f);
    if(!f->floating_scrollbars) {
        w->w += frame_sss;
        w->h += frame_sss;
    }
    jwidget_set_minimum_size(f, frame_sss + 4, frame_sss + 4);
}

static void jframe_poly_layout(void *f0)
{
    jframe *f = f0;
    jwidget *child = frame_child(f);

    if(!child) {
        f->scrollbar_x = f->scrollbar_y = f->scrollbars_always_visible;
        f->scroll_x = f->scroll_y = 0;
        f->max_scroll_x = f->max_scroll_y = 0;
        return;
    }

    int child_w = jwidget_full_width(child);
    int child_h = jwidget_full_height(child);

    int frame_w = jwidget_content_width(f);
    int frame_h = jwidget_content_height(f);
    int frame_sss = frame_scrollbar_space_size(f);
    int sss_x = 0;
    int sss_y = 0;

    /* We enable scrollbars if:
       (1) They were forced in; or
       (2) The child widget wouldn't fit without them.

       Scrollbars are linked; adding a scrollbar for one direction can reduce
       the space available in the other direction, thus causing the other
       scrollbar to appear. Hence, we need to iterate. */

    f->scrollbar_y = f->scrollbars_always_visible || child_h + sss_y > frame_h;
    if(f->scrollbar_y)
        sss_x = frame_sss;

    f->scrollbar_x = f->scrollbars_always_visible || child_w + sss_x > frame_w;
    if(f->scrollbar_x)
        sss_y = frame_sss;

    f->scrollbar_y = f->scrollbars_always_visible || child_h + sss_y > frame_h;
    if(f->scrollbar_y)
        sss_x = frame_sss;

    /* At this stage we have a fixpoint, because:
       - x is up-to-date. x can only be outdated if the 2nd y check just
         enabled scrollbar_y. But it can only do so if the x check enabled
         scrollbar_x, in which case scrollbar_x is already a stable true.
       - y is up-to-date since it was re-checked after x's last update. */

    f->max_scroll_x = max(0, child_w - (frame_w - sss_x));
    f->max_scroll_y = max(0, child_h - (frame_h - sss_y));
    shake(f);

    /* We can now set the inner widget's dimensions. The frame acts as a
       container, and thus sets the child's size, applying strech etc. One
       unique trait of the frame is that the child *always* gets its desired
       size since we can scroll it. */
    if(child->stretch_x > 0)
        child->w = max(child->w, min(frame_w, child->max_w));
    if(child->stretch_y > 0)
        child->h = max(child->h, min(frame_h, child->max_h));
}

static void jframe_poly_render(void *f0, int x, int y)
{
    jframe *f = f0;
    jwidget *child = frame_child(f);

    int child_w = jwidget_full_width(child);
    int child_h = jwidget_full_height(child);

    int frame_w = jwidget_content_width(f);
    int frame_h = jwidget_content_height(f);
    int frame_sss = frame_scrollbar_space_size(f);
    int sss_x = f->scrollbar_y ? frame_sss : 0;
    int sss_y = f->scrollbar_x ? frame_sss : 0;

    /* In each dimension:
       - If there is scrolling, we place according to the scroll offset;
       - Otherwise, we place according to alignment settings. */

    int render_x;
    if(f->scrollbar_x)
        render_x = x - f->scroll_x;
    else
        render_x = x + aligned_start_within(child_w, frame_w-sss_x, f->halign);

    int render_y;
    if(f->scrollbar_y)
        render_y = y - f->scroll_y;
    else
        render_y = y + aligned_start_within(child_h, frame_h-sss_y, f->valign);

    /* Render the child with dedicated clipping. */

    if(child) {
        struct dwindow win = {x, y, x + frame_w - sss_x, y + frame_h - sss_y};
        win = intersect_dwindow(win, dwindow);
        struct dwindow old_window = dwindow_set(win);
        jwidget_render(child, render_x, render_y);
        dwindow_set(old_window);
    }

    /* Render the scrollbars. */

    if(f->scrollbar_x) {
        int sb_x = x;
        int sb_y = y + frame_h - f->scrollbar_width;

        int sb_left = f->scroll_x * frame_w / child_w;
        int sb_width = frame_w * frame_w / child_w;

        drect(sb_x + sb_left, sb_y, sb_x + sb_left + sb_width - 1,
            sb_y + f->scrollbar_width - 1, C_BLACK);
    }
    if(f->scrollbar_y) {
        int sb_x = x + frame_w - f->scrollbar_width;
        int sb_y = y;

        int sb_top = f->scroll_y * frame_h / child_h;
        int sb_height = frame_h * frame_h / child_h;

        drect(sb_x, sb_y + sb_top, sb_x + f->scrollbar_width - 1,
            sb_y + sb_top + sb_height - 1, C_BLACK);
    }
}

static bool jframe_poly_event(void *f0, jevent e)
{
    jframe *f = f0;

    if(!f->keyboard_control || e.type != JWIDGET_KEY)
        return false;

    key_event_t ev = e.key;
    if(ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
        return false;

    int key = ev.key;
    if(key == KEY_LEFT && f->scrollbar_x && f->scroll_x > 0) {
        f->scroll_x--;
        f->widget.update = 1;
        return true;
    }
    if(key == KEY_RIGHT && f->scrollbar_x && f->scroll_x < f->max_scroll_x-1) {
        f->scroll_x++;
        f->widget.update = 1;
        return true;
    }
    if(key == KEY_UP && f->scrollbar_y && f->scroll_y > 0) {
        f->scroll_y--;
        f->widget.update = 1;
        return true;
    }
    if(key == KEY_DOWN && f->scrollbar_y && f->scroll_y < f->max_scroll_y-1) {
        f->scroll_y++;
        f->widget.update = 1;
        return true;
    }

    return false;
}

/* jframe type definition */
static jwidget_poly type_jframe = {
    .name   = "jframe",
    .csize  = jframe_poly_csize,
    .layout = jframe_poly_layout,
    .render = jframe_poly_render,
    .event  = jframe_poly_event,
};

__attribute__((constructor(1001)))
static void j_register_jframe(void)
{
    jframe_type_id = j_register_widget(&type_jframe, "jwidget");
}
