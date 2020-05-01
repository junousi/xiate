/* If no other argument vector is specified via the UNIX socket, then a
 * newly created terminal window will launch the user's shell. This
 * option specifies whether the shell will be a login shell or not. */
gboolean login_shell = TRUE;

/* Shall the escape sequence for "bold" also switch to a brighter color?
 *
 * Historically, X11 terminal emulators interpreted "bold" as "bold font
 * AND brighter color". Many programs ask the ncurses API for "A_BOLD,
 * COLOR_YELLOW" and expect to get bold, bright yellow. On the other
 * hand, there are documents that define this as "bold OR bright", such
 * as ECMA-48.
 *
 * There is currently (2020-ish) a movement to drop the old behaviour
 * and only have "bold" make text bold, not bright. The main goal is to
 * remove the ambiguity. Since GNOME is pushing this change, it can be
 * expected to see wider adoption in the next few years.
 *
 * More background information on this topic:
 *
 * - The main ticket for VTE's change (note comment 36, a support matrix):
 *   https://bugzilla.gnome.org/show_bug.cgi?id=762247
 * - How to deal with it in ncurses applications:
 *   https://bugzilla.redhat.com/show_bug.cgi?id=1687141#c3
 * - More links:
 *   https://github.com/alacritty/alacritty/issues/2779#issuecomment-528782774
 */
gboolean bold_is_bright = FALSE;

/* Default fonts and font sizes. These strings will be parsed by pango,
 * see the following URL:
 *
 * https://developer.gnome.org/pango/stable/pango-Fonts.html#pango-font-description-from-string
 *
 * You can define multiple fonts, at least one font must be defined. You
 * can switch between these fonts during runtime or you can select one
 * at startup (by default, the first one will be used). See manpage.
 */
char *fonts[] = {
    "Monospace 9",
    "Terminus 10",
};

/* Use 0 to disable scrolling completely or a negative value for
 * infinite scrolling. Keep the memory footprint in mind, though. */
guint scrollback_lines = 50000;

/* This regular expression is used to match links. You can easily spot
 * them by hovering over them with your mouse. Use your right mouse
 * button to invoke the link handler (defined below). */
char *link_regex = "[a-z]+://[[:graph:]]+";

/* Set this to the path of a tool to handle links. It will be invoked
 * with the following arguments:
 *
 *     argv[1] = "explicit" or "match"
 *     argv[2] = The link in question
 *
 * "explicit" will be used for explicit hyperlinks. They are explained
 * over here:
 *
 * https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
 *
 * "match" will be used for links that have been found using
 * "link_regex" as defined above.
 *
 * You don't need to specify an absolute path. The tool will be looked
 * for in your $PATH. */
char *link_handler = "xiate-link-handler";

/* Set this to the path of a tool to handle history dumps.
 *
 * History dumps work like this: You press Ctrl+Shift+F to tell xiate to
 * write the entire history of the terminal to a temporary file. Once
 * that's done, your tool will be called with the path to that file as
 * first argument.
 *
 * What you do with this file is entirely up to you. It's also your job
 * to remove it once you're done with it.
 *
 * You don't need to specify an absolute path. The tool will be looked
 * for in your $PATH. */
char *history_handler = "xiate-history-handler";

/* Whether or not to blink the cursor. Probably one of the following
 * values, but please check with upstream docs or your header files:
 *
 * VTE_CURSOR_BLINK_SYSTEM
 * VTE_CURSOR_BLINK_ON
 * VTE_CURSOR_BLINK_OFF
 *
 * https://developer.gnome.org/vte/unstable/VteTerminal.html */
VteCursorBlinkMode cursor_blink_mode = VTE_CURSOR_BLINK_OFF;

/* Shape of the cursor. Probably one of the following values, but please
 * check with upstream docs or your header files:
 *
 * VTE_CURSOR_SHAPE_BLOCK
 * VTE_CURSOR_SHAPE_IBEAM
 * VTE_CURSOR_SHAPE_UNDERLINE
 *
 * https://developer.gnome.org/vte/unstable/VteTerminal.html */
VteCursorShape cursor_shape = VTE_CURSOR_SHAPE_BLOCK;

/* Background color of text under the cursor. There's a special mode:
 * "If NULL, text under the cursor will be drawn with foreground and
 * background colors reversed." */
char *c_cursor = "#00FF00";

/* Foreground color of text under the cursor. Just like the background
 * color, NULL reverses foreground and background. */
char *c_cursor_foreground = "#000000";

/* Quoting from the VTE reference: "Sets the color used to draw bold
 * text in the default foreground color. If [...] NULL then the default
 * color is used." */
char *c_bold = NULL;

/* Set the terminal's color palette. Note that none of these values can
 * be NULL. */
char *c_foreground = "#AAAAAA";
char *c_background = "#000000";
char *c_palette[16] = {
    /* Dark */
    /* Black */   "#000000",
    /* Red */     "#AA0000",
    /* Green */   "#00AA00",
    /* Yellow */  "#AA5500",
    /* Blue */    "#0000AA",
    /* Magenta */ "#AA00AA",
    /* Cyan */    "#00AAAA",
    /* White */   "#AAAAAA",

    /* Bright */
    /* Black */   "#555555",
    /* Red */     "#FF5555",
    /* Green */   "#55FF55",
    /* Yellow */  "#FFFF55",
    /* Blue */    "#5555FF",
    /* Magenta */ "#FF55FF",
    /* Cyan */    "#55FFFF",
    /* White */   "#FFFFFF",
};
