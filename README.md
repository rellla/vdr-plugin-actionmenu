vdr-plugin-actionmenu
==================

A plugin for the Video Disk Recorder (VDR), which opens a menu with actions for the keys of the
remote control. The actions are configured in actionmenu.conf, e.g. the SVDRP commands of other
plugins, which have no key of their own.

Written by Andreas Baierl <ichgeh@imkreisrum.de>.

License: AGPL-3.0-or-later (see LICENSE.md).

How does it work?
-----------------
The menu is opened with a key macro in VDR's keymacros.conf, e.g. with a User key, which is
assigned to a free key of the remote control in remote.conf:

	User1   @actionmenu

It can also be opened with the main menu entry (it can be hidden in the setup).

While the menu is open, it gets the keys of the remote control, VDR doesn't handle them:

- A number key runs the entry with this number.
- A color key runs the entry of its color button. The color buttons of the skin show the entries
  with a color key, so they are the favorites of the menu.
- Up/Down select an entry of the list, Ok runs it, Left/Right page.
- An entry can open a submenu, Back goes back to the menu above and closes the menu at the top.

VDR keeps some keys in every menu: Menu, Info, Power, the volume keys, Mute, Channel +/-, Audio,
Subtitles, the direct keys (Schedule, Channels, Timers, Recordings, Setup, Commands) and the User
keys. They can't be used in the menu. A User key, which opens the menu, opens it again, Back
closes it.

After an action, the menu is closed and VDR shows the reply of an SVDRP command as a message.
Actions with a "!" keep the menu open (e.g. to switch the pip channel several times), they show
the reply in the status line of the menu.

Configuration
-------------
The entries are read from actionmenu.conf in the config directory of the plugin (e.g.
/var/lib/vdr/plugins/actionmenu/actionmenu.conf) or from the file of the command line option
--config. The file is read every time the menu is opened, so changes need no restart of VDR. An
error is shown with its line, when the menu is opened.

Every line is an entry or the end of a submenu:

	<key> "<text>" <action>
	<key> "<text>" {
	    <entries of the submenu>
	}

key
	0 - 9, Red, Green, Yellow, Blue or "-" (no key, the entry is only selectable in the list).
	A key can only be used once in a menu. The entries of the color keys aren't in the list.

text
	the text of the entry in the menu or on the color button (keep it short there, the skins
	cut long texts off)

action
	svdrp <plugin> <command> [<options>]
		Run an SVDRP command of a plugin, e.g. "svdrp drmdevice PITO" (like
		"svdrpsend PLUG drmdevice PITO", but directly in VDR, without SVDRP connection).
	plugin <plugin>
		Open the main menu of a plugin (like the key macro "@plugin").
	key <key> [<key> ...]
		Send keys to VDR after the menu is closed, e.g. "key Schedule" or "key Menu Down Ok".
		The names are the ones of keymacros.conf.
	shell <command>
		Run a command in the background (with the user of VDR, without output).
	channel <number | channel ID>
		Switch to a channel, e.g. "channel 1" or "channel S19.2E-1-1019-10301" (the ID
		stays valid, if the channels are sorted differently). With the color keys,
		these are favorite channels.
	svdrp! and shell!
		The same, but the menu stays open.

Empty lines and lines starting with "#" are ignored. See actionmenu.conf.example.

Command line options
--------------------

	-c FILE,  --config=FILE  read the menu from FILE
	                         (default: actionmenu.conf in the config directory)

Setup
-----

	actionmenu.HideMainMenuEntry = 0
		1 = hide the main menu entry

Unit tests
----------
The tests use Catch2 v3: make test
