#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vte/vte.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>

enum ConfigItemType {
    STRING,
    STRINGLIST,
    BOOLEAN,
    INT64,
    UINT64,
};

struct ConfigItem {
    char *s;
    char *n;
    enum ConfigItemType t;
    size_t l;  /* Only used for STRINGLIST */
    union {
        char *s;
        char **sl;
        gboolean b;
        gint64 i;
        guint64 ui;
    } v;
};

#include "defaults.h"


struct Terminal
{
    gboolean hold;
    GtkWidget *term;
    GtkWidget *win;
    gboolean has_child_exit_status;
    gint child_exit_status;
    size_t current_font;
};


void cb_spawn_async(VteTerminal *, GPid, GError *, gpointer);
struct ConfigItem *cfg(char *, char *);
VteCursorBlinkMode get_cursor_blink_mode(void);
VteCursorShape get_cursor_shape(void);
guint get_keyval(char *);
void handle_history(VteTerminal *);
void ini_load(char *);
char *safe_emsg(GError *);
void sig_bell(VteTerminal *, gpointer);
gboolean sig_button_press(GtkWidget *, GdkEvent *, gpointer);
void sig_char_size_changed(VteTerminal *, guint, guint, gpointer);
void sig_child_exited(VteTerminal *, gint, gpointer);
void sig_hyperlink_changed(VteTerminal *, gchar *, GdkRectangle *, gpointer);
gboolean sig_key_press(GtkWidget *, GdkEvent *, gpointer);
gboolean sig_motion_notify_event(GtkWidget *, GdkEventMotion *, gpointer);
void sig_window_destroy(GtkWidget *, gpointer);
void sig_window_resize(VteTerminal *, guint, guint, gpointer);
void sig_window_title_changed(VteTerminal *, gpointer);
void term_new(struct Terminal *, int, char **);
void term_activate_current_font(struct Terminal *, gboolean);
void term_change_font_scale(struct Terminal *, gint);
void term_set_geometry_hints(struct Terminal *);
void term_set_size(struct Terminal *t, glong, glong, gboolean);


void
cb_spawn_async(VteTerminal *term, GPid pid, GError *err, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;

    (void)term;

    if (pid == -1 && err != NULL)
    {
        fprintf(stderr, __NAME__": Spawning child failed: %s\n", err->message);
        gtk_widget_destroy(t->win);
    }
}

struct ConfigItem *
cfg(char *s, char *n)
{
    size_t i;

    for (i = 0; i < sizeof config / sizeof config[0]; i++)
    {
        if (strcmp(config[i].s, s) == 0 && strcmp(config[i].n, n) == 0)
            return &config[i];
    }

    fprintf(stderr, __NAME__": Internal error, unable to find cfg '%s'/'%s'\n",
            s, n);
    return NULL;
}

VteCursorBlinkMode
get_cursor_blink_mode(void)
{
    char *cfg_s = cfg("Options", "cursor_blink_mode")->v.s;

    if (strcmp(cfg_s, "VTE_CURSOR_BLINK_SYSTEM") == 0)
        return VTE_CURSOR_BLINK_SYSTEM;
    else if (strcmp(cfg_s, "VTE_CURSOR_BLINK_OFF") == 0)
        return VTE_CURSOR_BLINK_OFF;
    else
        return VTE_CURSOR_BLINK_ON;
}

VteCursorShape
get_cursor_shape(void)
{
    char *cfg_s = cfg("Options", "cursor_shape")->v.s;

    if (strcmp(cfg_s, "VTE_CURSOR_SHAPE_IBEAM") == 0)
        return VTE_CURSOR_SHAPE_IBEAM;
    else if (strcmp(cfg_s, "VTE_CURSOR_SHAPE_UNDERLINE") == 0)
        return VTE_CURSOR_SHAPE_UNDERLINE;
    else
        return VTE_CURSOR_SHAPE_BLOCK;
}

guint
get_keyval(char *name)
{
    char *cfg_s = cfg("Controls", name)->v.s;
    return gdk_keyval_from_name(cfg_s);
}

