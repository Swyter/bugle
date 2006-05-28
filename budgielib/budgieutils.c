/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
#define _POSIX_SOURCE
#define _GNU_SOURCE /* For open_memstream */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <setjmp.h>
#include <ltdl.h>
#include "budgieutils.h"
#include "common/bool.h"
#include "common/threads.h"
#include "common/safemem.h"

void budgie_dump_bitfield(unsigned int value, FILE *out,
                          const bitfield_pair *tags, int count)
{
    bool first = true;
    int i;
    for (i = 0; i < count; i++)
        if (value & tags[i].value)
        {
            if (!first) fputs(" | ", out); else first = false;
            fputs(tags[i].name, out);
            value &= ~tags[i].value;
        }
    if (value)
    {
        if (!first) fputs(" | ", out);
        fprintf(out, "%08x", value);
    }
}

/* Calls "call" with a FILE * and "data". The data that "call" writes into
 * the file is returned in as a malloc'ed string.
 * FIXME: error checking.
 * FIXME: is ftell portable on _binary_ streams (which tmpfile is)?
 */
char *budgie_string_io(void (*call)(FILE *, void *), void *data)
{
    FILE *f;
    size_t size;
    char *buffer;

#if HAVE_OPEN_MEMSTREAM
    f = open_memstream(&buffer, &size);
    (*call)(f, data);
    fclose(f);
#else
    f = tmpfile();
    (*call)(f, data);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
#endif

    return buffer;
}

bool budgie_dump_string(const char *value, FILE *out)
{
    /* FIXME: handle illegal dereferences */
    if (value == NULL) fputs("NULL", out);
    else
    {
        fputc('"', out);
        while (value[0])
        {
            switch (value[0])
            {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            default:
                if (iscntrl(value[0]))
                    fprintf(out, "\\%03o", (int) value[0]);
                else
                    fputc(value[0], out);
            }
            value++;
        }
        fputc('"', out);
    }
    return true;
}

bool budgie_dump_string_length(const char *value, size_t length, FILE *out)
{
    size_t i;
    /* FIXME: handle illegal dereferences */
    if (value == NULL) fputs("NULL", out);
    else
    {
        fputc('"', out);
        for (i = 0; i < length; i++)
        {
            switch (value[0])
            {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            default:
                if (iscntrl(value[0]))
                    fprintf(out, "\\%03o", (int) value[0]);
                else
                    fputc(value[0], out);
            }
            value++;
        }
        fputc('"', out);
    }
    return true;
}

int budgie_count_string(const char *value)
{
    /* FIXME: handle illegal dereferences */
    const char *str = (const char *) value;
    if (str == NULL) return 0;
    else return strlen(str) + 1;
}

void budgie_dump_any_type(budgie_type type, const void *value, int length, FILE *out)
{
    const type_data *info;
    budgie_type new_type;

    assert(type >= 0);
    info = &budgie_type_table[type];
    if (info->get_type) /* highly unlikely */
    {
        new_type = (*info->get_type)(value);
        if (new_type != type)
        {
            budgie_dump_any_type(new_type, value, length, out);
            return;
        }
    }
    /* More likely e.g. for strings. Note that we don't override the length
     * if specified, since it may come from a parameter override
     */
    if (info->get_length && length == -1) 
        length = (*info->get_length)(value);

    assert(info->dumper);
    (*info->dumper)(value, length, out);
}

void budgie_dump_any_type_extended(budgie_type type,
                                   const void *value,
                                   int length,
                                   int outer_length,
                                   const void *pointer,
                                   FILE *out)
{
    int i;
    const char *v;

    if (pointer)
        fprintf(out, "%p -> ", pointer);
    if (outer_length == -1)
        budgie_dump_any_type(type, value, length, out);
    else
    {
        v = (const char *) value;
        fputs("{ ", out);
        for (i = 0; i < outer_length; i++)
        {
            if (i) fputs(", ", out);
            budgie_dump_any_type(type, (const void *) v, length, out);
            v += budgie_type_table[type].size;
        }
        fputs(" }", out);
    }
}

