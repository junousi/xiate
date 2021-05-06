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
    /* Dark Black */     "#000000",
    /* Dark Red */       "#AA0000",
    /* Dark Green */     "#00AA00",
    /* Dark Yellow */    "#AA5500",
    /* Dark Blue */      "#0000AA",
    /* Dark Magenta */   "#AA00AA",
    /* Dark Cyan */      "#00AAAA",
    /* Dark White */     "#AAAAAA",
    /* Bright Black */   "#555555",
    /* Bright Red */     "#FF5555",
    /* Bright Green */   "#55FF55",
    /* Bright Yellow */  "#FFFF55",
    /* Bright Blue */    "#5555FF",
    /* Bright Magenta */ "#FF55FF",
    /* Bright Cyan */    "#55FFFF",
    /* Bright White */   "#FFFFFF",
};
