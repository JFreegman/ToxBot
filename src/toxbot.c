/*  toxbot.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <inttypes.h>

#include <tox/tox.h>
#include <tox/toxav.h>

#include "misc.h"
#include "commands.h"
#include "toxbot.h"
#include "groupchats.h"

#define VERSION "0.0.2"
#define FRIEND_PURGE_INTERVAL (60 * 60)
#define GROUP_PURGE_INTERVAL (60 * 10)
#define MAX_NUM_GROUPS 256

bool FLAG_EXIT = false;    /* set on SIGINT */
char *DATA_FILE = "toxbot_save";
char *MASTERLIST_FILE = "masterkeys";

struct Tox_Bot Tox_Bot;

static void init_toxbot_state(void)
{
    Tox_Bot.start_time = (uint64_t) time(NULL);
    Tox_Bot.default_groupnum = 0;
    Tox_Bot.chats_idx = 0;
    Tox_Bot.num_online_friends = 0;

    /* 1 year default; anything lower should be explicitly set until we have a config file */
    Tox_Bot.inactive_limit = 31536000;
}

static void catch_SIGINT(int sig)
{
    FLAG_EXIT = true;
}

static void exit_groupchats(Tox *m, size_t numchats)
{
    memset(Tox_Bot.g_chats, 0, Tox_Bot.chats_idx * sizeof(struct Group_Chat));
    realloc_groupchats(0);

    uint32_t chatlist[numchats];
    tox_conference_get_chatlist(m, chatlist);

    size_t i;

    for (i = 0; i < numchats; ++i) {
        tox_conference_delete(m, chatlist[i], NULL);
    }
}

static void exit_toxbot(Tox *m)
{
    size_t numchats = tox_conference_get_chatlist_size(m);

    if (numchats) {
        exit_groupchats(m, numchats);
    }

    save_data(m, DATA_FILE);
    tox_kill(m);
    exit(EXIT_SUCCESS);
}

/* Returns true if friendnumber's Tox ID is in the masterkeys list, false otherwise.
   Note that it only compares the public key portion of the IDs. */
bool friend_is_master(Tox *m, uint32_t friendnumber)
{
    if (!file_exists(MASTERLIST_FILE)) {
        FILE *fp = fopen(MASTERLIST_FILE, "w");

        if (fp == NULL) {
            fprintf(stderr, "Warning: failed to create masterkeys file\n");
            return false;
        }

        fclose(fp);
        fprintf(stderr, "Warning: creating new masterkeys file. Did you lose the old one?\n");
        return false;
    }

    FILE *fp = fopen(MASTERLIST_FILE, "r");

    if (fp == NULL) {
        fprintf(stderr, "Warning: failed to read masterkeys file\n");
        return false;
    }

    char friend_key[TOX_PUBLIC_KEY_SIZE];
    if (tox_friend_get_public_key(m, friendnumber, (uint8_t *) friend_key, NULL) == 0) {
        fclose(fp);
        return false;
    }

    char id[256];

    while (fgets(id, sizeof(id), fp)) {
        int len = strlen(id);

        if (--len < TOX_PUBLIC_KEY_SIZE)
            continue;

        char *key_bin = hex_string_to_bin(id);

        if (memcmp(key_bin, friend_key, TOX_PUBLIC_KEY_SIZE) == 0) {
            free(key_bin);
            fclose(fp);
            return true;
        }

        free(key_bin);
    }

    fclose(fp);
    return false;
}

/* START CALLBACKS */
static void cb_self_connection_change(Tox *m, TOX_CONNECTION connection_status, void *userdata)
{
    switch (connection_status) {
        case TOX_CONNECTION_NONE:
            fprintf(stderr, "Connection to Tox network has been lost\n");
            break;

        case TOX_CONNECTION_TCP:
            fprintf(stderr, "Connection to Tox network is weak (using TCP)\n");
            break;

        case TOX_CONNECTION_UDP:
            fprintf(stderr, "Connection to Tox network is strong (using UDP)\n");
            break;
    }
}

