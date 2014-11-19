/*  commands.c
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
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>

#include <tox/tox.h>
#include <tox/toxav.h>

#include "toxbot.h"
#include "misc.h"
#include "groupchats.h"

#define MAX_COMMAND_LENGTH TOX_MAX_MESSAGE_LENGTH
#define MAX_NUM_ARGS 4

extern char *DATA_FILE;
extern char *MASTERLIST_FILE;
extern struct Tox_Bot Tox_Bot;

static void authent_failed(Tox *m, int friendnum)
{
    const char *outmsg = "Y..you're not my master";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
}

static void cmd_default(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Room number required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if ((groupnum == 0 && strcmp(argv[1], "0")) || groupnum < 0) {
        outmsg = "Error: Invalid room number";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    Tox_Bot.default_groupnum = groupnum;

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Default room number set to %d", groupnum);
    tox_send_message(m, friendnum, (uint8_t *) msg, strlen(msg));

    char name[TOX_MAX_NAME_LENGTH];
    int len = tox_get_name(m, friendnum, (uint8_t *) name);
    name[len] = '\0';

    printf("Default room number set to %d by %s", groupnum, name);
}

static void cmd_gmessage(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Group number required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argc < 2) {
        outmsg = "Error: Message required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (group_index(groupnum) == -1) {
        outmsg = "Error: Invalid group number";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "Error: Message must be enclosed in quotes";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[2][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    if (tox_group_message_send(m, groupnum, (uint8_t *) msg, strlen(msg)) == -1) {
        outmsg = "Error: Failed to send message.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return; 
    }

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';
    outmsg = "Message sent.";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    printf("<%s> message to group %d: %s\n", name, groupnum, msg);
}

static void cmd_group(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Please specify the group type: audio or text";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    uint8_t type = TOX_GROUPCHAT_TYPE_AV ? !strcasecmp(argv[1], "audio") : TOX_GROUPCHAT_TYPE_TEXT;

    char name[TOX_MAX_NAME_LENGTH];
    int len = tox_get_name(m, friendnum, (uint8_t *) name);
    name[len] = '\0';

    int groupnum = -1;

    if (type == TOX_GROUPCHAT_TYPE_TEXT)
        groupnum = tox_add_groupchat(m);
    else if (type == TOX_GROUPCHAT_TYPE_AV)
        groupnum = toxav_add_av_groupchat(m, NULL, NULL);
    
    if (groupnum == -1) {
        printf("Group chat creation by %s failed to initialize\n", name);
        outmsg = "Group chat instance failed to initialize.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    const char *password = argc >= 2 ? argv[2] : NULL;

    if (password && strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        printf("Group chat creation by %s failed: Password too long\n", name);
        outmsg = "Group chat instance failed to initialize: Password too long";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (group_add(groupnum, type, password) == -1) {
        printf("Group chat creation by %s failed", name);
        outmsg = "Group chat creation failed";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        tox_del_groupchat(m, groupnum);
        return;  
    }

    const char *pw = password ? " (Password protected)" : "";
    printf("Group chat %d created by %s%s\n", groupnum, name, pw);

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Group chat %d created%s", groupnum, pw);
    tox_send_message(m, friendnum, (uint8_t *) msg, strlen(msg));
}

static void cmd_help(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    outmsg = "info : Print my current status and list active group chats";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    outmsg = "id : Print my Tox ID";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    outmsg = "invite : Request invite to default group chat";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    outmsg = "invite <n> <p> : Request invite to group chat n (with password p if protected)";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    if (friend_is_master(m, friendnum)) {
        outmsg = "For a list of master commands see the commands.txt file";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    }
}

static void cmd_id(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char myid[TOX_CLIENT_ID_SIZE * 2 + 1] = {0};
    char address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(m, (uint8_t *) address);
    int i;

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
        char d[3];
        snprintf(d, sizeof(d), "%02X", address[i] & 0xff);
        strcat(myid, d);
    }

    char outmsg[MAX_COMMAND_LENGTH];
    snprintf(outmsg, sizeof(outmsg), "%s", myid);
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
}

static void cmd_info(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    char outmsg[MAX_COMMAND_LENGTH];
    char timestr[64];

    uint64_t curtime = (uint64_t) time(NULL);
    get_elapsed_time_str(timestr, sizeof(timestr), curtime - Tox_Bot.start_time);
    snprintf(outmsg, sizeof(outmsg), "Uptime: %s", timestr);
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    uint32_t numfriends = tox_count_friendlist(m);
    uint32_t numonline = tox_get_num_online_friends(m);
    snprintf(outmsg, sizeof(outmsg), "Friends: %d (%d online)", numfriends, numonline);
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    snprintf(outmsg, sizeof(outmsg), "Inactive friends are purged after %lu days",
                                      Tox_Bot.inactive_limit / SECONDS_IN_DAY);
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));

    /* List active group chats and number of peers in each */
    uint32_t numchats = tox_count_chatlist(m);

    if (numchats == 0) {
        tox_send_message(m, friendnum, (uint8_t *) "No active groupchats", strlen("No active groupchats"));
        return;
    }

    int32_t *groupchat_list = malloc(numchats * sizeof(int32_t));

    if (groupchat_list == NULL)
        exit(EXIT_FAILURE);

    if (tox_get_chatlist(m, groupchat_list, numchats) == 0) {
        free(groupchat_list);
        return;
    }

    uint32_t i;

    for (i = 0; i < numchats; ++i) {
        uint32_t groupnum = groupchat_list[i]; 
        int num_peers = tox_group_number_peers(m, groupnum);

        if (num_peers != -1) {
            int idx = group_index(groupnum);
            const char *title = Tox_Bot.g_chats[idx].title_len
                              ? Tox_Bot.g_chats[idx].title : "None";
            const char *type = tox_group_get_type(m, groupnum) == TOX_GROUPCHAT_TYPE_TEXT ? "Text" : "Audio";
            snprintf(outmsg, sizeof(outmsg), "Group %d | %s | peers: %d | Title: %s", groupnum, type,
                                                                                      num_peers, title);
            tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        }
    }

    free(groupchat_list);
}

