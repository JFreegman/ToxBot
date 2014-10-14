# ToxBot
ToxBot is a remotely controlled [Tox](https://tox.im) bot whose purpose is to auto-invite friends to Tox groupchats. It accepts friend requests automatically and will auto-invite friends to the specified group chat (default is the first one it joins). It also has the ability to create and leave group chats, as well as send messages to a specified group chat. 

Although current functionality is barebones, it will be easy to expand the bot to act in more comprehensive ways once Tox group chats are fully implemented (e.g. admin duties); this was the main motivation behind creating a proper Tox bot.

## Controlling
In order to control the bot you must add your Tox ID to the masterkeys file. Once you add the bot as a friend, you can send it [privileged commands](https://github.com/JFreegman/ToxBot/blob/master/commands.txt) as normal messages.

Note: ToxBot will automatically accept a groupchat invite from a master.

### Non-privileged commands
* `help` - Print this message
* `info` - Print current status
* `id` - Print Tox ID
* `invite` - Request invite to default group chat
* `invite <n>` - Request invite to group chat n (use the info command to see active group chats)

## Dependencies
[libtoxcore](https://github.com/irungentoo/toxcore)

## Compiling
Run `make`
