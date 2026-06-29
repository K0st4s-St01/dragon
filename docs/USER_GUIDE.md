# Dragon User Guide

Dragon is a modal editor with Helix-inspired selections, OpenGL rendering,
tree-sitter syntax support, LSP features, command completion, split windows, and
an embedded terminal.

This guide documents the implemented user-facing behavior in the current tree.

## Starting Dragon

```bash
dragon_editor
dragon_editor path/to/file.c
```

Dragon uses the current working directory as the workspace root unless changed
from the file browser or command mode.

## Modes

| Mode | Purpose |
|------|---------|
| Normal | Move, select, run commands, open panels, and operate on selections. |
| Insert | Insert text at one or more cursors. |
| Select | Extend and edit selections. |
| Command | Run `:` commands with completion. |

`Esc` closes the active panel first. If no panel is active, it returns to Normal
mode. In Insert mode it also clears extra cursors.

## Command Mode

Press `:` from Normal mode. Type a command and press `Enter`.

Completion:

| Key | Action |
|-----|--------|
| `Tab` | Accept selected completion. |
| `Shift+Tab` | Move to the previous completion and accept it. |
| `Up` / `Down` | Move through completion results. |
| `Backspace` / `Ctrl+h` | Delete a character. |
| `Esc` | Leave Command mode. |

Command completion covers command names, themes, plugins, file paths for file
commands, and open buffers for buffer commands.

Common commands:

| Command | Action |
|---------|--------|
| `:w`, `:write` | Save the current buffer. |
| `:w path`, `:write path` | Save current buffer as `path`. |
| `:q`, `:quit` | Quit. |
| `:wq`, `:x` | Save and quit. |
| `:wqa`, `:write-quit-all` | Save all buffers and quit. |
| `:qa`, `:quit-all` | Quit all. |
| `:e path`, `:open path`, `:edit path` | Open a file. |
| `:r path`, `:read path` | Insert file contents at the cursor. |
| `:mv path`, `:move path` | Move/rename the current file. |
| `:new`, `:n` | New buffer. |
| `:bn`, `:bnext` | Next buffer. |
| `:bp`, `:bprev` | Previous buffer. |
| `:b query`, `:buffer query` | Switch to a buffer by number, name, or path. |
| `:bc`, `:bclose` | Close current buffer. |
| `:bc query`, `:bclose query` | Close a buffer by number, name, or path. |
| `:cwd path` | Change process directory and workspace root. |
| `:open-workspace path` | Set workspace root without changing process dir. |
| `:reload`, `:rl` | Reload current file from disk. |
| `:sort` | Sort the current selection. |
| `:fmt`, `:format` | Format current document through configured formatter/LSP. |
| `:reflow N` | Reflow selected text to width `N`. |
| `:retab` | Convert indentation to tabs. |
| `:expandtab` | Convert indentation to spaces. |
| `:theme name`, `:colorscheme name` | Apply a theme. |
| `:theme list` | Show available theme names. |
| `:settings` | Open the settings viewer. |
| `:plugins` | Open the plugin manager. |
| `:config-reload` | Reload `dragon.toml`. |
| `:lsp-stop` | Stop all LSP clients. |
| `:lsp-restart` | Restart all LSP clients. |
| `:workspace-symbols` | Open workspace symbol picker. |
| `:workspace-diagnostics` | Open workspace diagnostics picker. |
| `:tree-sitter-subtree`, `:ts-subtree` | Open the tree-sitter inspector. |

Numeric command input such as `:42` jumps to that line.

## Normal Mode

Movement:

