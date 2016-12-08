# eon

A clever little console text editor. Written in C, forked out of [mle](https://github.com/adsr/mle).

## Building

Install main deps first:

    $ apt install tree cmake libpcre-dev # or brew install / apk add

The `tree` command is for browsing directories. It's optional but you definitely want it. 

If you want to try the experimental plugin system, you'll need to install LuaJIT:

    $ apt install libluajit-5.1-dev # brew install luajit, etc

Clone the repo and initialize submodules:

    $ git clone https://github.com/tomas/eon.git
    $ cd eon
    $ git submodule update --init --recursive

And off you go!

    $ make

To disable the plugin system open the Makefile and comment the WITH_MODULES line at the top. You can also run `make eon_static` in which case you'll get a static binary.

## Usage

You can open `eon` by providing a directory or a file name. In the first case, it'll show a list of files within that directory (provided you installed the `tree` command).

    $ eon path/to/stuff

In the second case it will, ehm, open the file.

    $ eon index.js

You can also pass a line number if you want to:

    $ eon index.js:82

Or simply start with an empty document:

    $ eon

## Tabs

To open a new tab within the editor, you can either hit `Ctrl-B` or `Ctrl-N`. The former will open a new file browser view, and the latter will start a new empty document.

As you'll see, `eon` fully supports mouse movement, so you can move around by clicking in a tab, and you can also click the middle button to close one. Double click is supported (word select on editor view and open file in file browser view) as well as text selection. Like you'd expect on the 21st century. :)

## Keybindings

Yes, `eon` have a very sane set of default keybindings. `Ctrl-C` copies, `Ctrl-V` pastes, `Ctrl-Z` performs an undo and `Ctrl-Shift-Z` triggers a redo. `Ctrl-F` starts the incremental search function. To exit, either hit `Ctrl-D` or `Ctrl-Q`.

Meta keys are supported, so you can also hit `Shift+Arrow Keys` to select text and then cut-and-paste it as you please. Last but not least, `eon` supports multi-cursor editing. To insert new cursors, either hit `Ctrl+Shift+Up/Down` or `Ctrl+Alt+Up/Down`. To cancel multi-cursor mode hit `Ctrl-D` or the `Esc` key.

The reason why `eon` has two keybindings for a few things is because every terminal supports a different set of key combos. The officially list of supported terminals is currently xterm, urxvt (rxvt-unicode), mrxvt, xfce4-terminal and iTerm. Please don't try to use `eon` from within the default OSX terminal, as most key combos won't work so you won't get the full `eon` experience. If you really want to, then read below for a few configuration tips.

## Mouse mode

If you want to disable the mouse mode you can toggle it by hitting `Alt-Backspace` or `Shift-Backspace`. This is useful if you want to copy or paste a chunk of text from or to another window in your session.

## Setting up OSX Terminal

Apple's official terminal doesn't handle two 'meta' keys simultaneously (like Ctrl or Alt + Shift) and by default doesn't event send even the basic escape sequences other terminals do. However you can change the latter so at least some of the key combinations will work. To do this, open up the app's Preferences pane and open the "Keyboard" tab within Settings. Tick the "Use option as meta key" checkbox, and then hit the Plus sign above to add the following:

 - key: cursor up,    modifier: control, action: send string to shell --> \033Oa
 - key: cursor down,  modifier: control, action: send string to shell --> \033Ob
 - key: cursor right, modifier: control, action: send string to shell --> \033Oc
 - key: cursor left,  modifier: control, action: send string to shell --> \033Ob

 - key: cursor up,    modifier: shift,   action: send string to shell --> \033[a
 - key: cursor down,  modifier: shift,   action: send string to shell --> \033[b
 - key: cursor right, modifier: shift,   action: send string to shell --> \033[c
 - key: cursor left,  modifier: shift,   action: send string to shell --> \033[b

These will let you use Shift and Control + Arrow Keys. Note that you might have some of these combinations assigned to Mission Control functions (e.g. Move left a space). In this case you'll
need to decide which one you'll want to keep. My suggestion is to remove them given that most of
these commands can be accessed via mouse gestures anyway.

## Setting up xfce4-terminal

By default Xfce's terminal maps Shift+Up/Down to scroll-one-line behaviour. In order to deactivate this so you regain that mapping for `eon`, just untick the "Scroll single line using Shift-Up/Down keys" option in the app's preferences pane.

## TODO

A bunch of stuff, but most importantly:

 - [ ] update this readme
 - [ ] the ability to customize keybindings
 - [ ] code snippets, probably via a plugin
 - [ ] language-specific syntax highlighting (it currently uses a generic highlighter for all languages)
 - [ ] ability to customize syntax highlighting colours

## Credits

Original code by [Adam Saponara](http://github.com/adsr).
Modifications by Tomás Pollak.
Contributions by you, hopefully. :)

## Copyright

(c) Apache License 2.0
