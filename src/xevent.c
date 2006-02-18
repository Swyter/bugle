/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
#include "common/linkedlist.h"
#include "common/bool.h"
#include "common/safemem.h"
#include <X11/Xlib.h>
#include <ltdl.h>
#include <stdio.h>
#include <stddef.h>

typedef struct
{
    KeySym key;
    unsigned int state;
    unsigned int state_mask;
    void *arg;
    bool (*wanted)(KeySym key, unsigned int state, void *arg);
    void (*callback)(KeySym key, unsigned int state, void *arg);
} handler;

static bool active = false;
static bugle_linked_list handlers;

static int (*real_XNextEvent)(Display *, XEvent *) = NULL;
static int (*real_XPeekEvent)(Display *, XEvent *) = NULL;
static int (*real_XWindowEvent)(Display *, Window, long, XEvent *) = NULL;
static Bool (*real_XCheckWindowEvent)(Display *, Window, long, XEvent *) = NULL;
static int (*real_XMaskEvent)(Display *, long, XEvent *) = NULL;
static Bool (*real_XCheckMaskEvent)(Display *, long, XEvent *) = NULL;
static Bool (*real_XCheckTypedEvent)(Display *, int, XEvent *) = NULL;
static Bool (*real_XCheckTypedWindowEvent)(Display *, Window, int, XEvent *) = NULL;

static int (*real_XIfEvent)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) = NULL;
static Bool (*real_XCheckIfEvent)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) = NULL;
static int (*real_XPeekIfEvent)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) = NULL;

static int (*real_XEventsQueued)(Display *, int) = NULL;
static int (*real_XPending)(Display *) = NULL;

/* Determines whether bugle wants to intercept an event */
static Bool event_predicate(Display *dpy, XEvent *event, XPointer arg)
{
    KeySym key;
    unsigned int state;
    bugle_list_node *i;
    handler *h;

    if (event->type != KeyPress) return False;
    key = XKeycodeToKeysym(dpy, event->xkey.keycode, 1);
    state = event->xkey.state;
    for (i = bugle_list_head(&handlers); i; i = bugle_list_next(i))
    {
        h = (handler *) bugle_list_data(i);
        if (h->key == key && (((h->state ^ state) & h->state_mask) == 0)
            && (!h->wanted || (*h->wanted)(key, state, h->arg)))
            return True;
    }
    return False;
}

/* Note: assumes that event_predicate is true */
static void handle_event(Display *dpy, XEvent *event)
{
    KeySym key;
    unsigned int state;
    bugle_list_node *i;
    handler *h;

    if (event->type != KeyPress) return; /* Should never happen */
    key = XKeycodeToKeysym(dpy, event->xkey.keycode, 1);
    state = event->xkey.state;
    for (i = bugle_list_head(&handlers); i; i = bugle_list_next(i))
    {
        h = (handler *) bugle_list_data(i);
        if (h->key == key && (((h->state ^ state) & h->state_mask) == 0)
            && (!h->wanted || (*h->wanted)(key, state, h->arg)))
            (*h->callback)(key, state, h->arg);
    }
}

static void extract_events(Display *dpy)
{
    XEvent event;
    while ((*real_XCheckIfEvent)(dpy, &event, event_predicate, NULL))
        handle_event(dpy, &event);
}

