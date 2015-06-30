# ToxBot
ToxBot is a remotely controlled [Tox](https://tox.chat) bot whose purpose is to auto-invite friends to Tox groupchats. It accepts friend requests automatically and will auto-invite friends to the specified group chat (default is group 0 unless set otherwise). It also has the ability to create and leave groups, password protect invites, and send messages to groups.

Although current functionality is barebones, it will be easy to expand the bot to act in more comprehensive ways once Tox group chats are fully implemented (e.g. admin duties); this was the main motivation behind creating a proper Tox bot.

## Controlling
In order to control the bot you must add your Tox ID to the masterkeys file. Once you add the bot as a friend, you can send it [privileged commands](https://github.com/JFreegman/ToxBot/blob/master/commands.txt) as normal messages; there is no front-end.

Note: ToxBot will automatically accept a groupchat invite from a master.

### Non-privileged commands
* `help` - Print this message
* `info` - Print current status and list active group chats
* `id` - Print Tox ID
* `invite` - Request invite to default group chat
* `invite <n> <pass>` - Request invite to group chat n (with password if necessary)
* `group <type> <pass>` - Creates a new groupchat with type: text | audio (optional password)

## Dependencies
pkg-config
[libtoxcore](https://github.com/irungentoo/toxcore)
libtoxav

## Compiling
Run `make`

Note: If you get an error that says `cannot open shared object file: No such file or directory`, try running `sudo ldconfig`.
