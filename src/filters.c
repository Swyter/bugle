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
#include "src/utils.h"
#include "filters.h"
#include "tracker.h"
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

static linked_list filter_sets;
static linked_list active_filters;
/* hash table of linked lists of strings */
static hash_table filter_dependencies;
static linked_list filter_set_dependencies[2];
static bool dirty_active = false;
static void *call_data = NULL;
static size_t call_data_size = 0, context_data_size = 0;

static void *current_dl_handle = NULL;

/* FIXME: use dlclose on the modules */
void destroy_filters(void)
{
    list_node *i, *j;
    filter_set *s;
    filter *f;
    linked_list *dep;

    list_clear(&filter_set_dependencies[0], true);
    list_clear(&filter_set_dependencies[1], true);
    list_clear(&active_filters, false);
    for (i = list_head(&filter_sets); i; i = list_next(i))
    {
        s = (filter_set *) list_data(i);
        if (s->initialised)
        {
            if (s->done)
                (*s->done)(s);
            for (j = list_head(&s->filters); j; j = list_next(j))
            {
                f = (filter *) list_data(j);
                dep = (linked_list *) hash_get(&filter_dependencies, f->name);
                if (dep)
                {
                    list_clear(dep, true);
                    free(dep);
                }
                free(f->name);
                free(f);
            }
            list_clear(&s->filters, false);
        }
        free(s->name);
        free(s);
    }
    list_clear(&filter_sets, false);
    hash_clear(&filter_dependencies, false);
}

void initialise_filters(void)
{
    DIR *dir;
    struct dirent *ent;
    char *full_name;
    size_t len;
    void *handle;
    void (*init)(void);
    const char *libdir;

    list_init(&filter_sets);
    list_init(&active_filters);
    hash_init(&filter_dependencies);
    list_init(&filter_set_dependencies[0]);
    list_init(&filter_set_dependencies[1]);

    libdir = getenv("BUGLE_FILTER_DIR");
    if (!libdir) libdir = PKGLIBDIR;
    dir = opendir(libdir);
    if (!dir)
    {
        fprintf(stderr, "failed to open %s: %s", libdir, strerror(errno));
        exit(1);
    }

    while ((ent = readdir(dir)) != NULL)
    {
        len = strlen(ent->d_name);
        if (len < 3) continue;
        if (strcmp(ent->d_name + len - 3, ".so") != 0) continue;
        full_name = (char *) xmalloc(strlen(libdir) + strlen(ent->d_name) + 2);
        sprintf(full_name, "%s/%s", libdir, ent->d_name);
        handle = dlopen(full_name, RTLD_LAZY);
        if (handle == NULL) continue;
        init = (void (*)(void)) dlsym(handle, "initialise_filter_library");
        if (init == NULL) continue;
        current_dl_handle = handle;
        (*init)();
        current_dl_handle = NULL;
        free(full_name);
    }

    closedir(dir);
    atexit(destroy_filters);
}

bool filter_set_command(filter_set *handle, const char *name, const char *value)
{
    if (!handle->command_handler) return false;
    return (*handle->command_handler)(handle, name, value);
}

void enable_filter_set(filter_set *handle)
{
    list_node *i, *j;
    filter_set *f;

    if (!handle->enabled)
    {
        if (!handle->initialised)
        {
            if (!(*handle->init)(handle))
            {
                fprintf(stderr, "Failed to initialise filter-set %s\n", handle->name);
                exit(1);
            }
            handle->initialised = true;
        }
        handle->enabled = true;
        /* deps */
        for (i = list_head(&filter_set_dependencies[0]),
             j = list_head(&filter_set_dependencies[1]);
             i != NULL;
             i = list_next(i), j = list_next(j))
        {
            if (strcmp(handle->name, (const char *) list_data(i)) == 0)
            {
                f = get_filter_set_handle((const char *) list_data(j));
                if (!f)
                {
                    fprintf(stderr, "filter-set %s depends on unknown filter-set %s\n",
                            ((const char *) list_data(i)),
                            ((const char *) list_data(j)));
                    exit(1);
                }
                enable_filter_set(f);
            }
        }
        for (i = list_head(&handle->filters); i != NULL; i = list_next(i))
            list_append(&active_filters, list_data(i));
        dirty_active = true;
    }
}

void disable_filter_set(filter_set *handle)
{
    list_node *i, *j;
    filter_set *f;

    if (handle->enabled)
    {
        handle->enabled = false;
        /* reverse deps */
        for (i = list_head(&filter_set_dependencies[0]),
             j = list_head(&filter_set_dependencies[1]);
             i != NULL;
             i = list_next(i), j = list_next(j))
        {
            if (strcmp(handle->name, (const char *) list_data(j)) == 0)
            {
                f = get_filter_set_handle((const char *) list_data(i));
                disable_filter_set(f);
            }
        }
        i = list_head(&active_filters);
        while (i)
        {
            j = list_next(i);
            if (((filter *) list_data(i))->parent == handle)
                list_erase(&active_filters, i, false);
            i = j;
        }
        dirty_active = true;
    }
}