int XNextEvent(Display *dpy, XEvent *event)
{
    int ret;

    if (!active) return (*real_XNextEvent)(dpy, event);
    extract_events(dpy);
    while ((ret = (*real_XNextEvent)(dpy, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

int XPeekEvent(Display *dpy, XEvent *event)
{
    int ret;

    if (!active) return (*real_XPeekEvent)(dpy, event);
    extract_events(dpy);
    while ((ret = (*real_XPeekEvent)(dpy, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        /* Peek doesn't remove the event, so do another extract run to
         * clear it */
        extract_events(dpy);
    }
    return ret;
}

int XWindowEvent(Display *dpy, Window w, long event_mask, XEvent *event)
{
    int ret;

    if (!active) return (*real_XWindowEvent)(dpy, w, event_mask, event);
    extract_events(dpy);
    while ((ret = (*real_XWindowEvent)(dpy, w, event_mask, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

Bool XCheckWindowEvent(Display *dpy, Window w, long event_mask, XEvent *event)
{
    Bool ret;

    if (!active) return (*real_XCheckWindowEvent)(dpy, w, event_mask, event);
    extract_events(dpy);
    while ((ret = (*real_XCheckWindowEvent)(dpy, w, event_mask, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

int XMaskEvent(Display *dpy, long event_mask, XEvent *event)
{
    int ret;

    if (!active) return (*real_XMaskEvent)(dpy, event_mask, event);
    extract_events(dpy);
    while ((ret = (*real_XMaskEvent)(dpy, event_mask, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

Bool XCheckMaskEvent(Display *dpy, long event_mask, XEvent *event)
{
    Bool ret;

    if (!active) return (*real_XCheckMaskEvent)(dpy, event_mask, event);
    extract_events(dpy);
    while ((ret = (*real_XCheckMaskEvent)(dpy, event_mask, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

Bool XCheckTypedEvent(Display *dpy, int type, XEvent *event)
{
    Bool ret;

    if (!active) return (*real_XCheckTypedEvent)(dpy, type, event);
    extract_events(dpy);
    while ((ret = (*real_XCheckTypedEvent)(dpy, type, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

Bool XCheckTypedWindowEvent(Display *dpy, Window w, int type, XEvent *event)
{
    Bool ret;

    if (!active) return (*real_XCheckTypedWindowEvent)(dpy, w, type, event);
    extract_events(dpy);
    while ((ret = (*real_XCheckTypedWindowEvent)(dpy, w, type, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

int XIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    int ret;

    if (!active) return (*real_XIfEvent)(dpy, event, predicate, arg);
    extract_events(dpy);
    while ((ret = (*real_XIfEvent)(dpy, event, predicate, arg)) != 0)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

Bool XCheckIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    Bool ret;

    if (!active) return (*real_XCheckIfEvent)(dpy, event, predicate, arg);
    extract_events(dpy);
    while ((ret = (*real_XCheckIfEvent)(dpy, event, predicate, arg)) == True)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        handle_event(dpy, event);
    }
    return ret;
}

int XPeekIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    int ret;

    if (!active) return (*real_XPeekIfEvent)(dpy, event, predicate, arg);
    extract_events(dpy);
    while ((ret = (*real_XPeekIfEvent)(dpy, event, predicate, arg)) != 0)
    {
        if (!event_predicate(dpy, event, NULL)) return ret;
        /* Peek doesn't remove the event, so do another extract run to
         * clear it */
        extract_events(dpy);
    }
    return ret;
}

int XEventsQueued(Display *dpy, int mode)
{
    if (active) extract_events(dpy);
    return (*real_XEventsQueued)(dpy, mode);
}

int XPending(Display *dpy)
{
    if (active) extract_events(dpy);
    return (*real_XPending)(dpy);
}

void bugle_register_xevent(KeySym key, unsigned int state, unsigned int state_mask,
                           bool (*wanted)(KeySym key, unsigned int state, void *arg),
                           void (*callback)(KeySym key, unsigned int state, void *arg),
                           void *arg)
{
    handler *h;

    h = (handler *) bugle_malloc(sizeof(handler));
    h->key = key;
    h->state = state;
    h->state_mask = state_mask;
    h->wanted = wanted;
    h->callback = callback;
    h->arg = arg;

    if (state_mask == 0) h->state_mask = ShiftMask | ControlMask | Mod1Mask;
    bugle_list_append(&handlers, h);
    active = true;
}

void initialise_xevent(void)
{
    lt_dlhandle handle;

    handle = lt_dlopenext("libX11");
    if (handle == NULL)
    {
        fputs("WARNING: unable to intercept X events\n", stderr);
        return;
    }

    real_XNextEvent = (int (*)(Display *, XEvent *)) lt_dlsym(handle, "XNextEvent");
    real_XPeekEvent = (int (*)(Display *, XEvent *)) lt_dlsym(handle, "XPeekEvent");
    real_XWindowEvent = (int (*)(Display *, Window, long, XEvent *)) lt_dlsym(handle, "XWindowEvent");
    real_XCheckWindowEvent = (Bool (*)(Display *, Window, long, XEvent *)) lt_dlsym(handle, "XCheckWindowEvent");
    real_XMaskEvent = (int (*)(Display *, long, XEvent *)) lt_dlsym(handle, "XMaskEvent");
    real_XCheckMaskEvent = (Bool (*)(Display *, long, XEvent *)) lt_dlsym(handle, "XCheckMaskEvent");
    real_XCheckTypedEvent = (Bool (*)(Display *, int, XEvent *)) lt_dlsym(handle, "XCheckTypedEvent");
    real_XCheckTypedWindowEvent = (Bool (*)(Display *, Window, int, XEvent *)) lt_dlsym(handle, "XCheckTypedWindowEvent");

    real_XIfEvent = (int (*)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer)) lt_dlsym(handle, "XIfEvent");
    real_XCheckIfEvent = (Bool (*)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer)) lt_dlsym(handle, "XCheckIfEvent");
    real_XPeekIfEvent = (int (*)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer)) lt_dlsym(handle, "XPeekIfEvent");

    real_XEventsQueued = (int (*)(Display *, int)) lt_dlsym(handle, "XEventsQueued");
    real_XPending = (int (*)(Display *)) lt_dlsym(handle, "XPending");

    if (!real_XNextEvent
        || !real_XPeekEvent
        || !real_XWindowEvent
        || !real_XCheckWindowEvent
        || !real_XMaskEvent
        || !real_XCheckMaskEvent
        || !real_XCheckTypedEvent
        || !real_XCheckTypedWindowEvent
        || !real_XIfEvent
        || !real_XCheckIfEvent
        || !real_XPeekIfEvent
        || !real_XEventsQueued
        || !real_XPending)
        fputs("WARNING: unable to obtain X symbols. A crash is highly likely.\n", stderr);

    bugle_list_init(&handlers, true);
    return;
}
