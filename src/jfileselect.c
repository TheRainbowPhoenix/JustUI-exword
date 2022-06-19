#include <justui/jwidget.h>
#include <justui/jwidget-api.h>
#include <justui/jfileselect.h>

#include <gint/display.h>
#include <gint/gint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Type identifier for jfileselect */
static int jfileselect_type_id = -1;

/* Events */
uint16_t JFILESELECT_LOADED;
uint16_t JFILESELECT_VALIDATED;
uint16_t JFILESELECT_CANCELED;

/* We can try pretty hard to not duplicate the information held by the
   directory descriptor, which is already a full array of all entries on gint.
   However, the standard API behind readdir(3) does not allow us any complex
   operations; no filtering, no sorting, no accessing auxiliary data such as
   the file size. So we do this manually. */
struct fileinfo {
	/* Entry name (owned by the structure) */
	char *name;
	/* File size in bytes if file, number of entries if folder */
	int size;
	/* Type from [struct dirent] */
	int type;
};

jfileselect *jfileselect_create(void *parent)
{
	if(jfileselect_type_id < 0) return NULL;

	jfileselect *fs = malloc(sizeof *fs);
	if(!fs) return NULL;

	jwidget_init(&fs->widget, jfileselect_type_id, parent);

	fs->path = NULL;
	fs->entries = NULL;
	fs->entry_count = 0;

	fs->selected_file = NULL;

	fs->cursor = -1;
	fs->scroll = 0;
	fs->visible_lines = 0;

	fs->line_spacing = 4;
	fs->font = dfont_default();
	fs->show_file_size = false;

	return fs;
}

static void count_visible_lines(jfileselect *fs)
{
	int ch = jwidget_content_height(fs);
	int line_height = fs->font->line_height + fs->line_spacing;
	fs->visible_lines = ch / line_height;
}

static void set_finfo(jfileselect *fs, struct fileinfo *finfo, int n)
{
	struct fileinfo *old = fs->entries;
	if(old) {
		for(int i = 0; i < fs->entry_count; i++)
			free(old[i].name);
		free(old);
	}
	fs->entries = finfo;
	fs->entry_count = n;
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

void jfileselect_set_show_file_size(jfileselect *fs, bool show_file_size)
{
	fs->show_file_size = show_file_size;
	fs->widget.update = true;
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

static bool accept_entry(struct dirent *ent)
{
	/* TODO: jfileselect: Programmable filter */
	if(!strcmp(ent->d_name, "@MainMem"))
		return false;
	if(!strcmp(ent->d_name, "SAVE-F"))
		return false;
	if(!strcmp(ent->d_name, "."))
		return false;
	if(!strcmp(ent->d_name, ".."))
		return false;
	return true;
}

static int count_accepted_entries(DIR *dp)
{
	int n = 0;
	struct dirent *ent;

	rewinddir(dp);
	while((ent = readdir(dp)))
		n += accept_entry(ent);

	return n;
}

static int compare_entries(void const *i1_0, void const *i2_0)
{
	struct fileinfo const *i1 = i1_0, *i2 = i2_0;
	int d1 = (i1->type == DT_DIR);
	int d2 = (i2->type == DT_DIR);

	/* Group directories first */
	if(d1 != d2)
		return d2 - d1;
	/* Then group by name */
	return strcmp(i1->name, i2->name);
}

static bool load_folder_switch(jfileselect *fs, char *path)
{
	set_finfo(fs, NULL, 0);

	struct dirent *ent;
	DIR *dp = opendir(path);
	if(!dp)
		return false;

	/* Count entries */
	int n = count_accepted_entries(dp);

	/* Allocate memory for the fileinfo structures */
	struct fileinfo *finfo = malloc(n * sizeof *finfo);
	if(!finfo) {
		closedir(dp);
		return false;
	}

	/* Read the fileinfo structures */
	rewinddir(dp);
	for(int i = 0; i < n && (ent = readdir(dp));) {
		if(!accept_entry(ent))
			continue;

		finfo[i].name = strdup(ent->d_name);
		finfo[i].type = ent->d_type;
		finfo[i].size = -1;

		if(!finfo[i].name) {
			/* Profesionnal unwinding isn't it? */
			for(int j = 0; j < i; j++) free(finfo[j].name);
			free(finfo);
			closedir(dp);
			return false;
		}

		char *full_path = path_down(path, ent->d_name);
		if(full_path) {
			if(ent->d_type == DT_DIR) {
				DIR *sub = opendir(full_path);
				if(sub) {
					finfo[i].size = count_accepted_entries(sub);
					closedir(sub);
				}
			}
			else {
				struct stat st;
				if(stat(full_path, &st) >= 0)
					finfo[i].size = st.st_size;
			}
		}

		i++;
	}

	qsort(finfo, n, sizeof *finfo, compare_entries);

	closedir(dp);
	free(fs->path);
	fs->path = path;
	set_finfo(fs, finfo, n);
	fs->widget.update = true;

	jwidget_emit(fs, (jevent){ .type = JFILESELECT_LOADED });
	return true;
}

static bool load_folder(jfileselect *fs, char *path)
{
	return gint_world_switch(GINT_CALL(load_folder_switch, (void *)fs, path));
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
	if(!fs->path || !fs->entries)
		return;

	font_t const *old_font = dfont(fs->font);
	int line_height = fs->font->line_height + fs->line_spacing;
	int cw = jwidget_content_width(fs);
	struct fileinfo *finfo = fs->entries;

	for(int i = 0; i < fs->visible_lines && i < fs->entry_count; i++) {
		bool selected = (fs->cursor == fs->scroll + i);
		struct fileinfo *info = &finfo[fs->scroll + i];
		bool isfolder = (info->type == DT_DIR);

		int line_y = y + line_height * i;
		if(selected)
			drect(x, line_y, x + cw - 1, line_y + line_height - 1, C_BLACK);

		/* Round `line_spacing / 2` down so there is more spacing below */
		int text_y = line_y + (fs->line_spacing + 0) / 2;
		int fg = selected ? C_WHITE : C_BLACK;

		dprint(x+2, text_y, fg, "%s%s", info->name, isfolder ? "/" : "");
		if(fs->show_file_size && info->size >= 0) {
			dprint_opt(x + cw - 3, text_y, fg, C_NONE, DTEXT_RIGHT, DTEXT_TOP,
				isfolder ? "%d entries" : "%d B", info->size);
		}
	}

	dfont(old_font);
}

static bool jfileselect_poly_event(void *fs0, jevent e)
{
	jfileselect *fs = fs0;
	if(!fs->path || !fs->entries)
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
		if(key == KEY_DOWN && fs->cursor < fs->entry_count - 1) {
			fs->cursor = ev.shift ? fs->entry_count - 1 : fs->cursor + 1;
			moved = true;
		}

		if(fs->scroll > 0 && fs->cursor <= fs->scroll)
			fs->scroll = max(fs->cursor - 1, 0);
		if(fs->scroll + fs->visible_lines < fs->entry_count
				&& fs->cursor >= fs->scroll + fs->visible_lines - 2) {
			fs->scroll = min(fs->cursor - fs->visible_lines + 2,
							 fs->entry_count - fs->visible_lines);
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
			struct fileinfo *finfo = fs->entries;
			struct fileinfo *i = &finfo[fs->cursor];
			if(i->type == DT_DIR) {
				char *child = path_down(fs->path, i->name);
				if(child) {
					load_folder(fs, child);
					fs->cursor = 0;
					fs->scroll = 0;
					return true;
				}
			}
			else {
				fs->selected_file = path_down(fs->path, i->name);
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
	set_finfo(fs, NULL, 0);
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
