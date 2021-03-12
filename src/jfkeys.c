#include <justui/jfkeys.h>
#include <justui/jwidget-api.h>
#include <gint/std/stdlib.h>
#include <gint/std/string.h>

/* Type identified for jfkeys */
static int jfkeys_type_id = -1;

#ifdef FX9860G
jfkeys *jfkeys_create(bopti_image_t const *img, void *parent)
{
	if(jfkeys_type_id < 0) return NULL;

	jfkeys *f = malloc(sizeof *f);
	jwidget_init(&f->widget, jfkeys_type_id, parent);
	jfkeys_set(f, img);
	return f;
}

void jfkeys_set(jfkeys *f, bopti_image_t const *img)
{
	f->img = img;
	f->level = 0;
}
#endif /* FX9860G */

#ifdef FXCG50
jfkeys *jfkeys_create(char const *labels, void *parent)
{
	if(jfkeys_type_id < 0) return NULL;

	jfkeys *f = malloc(sizeof *f);
	jwidget_init(&f->widget, jfkeys_type_id, parent);
	jfkeys_set(f, labels);
	return f;
}

void jfkeys_set(jfkeys *f, char const *labels)
{
	f->labels = labels;
	f->level = 0;
}

/* get_level(): Find the level inside a function definition */
static char const *get_level(char const *labels, int level)
{
	/* Navigate to level */
	while(level > 0) labels = strchrnul(labels, '|');
	return (*labels == 0) ? NULL : labels;
}

/* get_label(): Find a key within a level */
static char const *get_label(char const *level, int key, size_t *len)
{
	int current_key = 0;

	while(current_key <= key) {
		/* We reached the end of the level without finding the key */
		if(*level == 0 || *level == '|') return NULL;
		/* Next entry */
		if(*level == ';') {
			current_key++;
			level++;
			continue;
		}
		/* Found contents */
		if(current_key == key) {
			*len = 0;
			while(level[*len] != ';' && level[*len] != '|' && level[*len])
				(*len)++;
			return level;
		}

		level++;
	}

	return NULL;
}
#endif /* FXCG50 */

//---
// Polymorphic widget operations
//---

static void jfkeys_poly_csize(void *f0)
{
	jfkeys *f = f0;

	#ifdef FX9860G
	f->widget.w = 128;
	f->widget.h = 8;
	#endif

	#ifdef FXCG50
	f->widget.w = 396;
	f->widget.h = 17;
	#endif
}

static void jfkeys_poly_render(void *f0, int base_x, int y)
{
	jfkeys *f = f0;

	#ifdef FX9860G
	dsubimage(base_x, y, f->img, 0, 9*f->level, f->img->width, 8, DIMAGE_NONE);
	#endif

	#ifdef FXCG50
	font_t const *old_font = dfont(dfont_default());
	char const *level = get_level(f->labels, f->level);
	if(!level) return;

	for(int position = 0; position < 6; position++) {
		size_t length = 0;
		char const *text = get_label(level, position, &length);
		if(!text || (*text != '.' && *text != '/' && *text != '@'
			&& *text != '#')) continue;

		int x = base_x + 4 + 65 * position;
		int w = 63;
		int color = (text[0] == '#') ? C_BLACK : C_WHITE;

		if(text[0] == '.') {
			drect(x, y, x + w - 1, y + 14, C_BLACK);
		}
		if(text[0] == '/' || text[0] == '@') {
			dline(x + 1, y, x + w - 2, y, C_BLACK);
			dline(x + 1, y + 14, x + w - 2, y + 14, C_BLACK);
			drect(x, y + 1, x + w - 1, y + 13, C_BLACK);
		}
		if(text[0] == '/') {
			dline(x + w - 1, y + 10, x + w - 5, y + 14, C_WHITE);
			dline(x + w - 1, y + 11, x + w - 4, y + 14, C_WHITE);
			dline(x + w - 1, y + 12, x + w - 3, y + 14, C_WHITE);
			dline(x + w - 1, y + 13, x + w - 2, y + 14, C_WHITE);
		}
		if(text[0] == '#') {
			dline(x + 1, y, x + w - 2, y, C_BLACK);
			dline(x + 1, y + 14, x + w - 2, y + 14, C_BLACK);
			drect(x, y + 1, x + 1, y + 13, C_BLACK);
			drect(x + w - 2, y + 1, x + w - 1, y + 13, C_BLACK);
		}

		dtext_opt(x + (w >> 1), y + 3, color, C_NONE, DTEXT_CENTER, DTEXT_TOP,
			text + 1, length - 1);
	}

	dfont(old_font);
	#endif /* FXCG50 */
}

int jfkeys_level(jfkeys *f)
{
	return f->level;
}

void jfkeys_set_level(jfkeys *f, int level)
{
	if(f->level == level) return;
	f->level = level;
	f->widget.update = 1;
}

/* jfkeys type definition */
static jwidget_poly type_jfkeys = {
	.name    = "jfkeys",
	.csize   = jfkeys_poly_csize,
	.layout  = NULL,
	.render  = jfkeys_poly_render,
	.event   = NULL,
	.destroy = NULL,
};

/* Type registration */
__attribute__((constructor(1005)))
static void j_register_jfkeys(void)
{
	jfkeys_type_id = j_register_widget(&type_jfkeys, "jwidget");
}