void
handle_history(VteTerminal *term)
{
    GFile *tmpfile = NULL;
    GFileIOStream *io_stream = NULL;
    GOutputStream *out_stream = NULL;
    GError *err = NULL;
    char *argv[] = { NULL, NULL, NULL };
    GSpawnFlags spawn_flags = G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH;

    argv[0] = cfg("Options", "history_handler")->v.s;

    tmpfile = g_file_new_tmp(NULL, &io_stream, &err);
    if (tmpfile == NULL)
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n",
                safe_emsg(err));
        goto free_and_out;
    }

    out_stream = g_io_stream_get_output_stream(G_IO_STREAM(io_stream));
    if (!vte_terminal_write_contents_sync(term, out_stream, VTE_WRITE_DEFAULT,
                                          NULL, &err))
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n",
                safe_emsg(err));
        goto free_and_out;
    }

    if (!g_io_stream_close(G_IO_STREAM(io_stream), NULL, NULL))
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n",
                safe_emsg(err));
        goto free_and_out;
    }

    argv[1] = g_file_get_path(tmpfile);
    if (!g_spawn_async(NULL, argv, NULL, spawn_flags, NULL, NULL, NULL, &err))
    {
        fprintf(stderr, __NAME__": Could not launch history handler: %s\n",
                safe_emsg(err));
    }

free_and_out:
    if (argv[1] != NULL)
        g_free(argv[1]);
    if (io_stream != NULL)
        g_object_unref(io_stream);
    if (tmpfile != NULL)
        g_object_unref(tmpfile);
    g_clear_error(&err);
}

void
ini_load(char *config_file)
{
    GKeyFile *ini = NULL;
    GError *err;
    gchar *p;
    gboolean ret;
    gchar **lst;
    gint64 int64;
    guint64 uint64;
    gsize len;
    size_t i;

    if (config_file == NULL)
        p = g_build_filename(g_get_user_config_dir(), __NAME__, "config.ini",
                             NULL);
    else
        p = g_strdup(config_file);

    ini = g_key_file_new();
    if (!g_key_file_load_from_file(ini, p, G_KEY_FILE_NONE, NULL))
    {
        /* Only warn if we *should have* read a config file. That's the
         * case when the user requested a specific config or when the
         * user uses the default path but that file is corrupt. */
        if (config_file != NULL || g_file_test(p, G_FILE_TEST_EXISTS))
            fprintf(stderr, __NAME__": Config '%s' could not be loaded\n", p);
        g_free(p);
        return;
    }

    g_free(p);

    for (i = 0; i < sizeof config / sizeof config[0]; i++)
    {
        err = NULL;
        switch (config[i].t)
        {
            case STRING:
                p = g_key_file_get_string(ini, config[i].s, config[i].n, &err);
                if (p != NULL)
                {
                    if (strcmp(p, "NULL") == 0)
                    {
                        config[i].v.s = NULL;
                        g_free(p);
                    }
                    else
                        config[i].v.s = p;
                }
                break;
            case STRINGLIST:
                lst = g_key_file_get_string_list(ini, config[i].s, config[i].n,
                                                 &len, &err);
                if (lst != NULL)
                {
                    config[i].v.sl = lst;
                    config[i].l = len;
                }
                break;
            case BOOLEAN:
                ret = g_key_file_get_boolean(ini, config[i].s, config[i].n,
                                             &err);
                if (err == NULL)
                    config[i].v.b = ret;
                break;
            case INT64:
                int64 = g_key_file_get_int64(ini, config[i].s, config[i].n,
                                             &err);
                if (err == NULL)
                    config[i].v.i = int64;
                break;
            case UINT64:
                uint64 = g_key_file_get_uint64(ini, config[i].s, config[i].n,
                                               &err);
                if (err == NULL)
                    config[i].v.ui = uint64;
                break;
        }
    }
}

char *
safe_emsg(GError *err)
{
    return err == NULL ? "<GError is NULL>" : err->message;
}

