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
#include "common/linkedlist.h"
#include "common/bool.h"
#include "common/safemem.h"
#include "src/filters.h"
#include "src/xevent.h"
#include <X11/Xlib.h>
#include <ltdl.h>
#include <stdio.h>
#include <stddef.h>

#define STATE_MASK (ControlMask | ShiftMask | Mod1Mask)

#define XEVENT_LOG

typedef struct
{
    xevent_key key;
    void *arg;
    bool (*wanted)(const xevent_key *, void *);
    void (*callback)(const xevent_key *, void *);
} handler;

static bool active = false;
static bugle_linked_list handlers;

static bool mouse_active = false;
static bool mouse_first = true;
static Window mouse_window;
static int mouse_x, mouse_y;
static void (*mouse_callback)(int dx, int dy) = NULL;

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

static Window (*real_XCreateWindow)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *) = NULL;

extern void bugle_initialise_all(void);

/* Determines whether bugle wants to intercept an event */
static Bool event_predicate(Display *dpy, XEvent *event, XPointer arg)
{
    xevent_key key;
    bugle_list_node *i;
    handler *h;

    if (mouse_active && event->type == MotionNotify) return True;
    if (event->type != KeyPress && event->type != KeyRelease) return False;
    key.keysym = XKeycodeToKeysym(dpy, event->xkey.keycode, 1);
    key.state = event->xkey.state & STATE_MASK;
    key.press = event->type == KeyPress;
    for (i = bugle_list_head(&handlers); i; i = bugle_list_next(i))
    {
        h = (handler *) bugle_list_data(i);
        if (h->key.keysym == key.keysym && h->key.state == key.state
            && (!h->wanted || (*h->wanted)(&key, h->arg)))
            return True;
    }
    return False;
}

/* Note: assumes that event_predicate is true */
static void handle_event(Display *dpy, XEvent *event)
{
    xevent_key key;
    bugle_list_node *i;
    handler *h;

    if (mouse_active && event->type == MotionNotify)
    {
        if (mouse_first)
        {
            XWindowAttributes attr;
            XGetWindowAttributes(dpy, event->xmotion.window, &attr);

            mouse_window = event->xmotion.window;
            mouse_x = attr.width / 2;
            mouse_y = attr.height / 2;
            mouse_first = False;
            XWarpPointer(dpy, event->xmotion.window, event->xmotion.window,
                         0, 0, 0, 0, mouse_x, mouse_y);
            mouse_first = false;
        }
        else if (event->xmotion.window != mouse_window)
            XWarpPointer(dpy, None, mouse_window, 0, 0, 0, 0, mouse_x, mouse_y);
        else if (mouse_x != event->xmotion.x || mouse_y != event->xmotion.y)
        {
#ifdef XEVENT_LOG
            fprintf(stderr, "Mouse moved by (%d, %d) ref = (%d, %d)\n",
                    event->xmotion.x - mouse_x, event->xmotion.y - mouse_y,
                    mouse_x, mouse_y);
#endif
            (*mouse_callback)(event->xmotion.x - mouse_x, event->xmotion.y - mouse_y);
            XWarpPointer(dpy, None, event->xmotion.window, 0, 0, 0, 0, mouse_x, mouse_y);
        }
        return;
    }
    /* event_predicate returns true on release as well, so that the
     * release event does not appear to the app without the press event.
     * We only conditionally pass releases to the filterset.
     */
    if (event->type != KeyPress && event->type != KeyRelease) return;

    key.keysym = XKeycodeToKeysym(dpy, event->xkey.keycode, 1);
    key.state = event->xkey.state & STATE_MASK;
    key.press = (event->type == KeyPress);
    for (i = bugle_list_head(&handlers); i; i = bugle_list_next(i))
    {
        h = (handler *) bugle_list_data(i);
        if (h->key.keysym == key.keysym && h->key.state == key.state
            && h->key.press == key.press
            && (!h->wanted || (*h->wanted)(&key, h->arg)))
            (*h->callback)(&key, h->arg);
    }
}

static bool extract_events(Display *dpy)
{
    XEvent event;
    bool any = false;

    while ((*real_XCheckIfEvent)(dpy, &event, event_predicate, NULL))
    {
        handle_event(dpy, &event);
        any = true;
    }
    return any;
}

