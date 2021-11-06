/*  misc.h
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

#ifndef MISC_H
#define MISC_H

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#include <inttypes.h>
#include <sys/types.h>
#include <stdbool.h>
#include <time.h>

bool timed_out(time_t timestamp, time_t curtime, uint64_t timeout);

/* Returns current unix timestamp */
time_t get_time(void);

/* converts hexidecimal string to binary */
char *hex_string_to_bin(const char *hex_string);

/* returns file size or 0 on error */
off_t file_size(const char *path);

/* Return true if a file exists at `path`. */
bool file_exists(const char *path);

/* copies data to msg buffer.
   returns length of msg, which will be no larger than size-1 */
uint16_t copy_tox_str(char *msg, size_t size, const char *data, uint16_t length);

/* returns index of the first instance of ch in s starting at idx.
   returns length of s if char not found */
int char_find(int idx, const char *s, char ch);

/* Converts seconds to string in format days hours minutes */
void get_elapsed_time_str(char *buf, int bufsize, uint64_t secs);

/*
 * Searches plain text file pointed to by path for lines that match public_key.
 *
 * Returns 1 if a match is found.
 * Returns 0 if a match is not found.
 * Returns -1 on file operation failure.
 *
 * public_key must be a binary representation of a Tox public key.
 */
int file_contains_key(const char *public_key, const char *path);

#endif /* MISC_H */

