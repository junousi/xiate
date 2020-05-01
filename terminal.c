#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vte/vte.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "config.h"


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
void handle_history(VteTerminal *);
char *safe_emsg(GError *);
void sig_bell(VteTerminal *, gpointer);
gboolean sig_button_press(GtkWidget *, GdkEvent *, gpointer);
void sig_child_exited(VteTerminal *, gint, gpointer);
void sig_hyperlink_changed(VteTerminal *, gchar *, GdkRectangle *, gpointer);
gboolean sig_key_press(GtkWidget *, GdkEvent *, gpointer);
void sig_window_destroy(GtkWidget *, gpointer);
void sig_window_resize(VteTerminal *, guint, guint, gpointer);
void sig_window_title_changed(VteTerminal *, gpointer);
void term_new(struct Terminal *, int, char **);
void term_activate_current_font(struct Terminal *, gboolean);
void term_change_font_scale(struct Terminal *, gint);
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

void
handle_history(VteTerminal *term)
{
    GFile *tmpfile = NULL;
    GFileIOStream *io_stream = NULL;
    GOutputStream *out_stream = NULL;
    GError *err = NULL;
    char *argv[] = { history_handler, NULL, NULL };
    GSpawnFlags spawn_flags = G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH;

    tmpfile = g_file_new_tmp(NULL, &io_stream, &err);
    if (tmpfile == NULL)
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n", safe_emsg(err));
        goto free_and_out;
    }

    out_stream = g_io_stream_get_output_stream(G_IO_STREAM(io_stream));
    if (!vte_terminal_write_contents_sync(term, out_stream, VTE_WRITE_DEFAULT,
                                          NULL, &err))
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n", safe_emsg(err));
        goto free_and_out;
    }

    if (!g_io_stream_close(G_IO_STREAM(io_stream), NULL, NULL))
    {
        fprintf(stderr, __NAME__": Could not write history: %s\n", safe_emsg(err));
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

char *
safe_emsg(GError *err)
{
    return err == NULL ? "<GError is NULL>" : err->message;
}

void
sig_bell(VteTerminal *term, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;

    (void)term;

    /* Credits go to sakura. The author says:
     * Remove the urgency hint. This is necessary to signal the window
     * manager that a new urgent event happened when the urgent hint is
     * set next time.
     */
    gtk_window_set_urgency_hint(GTK_WINDOW(t->win), FALSE);
    gtk_window_set_urgency_hint(GTK_WINDOW(t->win), TRUE);
}

gboolean
sig_button_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    char *url = NULL;
    char *argv[] = { link_handler, NULL, NULL, NULL };
    GError *err = NULL;
    gboolean retval = FALSE;
    GSpawnFlags spawn_flags = G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH;

    (void)data;

    if (event->type == GDK_BUTTON_PRESS)
    {
        if (((GdkEventButton *)event)->button == 3)
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
sig_child_exited(VteTerminal *term, gint status, gpointer data)
{
    struct Terminal *t = (struct Terminal *)data;
    GdkRGBA c_background_gdk;

    t->has_child_exit_status = TRUE;
    t->child_exit_status = status;

    if (t->hold)
    {
        gdk_rgba_parse(&c_background_gdk, c_background);
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

    if (((GdkEventKey *)event)->state & GDK_CONTROL_MASK)
    {
        switch (((GdkEventKey *)event)->keyval)
        {
            case GDK_KEY_C:
                vte_terminal_copy_clipboard_format(term, VTE_FORMAT_TEXT);
                return TRUE;
            case GDK_KEY_V:
                vte_terminal_paste_clipboard(term);
                return TRUE;
            case GDK_KEY_H:
                handle_history(term);
                return TRUE;
            case GDK_KEY_N:
                t->current_font++;
                t->current_font %= sizeof fonts / sizeof fonts[0];
                term_activate_current_font(t, TRUE);
                return TRUE;
            case GDK_KEY_R:
                term_change_font_scale(t, 0);
                return TRUE;
            case GDK_KEY_O:
                term_change_font_scale(t, -1);
                return TRUE;
            case GDK_KEY_I:
                term_change_font_scale(t, 1);
                return TRUE;
        }
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
    char *title = __NAME__, *wm_class = __NAME_CAPITALIZED__, *wm_name = __NAME__;
    int i;
    GdkRGBA c_foreground_gdk;
    GdkRGBA c_background_gdk;
    GdkRGBA c_palette_gdk[16];
    GdkRGBA c_gdk;
    VteRegex *url_vregex = NULL;
    GError *err = NULL;
    GSpawnFlags spawn_flags;

    /* Handle arguments. */
    t->current_font = 0;
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-class") == 0 && i < argc - 1)
            wm_class = argv[++i];
        else if (strcmp(argv[i], "-hold") == 0)
            t->hold = TRUE;
        else if (strcmp(argv[i], "-name") == 0 && i < argc - 1)
            wm_name = argv[++i];
        else if (strcmp(argv[i], "-title") == 0 && i < argc - 1)
            title = argv[++i];
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

    /* Create GTK+ widgets. */
    t->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(t->win), title);
    gtk_window_set_wmclass(GTK_WINDOW(t->win), wm_name, wm_class);
    g_signal_connect(G_OBJECT(t->win), "destroy", G_CALLBACK(sig_window_destroy), t);

    t->term = vte_terminal_new();
    gtk_container_add(GTK_CONTAINER(t->win), t->term);

    /* Appearance. */
    term_activate_current_font(t, FALSE);
    gtk_widget_show_all(t->win);

    vte_terminal_set_bold_is_bright(VTE_TERMINAL(t->term), bold_is_bright);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(t->term), cursor_blink_mode);
    vte_terminal_set_cursor_shape(VTE_TERMINAL(t->term), cursor_shape);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(t->term), TRUE);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(t->term), scrollback_lines);
    vte_terminal_set_allow_hyperlink(VTE_TERMINAL(t->term), TRUE);

    gdk_rgba_parse(&c_foreground_gdk, c_foreground);
    gdk_rgba_parse(&c_background_gdk, c_background);
    for (i = 0; i < 16; i++)
        gdk_rgba_parse(&c_palette_gdk[i], c_palette[i]);
    vte_terminal_set_colors(VTE_TERMINAL(t->term), &c_foreground_gdk,
                            &c_background_gdk, c_palette_gdk, 16);

    if (c_bold != NULL)
    {
        gdk_rgba_parse(&c_gdk, c_bold);
        vte_terminal_set_color_bold(VTE_TERMINAL(t->term), &c_gdk);
    }

    if (c_cursor != NULL)
    {
        gdk_rgba_parse(&c_gdk, c_cursor);
        vte_terminal_set_color_cursor(VTE_TERMINAL(t->term), &c_gdk);
    }

    if (c_cursor_foreground != NULL)
    {
        gdk_rgba_parse(&c_gdk, c_cursor_foreground);
        vte_terminal_set_color_cursor_foreground(VTE_TERMINAL(t->term), &c_gdk);
    }

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

    /* Signals. */
    g_signal_connect(G_OBJECT(t->term), "bell",
                     G_CALLBACK(sig_bell), t);
    g_signal_connect(G_OBJECT(t->term), "button-press-event",
                     G_CALLBACK(sig_button_press), t);
    g_signal_connect(G_OBJECT(t->term), "child-exited",
                     G_CALLBACK(sig_child_exited), t);
    g_signal_connect(G_OBJECT(t->term), "hyperlink-hover-uri-changed",
                     G_CALLBACK(sig_hyperlink_changed), t);
    g_signal_connect(G_OBJECT(t->term), "key-press-event",
                     G_CALLBACK(sig_key_press), t);
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
            if (login_shell)
                args_default[1] = g_strdup_printf("-%s", args_default[0]);
            else
                args_default[1] = args_default[0];
        }
        args_use = args_default;
        spawn_flags = G_SPAWN_SEARCH_PATH | G_SPAWN_FILE_AND_ARGV_ZERO;
    }

    vte_terminal_spawn_async(VTE_TERMINAL(t->term), VTE_PTY_DEFAULT, NULL,
                             args_use, NULL, spawn_flags, NULL, NULL, NULL, 60,
                             NULL, cb_spawn_async, t);
}

void
term_activate_current_font(struct Terminal *t, gboolean win_ready)
{
    PangoFontDescription *font_desc = NULL;
    glong width, height;

    if (t->current_font >= sizeof fonts / sizeof fonts[0])
    {
        fprintf(stderr, __NAME__": Warning: Invalid font index\n");
        return;
    }

    width = vte_terminal_get_column_count(VTE_TERMINAL(t->term));
    height = vte_terminal_get_row_count(VTE_TERMINAL(t->term));

    font_desc = pango_font_description_from_string(fonts[t->current_font]);
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
