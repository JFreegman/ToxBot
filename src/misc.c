/*  misc.c
 *
 *
 *  Copyright (C) 2014 toxbot All Rights Reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <tox/tox.h>

#include "misc.h"

bool timed_out(uint64_t timestamp, uint64_t curtime, uint64_t timeout)
{
    return timestamp + timeout <= curtime;
}

char *hex_string_to_bin(const char *hex_string)
{
    size_t len = strlen(hex_string);
    char *val = malloc(len);

    if (val == NULL) {
        exit(EXIT_FAILURE);
    }

    size_t i;

    for (i = 0; i < len; ++i, hex_string += 2) {
        sscanf(hex_string, "%2hhx", &val[i]);
    }

    return val;
}

off_t file_size(const char *path)
{
    struct stat st;

    if (stat(path, &st) == -1) {
        return 0;
    }

    return st.st_size;
}

uint16_t copy_tox_str(char *msg, size_t size, const char *data, uint16_t length)
{
    int len = MIN(length, size - 1);
    memcpy(msg, data, len);
    msg[len] = '\0';
    return len;
}

int char_find(int idx, const char *s, char ch)
{
    int i = idx;

    for (i = idx; s[i]; ++i) {
        if (s[i] == ch) {
            break;
        }
    }

    return i;
}

void get_elapsed_time_str(char *buf, int bufsize, uint64_t secs)
{
    long unsigned int minutes = (secs % 3600) / 60;
    long unsigned int hours = (secs / 3600) % 24;
    long unsigned int days = (secs / 3600) / 24;

    snprintf(buf, bufsize, "%lud %luh %lum", days, hours, minutes);
}

/*
 * Searches plain text file pointed to by path for lines that match public_key.
 *
 * Returns 1 if a match is found.
 * Returns 0 if a match is not found.
 * Returns -1 on file operation failure.
 *
 * public_key must be a binary representation of a Tox public key.
 */
int file_contains_key(const char *public_key, const char *path)
{
    FILE *fp = NULL;

    struct stat s;

    if (stat(path, &s) != 0) {
        FILE *fp = fopen(path, "w");

        if (fp == NULL) {
            fprintf(stderr, "Warning: failed to create '%s' file\n", path);
            return -1;
        }

        fprintf(stderr, "Warning: creating new '%s' file. Did you lose the old one?\n", path);
        fclose(fp);
        return 0;
    }

    fp = fopen(path, "r");

    if (fp == NULL) {
        fprintf(stderr, "Warning: failed to read '%s' file\n", path);
        return -1;
    }

    char id[256];

    while (fgets(id, sizeof(id), fp)) {
        int len = strlen(id);

        if (--len < TOX_PUBLIC_KEY_SIZE) {
            continue;
        }

        char *key_bin = hex_string_to_bin(id);

        if (memcmp(key_bin, public_key, TOX_PUBLIC_KEY_SIZE) == 0) {
            free(key_bin);
            fclose(fp);
            return 1;
        }

        free(key_bin);
    }

    fclose(fp);
    return 0;
}