void
sig_bell(VteTerminal *term, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;
    const char *dname;

    (void)term;

    /* Credits go to sakura. The author says:
     * Remove the urgency hint. This is necessary to signal the window
     * manager that a new urgent event happened when the urgent hint is
     * set next time.
     */
    gtk_window_set_urgency_hint(GTK_WINDOW(t->win), FALSE);
    gtk_window_set_urgency_hint(GTK_WINDOW(t->win), TRUE);

    /* On Wayland, this should trigger xdg_activation_v1 in GTK (Wayland
     * has no concept of urgency hints, XDG activation is the next best
     * thing). It depends on your compositor, though: Some transfer the
     * input focus to xiate, which is very undesirable. That's why this
     * function is guarded behind a user option.
     *
     * On X11, though, the call below always triggers focus stealing.
     * We don't want that at all, hence we try to only issue it on
     * Wayland. */
    if (cfg("Options", "wayland_activate_on_bell")->v.b)
    {
        dname = gdk_display_get_name(gdk_display_get_default());
        if (strncmp(dname, "wayland-", 8) == 0)
        {
            gtk_window_present_with_time(
                GTK_WINDOW(t->win),
                gtk_get_current_event_time()
            );
        }
    }
}

gboolean
sig_button_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    char *url = NULL;
    char *argv[] = { NULL, NULL, NULL, NULL };
    GError *err = NULL;
    gboolean retval = FALSE;
    GSpawnFlags spawn_flags = G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH;

    argv[0] = cfg("Options", "link_handler")->v.s;

    (void)data;

    if (event->type == GDK_BUTTON_PRESS)
    {
        if (((GdkEventButton *)event)->button == cfg("Controls", "button_link")->v.ui)
        {
            if ((url = vte_terminal_hyperlink_check_event(VTE_TERMINAL(widget),
                                                          event)) != NULL)
            {
                argv[1] = "explicit";
            }
            else if ((url = vte_terminal_match_check_event(VTE_TERMINAL(widget),
                                                           event, NULL)) != NULL)
            {
                argv[1] = "match";
            }

            if (url != NULL)
            {
                argv[2] = url;
                if (!g_spawn_async(NULL, argv, NULL, spawn_flags, NULL, NULL,
                                   NULL, &err))
                {
                    fprintf(stderr, __NAME__": Could not spawn link handler: "
                            "%s\n", safe_emsg(err));
                    g_clear_error(&err);
                }
                else
                    retval = TRUE;
            }
        }
    }

    g_free(url);
    return retval;
}

void
sig_char_size_changed(VteTerminal *term, guint width, guint height, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;

    (void)term;
    (void)width;
    (void)height;

    term_set_geometry_hints(t);
}

void
sig_child_exited(VteTerminal *term, gint status, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;
    GdkRGBA c_background_gdk;

    t->has_child_exit_status = TRUE;
    t->child_exit_status = status;

    if (t->hold)
    {
        gdk_rgba_parse(&c_background_gdk, cfg("Colors", "background")->v.s);
        vte_terminal_set_color_cursor(term, &c_background_gdk);
        gtk_window_set_title(GTK_WINDOW(t->win), __NAME__" - CHILD HAS QUIT");
    }
    else
        gtk_widget_destroy(t->win);
}

void
sig_hyperlink_changed(VteTerminal *term, gchar *uri, GdkRectangle *bbox,
                      gpointer data)
{
    (void)bbox;
    (void)data;

    if (uri == NULL)
        gtk_widget_set_has_tooltip(GTK_WIDGET(term), FALSE);
    else
        gtk_widget_set_tooltip_text(GTK_WIDGET(term), uri);
}

