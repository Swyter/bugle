/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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

#ifndef BUDGIELIB_TYPEUTILS_H
#define BUDGIELIB_TYPEUTILS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stddef.h>
#include "common/bool.h"

typedef int budgie_type;
#define NULL_TYPE (-1)

typedef enum
{
    CODE_ENUMERAL,
    CODE_INTEGRAL,
    CODE_FLOAT,
    CODE_COMPLEX,
    CODE_POINTER,
    CODE_ARRAY,
    CODE_RECORD,
    CODE_OTHER
} type_code;

typedef struct
{
    budgie_type type;
    ptrdiff_t offset;
} type_record_data;

typedef void (*type_dumper)(const void *, int, FILE *);
typedef budgie_type (*type_get_type)(const void *);
typedef int (*type_get_length)(const void *);

typedef struct
{
    type_code code;
    budgie_type type;    /* base type of pointers, arrays, functions etc */
    budgie_type pointer; /* pointer to this type (NULL_TYPE if not generated) */
    const type_record_data *fields;
    size_t size;
    size_t length;
    type_dumper dumper;
    type_get_type get_type;
    type_get_length get_length;
} type_data;

typedef struct
{
    unsigned int value;
    const char *name;
} bitfield_pair;

extern const type_data budgie_type_table[];
extern int number_of_types;

/* Code used by budgie output */

void budgie_dump_bitfield(unsigned int value, FILE *out,
                          const bitfield_pair *tags, int count);

/* User functions */

bool budgie_dump_string(const char *value, FILE *out);
int budgie_count_string(const char *value);
/* Like dump_string but takes an explicit length rather than NULL terminator */
bool budgie_dump_string_length(const char *value, size_t length, FILE *out);

void budgie_dump_any_type(budgie_type type, const void *value, int length, FILE *out);
/* Calls budgie_dump_any_type, BUT:
 * if outer_length != -1, then it considers the type as if it were an
 * array of that type, of length outer_length. If pointer is
 * non-NULL, then it outputs "<pointer> -> " first as if dumping a
 * pointer to the array.
 */
void budgie_dump_any_type_extended(budgie_type type,
                                   const void *value,
                                   int length,
                                   int outer_length,
                                   const void *pointer,
                                   FILE *out);

#endif /* !BUDGIELIB_TYPEUTILS_H */