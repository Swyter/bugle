/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008  Bruce Merry
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

#ifndef BUGLE_GLX_GLWINTYPES_H
#define BUGLE_GLX_GLWINTYPES_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <GL/glx.h>
#include <GL/glxext.h>

typedef Display *   glwin_display;
typedef GLXContext  glwin_context;
typedef GLXDrawable glwin_drawable;

#ifdef GLX_ARB_get_proc_address
#define BUGLE_GLWIN_GET_PROC_ADDRESS glXGetProcAddressARB
#endif

#endif /* !BUGLE_GLX_GLWINTYPES_H */
