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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glx.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bugle/hashtable.h>
#include <bugle/tracker.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/glutils.h>
#include <bugle/glreflect.h>
#include <budgie/call.h>
#include "xalloc.h"

typedef struct
{
    bool *flags;
    hash_table names;
} context_extensions;

static object_view trackextensions_view = 0;

/* Extracts all the words from str, and sets arbitrary non-NULL values in
 * the hash table for each token. It does NOT clear the hash table first.
 */
static void tokenise(const char *str, hash_table *tokens)
{
    const char *p, *q;
    char *tmp;

    tmp = xcharalloc(strlen(str) + 1);
    p = str;
    while (*p == ' ') p++;
    while (*p != '\0')
    {
        q = p;
        while (*q != ' ' && *q != '\0')
            q++;
        memcpy(tmp, p, q - p);
        tmp[q - p] = '\0';
        bugle_hash_set(tokens, tmp, tokens);
        p = q;
        while (*p == ' ') p++;
    }
    free(tmp);
}

static void context_init(const void *key, void *data)
{
    context_extensions *ce;
    const char *glver, *glexts, *glxexts = NULL;
    int glx_major = 0, glx_minor = 0;
    bugle_gl_extension i;
    Display *dpy;

    ce = (context_extensions *) data;
    ce->flags = XCALLOC(bugle_gl_extension_count(), bool);
    bugle_hash_init(&ce->names, NULL);

    glexts = (const char *) CALL(glGetString)(GL_EXTENSIONS);
    glver = (const char *) CALL(glGetString)(GL_VERSION);
    /* Don't lock, because we're already inside a lock */
    dpy = bugle_get_current_display_internal(false);
    if (dpy)
    {
        CALL(glXQueryVersion)(dpy, &glx_major, &glx_minor);
        glxexts = CALL(glXQueryExtensionsString)(dpy, 0); /* FIXME: get screen number */
    }

    tokenise(glexts, &ce->names);
    if (glxexts) tokenise(glxexts, &ce->names);
    for (i = 0; i < bugle_gl_extension_count(); i++)
    {
        const char *name, *ver;
        name = bugle_gl_extension_name(i);
        ver = bugle_gl_extension_version(i);
        if (ver)
        {
            if (!bugle_gl_extension_is_glx(i))
                ce->flags[i] = strcmp(glver, ver) >= 0;
            else if (dpy)
            {
                int major = 0, minor = 0;
                sscanf(ver, "%d.%d", &major, &minor);
                ce->flags[i] = glx_major > major
                    || (glx_major == major && glx_minor >= minor);
            }
            if (ce->flags[i])
                bugle_hash_set(&ce->names, name, ce); /* arbitrary non-NULL */
        }
        else
            ce->flags[i] = bugle_hash_count(&ce->names, name);
    }
}

static void context_clear(void *data)
{
    context_extensions *ce;

    ce = (context_extensions *) data;
    free(ce->flags);
    bugle_hash_clear(&ce->names);
}

static bool trackextensions_filter_set_initialise(filter_set *handle)
{
    trackextensions_view = bugle_object_view_new(bugle_context_class,
                                                 context_init,
                                                 context_clear,
                                                 sizeof(context_extensions));
    return true;
}

/* The output can be inverted by passing ~ext instead of ext (which basically
 * means "true if this extension is not present"). This is used in the
 * state tables.
 */
bool bugle_gl_has_extension(bugle_gl_extension ext)
{
    const context_extensions *ce;

    /* bugle_gl_extension_id returns -1 for unknown extensions - play it safe */
    if (ext == NULL_EXTENSION) return false;
    if (ext < 0) return !bugle_gl_has_extension(~ext);
    assert(ext < bugle_gl_extension_count());
    ce = (const context_extensions *) bugle_object_get_current_data(bugle_context_class, trackextensions_view);
    if (!ce) return false;
    else return ce->flags[ext];
}

bool bugle_gl_has_extension2(int ext, const char *name)
{
    const context_extensions *ce;

    assert(ext >= -1 && ext < bugle_gl_extension_count());
    /* bugle_gl_extension_id returns -1 for unknown extensions - play it safe */
    ce = (const context_extensions *) bugle_object_get_current_data(bugle_context_class, trackextensions_view);
    if (!ce) return false;
    if (ext >= 0)
        return ce->flags[ext];
    else
        return bugle_hash_count(&ce->names, name);
}

/* The output can be inverted by passing ~ext instead of ext (which basically
 * means "true if none of these extensions are present").
 */
bool bugle_gl_has_extension_group(bugle_gl_extension ext)
{
    const context_extensions *ce;
    size_t i;
    const bugle_gl_extension *exts;

    if (ext < 0) return !bugle_gl_has_extension_group(~ext);
    assert(ext < bugle_gl_extension_count());
    ce = (const context_extensions *) bugle_object_get_current_data(bugle_context_class, trackextensions_view);
    if (!ce) return false;
    exts = bugle_gl_extension_group_members(ext);

    for (i = 0; exts[i] != NULL_EXTENSION; i++)
        if (ce->flags[exts[i]]) return true;
    return false;
}

bool bugle_gl_has_extension_group2(bugle_gl_extension ext, const char *name)
{
    return (ext == NULL_EXTENSION)
        ? bugle_gl_has_extension2(ext, name) : bugle_gl_has_extension_group(ext);
}

void trackextensions_initialise(void)
{
    static const filter_set_info trackextensions_info =
    {
        "trackextensions",
        trackextensions_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* No documentation */
    };

    bugle_filter_set_new(&trackextensions_info);

    bugle_filter_set_renders("trackextensions");
}
