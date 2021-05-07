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
struct NamedColor named_colors[] = {
    /* Order must match standard 16 color order. */
    { .name = "dark_black",      .value = "#000000", },
    { .name = "dark_red",        .value = "#AA0000", },
    { .name = "dark_green",      .value = "#00AA00", },
    { .name = "dark_yellow",     .value = "#AA5500", },
    { .name = "dark_blue",       .value = "#0000AA", },
    { .name = "dark_magenta",    .value = "#AA00AA", },
    { .name = "dark_cyan",       .value = "#00AAAA", },
    { .name = "dark_white",      .value = "#AAAAAA", },

    { .name = "bright_black",    .value = "#555555", },
    { .name = "bright_red",      .value = "#FF5555", },
    { .name = "bright_green",    .value = "#55FF55", },
    { .name = "bright_yellow",   .value = "#FFFF55", },
    { .name = "bright_blue",     .value = "#5555FF", },
    { .name = "bright_magenta",  .value = "#FF55FF", },
    { .name = "bright_cyan",     .value = "#55FFFF", },
    { .name = "bright_white",    .value = "#FFFFFF", },
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
