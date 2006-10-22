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

#ifndef BUGLE_SRC_TRACKER_H
#define BUGLE_SRC_TRACKER_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include <GL/gl.h>
#include "common/radixtree.h"
#include "common/bool.h"
#include "src/filters.h"
#include "src/utils.h"
#include "src/objects.h"

typedef enum
{
    BUGLE_TRACKOBJECTS_TEXTURE,
    BUGLE_TRACKOBJECTS_BUFFER,
    BUGLE_TRACKOBJECTS_QUERY,
    BUGLE_TRACKOBJECTS_SHADER,      /* GLSL */
    BUGLE_TRACKOBJECTS_PROGRAM,
    BUGLE_TRACKOBJECTS_OLD_PROGRAM, /* glGenProgramsARB */
    BUGLE_TRACKOBJECTS_RENDERBUFFER,
    BUGLE_TRACKOBJECTS_FRAMEBUFFER,
    BUGLE_TRACKOBJECTS_COUNT
} bugle_trackobjects_type;

extern bugle_object_class bugle_context_class, bugle_namespace_class;
extern bugle_object_class bugle_displaylist_class;

/* True if we are in begin/end, OR if there is no current context.
 * trackbeginend is required.
 */
bool bugle_in_begin_end(void);

/* Gets a context with the same config. Please leave all state as default.
 * There is a choice of a shared or an unshared context, which refers to
 * whether object state is shared with the primary context. The former is
 * best for querying object state, the latter for creating internal objects
 * so as not to pollute the primary namespace.
 */
GLXContext bugle_get_aux_context(bool shared);

/* Draws text at the specified location. The current colour is used to
 * render the text. The text is rendered with alpha, so alpha-test or
 * alpha-blending can be used to obtain a transparent background.
 * Pre-conditions:
 * - the unshared aux context is active
 * - the projection matrix is the identity
 * - the modelview matrix is the identity, scaled and optionally translated
 *   such that one unit is one pixel
 * - the pixel unpack state is the default
 * - no textures are enabled
 * - must be inside begin_internal_render/end_internal_render
 * - there must be room for one push on the attribute stack
 */
void bugle_text_render(const char *msg, int x, int y);

/* The number and mode of the current display list being compiled,
 * or 0 and GL_NONE if none.
 * trackdisplaylist is required.
 */
GLuint bugle_displaylist_list(void);
GLenum bugle_displaylist_mode(void);
/* The display list object associated with a numbered list */
void *bugle_displaylist_get(GLuint list);

/* Calls back walker for each object of the specified type. The parameters
 * to the walker are object, target (if available or applicable), and the
 * user data.
 */
void bugle_trackobjects_walk(bugle_trackobjects_type type,
                             void (*walker)(GLuint, GLenum, void *),
                             void *);
/* Determines the target associated with a particular object, or GL_NONE
 * if none is known.
 */
GLenum bugle_trackobjects_get_target(bugle_trackobjects_type type, GLuint id);

/* Checks for GL extensions by #define from glexts.h */
bool bugle_gl_has_extension(int ext);
bool bugle_gl_has_extension_group(int ext);

/* Used by the initialisation code */
void trackcontext_initialise(void);
void trackbeginend_initialise(void);
void trackdisplaylist_initialise(void);
void trackextensions_initialise(void);
void trackobjects_initialise(void);

#endif /* !BUGLE_SRC_TRACKER_H */