static void cmd_invite(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;
    int groupnum = Tox_Bot.default_groupnum;

    if (argc >= 1) {
        groupnum = atoi(argv[1]);

        if (groupnum == 0 && strcmp(argv[1], "0")) {
            outmsg = "Error: Invalid group number";
            tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
            return;
        }
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
        outmsg = "Group doesn't exist.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int has_pass = Tox_Bot.g_chats[idx].has_pass;

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    const char *passwd = NULL;

    if (argc >= 2)
        passwd = argv[2];

    if (has_pass && (!passwd || strcmp(argv[2], Tox_Bot.g_chats[idx].password) != 0)) {
        fprintf(stderr, "Failed to invite %s to group %d (invalid password)\n", name, groupnum);
        outmsg = "Invalid password.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (tox_invite_friend(m, friendnum, groupnum) == -1) {
        fprintf(stderr, "Failed to invite %s to group %d\n", name, groupnum);
        outmsg = "Invite failed. Please report this problem on irc #tox @freenode.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    printf("Invited %s to group %d\n", name, groupnum);
}

static void cmd_leave(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Group number required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (tox_del_groupchat(m, groupnum) == -1) {
        outmsg = "Error: Invalid group number";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    char msg[MAX_COMMAND_LENGTH];
    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    group_leave(groupnum);

    printf("Left group %d (%s)\n", groupnum, name);
    snprintf(msg, sizeof(msg), "Left group %d", groupnum);
    tox_send_message(m, friendnum, (uint8_t *) msg, strlen(msg));
}

static void cmd_master(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Tox ID required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    const char *id = argv[1];

    if (strlen(id) != TOX_FRIEND_ADDRESS_SIZE * 2) {
        outmsg = "Error: Invalid Tox ID";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    FILE *fp = fopen(MASTERLIST_FILE, "a");

    if (fp == NULL) {
        outmsg = "Error: could not find masterkeys file";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    fprintf(fp, "%s\n", id);
    fclose(fp);

    char name[TOX_MAX_NAME_LENGTH];
    int len = tox_get_name(m, friendnum, (uint8_t *) name);
    name[len] = '\0';

    printf("%s added master: %s\n", name, id);
    outmsg = "ID added to masterkeys list";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
}

static void cmd_name(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: Name required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
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
    tox_set_name(m, (uint8_t *) name, (uint16_t) len);

    char m_name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) m_name);
    m_name[nlen] = '\0';

    printf("%s set name to %s\n", m_name, name);
    save_data(m, DATA_FILE);
}

static void cmd_passwd(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (argc < 1) {
        outmsg = "Error: group number required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int idx = group_index(groupnum);

    if (idx == -1) {
        outmsg = "Error: Invalid group number";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    /* no password */
    if (argc < 2) {
        Tox_Bot.g_chats[idx].has_pass = false;
        memset(Tox_Bot.g_chats[idx].password, 0, MAX_PASSWORD_SIZE);

        outmsg = "No password set";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        printf("No password set for group %d by %s\n", groupnum, name);
        return;
    }

    if (strlen(argv[2]) >= MAX_PASSWORD_SIZE) {
        outmsg = "Password too long";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    Tox_Bot.g_chats[idx].has_pass = true;
    snprintf(Tox_Bot.g_chats[idx].password, sizeof(Tox_Bot.g_chats[idx].password), "%s", argv[2]);

    outmsg = "Password set";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    printf("Password for group %d set by %s\n", groupnum, name);

}

static void cmd_purge(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: number > 0 required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    uint64_t days = (uint64_t) atoi(argv[1]);

    if (days <= 0) {
        outmsg = "Error: number > 0 required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    uint64_t seconds = days * SECONDS_IN_DAY;
    Tox_Bot.inactive_limit = seconds;

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "Purge time set to %lu days", days);
    tox_send_message(m, friendnum, (uint8_t *) msg, strlen(msg));

    printf("Purge time set to %lu days by %s\n", days, name);
}

static void cmd_status(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: status required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    TOX_USERSTATUS type;
    const char *status = argv[1];

    if (strcasecmp(status, "online") == 0)
        type = TOX_USERSTATUS_NONE;
    else if (strcasecmp(status, "away") == 0)
        type = TOX_USERSTATUS_AWAY;
    else if (strcasecmp(status, "busy") == 0)
        type = TOX_USERSTATUS_BUSY;
    else {
        outmsg = "Invalid status. Valid statuses are: online, busy and away.";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    tox_set_user_status(m, type);

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    printf("%s set status to %s\n", name, status);
    save_data(m, DATA_FILE);
}

static void cmd_statusmessage(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 1) {
        outmsg = "Error: message required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argv[1][0] != '\"') {
        outmsg = "Error: message must be enclosed in quotes";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_COMMAND_LENGTH];
    snprintf(msg, sizeof(msg), "%s", &argv[1][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    tox_set_status_message(m, (uint8_t *) msg, len);

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    printf("%s set status message to \"%s\"\n", name, msg);
    save_data(m, DATA_FILE);
}

static void cmd_title_set(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH])
{
    const char *outmsg;

    if (!friend_is_master(m, friendnum)) {
        authent_failed(m, friendnum);
        return;
    }

    if (argc < 2) {
        outmsg = "Error: Two arguments are required";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    if (argv[2][0] != '\"') {
        outmsg = "Error: title must be enclosed in quotes";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {
        outmsg = "Error: Invalid group number";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        return;
    }

    /* remove opening and closing quotes */
    char title[MAX_COMMAND_LENGTH];
    snprintf(title, sizeof(title), "%s", &argv[2][1]);
    int len = strlen(title) - 1;
    title[len] = '\0';

    char name[TOX_MAX_NAME_LENGTH];
    int nlen = tox_get_name(m, friendnum, (uint8_t *) name);
    name[nlen] = '\0';

    if (tox_group_set_title(m, groupnum, (uint8_t *) title, len) != 0) {
        outmsg = "Failed to set title. This may be caused by an invalid group number or an empty room";
        tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
        printf("%s failed to set the title '%s' for group %d\n", name, title, groupnum);
        return;
    }

    int idx = group_index(groupnum);
    memcpy(Tox_Bot.g_chats[idx].title, title, len);
    Tox_Bot.g_chats[idx].title_len = len;

    outmsg = "Group title set";
    tox_send_message(m, friendnum, (uint8_t *) outmsg, strlen(outmsg));
    printf("%s set group %d title to %s\n", name, groupnum, title);
}

/* Parses input command and puts args into arg array.
   Returns number of arguments on success, -1 on failure. */
static int parse_command(const char *input, char (*args)[MAX_COMMAND_LENGTH])
{
    char *cmd = strdup(input);

    if (cmd == NULL)
        exit(EXIT_FAILURE);

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

        if (cmd[i] == '\0')    /* no more args */
            break;

        char tmp[MAX_COMMAND_LENGTH];
        snprintf(tmp, sizeof(tmp), "%s", &cmd[i + 1]);
        strcpy(cmd, tmp);    /* tmp will always fit inside cmd */
    }

    free(cmd);
    return num_args;
}

static struct {
    const char *name;
    void (*func)(Tox *m, int friendnum, int argc, char (*argv)[MAX_COMMAND_LENGTH]);
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

static int do_command(Tox *m, int friendnum, int num_args, char (*args)[MAX_COMMAND_LENGTH])
{
    int i;

    for (i = 0; commands[i].name; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(m, friendnum, num_args - 1, args);
            return 0;
        }
    }

    return -1;
}

int execute(Tox *m, int friendnum, const char *input, int length)
{
    if (length >= MAX_COMMAND_LENGTH)
        return -1;

    char args[MAX_NUM_ARGS][MAX_COMMAND_LENGTH];
    int num_args = parse_command(input, args);

    if (num_args == -1)
        return -1;

    return do_command(m, friendnum, num_args, args);
}
