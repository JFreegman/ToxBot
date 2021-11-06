/*  commands.c
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
#include <strings.h>

#include <tox/tox.h>
#include <tox/toxav.h>

#include "toxbot.h"
#include "misc.h"
#include "groupchats.h"
#include "log.h"

#define MAX_COMMAND_LENGTH TOX_MAX_MESSAGE_LENGTH
#define MAX_NUM_ARGS 4

extern struct Tox_Bot Tox_Bot;

static void authent_failed(Tox *m, uint32_t friendnum)
{
    const char *outmsg = "You do not have permission to use this command.";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void send_error(Tox *m, uint32_t friendnum, const char *message, int err)
{
    char outmsg[TOX_MAX_MESSAGE_LENGTH];
    snprintf(outmsg, sizeof(outmsg), "%s (error %d)", message, err);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_default(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Room number required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if ((groupnum == 0 && strcmp(argv[1], "0")) || groupnum < 0) {
        outmsg = "Error: Invalid room number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    Tox_Bot.default_groupnum = groupnum;

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Default room number set to %d", groupnum);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    log_timestamp("Default room number set to %d by %s", groupnum, name);
}

static void cmd_gmessage(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Group number required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (argc < 2) {
        outmsg = "Error: Message required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (group_index(groupnum) == -1) {
        outmsg = "Error: Invalid group number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "Error: Message must be enclosed in quotes";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[2][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    TOX_ERR_CONFERENCE_SEND_MESSAGE err;

    if (!tox_conference_send_message(m, groupnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), &err)) {
        outmsg = "Error: Failed to send message.";
        send_error(m, friendnum, outmsg, err);
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    outmsg = "Message sent.";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    log_timestamp("<%s> message to group %d: %s", name, groupnum, msg);
}

static void cmd_group(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (argc < 1) {
        outmsg = "Please specify the group type: audio or text";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    uint8_t type = TOX_CONFERENCE_TYPE_AV ? !strcasecmp(argv[1], "audio") : TOX_CONFERENCE_TYPE_TEXT;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    int groupnum = -1;

    if (type == TOX_CONFERENCE_TYPE_TEXT) {
        TOX_ERR_CONFERENCE_NEW err;
        groupnum = tox_conference_new(m, &err);

        if (err != TOX_ERR_CONFERENCE_NEW_OK) {
            log_error_timestamp(err, "Group chat creation by %s failed to initialize", name);
            outmsg = "Group chat instance failed to initialize.";
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
            return;
        }
    } else if (type == TOX_CONFERENCE_TYPE_AV) {
        groupnum = toxav_add_av_groupchat(m, NULL, NULL);

        if (groupnum == -1) {
            log_error_timestamp(-1, "Group chat creation by %s failed to initialize", name);
            outmsg = "Group chat instance failed to initialize.";
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
            return;
        }
    }

    const char *password = argc >= 2 ? argv[2] : NULL;

    if (password && strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        log_error_timestamp(-1, "Group chat creation by %s failed: Password too long", name);
        outmsg = "Group chat instance failed to initialize: Password too long";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (group_add(groupnum, type, password) == -1) {
        log_error_timestamp(-1, "Group chat creation by %s failed", name);
        outmsg = "Group chat creation failed";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        tox_conference_delete(m, groupnum, NULL);
        return;
    }

    const char *pw = password ? " (Password protected)" : "";
    log_timestamp("Group chat %d created by %s%s", groupnum, name, pw);

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Group chat %d created%s", groupnum, pw);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);
}

static void cmd_help(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    outmsg = "info : Print my current status and list active group chats";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    outmsg = "id : Print my Tox ID";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    outmsg = "invite : Request invite to default group chat";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    outmsg = "invite <n> <p> : Request invite to group chat n (with password p if protected)";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    outmsg = "group <type> <pass> : Creates a new groupchat with type: text | audio (optional password)";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    if (friend_is_master(m, friendnum)) {
        outmsg = "For a list of master commands see the commands.txt file";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    }
}

static void cmd_id(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char outmsg[TOX_ADDRESS_SIZE * 2 + 1];
    char address[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) address);

    for (size_t i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        char d[3];
        sprintf(d, "%02X", address[i] & 0xff);
        memcpy(outmsg + i * 2, d, 2);
    }

    outmsg[TOX_ADDRESS_SIZE * 2] = '\0';
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_info(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char outmsg[MAX_COMMAND_LENGTH];
    char timestr[64];

    time_t curtime = get_time();
    get_elapsed_time_str(timestr, sizeof(timestr), curtime - Tox_Bot.start_time);
    snprintf(outmsg, sizeof(outmsg), "Uptime: %s", timestr);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    uint32_t numfriends = tox_self_get_friend_list_size(m);
    snprintf(outmsg, sizeof(outmsg), "Friends: %d (%d online)", numfriends, Tox_Bot.num_online_friends);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    snprintf(outmsg, sizeof(outmsg), "Inactive friends are purged after %"PRIu64" days",
             Tox_Bot.inactive_limit / SECONDS_IN_DAY);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);

    /* List active group chats and number of peers in each */
    size_t num_chats = tox_conference_get_chatlist_size(m);

    if (num_chats == 0) {
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) "No active groupchats",
                                strlen("No active groupchats"), NULL);
        return;
    }

    uint32_t groupchat_list[num_chats];

    tox_conference_get_chatlist(m, groupchat_list);

    for (size_t i = 0; i < num_chats; ++i) {
        TOX_ERR_CONFERENCE_PEER_QUERY err;
        uint32_t groupnum = groupchat_list[i];
        uint32_t num_peers = tox_conference_peer_count(m, groupnum, &err);

        if (err == TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
            int idx = group_index(groupnum);
            const char *title = Tox_Bot.g_chats[idx].title_len
                                ? Tox_Bot.g_chats[idx].title : "None";
            const char *type = tox_conference_get_type(m, groupnum, NULL) == TOX_CONFERENCE_TYPE_AV ? "Audio" : "Text";
            snprintf(outmsg, sizeof(outmsg), "Group %d | %s | peers: %d | Title: %s", groupnum, type,
                     num_peers, title);
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        }
    }
}

