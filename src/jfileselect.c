#include <justui/jwidget.h>
#include <justui/jwidget-api.h>
#include <justui/jfileselect.h>

#include <gint/display.h>
#include <gint/gint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Type identifier for jfileselect */
static int jfileselect_type_id = -1;

/* Events */
uint16_t JFILESELECT_LOADED;
uint16_t JFILESELECT_VALIDATED;
uint16_t JFILESELECT_CANCELED;

jfileselect *jfileselect_create(void *parent)
{
	if(jfileselect_type_id < 0) return NULL;

	jfileselect *fs = malloc(sizeof *fs);
	if(!fs) return NULL;

	jwidget_init(&fs->widget, jfileselect_type_id, parent);

	fs->path = NULL;
	fs->dp = NULL;
	fs->selected_file = NULL;

	fs->folder_entries = -1;
	fs->cursor = -1;
	fs->scroll = 0;
	fs->visible_lines = 0;

	fs->line_spacing = 4;
	fs->font = dfont_default();

	return fs;
}

static void count_visible_lines(jfileselect *fs)
{
	int ch = jwidget_content_height(fs);
	int line_height = fs->font->line_height + fs->line_spacing;
	fs->visible_lines = ch / line_height;
}

//---
// Getters and setters
//---

void jfileselect_set_font(jfileselect *fs, font_t const *font)
{
	fs->font = font ? font : dfont_default();
	count_visible_lines(fs);
}

void jfileselect_set_line_spacing(jfileselect *fs, int line_spacing)
{
	fs->line_spacing = line_spacing;
	count_visible_lines(fs);
}

//---
// Path and folder manipulation
//---

static char *path_down(char const *path, char const *name)
{
	char *child = malloc(strlen(path) + strlen(name) + 2);
	if(!child)
		return NULL;

	strcpy(child, path);
	if(strcmp(path, "/") != 0)
		strcat(child, "/");
	strcat(child, name);

	return child;
}

static char *path_up(char const *path)
{
	char *parent = strdup(path);
	if(!parent)
		return NULL;

	char *p = strrchr(parent, '/');
	if(p == parent)
		*(p+1) = 0;
	else if(p)
		*p = 0;

	return parent;
}

static int accept_entry(struct dirent *ent)
{
	/* TODO: jfileselect: Programmable filter */
	if(!strcmp(ent->d_name, "@MainMem"))
		return 0;
	if(!strcmp(ent->d_name, "SAVE-F"))
		return 0;
	if(!strcmp(ent->d_name, "."))
		return 0;
	if(!strcmp(ent->d_name, ".."))
		return 0;
	return 1;
}

struct dirent *read_nth_dir_entry(DIR *dp, int n)
{
	struct dirent *entry = NULL;
	rewinddir(dp);

	for(int i = 0; i <= n;) {
		entry = readdir(dp);
		if(!entry)
			return NULL;
		i += accept_entry(entry);
	}

	return entry;
}

static bool load_folder(jfileselect *fs, char *path)
{
	if(fs->dp)
		closedir(fs->dp);

	fs->dp = (DIR *)gint_world_switch(GINT_CALL(opendir, path));
	if(!fs->dp)
		return false;

	free(fs->path);
	fs->path = path;

	fs->folder_entries = 0;
	struct dirent *ent;
	while ((ent = readdir(fs->dp)))
		fs->folder_entries += accept_entry(ent);

	fs->widget.update = true;

	jwidget_emit(fs, (jevent){ .type = JFILESELECT_LOADED });
	return true;
}

bool jfileselect_browse(jfileselect *fs, char const *path)
{
	char *path_copy = strdup(path);
	if(!path_copy)
		return false;

	if(!load_folder(fs, path_copy))
		return false;

	free(fs->selected_file);
	fs->selected_file = NULL;

	fs->cursor = 0;
	fs->scroll = 0;
	return true;
}

char const *jfileselect_selected_file(jfileselect *fs)
{
	return fs->selected_file;
}

char const *jfileselect_current_folder(jfileselect *fs)
{
	return fs->path;
}

//---
// Polymorphic widget operations
//---

static void jfileselect_poly_csize(void *fs0)
{
	jfileselect *fs = fs0;
	jwidget *w = &fs->widget;

	w->w = 128;
	w->h = 6 * max(fs->font->line_height + fs->line_spacing, 0);
}

static void jfileselect_poly_layout(void *fs0)
{
	jfileselect *fs = fs0;
	count_visible_lines(fs);
}

