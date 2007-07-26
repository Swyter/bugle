/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Shader pane. */

#if HAVE_CONFIG_H
# include <config.h>
#endif
/* FIXME: Not sure if this is the correct definition of GETTEXT_PACKAGE... */
#ifndef GETTEXT_PACKAGE
# define GETTEXT_PACKAGE "bugle00"
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>
#include "common/protocol.h"
#include "common/safemem.h"
#include "common/radixtree.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-shader.h"

struct _GldbShaderPane
{
    GldbPane parent;

    /* private */
    gboolean dirty;
    GtkWidget *top_widget;
    GtkWidget *id;
    GtkWidget *source;
};

struct _GldbShaderPaneClass
{
    GldbPaneClass parent;
};

enum
{
    COLUMN_SHADER_ID_ID,
    COLUMN_SHADER_ID_TARGET,
    COLUMN_SHADER_ID_BOLD,
    COLUMN_SHADER_ID_TEXT
};

static gboolean gldb_shader_pane_response_callback(gldb_response *response,
                                                   gpointer user_data)
{
    gldb_response_data_shader *r;
    GtkTextBuffer *buffer;
    GldbShaderPane *pane;

    pane = GLDB_SHADER_PANE(user_data);
    r = (gldb_response_data_shader *) response;
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->source));
    if (response->code != RESP_DATA || !r->length)
        gtk_text_buffer_set_text(buffer, "", 0);
    else
        gtk_text_buffer_set_text(buffer, r->data, r->length);
    gldb_free_response(response);
    return TRUE;
}

static void gldb_shader_pane_update_ids(GldbShaderPane *pane)
{
    gldb_state *root, *s, *t, *u;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GValue old[2];
    bugle_list_node *nt;
    guint trg;
    char *name;
    guint active_arb[2] = {0, 0};
    bugle_radix_tree active_glsl;
    GLuint program;

    const GLenum targets[] =
    {
        GL_VERTEX_PROGRAM_ARB,
        GL_FRAGMENT_PROGRAM_ARB,
        GL_VERTEX_SHADER_ARB,
#ifdef GL_EXT_geometry_shader4
        GL_GEOMETRY_SHADER_EXT,
#endif
        GL_FRAGMENT_SHADER_ARB,
    };
    const gchar *target_names[] =
    {
        "ARB VP",
        "ARB FP",
        "GLSL VS",
#ifdef GL_EXT_geometry_shader4
        "GLSL GS",
#endif
        "GLSL FS"
    };

    gldb_gui_combo_box_get_old(GTK_COMBO_BOX(pane->id), old,
                               COLUMN_SHADER_ID_ID, COLUMN_SHADER_ID_TARGET, -1);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
    gtk_list_store_clear(GTK_LIST_STORE(model));

    root = gldb_state_update();

    /* Identify active shaders */
    for (trg = 0; trg < 2; trg++)
    {
        s = gldb_state_find_child_enum(root, targets[trg]);
        if (!s) continue;
        t = gldb_state_find_child_enum(s, GL_PROGRAM_BINDING_ARB);
        if (t)
            active_arb[trg] = *(GLint *) t->data;
    }

    bugle_radix_tree_init(&active_glsl, false);
#ifdef GL_VERSION_2_0
    s = gldb_state_find_child_enum(root, GL_CURRENT_PROGRAM);
    program = s ? gldb_state_GLint(s) : 0;
    if (program)
    {
        bugle_asprintf(&name, "Program[%d]", program);
        s = gldb_state_find(root, name, strlen(name));
        free(name);
        if (s)
        {
            t = gldb_state_find(s, "Attached", strlen("Attached"));
            if (t)
            {
                size_t i;
                GLuint *ids;
                ids = (GLuint *) t->data;
                for (i = 0; i < t->length; i++)
                    bugle_radix_tree_set(&active_glsl, ids[i], root); /* arbitrary non-NULL */
            }
        }
    }
#endif

    for (trg = 0; trg < G_N_ELEMENTS(targets); trg++)
        switch (targets[trg])
        {
        case GL_VERTEX_PROGRAM_ARB:
        case GL_FRAGMENT_PROGRAM_ARB:
            s = gldb_state_find_child_enum(root, targets[trg]);
            if (!s) continue;
            for (nt = bugle_list_head(&s->children); nt != NULL; nt = bugle_list_next(nt))
            {
                t = (gldb_state *) bugle_list_data(nt);
                if (t->enum_name == 0 && t->name[0] >= '0' && t->name[0] <= '9')
                {
                    bugle_asprintf(&name, "%d (%s)",
                                   t->numeric_name, target_names[trg]);
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                       COLUMN_SHADER_ID_ID, (guint) t->numeric_name,
                                       COLUMN_SHADER_ID_TARGET, targets[trg],
                                       COLUMN_SHADER_ID_BOLD, (t->numeric_name == active_arb[trg]) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                                       COLUMN_SHADER_ID_TEXT, name,
                                       -1);
                    free(name);
                }
            }
            break;
        case GL_VERTEX_SHADER_ARB:
        case GL_FRAGMENT_SHADER_ARB:
#ifdef GL_EXT_geometry_shader4
        case GL_GEOMETRY_SHADER_EXT:
#endif
            for (nt = bugle_list_head(&root->children); nt != NULL; nt = bugle_list_next(nt))
            {
                t = (gldb_state *) bugle_list_data(nt);
                u = gldb_state_find_child_enum(t, GL_OBJECT_SUBTYPE_ARB);
                if (u && gldb_state_GLenum(u) == targets[trg])
                {
                    bool active;

                    active = bugle_radix_tree_get(&active_glsl, t->numeric_name);
                    bugle_asprintf(&name, "%d (%s)",
                                   t->numeric_name, target_names[trg]);
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                       COLUMN_SHADER_ID_ID, (guint) t->numeric_name,
                                       COLUMN_SHADER_ID_TARGET, targets[trg],
                                       COLUMN_SHADER_ID_BOLD, active ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                                       COLUMN_SHADER_ID_TEXT, name,
                                       -1);
                    free(name);
                }
            }
            break;
        }

    gldb_gui_combo_box_restore_old(GTK_COMBO_BOX(pane->id), old,
                                   COLUMN_SHADER_ID_ID, COLUMN_SHADER_ID_TARGET, -1);
    bugle_radix_tree_clear(&active_glsl);
}