gboolean
sig_key_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    VteTerminal *term = VTE_TERMINAL(widget);
    struct Terminal *t = (struct Terminal *)data;
    guint kv;

    if (((GdkEventKey *)event)->state & GDK_CONTROL_MASK)
    {
        kv = ((GdkEventKey *)event)->keyval;
        if (kv == get_keyval("key_copy_to_clipboard"))
        {
            vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
            return TRUE;
        }
        if (kv == get_keyval("key_paste_from_clipboard"))
        {
            vte_terminal_paste_clipboard(term);
            return TRUE;
        }
        if (kv == get_keyval("key_handle_history"))
        {
            handle_history(term);
            return TRUE;
        }
        if (kv == get_keyval("key_next_font"))
        {
            t->current_font++;
            t->current_font %= cfg("Options", "fonts")->l;
            term_activate_current_font(t, TRUE);
            return TRUE;
        }
        if (kv == get_keyval("key_previous_font"))
        {
            if (t->current_font == 0)
                t->current_font = cfg("Options", "fonts")->l - 1;
            else
                t->current_font--;
            term_activate_current_font(t, TRUE);
            return TRUE;
        }
        if (kv == get_keyval("key_zoom_in"))
        {
            term_change_font_scale(t, 1);
            return TRUE;
        }
        if (kv == get_keyval("key_zoom_out"))
        {
            term_change_font_scale(t, -1);
            return TRUE;
        }
        if (kv == get_keyval("key_zoom_reset"))
        {
            term_change_font_scale(t, 0);
            return TRUE;
        }
    }

    return FALSE;
}

gboolean
sig_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
    VteTerminal *term = VTE_TERMINAL(widget);
    char *text;

    (void)data;

    text = vte_terminal_match_check_event(term, (GdkEvent *)event, NULL);

    if (text == NULL)
        gtk_widget_set_has_tooltip(GTK_WIDGET(term), FALSE);
    else {
        char *cidr = text;
        char ip[NI_MAXHOST];
        memset(&ip, 0, NI_MAXHOST);
        int mask = 0;
        sscanf(cidr, "%[^/]/%d", ip, &mask);
        // fprintf(stderr, __NAME__": result '%d'\n", result);
        // fprintf(stderr, __NAME__": ip '%s'\n", ip);
        // fprintf(stderr, __NAME__": mask '%d'\n", mask);
        if (mask == 0) {
          fprintf(stderr, __NAME__": could not match to a cidr, testing bare address without mask\n");
          sscanf(cidr, "%[^/]", ip);
        }
        // fprintf(stderr, __NAME__": ip '%s'\n", ip);
        struct addrinfo hint, *res = NULL;
        int ret;
        memset(&hint, 0, sizeof(hint));
        hint.ai_family = PF_UNSPEC;
        hint.ai_flags = AI_NUMERICHOST;
        ret = getaddrinfo(ip, NULL, &hint, &res);
        if (ret) {
            fprintf(stderr, __NAME__": Invalid address '%s'\n", ip);
            return FALSE;
        }
        if (res->ai_family == AF_INET) {
            //TODO would be awesome if this lookup would be configurable.
            // "just the CIDR 0th"
            // "CIDR 0th + 1st"
            // "CIDR 0th + 1st + last"
            // "CIDR 0th + 1st + 2nd + 3rd"
            // "CIDR 0th + last"
            // etc.

            char * delimiter = " resolves to ";
            char * newline = "\n";
            char hbuf[NI_MAXHOST];
            char hbufsum[NI_MAXHOST];

            struct sockaddr_in sa;
            memset(&sa, 0, sizeof(struct sockaddr_in));
            socklen_t len;
            len = sizeof(struct sockaddr_in);
            sa.sin_family = AF_INET;
            inet_pton(AF_INET, ip, &(sa.sin_addr));

            if (getnameinfo((struct sockaddr *) &sa, len, hbuf, sizeof(hbuf),
                NULL, 0, NI_NAMEREQD)) {
                strcpy(hbuf, "N/A");
            }

            memset(hbufsum, 0, NI_MAXHOST);
            strcat(hbufsum, ip);
            strcat(hbufsum, delimiter);
            strcat(hbufsum, hbuf);

            if (mask != 0) {
                char ip2[NI_MAXHOST];
                memset(ip2, 0, NI_MAXHOST);
                struct sockaddr_in sa2;
                char hbuf2[NI_MAXHOST];
                memset(&sa2, 0, sizeof(struct sockaddr_in));
                sa2.sin_family = AF_INET;
                sa2.sin_addr.s_addr = htonl(ntohl(sa.sin_addr.s_addr) + 1);
                inet_ntop(AF_INET, &sa2.sin_addr, ip2, sizeof(ip2));
                if (getnameinfo((struct sockaddr *) &sa2, len, hbuf2, sizeof(hbuf2),
                    NULL, 0, NI_NAMEREQD)) {
                    strcpy(hbuf2, "N/A");
                }
                strcat(hbufsum, newline);
                strcat(hbufsum, ip2);
                strcat(hbufsum, delimiter);
                strcat(hbufsum, hbuf2);
            }
            gtk_widget_set_tooltip_text(GTK_WIDGET(term), hbufsum);
        } else if (res->ai_family == AF_INET6) {
            struct sockaddr_in6 sa6;
            socklen_t len;
            len = sizeof(struct sockaddr_in6);
            char hbuf[NI_MAXHOST];
            memset(&sa6, 0, sizeof(struct sockaddr_in6));
            sa6.sin6_family = AF_INET6;
            inet_pton(AF_INET6, ip, &(sa6.sin6_addr));
            if (getnameinfo((struct sockaddr *) &sa6, len, hbuf, sizeof(hbuf),
                NULL, 0, NI_NAMEREQD)) {
                fprintf(stderr, __NAME__": Unable to get PTR record for '%s'\n",
                ip);
            }
            else {
                gtk_widget_set_tooltip_text(GTK_WIDGET(term), hbuf);
            }
        } else {
            gtk_widget_set_has_tooltip(GTK_WIDGET(term), FALSE);
            printf("%s is an unknown address format %d\n", ip, res->ai_family);
        }
        freeaddrinfo(res);
        return TRUE;
    }
    return FALSE;
}

