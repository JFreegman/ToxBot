/*  toxbot.h
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

#ifndef TOXBOT_H
#define TOXBOT_H

struct Tox_Bot {
    uint64_t start_time;
    int room_num;
    uint64_t inactive_limit;
} Tox_Bot;

int load_Masters(const char *path);
int save_data(Tox *m, const char *path);
bool friend_is_master(Tox *m, int32_t friendnumber);

#endif /* TOXBOT_H */
