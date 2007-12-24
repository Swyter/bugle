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

#ifndef BUGLE_SRC_GLTOKENS_H
#define BUGLE_SRC_GLTOKENS_H

#include <GL/gl.h>

typedef struct
{
    const char *name;
    GLenum value;
    const char *version;     /* Min core version that defines this token (NULL for extension-only) */
    const char *extension;   /* Extension that defines this token (NULL for core) */
} gl_token;

extern const gl_token bugle_gl_tokens_name[];
extern const gl_token bugle_gl_tokens_value[];
extern int bugle_gl_token_count;

#endif /* !BUGLE_SRC_GLTOKENS_H */