static void gldb_shader_pane_id_changed(GtkComboBox *id_box, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint target, id;
    GldbShaderPane *pane;
    guint32 seq;

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        pane = GLDB_SHADER_PANE(user_data);

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(pane->id), &iter))
            return;
        gtk_tree_model_get(model, &iter,
                           COLUMN_SHADER_ID_ID, &id,
                           COLUMN_SHADER_ID_TARGET, &target,
                           -1);

        seq = gldb_gui_set_response_handler(gldb_shader_pane_response_callback, pane);
        gldb_send_data_shader(seq, id, target);
    }
}

static gboolean gldb_shader_pane_source_expose(GtkWidget *widget,
                                               GdkEventExpose *event,
                                               gpointer user_data)
{
    PangoLayout *layout;
    gint lines, firsty, lasty, dummy;
    GtkTextBuffer *buffer;
    GtkTextIter line;

    /* Check that this is an expose on the line number area */
    if (gtk_text_view_get_window_type(GTK_TEXT_VIEW(widget), event->window)
        != GTK_TEXT_WINDOW_LEFT)
        return FALSE;

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
    lines = gtk_text_buffer_get_line_count(buffer);
    layout = gtk_widget_create_pango_layout(widget, NULL);

    /* Find the first line that is in the expose area */
    firsty = event->area.y;
    lasty = event->area.y + event->area.height;
    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
                                          GTK_TEXT_WINDOW_LEFT,
                                          0, firsty,
                                          &dummy, &firsty);
    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
                                          GTK_TEXT_WINDOW_LEFT,
                                          0, lasty,
                                          &dummy, &lasty);
    gtk_text_view_get_line_at_y(GTK_TEXT_VIEW(widget), &line, firsty, NULL);

    /* Loop over the visible lines, displaying the line number */
    while (!gtk_text_iter_is_end(&line))
    {
        int y, height;
        int text_width, text_height;
        int winx, winy;
        char *no;

        gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(widget), &line,
                                      &y, &height);
        gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(widget),
                                              GTK_TEXT_WINDOW_LEFT,
                                              0, y, &winx, &winy);
        bugle_asprintf(&no, "%d", (int) gtk_text_iter_get_line(&line) + 1);
        pango_layout_set_text(layout, no, -1);
        free(no);
        pango_layout_get_pixel_size(layout, &text_width, &text_height);
        gtk_paint_layout(gtk_widget_get_style(widget), event->window,
                         GTK_STATE_NORMAL, FALSE, NULL,
                         widget, NULL, 35 - text_width, winy, layout);
        if (y + height >= lasty) break;
        gtk_text_iter_forward_line(&line);
    }
    g_object_unref(layout);
    return FALSE;
}

