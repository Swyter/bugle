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

/* This is not an OOP layer, like gobject. See doc/objects.html for an
 * explanation.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/linkedlist.h"
#include "common/safemem.h"
#include "common/bool.h"
#include "common/threads.h"
#include "src/objects.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

typedef struct
{
    void (*constructor)(const void *key, void *data);
    void (*destructor)(void *data);
    size_t size;
} object_class_info;

void bugle_object_class_init(bugle_object_class *klass, bugle_object_class *parent)
{
    bugle_list_init(&klass->info, true);
    klass->parent = parent;
    klass->count = 0;
    if (parent)
        klass->parent_view = bugle_object_class_register(parent, NULL, NULL, sizeof(bugle_object *));
    else
        bugle_thread_key_create(&klass->current, NULL);
}

void bugle_object_class_clear(bugle_object_class *klass)
{
    bugle_list_clear(&klass->info);
    if (!klass->parent)
        bugle_thread_key_delete(klass->current);
}

bugle_object_view bugle_object_class_register(bugle_object_class *klass,
                                              void (*constructor)(const void *key, void *data),
                                              void (*destructor)(void *data),
                                              size_t size)
{
    object_class_info *info;

    info = bugle_malloc(sizeof(object_class_info));
    info->constructor = constructor;
    info->destructor = destructor;
    info->size = size;
    bugle_list_append(&klass->info, info);
    return klass->count++;
}

bugle_object *bugle_object_new(const bugle_object_class *klass, const void *key, bool make_current)
{
    bugle_object *obj;
    bugle_list_node *i;
    const object_class_info *info;
    size_t j;

    obj = bugle_malloc(sizeof(bugle_object) + klass->count * sizeof(void *) - sizeof(void *));
    obj->klass = klass;
    obj->count = klass->count;

    for (i = bugle_list_head(&klass->info), j = 0; i; i = bugle_list_next(i), j++)
    {
        info = (const object_class_info *) bugle_list_data(i);
        if (info->size)
        {
            obj->views[j] = bugle_malloc(info->size);
            memset(obj->views[j], 0, info->size);
        }
        else
            obj->views[j] = NULL;
    }

    if (make_current) bugle_object_set_current(klass, obj);

    for (i = bugle_list_head(&klass->info), j = 0; i; i = bugle_list_next(i), j++)
    {
        info = (const object_class_info *) bugle_list_data(i);
        if (info->constructor)
            (*info->constructor)(key, obj->views[j]);
    }
    return obj;
}

void bugle_object_delete(const bugle_object_class *klass, bugle_object *obj)
{
    bugle_list_node *i;
    size_t j;
    const object_class_info *info;

    for (i = bugle_list_head(&klass->info), j = 0; i; i = bugle_list_next(i), j++)
    {
        info = (const object_class_info *) bugle_list_data(i);
        if (info->destructor)
            (*info->destructor)(obj->views[j]);
        free(obj->views[j]);
    }
    free(obj);
}

bugle_object *bugle_object_get_current(const bugle_object_class *klass)
{
    void *ans;

    if (klass->parent)
    {
        ans = bugle_object_get_current_data(klass->parent, klass->parent_view);
        if (!ans) return NULL;
        else return *(bugle_object **) ans;
    }
    else
        return (bugle_object *) bugle_thread_getspecific(klass->current);
}

void *bugle_object_get_current_data(const bugle_object_class *klass, bugle_object_view view)
{
    return bugle_object_get_data(klass, bugle_object_get_current(klass), view);
}

void bugle_object_set_current(const bugle_object_class *klass, bugle_object *obj)
{
    void *tmp;

    if (klass->parent)
    {
        tmp = bugle_object_get_current_data(klass->parent, klass->parent_view);
        if (tmp) *(bugle_object **) tmp = obj;
    }
    else
        bugle_thread_setspecific(klass->current, (void *) obj);
}

void *bugle_object_get_data(const bugle_object_class *klass, bugle_object *obj, bugle_object_view view)
{
    if (!obj) return NULL;
    else return obj->views[view];
}