| Key | Action |
|-----|--------|
| `h` / `j` / `k` / `l` | Move left/down/up/right. |
| `w` / `b` / `e` | Word forward/backward/end. |
| `W` / `B` / `E` | WORD forward/backward/end. |
| `0` / `$` | Start/end of line. |
| `gg` / `G` | Start/end of document. |
| `N G` | Go to line `N`. |
| `ge` | End of document. |
| `gs` | First non-blank on line. |
| `gh` / `gl` | Line start/end. |
| `gt` / `gc` / `gb` | Top/center/bottom of view. |
| `g.` | Last modification. |
| `g\|` with count | Go to column from count. |
| `Ctrl+d` / `Ctrl+u` | Half-page down/up. |
| `Ctrl+f` / `Ctrl+b` | Page down/up. |
| `Ctrl+o` / `Ctrl+i` | Jumplist backward/forward. |

Editing:

| Key | Action |
|-----|--------|
| `i` | Insert before cursor. |
| `a` / `A` | Insert after cursor / line end. |
| `I` | Insert at line start. |
| `o` / `O` | Open line below/above. |
| `u` | Undo. |
| `U`, `Ctrl+y`, `Ctrl+Shift+z` | Redo. |
| `x` | Select current line, or extend line selection. |
| `d`, `c`, `y` | Delete/change/yank selection or start an operator. |
| `p` / `P` | Paste after/before. |
| `r<char>` | Replace selection with character. |
| `R` | Replace selection with yanked text. |
| `>` / `<` | Indent/dedent selection. |
| `=` | Format document. |
| `J` | Join selected lines. |
| `-` | Trim whitespace. |
| `` ` `` / `~` | Lowercase / toggle case. |
| `%` | Select whole file. |
| `Ctrl+a` / `Ctrl+x` | Increment/decrement number. |
| `Ctrl+c` | Toggle line comment. |
| `Ctrl+Shift+c` | Toggle block comment. |

Search and selection:

| Key | Action |
|-----|--------|
| `/` | Search forward. |
| `?` | Search backward. |
| `n` / `N` | Next/previous search match. |
| `*` | Search word or selection with word boundaries. |
| `Alt+*` | Search word or selection without word boundaries. |
| `s` | Select all matches of the current selection. |
| `S` | Split selections on matches. |
| `K` | Keep selections matching a query. |
| `Alt+K` | Remove selections matching a query. |
| `f<char>` / `F<char>` | Find character forward/backward. |
| `t<char>` / `T<char>` | Till character forward/backward. |
| `Alt+.` | Repeat last `f/F/t/T` motion. |
| `;` | Collapse selection. |
| `,` | Keep primary selection. |
| `(` / `)` | Rotate selections backward/forward. |
| `Alt+(` / `Alt+)` | Rotate selection contents backward/forward. |

Text objects:

| Key | Action |
|-----|--------|
| `iw` / `aw` | Inside/around word. |
| `i(` / `a(` | Inside/around parentheses. |
| `i[` / `a[` | Inside/around brackets. |
| `i{` / `a{` | Inside/around braces. |
| `i<` / `a<` | Inside/around angle brackets. |
| `i"` / `a"` | Inside/around double quotes. |
| `i'` / `a'` | Inside/around single quotes. |
| `` i` `` / `` a` `` | Inside/around backticks. |
| `ip` / `ap` | Inside/around paragraph. |

Surround and match:

| Key | Action |
|-----|--------|
| `mm` | Go to matching bracket. |
| `ms<char>` | Surround selection with delimiter. |
| `md<char>` | Delete surrounding delimiter. |
| `mr<from><to>` | Replace surrounding delimiter. |

Macros:

| Key | Action |
|-----|--------|
| `q<letter>` | Start recording into a register. |
| `q` while recording | Stop recording. |
| `@<letter>` | Replay a register. |
| `@@` | Replay last macro. |

## Select Mode

Press `v` to enter Select mode. Motions extend the selection. Most Normal-mode
movement keys work here too.

Useful Select-mode operations:

| Key | Action |
|-----|--------|
| `Esc` | Clear the primary selection and return to Normal. |
| `d` / `c` / `y` | Delete/change/yank selection. |
| `p` / `P` | Paste after/before. |
| `s` / `S` | Select all matches / split on matches. |
| `K` / `Alt+K` | Keep/remove matching selections. |
| `C` | Copy selection below. |
| `Alt+C` | Copy selection above. |
| `Alt+s` | Split selections on newlines. |
| `Alt+-` / `Alt+_` | Merge selections / merge consecutive selections. |
| `Alt+,` | Remove the primary selection. |
| `;` | Collapse selections. |
| `,` | Keep the primary selection. |

## Multiple Cursors

Dragon stores up to 64 cursors per document. Multiple cursors can be created by
selection operations and LSP reference selection.

Common workflows:

| Action | Keys |
|--------|------|
| Select all matches of current selection | Select text, then `s`, enter query. |
| Split selection into multiple selections | Select text, then `S`, enter query. |
| Add/copy selection below | `C`. |
| Add/copy selection above | `Alt+C`. |
| Select LSP references under cursor | `Space h`. |
| Remove extra cursors | Enter Insert and press `Esc`, or collapse/keep selections with `;` / `,`. |

When multiple cursors are active, Insert-mode text, newline, tab, and backspace
operate at each cursor.

## Space Menu

Press `Space` in Normal mode to open the Space menu. Press the listed key to run
the command, or use arrows/PageUp/PageDown to scroll the menu.

| Key | Action |
|-----|--------|
| `Space f` | File browser at workspace root. |
| `Space F` | File browser at current directory. |
| `Space o` | File browser at `$HOME`. |
| `Space b` | Buffer picker. |
| `Space j` | Jumplist picker. |
| `Space g` | Changed-file picker. |
| `Space /` | Search panel. |
| `Space ?` | Command palette. |
| `Space k` | LSP hover. |
| `Space s` / `Space S` | Document/workspace symbols. |
| `Space d` / `Space D` | Document/workspace diagnostics. |
| `Space r` | Rename symbol. |
| `Space a` | Code actions. |
| `Space h` | Select references. |
| `Space t` | Tree-sitter inspector. |
| `Space T` | Terminal panel. |
| `Space c` / `Space C` | Toggle line/block comment. |
| `Space y` / `Space Y` | Yank selection/main selection to system clipboard. |
| `Space p` / `Space P` | Paste from system clipboard after/before. |
| `Space R` | Replace selection with system clipboard. |
| `Space w ...` | Window management submenu. |

Window submenu:

| Key | Action |
|-----|--------|
| `Space w v` | Split vertical. |
| `Space w h` | Split horizontal. |
| `Space w q` | Close split. |
| `Space w j/k/H/L` | Navigate down/up/left/right. |
| `Space w z` | Maximize split. |
| `Space w e` | Equalize splits. |
| `Space w n/p` | Next/previous window. |

`Ctrl+w` also enters window command mode. In that mode `v`, `s`, `q`, `h/j/k/l`,
arrow keys, `o`, and `=` perform split/window actions.

## Panels

All modal panels close with `Esc`.

File browser:

| Key | Action |
|-----|--------|
| `j/k`, arrows | Move selection. |
| `PageUp/PageDown`, `Home/End` | Jump through entries. |
| `Enter` | Open file, choose directory, or confirm prompt action. |
| `l` / `Right` | Expand directory. |
| `h` / `Left` | Collapse directory or move to parent. |
| `a` / `A` | New file / new folder. |
| `r` | Rename entry. |
| `d` | Delete entry, then confirm with `y`. |
| `w` | Set workspace root to selected directory. |

Picker panels such as buffer picker, jumplist, changed files, LSP results,
diagnostics, symbols, plugins, and settings use `j/k` or arrows to move, Enter
to accept where applicable, and PageUp/PageDown for larger jumps when supported.

## Terminal

Open the terminal with `Ctrl+~` or `Space T`. Hide it with `Esc`.

| Key | Action |
|-----|--------|
| `PageUp` / `PageDown` | Scroll terminal history older/newer. |
| `Ctrl+Home` / `Ctrl+End` | Jump to oldest history / live output. |
| `Ctrl+Shift+r` | Restart the shell. |
| `Enter` when stopped | Restart the shell. |
| Arrows, Home/End, Delete, Backspace, Tab | Sent to the shell with terminal escape sequences. |
| Printable text | Sent to the shell as UTF-8. |

Typing while scrolled returns the terminal to live output.

## LSP

LSP clients are configured per language in `dragon.toml`.

Implemented LSP-facing features:

| Feature | Keys / Command |
|---------|----------------|
| Completion | Insert mode `Ctrl+x`. |
| Hover | `Space k`. |
| Go to definition | `gd`. |
| Go to type definition | `gy`. |
| Go to references | `gr`. |
| Go to implementation | `gi`. |
| Select references | `Space h`. |
| Rename | `Space r`. |
| Code actions | `Space a`. |
| Document diagnostics | `Space d`. |
| Workspace diagnostics | `Space D`, `:workspace-diagnostics`. |
| Format | `=`, `:fmt`, `:format`. |
| Restart/stop LSP | `:lsp-restart`, `:lsp-stop`. |

Diagnostics are shown in the editor/statusbar, in document/workspace diagnostic
pickers, and in hover popups for line errors when available.

## Tree-Sitter

Tree-sitter is used for syntax-aware features where a parser is available.

| Key | Action |
|-----|--------|
| `Alt+o` / `Alt+Up` | Select parent node. |
| `Alt+i` / `Alt+Down` | Select child node. |
| `Alt+Shift+i` / `Alt+Shift+Down` | Select all children. |
| `Alt+p` / `Alt+Left` | Select previous sibling. |
| `Alt+n` / `Alt+Right` | Select next sibling. |
| `Alt+a` | Select all siblings. |
| `Alt+b` / `Alt+e` | Move to parent start/end edge. |
| `Space t`, `:ts-subtree` | Inspect node at cursor. |

Language entries can provide `tree_sitter` and `tree_sitter_path` in
`dragon.toml`.

## Configuration

Dragon loads configuration from:

1. `./dragon.toml`
2. `~/.config/dragon/dragon.toml`

Example:

```toml
[editor]
tab_width = 4
font_size = 14
line_numbers = 1
line_wrapping = 0
theme = "dragon"

[lsp]
auto_format = 0
auto_hover = 1
diagnostic_delay_ms = 100

[[language]]
id = "nix"
extensions = ["nix"]
tab_width = 2
use_tabs = false
line_comment = "#"
tree_sitter = "nix"
tree_sitter_path = "plugins/nix/libtree-sitter-nix.so"
format_command = "nixfmt {file}"
lsp_command = "nil"
lsp_args = []
keywords = ["let", "in", "rec", "with"]
type_keywords = ["path", "attrs"]
macro_keywords = ["builtins"]
```

Built-in themes include:

- `dragon`
- `ember`
- `glacier`
- `black+`

Apply a theme with `:theme <name>`.

## Plugins

Plugins are TOML manifests declared from `dragon.toml`:

```toml
[[plugin]]
name = "gleam-tools"
path = "plugins/gleam"
enabled = true
```

If `path` is a directory, Dragon reads `path/dragon-plugin.toml`.

Plugin workflows:

| Command | Action |
|---------|--------|
| `:plugins` | Open plugin manager. |
| `:plugin-enable <name>` | Enable a plugin. |
| `:plugin-disable <name>` | Disable a plugin. |
| `:plugin-toggle <name>` | Toggle a plugin. |
| `:config-reload` | Reload configuration and plugins. |

Runtime plugin toggles are saved per workspace in `.dragon/plugins.state`.

## Building And Testing

```bash
./install.sh --no-install
./test.sh
```

Manual build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/dragon_editor
```
