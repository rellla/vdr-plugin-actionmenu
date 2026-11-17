# vdr-plugin-actionmenu

A plugin for the Video Disk Recorder (VDR), which opens a menu with actions for the keys of the
remote control. The actions are configured in `actionmenu.conf`, e.g. the SVDRP commands of other
plugins, which have no key of their own.

Written by Andreas Baierl <ichgeh@imkreisrum.de>.

License: AGPL-3.0-or-later (see [LICENSE.md](LICENSE.md)).

- [How does it work?](#how-does-it-work)
- [Configuration](#configuration)
  - [Entries](#entries)
  - [States](#states)
  - [Headers](#headers)
  - [Actions](#actions)
  - [Chains](#chains)
  - [Sections](#sections)
  - [Placeholders](#placeholders)
- [Command line options](#command-line-options)
- [Setup](#setup)
- [Unit tests](#unit-tests)

## How does it work?

The menu is opened with a key macro in VDR's `keymacros.conf`, e.g. with a User key, which is
assigned to a free key of the remote control in `remote.conf`:

```
User1   @actionmenu
```

It can also be opened with the main menu entry (it can be hidden in the [setup](#setup)).

While the menu is open, it gets the keys of the remote control, VDR doesn't handle them:

| Key              | Function                                                              |
|------------------|-----------------------------------------------------------------------|
| 0 - 9            | run the entry with this number                                        |
| Red, Green, Yellow, Blue | run the entry of the color button                             |
| Up / Down        | select an entry of the list                                           |
| Ok               | run the selected entry                                                |
| Left / Right     | page                                                                  |
| Back             | go back to the menu above, close the menu at the top                  |

The color buttons of the skin show the entries with a color key, so they are the favorites of the
menu. An entry can open a submenu.

> [!NOTE]
> VDR keeps some keys in every menu: Menu, Info, Power, the volume keys, Mute, Channel +/-, Audio,
> Subtitles, the direct keys (Schedule, Channels, Timers, Recordings, Setup, Commands) and the
> User keys. They can't be used in the menu. A User key, which opens the menu, opens it again,
> Back closes it.

After an action, the menu is closed and VDR shows the reply of an SVDRP command as a message.
Actions with a `!` keep the menu open (e.g. to switch the pip channel several times), they show
the reply in the status line of the menu.

### Running an entry directly

Keys after `@actionmenu` in a key macro run an entry directly, without showing the menu:

```
User2   @actionmenu Blue 2      # runs entry 2 of the submenu behind Blue
User3   @actionmenu 1           # runs entry 1 of the menu
User4   @actionmenu Red         # opens the submenu behind Red (or runs the entry of Red)
```

The menu isn't shown and the action closes it, also with `!`. Replies and errors are shown as
messages, the output of `output` is shown on its page. If a key has no entry, the menu is opened
there and gets the rest of the keys.

## Configuration

The entries are read from `actionmenu.conf` in the config directory of the plugin (e.g.
`/var/lib/vdr/plugins/actionmenu/actionmenu.conf`) or from the file of the command line option
`--config`. The file is read every time the menu is opened, so changes need no restart of VDR. An
error is shown with its line, when the menu is opened.

Empty lines and lines starting with `#` are ignored. See
[actionmenu.conf.example](actionmenu.conf.example).

### Entries

Every line is an entry or the end of a submenu:

```
<key> "<text>"[?] [<state> [<refresh>]] <action>     # entry with an action
<key> "<text>" <state> [<refresh>]                   # entry, which only shows a state
- "<text>"                                           # header
<key> "<text>" [<state>] {                           # submenu
    <entries of the submenu>
}
<key> "<text>"[?] [<state>] (                        # chain of actions
    <action>
    ...
)
```

| Part      | Meaning |
|-----------|---------|
| `<key>`   | `0` - `9`, `Red`, `Green`, `Yellow`, `Blue` or `-` (no key, the entry is only selectable in the list). A key can only be used once in a menu. The entries of the color keys aren't in the list. |
| `<text>`  | the text of the entry in the menu or on the color button (keep it short there, the skins cut long texts off) |
| `?`       | optional, directly after the text: asks before the action (`<text>?` with Ok/Back) |
| `<state>` | optional, see [States](#states) |
| `<action>`| see [Actions](#actions) |

Example for `?`:

```
- "Restart VDR"?  shell systemctl restart vdr
```

### States

A command in brackets shows its reply or output as state of the entry:

| State                         | Example | Shows |
|-------------------------------|---------|-------|
| `[svdrp <plugin> <command>]`  | `1 "Passthrough" [svdrp drmdevice PASS] svdrp! drmdevice PASS toggle` | `Passthrough: on` |
| `[shell <command>]`           | `- "Temperature" [shell vcgencmd measure_temp \| cut -d= -f2] shell! true` | `Temperature: 45.0'C` |

- The state is the part of the first line of the reply or the output after the last `": "` (the
  reply `Passthrough: on` shows `on`), otherwise the whole line.
- The states are read, when the menu is opened, after an action, which keeps the menu open, and
  after a submenu is closed.
- The command must only read the state, it is run every time. VDR waits for a shell command, so it
  must be quick.
- Brackets in the command must be pairs, e.g. `[shell [ -f /tmp/x ] && echo yes || echo no]` or
  `[shell sed 's/[^0-9]//' /tmp/x]`.
- If the state can't be read (an error or no output), `?` is shown.

`[refresh]` or `[refresh <seconds>]` after the state reads it again periodically, while the menu
is shown (default: every 2 seconds, VDR handles the menu at least once per second):

```
- "Temperature" [shell vcgencmd measure_temp | cut -d= -f2] [refresh]
```

An entry with a state doesn't need an action, it only shows the state:

```
- "Temperature" [shell vcgencmd measure_temp | cut -d= -f2]
- "Output"      [svdrp drmdevice STAT]
```

It can't be selected in the list. Its key (a number or color key) reads all states again.

### Headers

An entry without key, action and state is a header, which structures the menu. It starts at the
left border and can't be selected:

```
- "Output"
1 "Detach"  svdrp drmdevice DETA
2 "Attach"  svdrp drmdevice ATTA
- "Picture in picture"
3 "On/off"  svdrp drmdevice PITO
```

### Actions

| Action | Function |
|--------|----------|
| `svdrp <plugin> <command> [<options>]` | Run an SVDRP command of a plugin, e.g. `svdrp drmdevice PITO` (like `svdrpsend PLUG drmdevice PITO`, but directly in VDR, without SVDRP connection). |
| `plugin <plugin>` | Open the main menu of a plugin (like the key macro `@plugin`). |
| `key <key> [<key> ...]` | Send keys to VDR after the menu is closed, e.g. `key Schedule` or `key Menu Down Ok`. The names are the ones of `keymacros.conf`. |
| `shell <command>` | Run a command in the background (with the user of VDR, without output). |
| `channel <number \| channel ID>` | Switch to a channel, e.g. `channel 1` or `channel S19.2E-1-1019-10301` (the ID stays valid, if the channels are sorted differently). With the color keys, these are favorite channels. |
| `output <command>` | Run a command and show its output (stdout, add `2>&1` for the errors) on a page with a fixed font, Back goes back to the menu. VDR waits for the end of the command, like with the commands of `commands.conf`. |
| `svdrp!`, `shell!` | The same, but the menu stays open. |

### Chains

A chain runs several actions in parentheses one after the other:

```
1 "TV off"? (
    svdrp drmdevice DETA
    shell /usr/local/bin/tv-off.sh
)
```

- The chain stops at the first error.
- The menu is closed after the chain, unless all actions keep it open (`svdrp!` and `shell!`).
- `plugin`, `key` and `output` can only be the last action, because they leave the menu.

### Sections

The lines `[live]`, `[replay]` and `[all]` start a section of a menu:

| Section    | The entries are shown |
|------------|-----------------------|
| `[live]`   | without replay |
| `[replay]` | during a replay (recordings and other players, e.g. the mediaplayer) |
| `[all]`    | always (also without a section) |

A section ends with the next one or the end of the (sub)menu. The entries of a submenu are always
shown, until it has its own sections. A key can be used once in `[live]` and once in `[replay]`:

```
1   "Ambient light"   svdrp hyperion AMBI toggle
[live]
Red "Das Erste"       channel S19.2E-1-1019-10301
[replay]
Red "Info"            key Info
```

The section is checked, when the menu is opened.

### Placeholders

The commands of `svdrp`, `shell` and `output` and of the states may contain placeholders, which
are replaced, when the command is run:

| Placeholder   | Replaced with |
|---------------|---------------|
| `%channel%`   | number of the current channel |
| `%channelid%` | ID of the current channel (e.g. `S19.2E-1-1019-10301`) |
| `%recording%` | directory of the replayed recording (empty without replay) |

E.g. `svdrp drmdevice PION %channel%` shows the current channel as pip. Other text with `%` stays
as it is.

In the commands of `shell`, `output` and `[shell]` states, the values are put in single quotes, so
the shell doesn't interpret them (e.g. a `$` in the name of a recording). Don't put the
placeholders in quotes there:

```
Green "Recording info"  output ls -l %recording%
```

## Command line options

```
-c FILE,  --config=FILE  read the menu from FILE
                         (default: actionmenu.conf in the config directory)
```

## Setup

| Setting                         | Default | Meaning |
|---------------------------------|---------|---------|
| `actionmenu.HideMainMenuEntry`  | `0`     | `1` = hide the main menu entry |

## Unit tests

The tests use Catch2 v3:

```
make test
```
