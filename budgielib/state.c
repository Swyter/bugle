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
#include "state.h"
#include "common/safemem.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

extern const state_spec *root_state_spec; /* defined in utils.c */
static struct state_root_s *root_state = NULL;

void update_state(state_generic *state)
{
    if (state->spec->updater)
        (*state->spec->updater)(state);
}

void update_state_recursive(state_generic *state)
{
    state_generic **i;

    update_state(state);
    for (i = state->children; *i; i++)
        update_state_recursive(*i);
}

void initialise_state(state_generic *s, state_generic *parent)
{
    /* other fields are set by constructors */
    s->key = NULL;
    s->parent = parent;
    s->indexed = NULL;
    s->num_indexed = 0;
    s->max_indexed = 0;
}

state_generic *create_state(const state_spec *spec, state_generic *parent)
{
    state_generic *s;

    s = xcalloc(1, spec->instance_size);
    s->name = (char *) spec->name;
    (*spec->constructor)(s, parent);
    return s;
}

void destroy_state(state_generic *state, bool indexed)
{
    int i;

    for (i = 0; i < state->num_indexed; i++)
        destroy_state(state->indexed[i], true);
    for (i = 0; state->children[i]; i++)
        destroy_state(state->children[i], false);
    if (state->indexed) free(state->indexed);
    if (indexed)
    {
        if (state->name) free(state->name);
        free(state);
    }
}

void *get_state_cached(state_generic *state)
{
    return state->data;
}

void *get_state_current(state_generic *state)
{
    update_state(state);
    return state->data;
}

static pthread_once_t root_once = PTHREAD_ONCE_INIT;
static void create_root_state(void)
{
    root_state = (struct state_root_s *) create_state(root_state_spec, NULL);
}

struct state_root_s *get_root_state(void)
{
    pthread_once(&root_once, create_root_state);
    return root_state;
}

typedef struct
{
    budgie_type type;
    int count;
    const void *value;
} dump_any_type_str_data;
/* A driver for string_io, that wraps dump_any_type */
static void dump_any_type_str(FILE *f, void *data)
{
    const dump_any_type_str_data *d;

    d = (const dump_any_type_str_data *) data;
    dump_any_type(d->type, d->value, d->count, f);
}

/* index-specific stuff */
state_generic *add_state_index(state_generic *node, const void *key, const char *name)
{
    state_generic *s;
    int i, j;

    if (node->num_indexed == node->max_indexed)
    {
        if (!node->max_indexed) node->max_indexed = 4;
        else node->max_indexed *= 2;
        node->indexed = realloc(node->indexed, node->max_indexed * sizeof(state_generic *));
    }
    s = create_state(node->spec->indexed, node);
    if (s->spec->key_type != NULL_TYPE)
        memcpy(s->key, key, type_table[s->spec->key_type].size);
    if (s->spec->key_compare)
    {
        i = 0;
        while (i < node->num_indexed
               && (*s->spec->key_compare)(node->indexed[i]->key, key) <= 0)
            i++;
    }
    else
        i = node->num_indexed;

    if (name)
        s->name = xstrdup(name);
    else if (s->spec->key_type != NULL_TYPE)
    {
        dump_any_type_str_data d;
        d.type = s->spec->key_type;
        d.count = 1;
        d.value = key;
        s->name = string_io(dump_any_type_str, &d);
    }
    else
        s->name = xstrdup("[]");

    for (j = node->num_indexed; j > i; j--)
        node->indexed[j] = node->indexed[j - 1];
    node->indexed[i] = s;
    node->num_indexed++;
    return s;
}

static int get_state_index_position(state_generic *node, const void *key)
{
    int l, r, m;
    const state_spec *spec;

    if (!node->indexed) return -1;
    spec = node->spec->indexed;
    if (spec && spec->key_compare)
    {
        l = 0;
        r = node->num_indexed;
        while (l + 1 < r)
        {
            m = (l + r) / 2;
            if ((*spec->key_compare)(key, node->indexed[m]->key) < 0)
                r = m;
            else
                l = m;
        }
        if ((*spec->key_compare)(key, node->indexed[l]->key) == 0)
            return l;
    }
    else
    {
        for (m = 0; m < node->num_indexed; m++)
            if (key == node->indexed[m]->key) return m;
    }
    return -1;
}

state_generic *get_state_index(state_generic *node, const void *key)
{
    int num;

    num = get_state_index_position(node, key);
    if (num == -1) return NULL;
    else return node->indexed[num];
}

state_generic *get_state_index_number(state_generic *node, int number)
{
    if (number < 0 || number >= node->num_indexed)
        return NULL;
    return node->indexed[number];
}

void remove_state_index(state_generic *node, const void *key)
{
    int num, i;

    num = get_state_index_position(node, key);
    if (num != -1)
    {
        destroy_state(node->indexed[num], true);
        node->num_indexed--;
        for (i = num; i < node->num_indexed; i++)
            node->indexed[i] = node->indexed[i + 1];
    }
}

state_generic *get_state_by_name(state_generic *base, const char *name)
{
    size_t len;
    const char *p;
    int i;

    if (name[0] == '.') name++;
    if (!name[0]) return base;
    if (name[0] == '[')
    {
        name++;
        if (!base->num_indexed) return NULL;
        p = strchr(name, ']');
        if (!p) return NULL; /* bad name */
        len = p - name;
        for (i = 0; i < base->num_indexed; i++)
            if (strncmp(name, base->indexed[i]->name, len) == 0)
                return get_state_by_name(base->indexed[i], p + 1);
    }
    else
    {
        len = strcspn(name, ".[]");
        for (i = 0; base->children[i]; i++)
            if (strncmp(name, base->children[i]->spec->name, len) == 0)
                return get_state_by_name(base->children[i], name + len);
    }
    return NULL;
}
