gboolean login_shell = TRUE;
gboolean bold_is_bright = FALSE;
char *fonts[] = {
    "Monospace 9",
    "Terminus 10",
};
glong scrollback_lines = 50000;
char *link_regex = "[a-z]+://[[:graph:]]+";
char *link_handler = "xiate-link-handler";
char *history_handler = "xiate-history-handler";
VteCursorBlinkMode cursor_blink_mode = VTE_CURSOR_BLINK_OFF;
VteCursorShape cursor_shape = VTE_CURSOR_SHAPE_BLOCK;
char *c_cursor = "#00FF00";
char *c_cursor_foreground = "#000000";
char *c_bold = NULL;
char *c_foreground = "#AAAAAA";
char *c_background = "#000000";
char *c_palette[16] = {
    "#000000",  /* Dark Black */
    "#AA0000",  /* Dark Red */
    "#00AA00",  /* Dark Green */
    "#AA5500",  /* Dark Yellow */
    "#0000AA",  /* Dark Blue */
    "#AA00AA",  /* Dark Magenta */
    "#00AAAA",  /* Dark Cyan */
    "#AAAAAA",  /* Dark White */
    "#555555",  /* Bright Black */
    "#FF5555",  /* Bright Red */
    "#55FF55",  /* Bright Green */
    "#FFFF55",  /* Bright Yellow */
    "#5555FF",  /* Bright Blue */
    "#FF55FF",  /* Bright Magenta */
    "#55FFFF",  /* Bright Cyan */
    "#FFFFFF",  /* Bright White */
};
guint button_link = 3;
struct NamedKey named_keys[] = {
    { .name = "key_copy_to_clipboard",     .value = GDK_KEY_C },
    { .name = "key_paste_from_clipboard",  .value = GDK_KEY_V },
    { .name = "key_handle_history",        .value = GDK_KEY_H },
    { .name = "key_next_font",             .value = GDK_KEY_N },
    { .name = "key_previous_font",         .value = GDK_KEY_P },
    { .name = "key_zoom_in",               .value = GDK_KEY_I },
    { .name = "key_zoom_out",              .value = GDK_KEY_O },
    { .name = "key_zoom_reset",            .value = GDK_KEY_R },
};