void
sig_window_destroy(GtkWidget *widget, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;
    int exit_code;

    (void)widget;

    /* Figure out exit code of our child. We deal with the full status
     * code as returned by wait(2) here, but there's no point in
     * returning the full integer, since we can't/won't try to fake
     * stuff like "the child had a segfault" and it's not possible to
     * discriminate between child exit codes and other errors related to
     * xiate's internals (GTK error, X11 died, something like that). */
    if (t->has_child_exit_status)
    {
        /* This "if" clause has been borrowed from suckless st. */
        if (!WIFEXITED(t->child_exit_status) || WEXITSTATUS(t->child_exit_status))
            exit_code = 1;
        else
            exit_code = 0;
    }
    else
        /* If there is no child exit status, it means the user has
         * forcibly closed the terminal window. We interpret this as
         * "ABANDON MISSION!!1!", so we won't return an exit code of 0
         * in this case.
         *
         * This will also happen if we fail to start the child in the
         * first place. */
        exit_code = 1;

    exit(exit_code);
}

void
sig_window_resize(VteTerminal *term, guint width, guint height, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;

    (void)term;

    term_set_size(t, width, height, TRUE);
}

void
sig_window_title_changed(VteTerminal *term, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;

    gtk_window_set_title(GTK_WINDOW(t->win), vte_terminal_get_window_title(term));
}