static void jfileselect_poly_render(void *fs0, int x, int y)
{
	jfileselect *fs = fs0;
	if(!fs->path || !fs->dp)
		return;

	font_t const *old_font = dfont(fs->font);
	int line_height = fs->font->line_height + fs->line_spacing;
	int cw = jwidget_content_width(fs);

	rewinddir(fs->dp);
	struct dirent *ent;
	char const *entry_name;
	bool isfolder;

	for(int i = -fs->scroll; i < fs->visible_lines;) {
		bool selected = (fs->cursor == fs->scroll + i);

		ent = readdir(fs->dp);
		if(!ent) break;
		if(!accept_entry(ent)) continue;

		entry_name = ent->d_name;
		isfolder = (ent->d_type == DT_DIR);

		if(i < 0) {
			i++;
			continue;
		}

		int line_y = y + line_height * i;
		if(selected)
			drect(x, line_y, x + cw - 1, line_y + line_height - 1, C_BLACK);

		/* Round `line_spacing / 2` down so there is more spacing below */
		dprint(x+2, line_y + (fs->line_spacing + 0) / 2,
				selected ? C_WHITE : C_BLACK,
		       "%s%s", entry_name, isfolder ? "/" : "");
		i++;
	}

	dfont(old_font);
}

static bool jfileselect_poly_event(void *fs0, jevent e)
{
	jfileselect *fs = fs0;
	if(!fs->path || !fs->dp)
		return false;

	if(e.type == JWIDGET_KEY) {
		key_event_t ev = e.key;
		if(ev.type != KEYEV_DOWN && ev.type != KEYEV_HOLD)
			return false;
		int key = ev.key;

		bool moved = false;

		if(key == KEY_UP && fs->cursor > 0) {
			fs->cursor = ev.shift ? 0 : fs->cursor - 1;
			moved = true;
		}
		if(key == KEY_DOWN && fs->cursor < fs->folder_entries - 1) {
			fs->cursor = ev.shift ? fs->folder_entries - 1 : fs->cursor + 1;
			moved = true;
		}

		if(fs->scroll > 0 && fs->cursor <= fs->scroll)
			fs->scroll = max(fs->cursor - 1, 0);
		if(fs->scroll + fs->visible_lines < fs->folder_entries
				&& fs->cursor >= fs->scroll + fs->visible_lines - 2) {
			fs->scroll = min(fs->cursor - fs->visible_lines + 2,
							 fs->folder_entries - fs->visible_lines);
		}

		if(moved) {
			fs->widget.update = true;
			return true;
		}

		if(key == KEY_EXIT) {
			if(!strcmp(fs->path, "/")) {
				jwidget_emit(fs, (jevent){ .type = JFILESELECT_CANCELED });
				return true;
			}
			char *parent = path_up(fs->path);
			if(parent) {
				load_folder(fs, parent);
				fs->cursor = 0;
				fs->scroll = 0;
				return true;
			}
		}
		else if(key == KEY_EXE) {
			struct dirent *ent = read_nth_dir_entry(fs->dp, fs->cursor);
		 	if(ent->d_type == DT_DIR) {
				char *child = path_down(fs->path, ent->d_name);
				if(child) {
					load_folder(fs, child);
					fs->cursor = 0;
					fs->scroll = 0;
					return true;
				}
			}
			else {
				fs->selected_file = path_down(fs->path, ent->d_name);
				if(fs->selected_file) {
					jwidget_emit(fs,(jevent){ .type = JFILESELECT_VALIDATED });
					return true;
				}
			}
		}
	}

	return false;
}

static void jfileselect_poly_destroy(void *fs0)
{
	jfileselect *fs = fs0;

	free(fs->path);
	if(fs->dp)
		closedir(fs->dp);
	free(fs->selected_file);
}

/* jfileselect type definition */
static jwidget_poly type_jfileselect = {
	.name    = "jfileselect",
	.csize   = jfileselect_poly_csize,
	.layout  = jfileselect_poly_layout,
	.render  = jfileselect_poly_render,
	.event   = jfileselect_poly_event,
	.destroy = jfileselect_poly_destroy,
};

__attribute__((constructor(1003)))
static void j_register_jfileselect(void)
{
	jfileselect_type_id = j_register_widget(&type_jfileselect, "jwidget");
	JFILESELECT_LOADED = j_register_event();
	JFILESELECT_VALIDATED = j_register_event();
	JFILESELECT_CANCELED = j_register_event();
}