static GtkWidget *gldb_shader_pane_id_new(GldbShaderPane *pane)
{
    GtkListStore *store;
    GtkWidget *id;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(4, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INT, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
                                         COLUMN_SHADER_ID_ID,
                                         GTK_SORT_ASCENDING);
    id = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(id), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(id), cell,
                                   "text", COLUMN_SHADER_ID_TEXT,
                                   "weight", COLUMN_SHADER_ID_BOLD,
                                   NULL);
    g_signal_connect(G_OBJECT(id), "changed",
                     G_CALLBACK(gldb_shader_pane_id_changed), pane);
    g_object_unref(G_OBJECT(store));

    return pane->id = id;
}

GldbPane *gldb_shader_pane_new(void)
{
    GldbShaderPane *pane;
    GtkWidget *vbox, *hbox, *id, *source, *scrolled;
    PangoFontDescription *font;

    pane = GLDB_SHADER_PANE(g_object_new(GLDB_SHADER_PANE_TYPE, NULL));
    id = gldb_shader_pane_id_new(pane);

    font = pango_font_description_new();
    pango_font_description_set_family_static(font, "Monospace");
    source = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(source), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(source), FALSE);
    gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(source),
                                         GTK_TEXT_WINDOW_LEFT,
                                         40);
    gtk_widget_modify_font(GTK_WIDGET(source), font);
    pango_font_description_free(font);
    g_signal_connect(G_OBJECT(source), "expose-event", G_CALLBACK(gldb_shader_pane_source_expose), NULL);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), source);

    vbox = gtk_vbox_new(FALSE, 0);
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), id, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);

    pane->source = source;
    gldb_pane_set_widget(GLDB_PANE(pane), hbox);
    return GLDB_PANE(pane);
}

static void gldb_shader_pane_real_update(GldbPane *self)
{
    GldbShaderPane *pane;

    pane = GLDB_SHADER_PANE(self);
    gldb_shader_pane_update_ids(pane);
}

/* GObject stuff */

static void gldb_shader_pane_class_init(GldbShaderPaneClass *klass)
{
    GldbPaneClass *pane_class;

    pane_class = GLDB_PANE_CLASS(klass);
    pane_class->do_real_update = gldb_shader_pane_real_update;
}

static void gldb_shader_pane_init(GldbShaderPane *self, gpointer g_class)
{
    self->id = NULL;
}

GType gldb_shader_pane_get_type(void)
{
    static GType type = 0;
    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(GldbShaderPaneClass),
            NULL,                       /* base_init */
            NULL,                       /* base_finalize */
            (GClassInitFunc) gldb_shader_pane_class_init,
            NULL,                       /* class_finalize */
            NULL,                       /* class_data */
            sizeof(GldbShaderPane),
            0,                          /* n_preallocs */
            (GInstanceInitFunc) gldb_shader_pane_init,
            NULL                        /* value table */
        };
        type = g_type_register_static(GLDB_PANE_TYPE,
                                      "GldbShaderPaneType",
                                      &info, 0);
    }
    return type;
}