void repair_filter_order(void)
{
    list_node *i, *j;
    linked_list active; /* replacement for active_filters */
    linked_list *deps;
    linked_list queue;
    hash_table names;   /* maps filter names to filters */
    hash_table valence; /* for re-ordering */
    filter *cur;
    /* We encode integers into pointers as base + n, so base is arbitrary
     * but must be non-NULL (so that NULL is different from 0)
     */
    char base[] = "", *d;
    const char *name;
    int count = 0; /* checks that everything made it without cycles */

    list_init(&active);
    hash_init(&valence);
    hash_init(&names);
    /* initialise name table, and set valence table to all 0's */
    for (i = list_head(&active_filters); i; i = list_next(i))
    {
        count++;
        cur = (filter *) list_data(i);
        hash_set(&names, cur->name, cur);
        hash_set(&valence, cur->name, base);
    }
    /* fill in valences */
    for (i = list_head(&active_filters); i; i = list_next(i))
    {
        cur = (filter *) list_data(i);
        deps = (linked_list *) hash_get(&filter_dependencies, cur->name);
        if (deps)
        {
            for (j = list_head(deps); j; j = list_next(j))
            {
                name = (const char *) list_data(j);
                d = hash_get(&valence, name);
                if (d) /* otherwise a non-existant filter */
                {
                    d++;
                    hash_set(&valence, name, d);
                }
            }
        }
    }
    /* prime the queue */
    list_init(&queue);
    for (i = list_head(&active_filters); i; i = list_next(i))
    {
        cur = (filter *) list_data(i);
        if (hash_get(&valence, cur->name) == base)
            list_append(&queue, cur);
    }
    /* do a topological walk, starting at the back */
    while (list_head(&queue))
    {
        count--;
        cur = (filter *) list_data(list_head(&queue));
        list_erase(&queue, list_head(&queue), false);
        list_prepend(&active, cur);
        deps = (linked_list *) hash_get(&filter_dependencies, cur->name);
        if (deps)
        {
            for (j = list_head(deps); j; j = list_next(j))
            {
                name = (const char *) list_data(j);
                d = hash_get(&valence, name);
                if (d) /* otherwise a non-existant filter */
                {
                    d--;
                    hash_set(&valence, name, d);
                    if (d == base)
                        list_append(&queue, hash_get(&names, name));
                }
            }
        }
    }
    if (count > 0)
    {
        fprintf(stderr, "cyclic dependency between filters, aborting\n");
        exit(1);
    }
    /* replace the old */
    list_clear(&active_filters, false);
    memcpy(&active_filters, &active, sizeof(active));
    /* clean up */
    list_clear(&queue, false);
    hash_clear(&valence, false);
    hash_clear(&names, false);
}

void *get_filter_set_call_state(function_call *call, filter_set *handle)
{
    if (handle && handle->call_state_offset >= 0)
        return (void *)(((char *) call->generic.user_data) + handle->call_state_offset);
    else
        return NULL;
}

void *get_filter_set_context_state(state_7context_I *ctx, filter_set *handle)
{
    if (ctx && ctx->c_internal.c_filterdata.data
        && handle->context_state_offset >= 0)
        return (void *)(((char *) ctx->c_internal.c_filterdata.data) + handle->context_state_offset);
    else
        return NULL;
}

void run_filters(function_call *call)
{
    list_node *i;
    filter *cur;
    callback_data data;

    if (dirty_active)
    {
        dirty_active = false;
        repair_filter_order();
    }

    call->generic.user_data = call_data;
    for (i = list_head(&active_filters); i != NULL; i = list_next(i))
    {
        cur = (filter *) list_data(i);
        data.call_data = get_filter_set_call_state(call, cur->parent);
        /* FIXME: implement */
        data.context_data = get_filter_set_context_state(tracker_get_context_state(),
                                                         cur->parent);
        if (!(*cur->callback)(call, &data)) break;
    }
}

filter_set *register_filter_set(const filter_set_info *info)
{
    filter_set *s;

    s = (filter_set *) xmalloc(sizeof(filter_set));
    s->name = xstrdup(info->name);
    list_init(&s->filters);
    s->init = info->init;
    s->done = info->done;
    s->command_handler = info->command_handler;
    s->initialised = false;
    s->enabled = false;
    s->dl_handle = current_dl_handle;
    if (info->call_state_space)
    {
        s->call_state_offset = call_data_size;
        call_data_size += info->call_state_space;
        call_data = xrealloc(call_data, call_data_size);
    }
    else s->call_state_offset = (ptrdiff_t) -1;
    if (info->context_state_space)
    {
        s->context_state_offset = context_data_size;
        context_data_size += info->context_state_space;
        tracker_set_context_space(context_data_size);
    }
    else s->context_state_offset = (ptrdiff_t) -1;

    list_append(&filter_sets, s);
    return s;
}

void register_filter(filter_set *handle, const char *name,
                     filter_callback callback)
{
    filter *f;

    f = (filter *) xmalloc(sizeof(filter));
    f->name = xstrdup(name);
    f->callback = callback;
    f->parent = handle;
    list_append(&handle->filters, f);
}

void register_filter_depends(const char *after, const char *before)
{
    linked_list *deps;
    deps = (linked_list *) hash_get(&filter_dependencies, after);
    if (!deps)
    {
        deps = xmalloc(sizeof(linked_list));
        list_init(deps);
        hash_set(&filter_dependencies, after, deps);
    }
    list_append(deps, xstrdup(before));
}

void register_filter_set_depends(const char *base, const char *dep)
{
    list_append(&filter_set_dependencies[0], xstrdup(base));
    list_append(&filter_set_dependencies[1], xstrdup(dep));
}

bool filter_set_is_enabled(const filter_set *handle)
{
    assert(handle);
    return handle->enabled;
}

filter_set *get_filter_set_handle(const char *name)
{
    list_node *i;
    filter_set *cur;

    for (i = list_head(&filter_sets); i != NULL; i = list_next(i))
    {
        cur = (filter_set *) list_data(i);
        if (strcmp(name, cur->name) == 0)
            return cur;
    }
    return NULL;
}

void *get_filter_set_symbol(filter_set *handle, const char *name)
{
    if (handle)
        return dlsym(handle->dl_handle, name);
    else
    {
        void *h, *sym = NULL;
        h = dlopen(NULL, RTLD_LAZY);
        if (h)
        {
            sym = dlsym(h, name);
            dlclose(h);
        }
        return sym;
    }
}
