/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include "src/filters.h"
#include "src/utils.h"
#include "src/canon.h"
#include "src/objects.h"
#include "src/tracker.h"
#include "src/glutils.h"
#include "common/bool.h"
#include "common/hashtable.h"
#include "common/threads.h"
#include "common/safemem.h"
#include <stddef.h>
#include <GL/gl.h>

bugle_object_class bugle_displaylist_class;
static bugle_hashptr_table displaylist_objects;
static bugle_thread_mutex_t displaylist_lock = BUGLE_THREAD_MUTEX_INITIALIZER;
static bugle_object_view displaylist_view;

typedef struct
{
    GLuint list;
    GLenum mode;
} displaylist_struct;

static void initialise_displaylist_struct(const void *key, void *data)
{
    memcpy(data, key, sizeof(displaylist_struct));
}

GLenum bugle_displaylist_mode(void)
{
    displaylist_struct *info;

    info = bugle_object_get_current_data(&bugle_displaylist_class, displaylist_view);
    if (!info) return GL_NONE;
    else return info->mode;
}

GLuint bugle_displaylist_list(void)
{
    displaylist_struct *info;

    info = bugle_object_get_current_data(&bugle_displaylist_class, displaylist_view);
    if (!info) return 0;
    else return info->list;
}

void *bugle_displaylist_get(GLuint list)
{
    void *ans;

    bugle_thread_mutex_lock(&displaylist_lock);
    ans = bugle_hashptr_get(&displaylist_objects, (void *) (size_t) list);
    bugle_thread_mutex_unlock(&displaylist_lock);
    return ans;
}

static bool trackdisplaylist_glNewList(function_call *call, const callback_data *data)
{
    displaylist_struct info;
    bugle_object *obj;
    GLint value;

    if (bugle_displaylist_list()) return true; /* Nested call */
    if (bugle_begin_internal_render())
    {
        CALL_glGetIntegerv(GL_LIST_INDEX, &value);
        info.list = value;
        CALL_glGetIntegerv(GL_LIST_MODE, &value);
        info.mode = value;
        if (info.list == 0) return true; /* Call failed? */
        obj = bugle_object_new(&bugle_displaylist_class, &info, true);
        bugle_end_internal_render("trackdisplaylist_callback", true);
    }
    return true;
}

static bool trackdisplaylist_glEndList(function_call *call, const callback_data *data)
{
    bugle_object *obj;
    displaylist_struct *info_ptr;

    obj = bugle_object_get_current(&bugle_displaylist_class);
    info_ptr = bugle_object_get_data(&bugle_displaylist_class, obj, displaylist_view);
    /* Note: we update the hash table when we end the list, since this is when OpenGL
     * says the new name comes into effect.
     */
    bugle_thread_mutex_lock(&displaylist_lock);
    bugle_hashptr_set(&displaylist_objects, (void *) (size_t) info_ptr->list, obj);
    bugle_thread_mutex_unlock(&displaylist_lock);
    bugle_object_set_current(&bugle_displaylist_class, NULL);
    return true;
}

static bool initialise_trackdisplaylist(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "trackdisplaylist");
    bugle_register_filter_depends("trackdisplaylist", "invoke");
    bugle_register_filter_catches(f, FUNC_glNewList, trackdisplaylist_glNewList);
    bugle_register_filter_catches(f, FUNC_glEndList, trackdisplaylist_glEndList);

    displaylist_view = bugle_object_class_register(&bugle_displaylist_class,
                                                   initialise_displaylist_struct,
                                                   NULL,
                                                   sizeof(displaylist_struct));
    return true;
}

void trackdisplaylist_initialise(void)
{
    static const filter_set_info trackdisplaylist_info =
    {
        "trackdisplaylist",
        initialise_trackdisplaylist,
        NULL,
        NULL,
        0,
        NULL /* No documentation */
    };

    bugle_object_class_init(&bugle_displaylist_class, &bugle_context_class);
    bugle_hashptr_init(&displaylist_objects, true);
    bugle_atexit((void (*)(void *)) bugle_hashptr_clear, &displaylist_objects);
    bugle_register_filter_set(&trackdisplaylist_info);

    bugle_register_filter_set_depends("trackdisplaylist", "trackcontext");
}