void
term_new(struct Terminal *t, int argc, char **argv)
{
    static char *args_default[] = { NULL, NULL, NULL };
    char **argv_cmdline = NULL, **args_use;
    char *config_file = NULL;
    char *title = __NAME__, *res_class = __NAME_CAPITALIZED__, *res_name = __NAME__;
    char *app_id, *link_regex, *ipaddr_regex;
    int i;
    GdkRGBA c_foreground_gdk;
    GdkRGBA c_background_gdk;
    GdkRGBA c_palette_gdk[16];
    GdkRGBA c_gdk;
    VteRegex *url_vregex = NULL;
    VteRegex *ipaddr_vregex = NULL;
    GError *err = NULL;
    GSpawnFlags spawn_flags;
    char *standard16order[] = {
        "dark_black",    "dark_red",        "dark_green",    "dark_yellow",
        "dark_blue",     "dark_magenta",    "dark_cyan",     "dark_white",
        "bright_black",  "bright_red",      "bright_green",  "bright_yellow",
        "bright_blue",   "bright_magenta",  "bright_cyan",   "bright_white",
    };

    /* Handle arguments. */
    t->current_font = 0;
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-class") == 0 && i < argc - 1)
            res_class = argv[++i];
        else if (strcmp(argv[i], "-hold") == 0)
            t->hold = TRUE;
        else if (strcmp(argv[i], "-name") == 0 && i < argc - 1)
            res_name = argv[++i];
        else if (strcmp(argv[i], "-title") == 0 && i < argc - 1)
            title = argv[++i];
        else if (strcmp(argv[i], "--config") == 0 && i < argc - 1)
            config_file = argv[++i];
        else if (strcmp(argv[i], "--fontindex") == 0 && i < argc - 1)
            t->current_font = atoi(argv[++i]);
        else if (strcmp(argv[i], "-e") == 0 && i < argc - 1)
        {
            argv_cmdline = &argv[++i];
            break;
        }
        else
        {
            fprintf(stderr, __NAME__": Invalid arguments, check manpage\n");
            exit(EXIT_FAILURE);
        }
    }

    ini_load(config_file);

    /* Create GTK+ widgets. */
    t->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(t->win), title);
    //gtk_window_set_wmclass(GTK_WINDOW(t->win), res_name, res_class);
    g_signal_connect(G_OBJECT(t->win), "destroy", G_CALLBACK(sig_window_destroy), t);

    /* Wayland only has "app_id", so we need to decide whether we put
     * res_class there or res_name. This is what ICCCM says:
     *
     *   res_name: A string that names the particular instance of the
     *   application to which the client that owns this window belongs.
     *
     *   res_class: A string that names the general class of
     *   applications to which the client that owns this window belongs.
     *   Examples of commonly used class names include: "Emacs",
     *   "XTerm", "XClock", "XLoad", and so on.
     *
     * https://www.x.org/releases/current/doc/xorg-docs/icccm/icccm.html#WM_CLASS_Property
     *
     * So, for example, res_class = "XTerm", res_name = "audio-player":
     * An XTerm that runs an audio player. You might then create a rule
     * in your window manager that specifies to put all of those windows
     * on workspace number 3, for example.
     *
     * If we simply were to drop one of those, we'd be losing
     * functionality. Drop res_name and you can no longer distinguish
     * between different XTerms. Drop res_class and suddenly all
     * "audio-players" are treated the same, whether they're running in
     * an XTerm or not.
     *
     * I think the best solution is to combine both res_class and
     * res_name into app_id. That's 99% backwards compatible. These two
     * strings are NUL-terminated strings, so we need to pick some
     * delimiter when combining them. I picked a dot, because,
     * historically (although not in xiate), res_name and res_class were
     * used in the X resource database where a dot has a special
     * meaning, so it shouldn't be used in res_name or res_class anyway.
     * (https://www.x.org/releases/current/doc/libX11/libX11/libX11.html#Resource_File_Syntax)
     *
     * In the WM_CLASS property, res_name comes first and res_class
     * second. In my opinion, that's confusing, because res_class is
     * "more top-level" than res_name. Still, in an effort to not
     * irritate users, we keep this order. */
    app_id = g_strdup_printf("%s.%s", res_name, res_class);
    g_set_prgname(app_id);
    g_free(app_id);

    t->term = vte_terminal_new();
    gtk_container_add(GTK_CONTAINER(t->win), t->term);

    /* Appearance. */
    term_activate_current_font(t, FALSE);
    gtk_widget_show_all(t->win);
    term_set_geometry_hints(t);

    vte_terminal_set_bold_is_bright(VTE_TERMINAL(t->term),
                                    cfg("Options", "bold_is_bright")->v.b);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(t->term),
                                       get_cursor_blink_mode());
    vte_terminal_set_cursor_shape(VTE_TERMINAL(t->term), get_cursor_shape());
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(t->term), TRUE);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(t->term),
                                      (glong)cfg("Options",
                                                 "scrollback_lines")->v.i);
    vte_terminal_set_allow_hyperlink(VTE_TERMINAL(t->term), TRUE);

    gdk_rgba_parse(&c_foreground_gdk, cfg("Colors", "foreground")->v.s);
    gdk_rgba_parse(&c_background_gdk, cfg("Colors", "background")->v.s);
    for (i = 0; i < 16; i++)
        gdk_rgba_parse(&c_palette_gdk[i], cfg("Colors", standard16order[i])->v.s);
    vte_terminal_set_colors(VTE_TERMINAL(t->term), &c_foreground_gdk,
                            &c_background_gdk, c_palette_gdk, 16);

    if (cfg("Colors", "bold")->v.s != NULL)
    {
        gdk_rgba_parse(&c_gdk, cfg("Colors", "bold")->v.s);
        vte_terminal_set_color_bold(VTE_TERMINAL(t->term), &c_gdk);
    }
    else
        vte_terminal_set_color_bold(VTE_TERMINAL(t->term), NULL);

    if (cfg("Colors", "cursor")->v.s != NULL)
    {
        gdk_rgba_parse(&c_gdk, cfg("Colors", "cursor")->v.s);
        vte_terminal_set_color_cursor(VTE_TERMINAL(t->term), &c_gdk);
    }
    else
        vte_terminal_set_color_cursor(VTE_TERMINAL(t->term), NULL);

    if (cfg("Colors", "cursor_foreground")->v.s != NULL)
    {
        gdk_rgba_parse(&c_gdk, cfg("Colors", "cursor_foreground")->v.s);
        vte_terminal_set_color_cursor_foreground(VTE_TERMINAL(t->term), &c_gdk);
    }
    else
        vte_terminal_set_color_cursor_foreground(VTE_TERMINAL(t->term), NULL);

    link_regex = cfg("Options", "link_regex")->v.s;
    url_vregex = vte_regex_new_for_match(link_regex, strlen(link_regex),
                                         PCRE2_MULTILINE | PCRE2_CASELESS, &err);
    if (url_vregex == NULL)
    {
        fprintf(stderr, __NAME__": link_regex: %s\n", safe_emsg(err));
        g_clear_error(&err);
    }
    else
    {
        vte_terminal_match_add_regex(VTE_TERMINAL(t->term), url_vregex, 0);
        vte_regex_unref(url_vregex);
    }

    ipaddr_regex = cfg("Options", "ipaddr_regex")->v.s;
    ipaddr_vregex = vte_regex_new_for_match(ipaddr_regex, strlen(ipaddr_regex),
                                         PCRE2_MULTILINE | PCRE2_CASELESS, &err);
    vte_terminal_match_add_regex(
        VTE_TERMINAL(t->term),
        ipaddr_vregex,
        0
    );

    /* Signals. */
    g_signal_connect(G_OBJECT(t->term), "bell",
                     G_CALLBACK(sig_bell), t);
    g_signal_connect(G_OBJECT(t->term), "button-press-event",
                     G_CALLBACK(sig_button_press), t);
    g_signal_connect(G_OBJECT(t->term), "char-size-changed",
                     G_CALLBACK(sig_char_size_changed), t);
    g_signal_connect(G_OBJECT(t->term), "child-exited",
                     G_CALLBACK(sig_child_exited), t);
    g_signal_connect(G_OBJECT(t->term), "hyperlink-hover-uri-changed",
                     G_CALLBACK(sig_hyperlink_changed), t);
    g_signal_connect(G_OBJECT(t->term), "key-press-event",
                     G_CALLBACK(sig_key_press), t);
    g_signal_connect(G_OBJECT(t->term), "motion-notify-event",
                     G_CALLBACK(sig_motion_notify_event), t);
    g_signal_connect(G_OBJECT(t->term), "resize-window",
                     G_CALLBACK(sig_window_resize), t);
    g_signal_connect(G_OBJECT(t->term), "window-title-changed",
                     G_CALLBACK(sig_window_title_changed), t);

    /* Spawn child. */
    if (argv_cmdline != NULL)
    {
        args_use = argv_cmdline;
        spawn_flags = G_SPAWN_SEARCH_PATH;
    }
    else
    {
        if (args_default[0] == NULL)
        {
            args_default[0] = vte_get_user_shell();
            if (args_default[0] == NULL)
                args_default[0] = "/bin/sh";
            if (cfg("Options", "login_shell")->v.b)
                args_default[1] = g_strdup_printf("-%s", args_default[0]);
            else
                args_default[1] = args_default[0];
        }
        args_use = args_default;
        spawn_flags = G_SPAWN_SEARCH_PATH | G_SPAWN_FILE_AND_ARGV_ZERO;
    }

    vte_terminal_spawn_async(VTE_TERMINAL(t->term), VTE_PTY_DEFAULT, NULL,
                             args_use, NULL, spawn_flags, NULL, NULL, NULL, -1,
                             NULL, cb_spawn_async, t);
}

