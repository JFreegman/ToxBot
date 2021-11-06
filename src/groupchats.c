/*  groupchats.c
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
#include <stdlib.h>
#include <string.h>

#include "toxbot.h"
#include "groupchats.h"

extern struct Tox_Bot Tox_Bot;

void realloc_groupchats(int n)
{
    if (n <= 0) {
        free(Tox_Bot.g_chats);
        Tox_Bot.g_chats = NULL;
        return;
    }

    struct Group_Chat *g = realloc(Tox_Bot.g_chats, n * sizeof(struct Group_Chat));

    if (g == NULL) {
        exit(EXIT_FAILURE);
    }

    Tox_Bot.g_chats = g;
}

int group_add(uint32_t groupnum, uint8_t type, const char *password)
{
    realloc_groupchats(Tox_Bot.chats_idx + 1);
    memset(&Tox_Bot.g_chats[Tox_Bot.chats_idx], 0, sizeof(struct Group_Chat));

    for (int i = 0; i <= Tox_Bot.chats_idx && i < MAX_NUM_GROUPS; ++i) {
        if (Tox_Bot.g_chats[i].active) {
            continue;
        }

        memset(&Tox_Bot.g_chats[i], 0, sizeof(struct Group_Chat));
        Tox_Bot.g_chats[i].groupnum = groupnum;
        Tox_Bot.g_chats[i].active = true;
        Tox_Bot.g_chats[i].type = type;

        if (password) {
            Tox_Bot.g_chats[i].has_pass = true;
            snprintf(Tox_Bot.g_chats[i].password, sizeof(Tox_Bot.g_chats[i].password), "%s", password);
        }

        if (Tox_Bot.chats_idx == i) {
            ++Tox_Bot.chats_idx;
        }

        return 0;
    }

    return -1;
}

void group_leave(uint32_t groupnum)
{
    int i;

    for (i = 0; i < Tox_Bot.chats_idx; ++i) {
        if (Tox_Bot.g_chats[i].active && Tox_Bot.g_chats[i].groupnum == groupnum) {
            memset(&Tox_Bot.g_chats[i], 0, sizeof(struct Group_Chat));
            break;
        }
    }

    for (i = Tox_Bot.chats_idx; i > 0; --i) {
        if (Tox_Bot.g_chats[i - 1].active) {
            break;
        }
    }

    Tox_Bot.chats_idx = i;
    realloc_groupchats(i);
}

int group_index(uint32_t groupnum)
{
    for (int i = 0; i < Tox_Bot.chats_idx; ++i) {
        if (Tox_Bot.g_chats[i].active && Tox_Bot.g_chats[i].groupnum == groupnum) {
            return i;
        }
    }

    return -1;
}