static void cb_friend_connection_change(Tox *m, uint32_t friendnumber, TOX_CONNECTION connection_status, void *userdata)
{
    /* Count the number of online friends.
     *
     * We have to do this the hard way because our convenient API function to get
     * the number of online friends has mysteriously vanished
     */

    Tox_Bot.num_online_friends = 0;

    size_t i, size = tox_self_get_friend_list_size(m);

    if (size == 0)
        return;

    uint32_t list[size];
    tox_self_get_friend_list(m, list);

    for (i = 0; i < size; ++i) {
        if (tox_friend_get_connection_status(m, list[i], NULL) != TOX_CONNECTION_NONE)
            ++Tox_Bot.num_online_friends;
    }
}

static void cb_friend_request(Tox *m, const uint8_t *public_key, const uint8_t *data, size_t length,
                              void *userdata)
{
    TOX_ERR_FRIEND_ADD err;
    tox_friend_add_norequest(m, public_key, &err);

    if (err != TOX_ERR_FRIEND_ADD_OK)
        fprintf(stderr, "tox_friend_add_norequest failed (error %d)\n", err);

    save_data(m, DATA_FILE);
}

static void cb_friend_message(Tox *m, uint32_t friendnumber, TOX_MESSAGE_TYPE type, const uint8_t *string,
                              size_t length, void *userdata)
{
    if (type != TOX_MESSAGE_TYPE_NORMAL)
        return;

    const char *outmsg;
    char message[TOX_MAX_MESSAGE_LENGTH];
    length = copy_tox_str(message, sizeof(message), (const char *) string, length);
    message[length] = '\0';

    if (length && execute(m, friendnumber, message, length) == -1) {
        outmsg = "Invalid command. Type help for a list of commands";
        tox_friend_send_message(m, friendnumber, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    }
}

static void cb_group_invite(Tox *m, uint32_t friendnumber, TOX_CONFERENCE_TYPE type,
                            const uint8_t *cookie, size_t length, void *userdata)
{
    if (!friend_is_master(m, friendnumber))
        return;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnumber, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnumber, NULL);
    name[len] = '\0';

    int groupnum = -1;

    if (type == TOX_CONFERENCE_TYPE_TEXT) {
        TOX_ERR_CONFERENCE_JOIN err;
        groupnum = tox_conference_join(m, friendnumber, cookie, length, &err);

        if (err != TOX_ERR_CONFERENCE_JOIN_OK) {
            goto on_error;
        }
    } else if (type == TOX_CONFERENCE_TYPE_AV) {
        groupnum = toxav_join_av_groupchat(m, friendnumber, cookie, length, NULL, NULL);

        if (groupnum == -1) {
            goto on_error;
        }
    }

    if (group_add(groupnum, type, NULL) == -1) {
        fprintf(stderr, "Invite from %s failed (group_add failed)\n", name);
        tox_conference_delete(m, groupnum, NULL);
        return;
    }

    printf("Accepted groupchat invite from %s [%d]\n", name, groupnum);
    return;

on_error:
    fprintf(stderr, "Invite from %s failed (core failure)\n", name);
}

static void cb_group_titlechange(Tox *m, uint32_t groupnumber, uint32_t peernumber, const uint8_t *title,
                                 size_t length, void *userdata)
{
    char message[TOX_MAX_MESSAGE_LENGTH];
    length = copy_tox_str(message, sizeof(message), (const char *) title, length);

    int idx = group_index(groupnumber);

    if (idx == -1)
        return;

    memcpy(Tox_Bot.g_chats[idx].title, message, length + 1);
    Tox_Bot.g_chats[idx].title_len = length;
}
/* END CALLBACKS */

int save_data(Tox *m, const char *path)
{
    if (path == NULL)
        goto on_error;

    FILE *fp = fopen(path, "wb");

    if (fp == NULL)
        return -1;

    size_t data_len = tox_get_savedata_size(m);
    char *data = malloc(data_len);

    if (data == NULL)
        goto on_error;

    tox_get_savedata(m, (uint8_t *) data);

    if (fwrite(data, data_len, 1, fp) != 1) {
        free(data);
        fclose(fp);
        goto on_error;
    }

    free(data);
    fclose(fp);
    return 0;

on_error:
    fprintf(stderr, "Warning: save_data failed\n");
    return -1;
}