void
term_activate_current_font(struct Terminal *t, gboolean win_ready)
{
    PangoFontDescription *font_desc = NULL;
    glong width, height;

    if (t->current_font >= cfg("Options", "fonts")->l)
    {
        fprintf(stderr, __NAME__": Warning: Invalid font index\n");
        return;
    }

    width = vte_terminal_get_column_count(VTE_TERMINAL(t->term));
    height = vte_terminal_get_row_count(VTE_TERMINAL(t->term));

    font_desc = pango_font_description_from_string(
        cfg("Options", "fonts")->v.sl[t->current_font]
    );
    vte_terminal_set_font(VTE_TERMINAL(t->term), font_desc);
    pango_font_description_free(font_desc);
    vte_terminal_set_font_scale(VTE_TERMINAL(t->term), 1);

    term_set_size(t, width, height, win_ready);
}

void
term_change_font_scale(struct Terminal *t, gint direction)
{
    gdouble s;
    glong width, height;

    width = vte_terminal_get_column_count(VTE_TERMINAL(t->term));
    height = vte_terminal_get_row_count(VTE_TERMINAL(t->term));

    if (direction != 0)
    {
        s = vte_terminal_get_font_scale(VTE_TERMINAL(t->term));
        s *= direction > 0 ? 1.1 : 1.0 / 1.1;
    }
    else
        s = 1;
    vte_terminal_set_font_scale(VTE_TERMINAL(t->term), s);
    term_set_size(t, width, height, TRUE);
}