void budgie_dump_any_call(const generic_function_call *call, int indent, FILE *out)
{
    size_t i;
    const group_data *data;
    arg_dumper cur_dumper;
    budgie_type type;
    int length;
    const group_parameter_data *info;

    budgie_make_indent(indent, out);
    fprintf(out, "%s(", budgie_function_table[call->id].name);
    data = &budgie_group_table[call->group];
    info = &data->parameters[0];
    for (i = 0; i < data->num_parameters; i++, info++)
    {
        if (i) fputs(", ", out);
        cur_dumper = info->dumper; /* custom dumper */
        length = -1;
        if (info->get_length) length = (*info->get_length)(call, i, call->args[i]);
        if (!cur_dumper || !(*cur_dumper)(call, i, call->args[i], length, out))
        {
            type = info->type;
            if (info->get_type) type = (*info->get_type)(call, i, call->args[i]);
            budgie_dump_any_type(type, call->args[i], length, out);
        }
    }
    fputs(")", out);
    if (call->retn)
    {
        fputs(" = ", out);
        info = &data->retn;
        cur_dumper = info->dumper; /* custom dumper */
        length = -1;
        if (info->get_length) length = (*info->get_length)(call, i, call->retn);
        if (!cur_dumper || !(*cur_dumper)(call, -1, call->retn, length, out))
        {
            type = info->type;
            if (info->get_type) type = (*info->get_type)(call, -1, call->retn);
            budgie_dump_any_type(type, call->retn, length, out);
        }
    }
}

void budgie_make_indent(int indent, FILE *out)
{
    int i;
    for (i = 0; i < indent; i++)
        fputc(' ', out);
}

static bugle_thread_once_t initialise_real_once = BUGLE_THREAD_ONCE_INIT;

static void initialise_real_work(void)
{
    lt_dlhandle handle;
    size_t i, j;
    size_t N, F;

    N = number_of_libraries;
    F = number_of_functions;

    lt_dlmalloc = bugle_malloc;
    lt_dlrealloc = bugle_realloc;
    lt_dlinit();
    for (i = 0; i < N; i++)
    {
        handle = lt_dlopen(library_names[i]);
        if (handle)
        {
            for (j = 0; j < F; j++)
                if (!budgie_function_table[j].real)
                {
                    budgie_function_table[j].real = (void (*)(void)) lt_dlsym(handle, budgie_function_table[j].name);
                    lt_dlerror(); /* clear the error flag */
                }
        }
        else
        {
            fprintf(stderr, "%s", lt_dlerror());
            exit(1);
        }
    }
}

void initialise_real(void)
{
    /* We have to provide this protection, because the interceptor functions
     * call initialise_real if they are re-entered so that they can bypass
     * the filter chain.
     */
    bugle_thread_once(&initialise_real_once, initialise_real_work);
}

/* Re-entrance protection. Note that we still wish to allow other threads
 * to enter from the user application; it is just that we do not want the
 * real library to call our fake functions.
 *
 * Also note that the protection is called *before* we first encounter
 * the interceptor, so we cannot rely on the interceptor to initialise us.
 */

static bugle_thread_key_t reentrance_key;
static bugle_thread_once_t reentrance_once = BUGLE_THREAD_ONCE_INIT;

static void initialise_reentrance(void)
{
    bugle_thread_key_create(&reentrance_key, NULL);
}

/* Sets the flag to mark entry, and returns true if we should call
 * the interceptor. We set an arbitrary non-NULL pointer.
 */
bool check_set_reentrance(void)
{
    /* Note that since the data is thread-specific, we need not worry
     * about contention.
     */
    bool ans;

    bugle_thread_once(&reentrance_once, initialise_reentrance);
    ans = bugle_thread_getspecific(reentrance_key) == NULL;
    bugle_thread_setspecific(reentrance_key, &ans);
    return ans;
}

void clear_reentrance(void)
{
    bugle_thread_setspecific(reentrance_key, NULL);
}
