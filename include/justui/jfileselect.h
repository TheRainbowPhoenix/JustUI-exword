//---
// JustUI.jfileselect: Basic file selector
//---

#ifndef _J_JFILESELECT
#define _J_JFILESELECT

#include <justui/defs.h>
#include <justui/jwidget.h>
#include <gint/display.h>
#include <dirent.h>

/* jfileselect: Basic file selector

   This widget is used to browse the filesystem and select a file. Visually, it
   only consists of a scrolling list of names showing a section of a folder's
   entries.

   TODO: jfileselect: Select a new file to write to

   Events:
   * JFILESELECT_LOADED when a folder is loaded into the view
   * JFILESELECT_VALIDATED when a file has been selected
   * JFILESELECT_CANCELED when if the user exits from the top-level folder */
typedef struct {
    jwidget widget;

    /* Folder currently being browsed */
    char *path;
    /* List of entries */
    void *entries;
    /* Number of entries in the current folder */
    int entry_count;

    /* Full path to file last selected with EXE */
    char *selected_file;

    /* Current cursor position (0 .. folder_entries-1) */
    int16_t cursor;
    /* Current scroll position */
    int16_t scroll;
    /* Number of visible lines */
    int8_t visible_lines;

    /* Additional pixels of spacing per line (base is font->height) */
    int8_t line_spacing;
    /* Whether to show the file size on the right */
    bool show_file_size;
    /* Rendering font */
    font_t const *font;

} jfileselect;

/* Type IDs */
extern uint16_t JFILESELECT_LOADED;
extern uint16_t JFILESELECT_VALIDATED;
extern uint16_t JFILESELECT_CANCELED;

/* jfileselect_create(): Create a file selection interface

   There is no initial folder. The widget will not handle any events nor emit
   any events in this state; a path must first be set before use. */
jfileselect *jfileselect_create(void *parent);

/* jfileselect_browse(): Browse a folder

   This function loads the specified folder and allows the user to select a
   file. (Remember to give the widget focus.) A JFILESELECT_LOADED event is
   emitted immediately, and further events are emitted based on user inputs.

   This function resets the selected file to NULL.

   Returns true on success, false if the path does not exist or cannot be
   browsed (in that case, check errno). */
bool jfileselect_browse(jfileselect *fs, char const *path);

/* jfileselect_selected_file(): Get the path to the selected file

   The selected file is NULL until jfileselect_browse() is called and the user
   selects a file in the interface. The returned pointer is owned by the
   widget. */
char const *jfileselect_selected_file(jfileselect *fs);

/* jfileselect_current_folder(): Get the path to the current folder */
char const *jfileselect_current_folder(jfileselect *fs);

/* Trivial properties */
void jfileselect_set_font(jfileselect *fs, font_t const *font);
void jfileselect_set_line_spacing(jfileselect *fs, int line_spacing);
void jfileselect_set_show_file_size(jfileselect *fs, bool show_file_size);

#endif /* _J_JFILESELECT */
