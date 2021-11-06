/*  log.c
 *
 *
 *  Copyright (C) 2021 toxbot All Rights Reserved.
 *
 *  This file is part of toxbot.
 *
 *  toxbot is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  toxbot is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with toxbot. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdarg.h>

#include "misc.h"

#define TIMESTAMP_SIZE 64
#define MAX_MESSAGE_SIZE 512

static struct tm *get_wall_time(void)
{
    struct tm *timeinfo;
    time_t t = get_time();
    timeinfo = localtime((const time_t *) &t);
    return timeinfo;
}

void log_timestamp(const char *message, ...)
{
    char format[MAX_MESSAGE_SIZE];

    va_list args;
    va_start(args, message);
    vsnprintf(format, sizeof(format), message, args);
    va_end(args);

    char ts[TIMESTAMP_SIZE];
    strftime(ts, TIMESTAMP_SIZE,"[%H:%M:%S]", get_wall_time());

    printf("%s %s\n", ts, format);
}

void log_error_timestamp(int err, const char *message, ...)
{
    char format[MAX_MESSAGE_SIZE];

    va_list args;
    va_start(args, message);
    vsnprintf(format, sizeof(format), message, args);
    va_end(args);

    char ts[TIMESTAMP_SIZE];
    strftime(ts, TIMESTAMP_SIZE,"[%H:%M:%S]", get_wall_time());

    fprintf(stderr, "%s %s (error %d)\n", ts, format, err);
}