void
term_set_geometry_hints(struct Terminal *t)
{
    GtkStyleContext *context;
    GtkBorder padding;
    GdkGeometry hints;
    int min_columns = 16, min_rows = 2;  /* From VTE source. Arbitrary anyway. */
    GdkWindowHints flags = GDK_HINT_RESIZE_INC |
                           GDK_HINT_MIN_SIZE |
                           GDK_HINT_BASE_SIZE;

    /* The following is mostly copied from VTE's source code. I don't
     * want use the VTE functions directly because they're deprecated in
     * VTE and might vanish at some point. */

    context = gtk_widget_get_style_context(t->term);
    gtk_style_context_get_padding(context,
                                  gtk_style_context_get_state(context),
                                  &padding);

    hints.base_width  = padding.left + padding.right;
    hints.base_height = padding.top  + padding.bottom;
    hints.width_inc   = vte_terminal_get_char_width(VTE_TERMINAL(t->term));
    hints.height_inc  = vte_terminal_get_char_height(VTE_TERMINAL(t->term));
    hints.min_width   = hints.base_width  + hints.width_inc  * min_columns;
    hints.min_height  = hints.base_height + hints.height_inc * min_rows;

    gtk_window_set_geometry_hints(GTK_WINDOW(t->win), NULL, &hints, flags);
}

void
term_set_size(struct Terminal *t, glong width, glong height, gboolean win_ready)
{
    GtkRequisition natural;

    /* This resizes the window to the exact size of the child widget.
     * This works even if the child uses padding or other cosmetic
     * attributes, and we don't need to know anything about it. */
    if (width > 0 && height > 0)
    {
        vte_terminal_set_size(VTE_TERMINAL(t->term), width, height);

        /* Widgets might not be fully realized yet, when called during
         * early initialization. */
        if (win_ready)
        {
            gtk_widget_get_preferred_size(t->term, NULL, &natural);
            gtk_window_resize(GTK_WINDOW(t->win), natural.width, natural.height);
        }
    }
}

int
main(int argc, char **argv)
{
    struct Terminal t = {0};

    gtk_init(&argc, &argv);
    term_new(&t, argc, argv);
    gtk_main();
}