int XNextEvent(Display *dpy, XEvent *event)
{
    int ret;

#ifdef XEVENT_LOG
    fputs("-> XNextEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XNextEvent)(dpy, event);
    extract_events(dpy);
    while ((ret = (*real_XNextEvent)(dpy, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XNextEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XNextEvent\n", stderr);
#endif
    return ret;
}

int XPeekEvent(Display *dpy, XEvent *event)
{
    int ret;

#ifdef XEVENT_LOG
    fputs("-> XPeekEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XPeekEvent)(dpy, event);
    extract_events(dpy);
    while ((ret = (*real_XPeekEvent)(dpy, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XPeekEvent\n", stderr);
#endif
            return ret;
        }
        /* Peek doesn't remove the event, so do another extract run to
         * clear it */
        extract_events(dpy);
    }
#ifdef XEVENT_LOG
    fputs("<- XPeekEvent\n", stderr);
#endif
    return ret;
}

int XWindowEvent(Display *dpy, Window w, long event_mask, XEvent *event)
{
    int ret;

#ifdef XEVENT_LOG
    fputs("-> XWindowEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XWindowEvent)(dpy, w, event_mask, event);
    extract_events(dpy);
    while ((ret = (*real_XWindowEvent)(dpy, w, event_mask, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XWindowEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XWindowEvent\n", stderr);
#endif
    return ret;
}

Bool XCheckWindowEvent(Display *dpy, Window w, long event_mask, XEvent *event)
{
    Bool ret;

#ifdef XEVENT_LOG
    fputs("-> XCheckWindowEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XCheckWindowEvent)(dpy, w, event_mask, event);
    extract_events(dpy);
    while ((ret = (*real_XCheckWindowEvent)(dpy, w, event_mask, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XCheckWindowEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XCheckWindowEvent\n", stderr);
#endif
    return ret;
}

int XMaskEvent(Display *dpy, long event_mask, XEvent *event)
{
    int ret;

#ifdef XEVENT_LOG
    fputs("-> XMaskEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XMaskEvent)(dpy, event_mask, event);
    extract_events(dpy);
    while ((ret = (*real_XMaskEvent)(dpy, event_mask, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XMaskEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XMaskEvent\n", stderr);
#endif
    return ret;
}

Bool XCheckMaskEvent(Display *dpy, long event_mask, XEvent *event)
{
    Bool ret;

#ifdef XEVENT_LOG
    fputs("-> XCheckMaskEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XCheckMaskEvent)(dpy, event_mask, event);
    extract_events(dpy);
    while ((ret = (*real_XCheckMaskEvent)(dpy, event_mask, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XCheckMaskEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XCheckMaskEvent\n", stderr);
#endif
    return ret;
}

Bool XCheckTypedEvent(Display *dpy, int type, XEvent *event)
{
    Bool ret;

#ifdef XEVENT_LOG
    fputs("-> XCheckTypedEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XCheckTypedEvent)(dpy, type, event);
    extract_events(dpy);
    while ((ret = (*real_XCheckTypedEvent)(dpy, type, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XCheckTypedEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XCheckTypedEvent\n", stderr);
#endif
    return ret;
}

Bool XCheckTypedWindowEvent(Display *dpy, Window w, int type, XEvent *event)
{
    Bool ret;

#ifdef XEVENT_LOG
    fputs("-> XCheckTypedWindowEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XCheckTypedWindowEvent)(dpy, w, type, event);
    extract_events(dpy);
    while ((ret = (*real_XCheckTypedWindowEvent)(dpy, w, type, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XCheckTypedWindowEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XCheckTypedWindowEvent\n", stderr);
#endif
    return ret;
}

int XIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    int ret;

#ifdef XEVENT_LOG
    fputs("-> XIfEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XIfEvent)(dpy, event, predicate, arg);
    extract_events(dpy);
    while ((ret = (*real_XIfEvent)(dpy, event, predicate, arg)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XIfEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XIfEvent\n", stderr);
#endif
    return ret;
}

Bool XCheckIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    Bool ret;

#ifdef XEVENT_LOG
    fputs("-> XCheckIfEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XCheckIfEvent)(dpy, event, predicate, arg);
    extract_events(dpy);
    while ((ret = (*real_XCheckIfEvent)(dpy, event, predicate, arg)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XCheckIfEvent\n", stderr);
#endif
            return ret;
        }
        handle_event(dpy, event);
    }
#ifdef XEVENT_LOG
    fputs("<- XCheckIfEvent\n", stderr);
#endif
    return ret;
}

int XPeekIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    int ret;

#ifdef XEVENT_LOG
    fputs("-> XPeekIfEvent\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XPeekIfEvent)(dpy, event, predicate, arg);
    extract_events(dpy);
    while ((ret = (*real_XPeekIfEvent)(dpy, event, predicate, arg)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
#ifdef XEVENT_LOG
            fputs("<- XPeekIfEvent\n", stderr);
#endif
            return ret;
        }
        /* Peek doesn't remove the event, so do another extract run to
         * clear it */
        extract_events(dpy);
    }
#ifdef XEVENT_LOG
    fputs("<- XPeekIfEvent\n", stderr);
#endif
    return ret;
}

int XEventsQueued(Display *dpy, int mode)
{
    int events;

#ifdef XEVENT_LOG
    fputs("-> XEventsQueued\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XEventsQueued)(dpy, mode);

    do
    {
        events = (*real_XEventsQueued)(dpy, mode);
    } while (events > 0 && extract_events(dpy));
#ifdef XEVENT_LOG
    fputs("<- XEventsQueued\n", stderr);
#endif
    return events;
}

int XPending(Display *dpy)
{
    int events;
#ifdef XEVENT_LOG
    fputs("-> XPending\n", stderr);
#endif
    bugle_initialise_all();
    if (!active) return (*real_XPending)(dpy);

    do
    {
        events = (*real_XPending)(dpy);
    } while (events > 0 && extract_events(dpy));
#ifdef XEVENT_LOG
    fputs("<- XPending\n", stderr);
#endif
    return (*real_XPending)(dpy);
}

Window XCreateWindow(Display *dpy, Window parent, int x, int y,
                     unsigned int width, unsigned int height,
                     unsigned int border_width, int depth,
                     unsigned int c_class, Visual *visual,
                     unsigned long valuemask,
                     XSetWindowAttributes *attributes)
{
#ifdef XEVENT_LOG
    fputs("-> XCreateWindow\n", stderr);
#endif
    long events = KeyPressMask | KeyReleaseMask | PointerMotionMask;
    XSetWindowAttributes my_attributes;
            bugle_initialise_all();
    if (active
        && (!(valuemask & CWEventMask) || (attributes->event_mask & events) != events))
    {
        my_attributes = *attributes;
        /* FIXME: what should the default attributes be? */
        if (!(valuemask & CWEventMask))
            my_attributes.event_mask = events;
        else
            my_attributes.event_mask |= events;
        valuemask |= CWEventMask;
        return (*real_XCreateWindow)(dpy, parent, x, y, width, height,
                                     border_width, depth, c_class, visual,
                                     valuemask, &my_attributes);
    }
#ifdef XEVENT_LOG
    fputs("<- XCreateWindow\n", stderr);
#endif
    return (*real_XCreateWindow)(dpy, parent, x, y, width, height,
                                 border_width, depth, c_class, visual,
                                 valuemask, attributes);
}

bool bugle_xevent_key_lookup(const char *name, xevent_key *key)
{
    KeySym keysym = NoSymbol;
    unsigned int state = 0;

    key->press = true;
    while (true)
    {
        if (name[0] == 'C' && name[1] == '-')
        {
            state |= ControlMask;
            name += 2;
        }
        else if (name[0] == 'S' && name[1] == '-')
        {
            state |= ShiftMask;
            name += 2;
        }
        else if (name[0] == 'A' && name[1] == '-')
        {
            state |= Mod1Mask;
            name += 2;
        }
        else
        {
            keysym = XStringToKeysym((char *) name);
            if (keysym != NoSymbol)
            {
                key->keysym = keysym;
                key->state = state;
                return true;
            }
            else
                return false;
        }
    }
}

void bugle_xevent_key_callback_flag(const xevent_key *key, void *arg)
{
    *(bool *) arg = true;
}

void bugle_register_xevent_key(const xevent_key *key,
                               bool (*wanted)(const xevent_key *, void *),
                               void (*callback)(const xevent_key *, void *),
                               void *arg)
{
    handler *h;

    h = (handler *) bugle_malloc(sizeof(handler));
    h->key = *key;
    h->wanted = wanted;
    h->callback = callback;
    h->arg = arg;

    bugle_list_append(&handlers, h);
    active = true;
}

void bugle_xevent_grab_pointer(void (*callback)(int, int))
{
    mouse_callback = callback;
    mouse_active = true;
    mouse_first = true;
}

void bugle_xevent_release_pointer()
{
    mouse_active = false;
}

void initialise_xevent(void)
{
    lt_dlhandle handle;

    handle = lt_dlopenext("libX11");
    if (handle == NULL)
    {
        fputs("WARNING: unable to intercept X events. A crash is highly likely.\n", stderr);
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

    real_XCreateWindow = (Window (*)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *)) lt_dlsym(handle, "XCreateWindow");

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
}
