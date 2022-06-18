#include <justui/jpainted.h>
#include <gint/std/stdlib.h>

/* Type identifier for jpainted */
static int jpainted_type_id = -1;

jpainted *jpainted_create(void *function, j_arg_t arg, int natural_w,
	int natural_h, void *parent)
{
	if(jpainted_type_id < 0) return NULL;

	jpainted *p = malloc(sizeof *p);
	jwidget_init(&p->widget, jpainted_type_id, parent);

	p->paint = function;
	p->arg = arg;
	p->natural_w = natural_w;
	p->natural_h = natural_h;

	return p;
}

//---
// Polymorphic widget operations
//---

static void jpainted_poly_csize(void *p0)
{
	jpainted *p = p0;
	p->widget.w = p->natural_w;
	p->widget.h = p->natural_h;
}

static void jpainted_poly_render(void *p0, int x, int y)
{
	jpainted *p = p0;
	p->paint(x, y, p->arg);
}

/* jpainted type definition */
static jwidget_poly type_jpainted = {
	.name    = "jpainted",
	.csize   = jpainted_poly_csize,
	.layout  = NULL,
	.render  = jpainted_poly_render,
	.event   = NULL,
	.destroy = NULL,
};

/* Type registration */
__attribute__((constructor(1004)))
static void j_register_jpainted(void)
{
	jpainted_type_id = j_register_widget(&type_jpainted, "jwidget");
}