static Tox *load_tox(struct Tox_Options *options, char *path)
{
    FILE *fp = fopen(path, "rb");
    Tox *m = NULL;

    if (fp == NULL) {
        TOX_ERR_NEW err;
        m = tox_new(options, &err);

        if (err != TOX_ERR_NEW_OK) {
            fprintf(stderr, "tox_new failed with error %d\n", err);
            return NULL;
        }

        save_data(m, path);
        return m;
    }

    off_t data_len = file_size(path);

    if (data_len == 0) {
        fclose(fp);
        return NULL;
    }

    char data[data_len];

    if (fread(data, sizeof(data), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    TOX_ERR_NEW err;
    options->savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
    options->savedata_data = (uint8_t *) data;
    options->savedata_length = data_len;

    m = tox_new(options, &err);

    if (err != TOX_ERR_NEW_OK) {
        fprintf(stderr, "tox_new failed with error %d\n", err);
        return NULL;
    }

    fclose(fp);
    return m;
}

static Tox *init_tox(void)
{
    struct Tox_Options tox_opts;
    memset(&tox_opts, 0, sizeof(struct Tox_Options));
    tox_options_default(&tox_opts);

    Tox *m = load_tox(&tox_opts, DATA_FILE);

    if (!m)
        return NULL;

    tox_callback_self_connection_status(m, cb_self_connection_change);
    tox_callback_friend_connection_status(m, cb_friend_connection_change);
    tox_callback_friend_request(m, cb_friend_request);
    tox_callback_friend_message(m, cb_friend_message);
    tox_callback_conference_invite(m, cb_group_invite);
    tox_callback_conference_title(m, cb_group_titlechange);

    size_t s_len = tox_self_get_status_message_size(m);

    if (s_len == 0) {
        const char *statusmsg = "Send me the the command 'help' for more info";
        tox_self_set_status_message(m, (uint8_t *) statusmsg, strlen(statusmsg), NULL);
    }

    size_t n_len = tox_self_get_name_size(m);

    if (n_len == 0)
        tox_self_set_name(m, (uint8_t *) "ToxBot", strlen("ToxBot"), NULL);

    return m;
}

/* TODO: hardcoding is bad stop being lazy */
static struct toxNodes {
    const char *ip;
    uint16_t    port;
    const char *key;
} nodes[] = {
    { "178.62.250.138",     33445, "788236D34978D1D5BD822F0A5BEBD2C53C64CC31CD3149350EE27D4D9A2F9B6B" },
    { "130.133.110.14",     33445, "461FA3776EF0FA655F1A05477DF1B3B614F7D6B124F7DB1DD4FE3C08B03B640F" },
    { "128.199.199.197",    33445, "B05C8869DBB4EDDD308F43C1A974A20A725A36EACCA123862FDE9945BF9D3E09" },
    { "146.185.136.123",    33445, "09993FAF174DFFDC515B398A2EFC5639C4E6D7B736FC864F89786B50EAF88C1A" },
    { "193.124.186.205",    5228,  "9906D65F2A4751068A59D30505C5FC8AE1A95E0843AE9372EAFA3BAB6AC16C2C" },
    { "185.25.116.107",     33445, "DA4E4ED4B697F2E9B000EEFE3A34B554ACD3F45F5C96EAEA2516DD7FF9AF7B43" },
    { "5.189.176.217",      33445, "2B2137E094F743AC8BD44652C55F41DFACC502F125E99E4FE24D40537489E32F" },
    { "46.101.197.175",     443,   "CD133B521159541FB1D326DE9850F5E56A6C724B5B8E5EB5CD8D950408E95707" },
    { NULL, 0, NULL },
};

static void bootstrap_DHT(Tox *m)
{
    int i;

    for (i = 0; nodes[i].ip; ++i) {
        char *key = hex_string_to_bin(nodes[i].key);

        TOX_ERR_BOOTSTRAP err;
        tox_bootstrap(m, nodes[i].ip, nodes[i].port, (uint8_t *) key, &err);
        free(key);

        if (err != TOX_ERR_BOOTSTRAP_OK)
            fprintf(stderr, "Failed to bootstrap DHT via: %s %d (error %d)\n", nodes[i].ip, nodes[i].port, err);
    }
}

static void print_profile_info(Tox *m)
{
    printf("ToxBot version %s\n", VERSION);
    printf("ID: ");

    char address[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) address);
    int i;

    for (i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        char d[3];
        snprintf(d, sizeof(d), "%02X", address[i] & 0xff);
        printf("%s", d);
    }

    printf("\n");

    char name[TOX_MAX_NAME_LENGTH];
    size_t len = tox_self_get_name_size(m);
    tox_self_get_name(m, (uint8_t *) name);
    name[len] = '\0';

    size_t numfriends = tox_self_get_friend_list_size(m);
    printf("Name: %s\n", name);
    printf("Contacts: %d\n", (int) numfriends);
    printf("Inactive contacts purged after %"PRIu64" days\n", Tox_Bot.inactive_limit / SECONDS_IN_DAY);
}

static void purge_inactive_friends(Tox *m)
{
    size_t numfriends = tox_self_get_friend_list_size(m);

    if (numfriends == 0)
        return;

    uint32_t friend_list[numfriends];
    tox_self_get_friend_list(m, friend_list);

    size_t i;

    for (i = 0; i < numfriends; ++i) {
        uint32_t friendnum = friend_list[i];

        if (!tox_friend_exists(m, friendnum))
            continue;

        TOX_ERR_FRIEND_GET_LAST_ONLINE err;
        uint64_t last_online = tox_friend_get_last_online(m, friendnum, &err);

        if (err != TOX_ERR_FRIEND_GET_LAST_ONLINE_OK)
            continue;

        if (((uint64_t) time(NULL)) - last_online > Tox_Bot.inactive_limit)
            tox_friend_delete(m, friendnum, NULL);
    }
}

static void purge_empty_groups(Tox *m)
{
    uint32_t i;

    for (i = 0; i < Tox_Bot.chats_idx; ++i) {
        if (!Tox_Bot.g_chats[i].active)
            continue;

        TOX_ERR_CONFERENCE_PEER_QUERY err;
        uint32_t num_peers = tox_conference_peer_count(m, Tox_Bot.g_chats[i].groupnum, &err);

        if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK || num_peers <= 1) {
            fprintf(stderr, "Deleting empty group %i\n", Tox_Bot.g_chats[i].groupnum);
            tox_conference_delete(m, Tox_Bot.g_chats[i].groupnum, NULL);
            group_leave(i);

            if (i >= Tox_Bot.chats_idx) {   // group_leave modifies chats_idx
                return;
            }
        }
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, catch_SIGINT);
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    Tox *m = init_tox();

    if (m == NULL)
        exit(EXIT_FAILURE);

    init_toxbot_state();
    print_profile_info(m);
    bootstrap_DHT(m);

    uint64_t last_friend_purge = 0;
    uint64_t last_group_purge = 0;

    while (!FLAG_EXIT) {
        uint64_t cur_time = (uint64_t) time(NULL);

        if (timed_out(last_friend_purge, cur_time, FRIEND_PURGE_INTERVAL)) {
            purge_inactive_friends(m);
            save_data(m, DATA_FILE);
            last_friend_purge = cur_time;
        }

        if (timed_out(last_group_purge, cur_time, GROUP_PURGE_INTERVAL)) {
            purge_empty_groups(m);
            last_group_purge = cur_time;
        }

        tox_iterate(m, NULL);
        usleep(tox_iteration_interval(m) * 1000);;
    }

    exit_toxbot(m);
    return 0;
}