static void cmd_invite(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;
    int groupnum = Tox_Bot.default_groupnum;

    if (argc >= 1) {
        groupnum = atoi(argv[1]);

        if (groupnum == 0 && strcmp(argv[1], "0")) {
            outmsg = "Error: Invalid group number";
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
            return;
        }
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
        outmsg = "Group doesn't exist.";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int has_pass = Tox_Bot.g_chats[idx].has_pass;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    const char *passwd = NULL;

    if (argc >= 2) {
        passwd = argv[2];
    }

    if (has_pass && (!passwd || strcmp(argv[2], Tox_Bot.g_chats[idx].password) != 0)) {
        log_error_timestamp(-1, "Failed to invite %s to group %d (invalid password)", name, groupnum);
        outmsg = "Invalid password.";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    TOX_ERR_CONFERENCE_INVITE err;

    if (!tox_conference_invite(m, friendnum, groupnum, &err)) {
        log_error_timestamp(err, "Failed to invite %s to group %d", name, groupnum);
        outmsg = "Invite failed";
        send_error(m, friendnum, outmsg, err);
        return;
    }

    log_timestamp("Invited %s to group %d", name, groupnum);
}

static void cmd_leave(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Group number required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (!tox_conference_delete(m, groupnum, NULL)) {
        outmsg = "Error: Invalid group number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    char msg[MAX_COMMAND_LENGTH];

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    group_leave(groupnum);

    log_timestamp("Left group %d (%s)", groupnum, name);
    snprintf(msg, sizeof(msg), "Left group %d", groupnum);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);
}

static void cmd_master(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Tox ID required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    const char *id = argv[1];

    if (strlen(id) != TOX_ADDRESS_SIZE * 2) {
        outmsg = "Error: Invalid Tox ID";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    FILE *fp = fopen(MASTERLIST_FILE, "a");

    if (fp == NULL) {
        outmsg = "Error: could not find masterkeys file";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    fprintf(fp, "%s\n", id);
    fclose(fp);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t len = tox_friend_get_name_size(m, friendnum, NULL);
    name[len] = '\0';

    log_timestamp("%s added master: %s", name, id);
    outmsg = "ID added to masterkeys list";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
}

static void cmd_name(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Name required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    int len = 0;

    if (argv[1][0] == '\"') {    /* remove opening and closing quotes */
        snprintf(name, sizeof(name), "%s", &argv[1][1]);
        len = strlen(name) - 1;
    } else {
        snprintf(name, sizeof(name), "%s", argv[1]);
        len = strlen(name);
    }

    name[len] = '\0';
    tox_self_set_name(m, (uint8_t *) name, (uint16_t) len, NULL);

    char m_name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) m_name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    m_name[nlen] = '\0';

    log_timestamp("%s set name to %s", m_name, name);
    save_data(m, DATA_FILE);
}

static void cmd_passwd(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: group number required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
        outmsg = "Error: Invalid group number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';


    /* no password */
    if (argc < 2) {
        Tox_Bot.g_chats[idx].has_pass = false;
        memset(Tox_Bot.g_chats[idx].password, 0, MAX_PASSWORD_SIZE);

        outmsg = "No password set";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        log_timestamp("No password set for group %d by %s", groupnum, name);
        return;
    }

    if (strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        outmsg = "Password too long";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    Tox_Bot.g_chats[idx].has_pass = true;
    snprintf(Tox_Bot.g_chats[idx].password, sizeof(Tox_Bot.g_chats[idx].password), "%s", argv[2]);

    outmsg = "Password set";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    log_timestamp("Password for group %d set by %s", groupnum, name);

}

static void cmd_purge(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: number > 0 required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    uint64_t days = (uint64_t) atoi(argv[1]);

    if (days <= 0) {
        outmsg = "Error: number > 0 required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    uint64_t seconds = days * SECONDS_IN_DAY;
    Tox_Bot.inactive_limit = seconds;

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Purge time set to %"PRIu64" days", days);
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) msg, strlen(msg), NULL);

    log_timestamp("Purge time set to %"PRIu64" days by %s", days, name);
}

static void cmd_status(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: status required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    TOX_USER_STATUS type;
    const char *status = argv[1];

    if (strcasecmp(status, "online") == 0) {
        type = TOX_USER_STATUS_NONE;
    } else if (strcasecmp(status, "away") == 0) {
        type = TOX_USER_STATUS_AWAY;
    } else if (strcasecmp(status, "busy") == 0) {
        type = TOX_USER_STATUS_BUSY;
    } else {
        outmsg = "Invalid status. Valid statuses are: online, busy and away.";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    tox_self_set_status(m, type);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    log_timestamp("%s set status to %s", name, status);
    save_data(m, DATA_FILE);
}

static void cmd_statusmessage(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: message required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (argv[1][0] != '\"') {
        outmsg = "Error: message must be enclosed in quotes";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[1][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    tox_self_set_status_message(m, (uint8_t *) msg, len, NULL);

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    log_timestamp("%s set status message to \"%s\"", name, msg);
    save_data(m, DATA_FILE);
}

static void cmd_title_set(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg = NULL;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 2) {
        outmsg = "Error: Two arguments are required";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "Error: title must be enclosed in quotes";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
        return;
    }

    /* remove opening and closing quotes */
    char title[MAX_COMMAND_LENGTH];
    snprintf(title, sizeof(title), "%s", &argv[2][1]);
    int len = strlen(title) - 1;
    title[len] = '\0';

    char name[TOX_MAX_NAME_LENGTH];
    tox_friend_get_name(m, friendnum, (uint8_t *) name, NULL);
    size_t nlen = tox_friend_get_name_size(m, friendnum, NULL);
    name[nlen] = '\0';

    TOX_ERR_CONFERENCE_TITLE err;

    if (!tox_conference_set_title(m, groupnum, (uint8_t *) title, len, &err)) {
        log_error_timestamp(err, "%s failed to set the title '%s' for group %d", name, title, groupnum);
        outmsg = "Failed to set title. This may be caused by an invalid group number or an empty room";
        send_error(m, friendnum, outmsg, err);
        return;
    }

    int idx = group_index(groupnum);
    memcpy(Tox_Bot.g_chats[idx].title, title, len + 1);
    Tox_Bot.g_chats[idx].title_len = len;

    outmsg = "Group title set";
    tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) outmsg, strlen(outmsg), NULL);
    log_timestamp("%s set group %d title to %s", name, groupnum, title);
}

/* Parses input command and puts args into arg array.
   Returns number of arguments on success, -1 on failure. */
static int parse_command(const char *input, char (*args)[MAX_COMMAND_LENGTH])
{
    char *cmd = strdup(input);

    if (cmd == NULL) {
        exit(EXIT_FAILURE);
    }

    int num_args = 0;
    int i = 0;    /* index of last char in an argument */

    /* characters wrapped in double quotes count as one arg */
    while (num_args < MAX_NUM_ARGS) {
        int qt_ofst = 0;    /* set to 1 to offset index for quote char at end of arg */

        if (*cmd == '\"') {
            qt_ofst = 1;
            i = char_find(1, cmd, '\"');

            if (cmd[i] == '\0') {
                free(cmd);
                return -1;
            }
        } else {
            i = char_find(0, cmd, ' ');
        }

        memcpy(args[num_args], cmd, i + qt_ofst);
        args[num_args++][i + qt_ofst] = '\0';

        if (cmd[i] == '\0') {  /* no more args */
            break;
        }

        char tmp[MAX_COMMAND_LENGTH];
        snprintf(tmp, sizeof(tmp), "%s", &cmd[i + 1]);
        strcpy(cmd, tmp);    /* tmp will always fit inside cmd */
    }

    free(cmd);
    return num_args;
}

static struct {
    const char *name;
    void (*func)(Tox *m, uint32_t friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH]);
} commands[] = {
    { "default",          cmd_default       },
    { "group",            cmd_group         },
    { "gmessage",         cmd_gmessage      },
    { "help",             cmd_help          },
    { "id",               cmd_id            },
    { "info",             cmd_info          },
    { "invite",           cmd_invite        },
    { "leave",            cmd_leave         },
    { "master",           cmd_master        },
    { "name",             cmd_name          },
    { "passwd",           cmd_passwd        },
    { "purge",            cmd_purge         },
    { "status",           cmd_status        },
    { "statusmessage",    cmd_statusmessage },
    { "title",            cmd_title_set     },
    { NULL,               NULL              },
};

static int do_command(Tox *m, uint32_t friendnum, int num_args, char (*args)[MAX_COMMAND_LENGTH])
{
    for (size_t i = 0; commands[i].name; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(m, friendnum, num_args - 1, args);
            return 0;
        }
    }

    return -1;
}

int execute(Tox *m, uint32_t friendnum, const char *input, int length)
{
    if (length >= MAX_COMMAND_LENGTH) {
        return -1;
    }

    char args[MAX_NUM_ARGS][MAX_COMMAND_LENGTH];
    int num_args = parse_command(input, args);

    if (num_args == -1) {
        return -1;
    }

    return do_command(m, friendnum, num_args, args);
}

