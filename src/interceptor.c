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
#include "src/lib.h"
#include "filters.h"
#include "canon.h"
#include "conffile.h"
#include "tracker.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#if HAVE_PTHREAD_H
# include <pthread.h>
#endif

#define FILTERFILE "/.bugle/filters"

extern FILE *yyin;
extern int yyparse(void);

static void load_config(void)
{
    const char *home;
    char *config = NULL, *chain_name;
    const linked_list *root;
    const config_chain *chain;
    const config_filterset *set;
    const config_variable *var;
    filter_set *f;
    list_node *i, *j;
    bool debugging;

    if (getenv("BUGLE_FILTERS"))
        config = xstrdup(getenv("BUGLE_FILTERS"));
    home = getenv("HOME");
    chain_name = getenv("BUGLE_CHAIN");
    debugging = getenv("BUGLE_DEBUGGER") != NULL;
    /* If using the debugger and no chain is specified, we use passthrough
     * mode.
     */
    if ((config || home) && (!debugging || chain_name))
    {
        if (!config)
        {
            config = xmalloc(strlen(home) + strlen(FILTERFILE) + 1);
            sprintf(config, "%s%s", home, FILTERFILE);
        }
        if ((yyin = fopen(config, "r")))
        {
            if (yyparse() == 0)
            {
                chain = NULL;
                if (chain_name)
                {
                    chain = config_get_chain(chain_name);
                    if (!chain)
                        fprintf(stderr, "could not find chain %s, trying default\n", chain_name);
                }
                if (!chain)
                {
                    root = config_get_root();
                    if (list_head(root))
                        chain = (const config_chain *) list_data(list_head(root));
                    if (!chain)
                        fputs("no chains defined; running in passthrough mode\n", stderr);
                }
                if (chain)
                {
                    for (i = list_head(&chain->filtersets); i; i = list_next(i))
                    {
                        set = (const config_filterset *) list_data(i);
                        f = get_filter_set_handle(set->name);
                        if (!f)
                        {
                            fprintf(stderr, "warning: ignoring unknown filter-set %s\n",
                                    set->name);
                            continue;
                        }
                        for (j = list_head(&set->variables); j; j = list_next(j))
                        {
                            var = (const config_variable *) list_data(j);
                            if (!filter_set_command(f,
                                                    var->name,
                                                    var->value))
                                fprintf(stderr, "warning: unknown command %s in filter-set %s\n",
                                        var->name, set->name);
                        }
                    }
                    for (i = list_head(&chain->filtersets); i; i = list_next(i))
                    {
                        set = (const config_filterset *) list_data(i);
                        f = get_filter_set_handle(set->name);
                        if (f) enable_filter_set(f);
                    }
                }
                config_destroy();
            }
            fclose(yyin);
        }
        else
            fprintf(stderr, "failed to open config file %s; running in passthrough mode\n", config);
        free(config);
    }
    else if (!debugging)
        fputs("$HOME not defined; running in passthrough mode\n", stderr);

    /* Always load the invoke filter-set */
    f = get_filter_set_handle("invoke");
    if (!f)
    {
        fputs("could not find the 'invoke' filter-set; aborting\n", stderr);
        exit(1);
    }
    enable_filter_set(f);
    /* Auto-load the debugger filter-set if using the debugger */
    if (debugging)
    {
        f = get_filter_set_handle("debugger");
        if (!f)
        {
            fputs("could not find the 'debugger' filter-set; aborting\n", stderr);
            exit(1);
        }
        enable_filter_set(f);
    }
}

static void initialise_core_filters(void)
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
    register_filter_set(&trackcontext_info);
    register_filter_set(&trackbeginend_info);
}

static pthread_once_t init_key_once = PTHREAD_ONCE_INIT;
static void initialise_all(void)
{
    initialise_hashing();
    initialise_real();
    initialise_canonical();
    initialise_filters();
    initialise_core_filters();
    initialise_dump_tables();
    load_config();
}

void interceptor(function_call *call)
{
    pthread_once(&init_key_once, initialise_all);
    run_filters(call);
}
