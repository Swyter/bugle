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
#include "src/tracker.h"
#include "budgielib/state.h"
#include "common/bool.h"
#include "common/safemem.h"
#include <assert.h>
#include <stddef.h>
#include <GL/glx.h>
#if HAVE_PTHREAD_H
# include <pthread.h>
#endif

static pthread_key_t context_state_key;
static size_t context_state_space = 0;

state_7context_I *tracker_get_context_state()
{
    return pthread_getspecific(context_state_key);
}

void tracker_set_context_space(size_t bytes)
{
    context_state_space = bytes;
}

bool trackcontext_callback(function_call *call, const callback_data *data)
{
    GLXContext ctx;
    state_generic *parent;
    state_7context_I *state;
    static pthread_mutex_t context_mutex = PTHREAD_MUTEX_INITIALIZER;

    switch (canonical_call(call))
    {
    case CFUNC_glXMakeCurrent:
#ifdef CFUNC_glXMakeContextCurrent
    case CFUNC_glXMakeContextCurrent:
#endif
        /* These calls may fail, so we must explicitly check for the
         * current context.
         */
        ctx = glXGetCurrentContext();
        if (!ctx)
            pthread_setspecific(context_state_key, NULL);
        else
        {
            parent = &get_root_state()->c_context.generic;
            pthread_mutex_lock(&context_mutex);
            if (!(state = (state_7context_I *) get_state_index(parent, &ctx)))
            {
                state = (state_7context_I *) add_state_index(parent, &ctx, NULL);
                if (context_state_space)
                {
                    state->c_internal.c_filterdata.data = xmalloc(context_state_space);
                    memset(state->c_internal.c_filterdata.data, 0, context_state_space);
                }
            }
            pthread_mutex_unlock(&context_mutex);
            pthread_setspecific(context_state_key, state);
        }
        break;
    }
    return true;
}

/* Track whether we are in glBegin/glEnd. This is trickier than it looks,
 * because glBegin can fail if given an illegal primitive, and we can't
 * check for the error because glGetError is illegal inside glBegin/glEnd.
 */

static bool trackbeginend_callback(function_call *call, const callback_data *data)
{
    state_7context_I *context_state;

    switch (canonical_call(call))
    {
    case CFUNC_glBegin:
        switch (*call->typed.glBegin.arg0)
        {
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_QUADS:
        case GL_QUAD_STRIP:
        case GL_POLYGON:
            if ((context_state = tracker_get_context_state()) != NULL)
                context_state->c_internal.c_in_begin_end.data = GL_TRUE;
        default: ;
        }
        break;
    case CFUNC_glEnd:
        if ((context_state = tracker_get_context_state()) != NULL)
            context_state->c_internal.c_in_begin_end.data = GL_FALSE;
        break;
    }
    return true;
}

static bool initialise_trackcontext(filter_set *handle)
{
    filter *f;

    f = register_filter(handle, "trackcontext", trackcontext_callback);
    register_filter_depends("trackcontext", "invoke");
    register_filter_catches(f, FUNC_glXMakeCurrent);
#ifdef FUNC_glXMakeContextCurrent
    register_filter_catches(f, FUNC_glXMakeContextCurrent);
#endif
    return true;
}

static bool initialise_trackbeginend(filter_set *handle)
{
    filter *f;

    f = register_filter(handle, "trackbeginend", trackbeginend_callback);
    register_filter_depends("trackbeginend", "invoke");
    register_filter_depends("trackbeginend", "trackcontext");
    register_filter_catches(f, FUNC_glBegin);
    register_filter_catches(f, FUNC_glEnd);
    register_filter_set_depends("trackbeginend", "trackcontext");
    return true;
}

void tracker_initialise(void)
{
    const filter_set_info trackcontext_info =
    {
        "trackcontext",
        initialise_trackcontext,
        NULL,
        NULL,
        0,
        0
    };
    const filter_set_info trackbeginend_info =
    {
        "trackbeginend",
        initialise_trackbeginend,
        NULL,
        NULL,
        0,
        0
    };
    pthread_key_create(&context_state_key, NULL);
    register_filter_set(&trackcontext_info);
    register_filter_set(&trackbeginend_info);
}
