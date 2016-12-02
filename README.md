# eon

A clever little console text editor. Written in C, forked out of [mle](https://github.com/adsr/mle).

## Building

Install deps first:

    $ apt install tree cmake libpcre-dev # or brew install / apk add

The `tree` command is for browsing directories. It's optional but you definitely want it.

Clone the repo and initialize submodules:

    $ git clone https://github.com/tomas/eon.git
    $ cd eon
    $ git checkout origin/work2 -b work2 # this is where I'm working right now
    $ git submodule update --init --recursive

And off you go!

    $ make

You can run `make eon_static` instead to build a static binary.

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

The reason why `eon` has two keybindings for a few things is because every terminal supports a different set of key combos. The officially list of supported terminals is currently xterm, urxvt (rxvt-unicode), mrxvt, xfce4-terminal and iTerm. Please don't try to use `eon` from within the default OSX terminal, as most key combos won't work so you won't get the full `eon` experience.

## Mouse mode

If you want to disable the mouse mode you can toggle it by hitting `Alt-Backspace` or `Shift-Backspace`. This is useful if you want to copy or paste a chunk of text from or to another window in your session.

## TODO

A bunch of stuff, but most importantly:

 - [ ] the ability to customize keybindings
 - [ ] code snippets
 - [ ] language-specific syntax highlighting (it currently uses a generic highlighter for all languages)
 - [ ] ability to customize syntax highlighting colours
