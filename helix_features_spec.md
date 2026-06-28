# Helix Feature Specification

This document replaces a Vim-style todo list with a Helix-style feature specification.

Helix is selection-first:
1. Move/select text.
2. Run an action on the current selection.
3. Select mode is the same command set as normal mode, but movement extends the selection.

## Priority Legend

- P0: core editing feature
- P1: important Helix behavior
- P2: advanced/editor quality feature
- P3: optional extension, not required for Helix compatibility

---

## 1. Core Editing Model

### P0 — Modes

- [x] ~~Implement Normal mode.~~
- [x] ~~Implement Insert mode.~~
- [x] ~~Implement Select/Extend mode.~~
- [x] ~~Implement Command mode with `:`.~~
- [x] ~~Implement minor modes:~~
  - [x] ~~Goto mode: `g`~~
  - [x] ~~Match mode: `m`~~
  - [x] ~~View mode: `z`~~
  - [x] ~~Sticky view mode: `Z`~~
  - [x] ~~Window mode: `Ctrl-w`~~
  - [x] ~~Space mode: `Space`~~

### P0 — Selections

- [x] ~~Store one or more selections.~~
- [x] ~~Store one primary selection.~~
- [x] ~~Each selection must have:~~
  - [x] ~~anchor~~
  - [x] ~~head/cursor~~
  - [x] ~~direction~~
- [x] ~~Normal-mode movement replaces the current selection.~~
- [x] ~~Select-mode movement extends the current selection.~~
- [x] ~~Editing commands operate on all selections.~~
- [x] ~~Commands that need a single cursor use the primary selection.~~

### P0 — Undo/Redo

- [x] ~~`u` — undo.~~
- [x] ~~`U` — redo.~~
- [x] ~~`Alt-u` — move backward in history.~~
- [x] ~~`Alt-U` — move forward in history.~~
- [x] ~~Insert-mode edits become one undo checkpoint when leaving insert mode.~~
- [x] ~~`Ctrl-s` in insert mode commits an undo checkpoint.~~

---

## 2. Normal Mode

## 2.1 Movement

- [x] ~~`h` / `Left` — move left.~~
- [x] ~~`j` / `Down` — move down visual line.~~
- [x] ~~`k` / `Up` — move up visual line.~~
- [x] ~~`l` / `Right` — move right.~~
- [x] ~~`w` — move to next word start.~~
- [x] ~~`b` — move to previous word start.~~
- [x] ~~`e` — move to next word end.~~
- [x] ~~`W` — move to next WORD start.~~
- [x] ~~`B` — move to previous WORD start.~~
- [x] ~~`E` — move to next WORD end.~~
- [x] ~~`f<char>` — find next character.~~
- [x] ~~`F<char>` — find previous character.~~
- [x] ~~`t<char>` — find till next character.~~
- [x] ~~`T<char>` — find till previous character.~~
- [x] ~~`Alt-.` — repeat last motion.~~
- [x] ~~`G` — go to line number if count is provided, otherwise end of file.~~
- [x] ~~`Home` — go to line start.~~
- [x] ~~`End` — go to line end.~~
- [x] ~~`Ctrl-f` / `PageDown` — page down.~~
- [x] ~~`Ctrl-b` / `PageUp` — page up.~~
- [x] ~~`Ctrl-d` — move cursor and view half page down.~~
- [x] ~~`Ctrl-u` — move cursor and view half page up.~~
- [x] ~~`Ctrl-o` — jump backward in jumplist.~~
- [x] ~~`Ctrl-i` — jump forward in jumplist.~~
- [x] ~~`Ctrl-s` — save current selection to jumplist.~~

Notes:
- `f`, `F`, `t`, and `T` are not limited to the current line.
- Use `Alt-.`, not `;`, to repeat the last character/match motion.

## 2.2 Changes

- [x] ~~`r<char>` — replace selection with character.~~
- [x] ~~`R` — replace selection with yanked text.~~
- [x] ~~`~` — toggle case of selected text.~~
- [x] ~~`` ` `` — lowercase selected text.~~
- [x] ~~`Alt-\`` — uppercase selected text.~~
- [x] ~~`i` — enter insert mode before selection.~~
- [x] ~~`a` — enter insert mode after selection.~~
- [x] ~~`I` — insert at line start.~~
- [x] ~~`A` — insert at line end.~~
- [x] ~~`o` — open new line below selection and enter insert mode.~~
- [x] ~~`O` — open new line above selection and enter insert mode.~~
- [x] ~~`.` — repeat last insert.~~
- [x] ~~`y` — yank selection.~~
- [x] ~~`p` — paste after selection.~~
- [x] ~~`P` — paste before selection.~~
- [ ] `"` followed by register — select register.
- [x] ~~`>` — indent selection.~~
- [x] ~~`<` — unindent selection.~~
- [x] ~~`=` — format selection using formatter/LSP.~~
- [x] ~~`d` — delete selection.~~
- [x] ~~`Alt-d` — delete selection without yanking.~~
- [x] ~~`c` — change selection: delete and enter insert mode.~~
- [x] ~~`Alt-c` — change selection without yanking.~~
- [x] ~~`Ctrl-a` — increment number/object under cursor.~~
- [x] ~~`Ctrl-x` — decrement number/object under cursor.~~

Removed Vim-style items:
- [ ] Do not implement `dd` as the canonical delete-line command. In Helix, use `x` then `d`.
- [ ] Do not implement `yy` as the canonical yank-line command. In Helix, use `x` then `y`.
- [ ] Do not implement `pp` as paste-before. In Helix, use `P`.
- [ ] Do not implement `>>` / `<<`. In Helix, use `>` / `<` on selections.
- [ ] Do not make `x` delete. In Helix, `x` selects/extends line.

## 2.3 Selection Manipulation

- [x] ~~`s` — select all matches inside current selections.~~
- [x] ~~`S` — split selections on matches.~~
- [x] ~~`Alt-s` — split selection on newlines.~~
- [x] ~~`Alt-minus` — merge selections.~~
- [x] ~~`Alt-_` — merge consecutive selections.~~
- [x] ~~`&` — align selections in columns.~~
- [x] ~~`_` — trim whitespace from selections.~~
- [x] ~~`;` — collapse selection to cursor.~~
- [x] ~~`Alt-;` — flip selection cursor and anchor.~~
- [x] ~~`Alt-:` — force selection direction forward.~~
- [x] ~~`,` — keep only the primary selection.~~
- [x] ~~`Alt-,` — remove primary selection.~~
- [x] ~~`C` — copy selection onto next line / add cursor below.~~
- [x] ~~`Alt-C` — copy selection onto previous line / add cursor above.~~
- [x] ~~`(` — rotate main selection backward.~~
- [x] ~~`)` — rotate main selection forward.~~
- [x] ~~`Alt-(` — rotate selection contents backward.~~
- [x] ~~`Alt-)` — rotate selection contents forward.~~
- [x] ~~`%` — select entire file.~~
- [x] ~~`x` — select current line; if already line-selected, extend to next line.~~
- [x] ~~`X` — extend selection to line bounds.~~
- [x] ~~`Alt-x` — shrink selection to line bounds.~~
- [x] ~~`J` — join lines inside selection.~~
- [x] ~~`Alt-J` — join lines with inserted space selected.~~
- [x] ~~`K` — keep selections matching pattern.~~
- [x] ~~`Alt-K` — remove selections matching pattern.~~
- [x] ~~`Ctrl-c` — comment/uncomment selections.~~
- [x] ~~`Alt-o` / `Alt-Up` — expand selection to parent syntax node.~~
- [x] ~~`Alt-i` / `Alt-Down` — shrink syntax-tree selection.~~
- [x] ~~`Alt-p` / `Alt-Left` — select previous syntax sibling.~~
- [x] ~~`Alt-n` / `Alt-Right` — select next syntax sibling.~~
- [x] ~~`Alt-a` — select all sibling syntax nodes.~~
- [x] ~~`Alt-I` / `Alt-Shift-Down` — select all child syntax nodes.~~
- [x] ~~`Alt-e` — move to end of parent syntax node.~~
- [x] ~~`Alt-b` — move to start of parent syntax node.~~

Corrected meanings:
- `;` is not repeat find; it collapses the selection.
- `,` is not reverse find; it keeps only the primary selection.
- `_` is not first non-blank; it trims whitespace.
- `%` is not match bracket; it selects the whole file.
- `s` is not substitute; it selects regex matches.

## 2.4 Search

- [x] ~~`/` — search forward using regex.~~
- [x] ~~`?` — search backward using regex.~~
- [x] ~~`n` — select next search match.~~
- [x] ~~`N` — select previous search match.~~
- [x] ~~`*` — use current selection as search pattern, with word boundaries.~~
- [x] ~~`Alt-*` — use current selection as search pattern without word-boundary detection.~~
- [ ] Support search register `/`.
- [ ] Allow selecting another search register with `"<register>`.

Removed Vim-style items:
- [ ] Do not require `#` as a default reverse word search binding.
- [ ] Do not require `Ctrl-n` as the default search match cycle in normal mode.
- [ ] Do not implement `:%s/old/new/g` as a Helix default command unless adding a custom extension.

## 2.5 Shell Commands

- [x] ~~`|` — pipe each selection through shell command and replace with output.~~
- [x] ~~`Alt-|` — pipe each selection into shell command and ignore output.~~
- [x] ~~`!` — run shell command and insert output before each selection.~~
- [x] ~~`Alt-!` — run shell command and append output after each selection.~~
- [x] ~~`$` — keep selections where shell command returns exit code 0.~~

---

## 3. Select / Extend Mode

- [x] ~~Enter with `v`.~~
- [x] ~~`Escape` returns to normal mode.~~
- [x] ~~Normal movement commands must become extending movement commands.~~
- [x] ~~Goto-mode motions must also extend selection.~~
- [x] ~~Editing commands remain mostly the same as normal mode.~~

Required select-mode equivalents:
- [x] ~~`h/j/k/l`, arrows — extend by character/visual line.~~
- [x] ~~`w/b/e/W/B/E` — extend by word/WORD.~~
- [x] ~~`f/F/t/T` — extend to character.~~
- [x] ~~`Alt-.` — repeat last motion while extending.~~
- [x] ~~`Ctrl-f` / `Ctrl-b` — page while extending.~~
- [x] ~~`Ctrl-d` / `Ctrl-u` — half-page while extending.~~
- [x] ~~`d` — delete selection.~~
- [x] ~~`Alt-d` — delete without yanking.~~
- [x] ~~`c` — change selection.~~
- [x] ~~`Alt-c` — change without yanking.~~
- [x] ~~`y` — yank selection.~~
- [x] ~~`p` / `P` — paste after/before selection.~~
- [x] ~~`>` / `<` — indent/unindent selection.~~
- [x] ~~`=` — format selection.~~
- [x] ~~`J` / `Alt-J` — join selections.~~
- [x] ~~`~`, `` ` ``, `Alt-\`` — case operations.~~
- [x] ~~`s` — select matches inside selection.~~
- [x] ~~`S` — split selection on matches.~~
- [x] ~~`Alt-s` — split on newlines.~~
- [x] ~~`;` — collapse selection.~~
- [x] ~~`Alt-;` — flip cursor and anchor.~~
- [x] ~~`,` — keep primary selection.~~
- [x] ~~`%` — select entire file.~~
- [x] ~~`n` / `N` — extend/add search matches in select mode.~~

---

## 4. Insert Mode

- [x] ~~`Escape` — return to normal mode.~~
- [x] ~~`Ctrl-s` — commit undo checkpoint.~~
- [x] ~~`Ctrl-x` — trigger completion.~~
- [x] ~~`Ctrl-r` — insert register content.~~
- [x] ~~`Ctrl-w` / `Alt-Backspace` — delete previous word.~~
- [x] ~~`Alt-d` / `Alt-Delete` — delete next word.~~
- [x] ~~`Ctrl-u` — delete to line start.~~
- [x] ~~`Ctrl-k` — delete to line end.~~
- [x] ~~`Ctrl-h` / `Backspace` / `Shift-Backspace` — delete previous char.~~
- [x] ~~`Ctrl-d` / `Delete` — delete next char.~~
- [x] ~~`Ctrl-j` / `Enter` — insert newline.~~
- [x] ~~`Up`, `Down`, `Left`, `Right` — optional beginner movement bindings.~~
- [x] ~~`PageUp`, `PageDown` — optional beginner page movement.~~
- [x] ~~`Home` — move to line start.~~
- [x] ~~`End` — move to line end including newline behavior.~~

Corrected meaning:
- `Ctrl-d` in insert mode deletes the next character. It is not dedent by default.

---

## 5. Goto Mode

Enter with `g`.

- [x] ~~`gg` — go to line number if count is provided, otherwise start of file.~~
- [x] ~~`ge` — go to end of file.~~
- [x] ~~`gf` — go to file in selection.~~
- [x] ~~`gh` — go to line start.~~
- [x] ~~`gl` — go to line end.~~
- [x] ~~`gs` — go to first non-whitespace character of line.~~
- [x] ~~`gt` — go to top of visible window.~~
- [x] ~~`gc` — go to center of visible window.~~
- [x] ~~`gb` — go to bottom of visible window.~~
- [x] ~~`gd` — go to definition using LSP. (Working - sends LSP request, parses response, navigates)~~
- [x] ~~`gy` — go to type definition using LSP. (Working - sends LSP request, parses response, navigates)~~
- [x] ~~`gr` — go to references using LSP. (Working - sends LSP request, parses response, navigates)~~
- [x] ~~`gi` — go to implementation using LSP. (Working - sends LSP request, parses response, navigates)~~
- [ ] `ga` — go to last accessed / alternate file.
- [ ] `gm` — go to last modified / alternate file.
- [x] ~~`gn` — go to next buffer.~~
- [x] ~~`gp` — go to previous buffer.~~
- [x] ~~`g.` — go to last modification in current file.~~
- [x] ~~`gj` — move down by textual line.~~
- [x] ~~`gk` — move up by textual line.~~
- [ ] `gw` — show word labels and jump/select by label.
- [x] ~~`g|` — go to column number if count is provided, otherwise start of line.~~

Removed/changed:
- [ ] Do not use `^` or `_` as the primary first-nonblank command. Use `gs`.
- [ ] Do not use `H`, `M`, `L` as default top/middle/bottom window commands. Use `gt`, `gc`, `gb`.
- [ ] Do not implement `gD` as a default declaration command unless adding a custom LSP extension.

---

## 6. Match Mode

Enter with `m`.

- [x] ~~`mm` — go to matching bracket/tree-sitter bracket.~~
- [x] ~~`ms<char>` — surround current selection with character.~~
- [x] ~~`md<char>` — delete surrounding delimiter.~~
- [x] ~~`mr<from><to>` — replace surrounding delimiter.~~
- [x] ~~`ma<object>` — select around text object.~~
- [x] ~~`mi<object>` — select inside text object.~~

Corrected meaning:
- `m` is match mode, not set-mark.
- `%` is select-all, not match bracket.

Optional extension:
- [ ] User marks/bookmarks may be added as a custom feature, but they are not part of the default Helix keymap.

---

## 7. View Mode

Enter with `z`.
Sticky view mode enters with `Z`.

- [x] ~~`zz` or `zc` — vertically center current line.~~
- [x] ~~`zt` — align current line to top.~~
- [x] ~~`zb` — align current line to bottom.~~
- [x] ~~`zm` — horizontally center/middle align.~~
- [x] ~~`zj` / `zDown` — scroll view down.~~
- [x] ~~`zk` / `zUp` — scroll view up.~~
- [x] ~~`zCtrl-f` / `zPageDown` — page down.~~
- [x] ~~`zCtrl-b` / `zPageUp` — page up.~~
- [x] ~~`zCtrl-u` — cursor/view half page up.~~
- [x] ~~`zCtrl-d` — cursor/view half page down.~~

Corrected meaning:
- `z` alone enters view mode. It is not just "center view".

---

## 8. Window Mode

Enter with `Ctrl-w`.

- [x] ~~`Ctrl-w w` / `Ctrl-w Ctrl-w` — switch to next window.~~
- [x] ~~`Ctrl-w v` / `Ctrl-w Ctrl-v` — vertical split.~~
- [x] ~~`Ctrl-w s` / `Ctrl-w Ctrl-s` — horizontal split.~~
- [ ] `Ctrl-w f` — open file in selection in horizontal split.
- [ ] `Ctrl-w F` — open file in selection in vertical split.
- [x] ~~`Ctrl-w h/j/k/l` — move to neighboring split.~~
- [x] ~~`Ctrl-w q` / `Ctrl-w Ctrl-q` — close current window.~~
- [x] ~~`Ctrl-w o` / `Ctrl-w Ctrl-o` — keep only current window.~~
- [x] ~~`Ctrl-w H/J/K/L` — swap current window left/down/up/right.~~

---

## 9. Space Mode

Enter with `Space`.

- [x] ~~`Ctrl-~` — terminal panel.~~
- [x] ~~`Space f` — file picker at LSP workspace root.~~
- [x] ~~`Space F` — file picker at current working directory.~~
- [x] ~~`Space o` — file picker at $HOME directory.~~
- [x] ~~`Space b` — buffer picker.~~
- [x] ~~`Space j` — jumplist picker.~~
- [x] ~~`Space g` — changed-file picker.~~
- [x] ~~`Space k` — hover documentation.~~
- [x] ~~`Space s` — document symbol picker.~~
- [x] ~~`Space S` — workspace symbol picker.~~
- [x] ~~`Space d` — document diagnostics picker.~~
- [x] ~~`Space D` — workspace diagnostics picker.~~
- [x] ~~`Space r` — rename symbol.~~
- [x] ~~`Space a` — code action.~~
- [x] ~~`Space h` — select references to symbol under cursor.~~
- [x] ~~`Space T` — terminal panel.~~
- [ ] `Space '` — reopen last fuzzy picker.
- [x] ~~`Space w` — enter window mode.~~
- [x] ~~`Space c` — comment/uncomment selections.~~
- [x] ~~`Space C` — block comment/uncomment.~~
- [ ] `Space Alt-c` — line comment/uncomment.
- [x] ~~`Space p` — paste system clipboard after selections.~~
- [x] ~~`Space P` — paste system clipboard before selections.~~
- [x] ~~`Space y` — yank selections to system clipboard.~~
- [x] ~~`Space Y` — yank main selection to system clipboard.~~
- [x] ~~`Space R` — replace selections with system clipboard.~~
- [x] ~~`Space /` — global search in workspace.~~
- [x] ~~`Space ?` — command palette.~~

---

## 10. Picker / File Picker

Helix has fuzzy pickers, not a traditional built-in tree file browser.

### Picker navigation

- [x] ~~`Tab` / `Down` / `Ctrl-n` — next entry.~~
- [x] ~~`Shift-Tab` / `Up` / `Ctrl-p` — previous entry.~~
- [x] ~~`PageDown` / `Ctrl-d` — page down.~~
- [x] ~~`PageUp` / `Ctrl-u` — page up.~~
- [x] ~~`Home` — first entry.~~
- [x] ~~`End` — last entry.~~
- [x] ~~`Enter` — open selected item.~~
- [ ] `Alt-Enter` — open selected item in background.
- [ ] `Ctrl-s` — open horizontally.
- [ ] `Ctrl-v` — open vertically.
- [ ] `Ctrl-t` — toggle preview.
- [x] ~~`Escape` / `Ctrl-c` — close picker.~~

### File picker behavior

- [ ] Support workspace-root file picker.
- [ ] Support current-directory file picker.
- [ ] Support ignore files:
  - [ ] `.gitignore`
  - [ ] `.ignore`
  - [ ] `.helix/ignore`
  - [ ] global Helix ignore file.
- [ ] Support hidden-file filtering.
- [ ] Support preview panel.

Optional extension:
- [ ] Tree file browser.
- [ ] Create file from browser.
- [ ] Create directory from browser.
- [ ] Delete file/directory from browser.
- [ ] Rename file/directory from browser.

These are useful, but not default Helix picker requirements.

---

## 11. Command Mode

Enter with `:`.

### Files and buffers

- [x] ~~`:open`, `:o`, `:edit`, `:e <path>` — open file.~~
- [x] ~~`:write`, `:w [path]` — write current buffer; optional path means save as.~~
- [x] ~~`:write!`, `:w! [path]` — force write.~~
- [x] ~~`:write-quit`, `:wq`, `:x [path]` — write and quit current view.~~
- [x] ~~`:write-quit!`, `:wq!`, `:x! [path]` — force write and quit.~~
- [x] ~~`:quit`, `:q` — close current view.~~
- [x] ~~`:quit!`, `:q!` — force close current view.~~
- [x] ~~`:quit-all`, `:qa` — close all views.~~
- [x] ~~`:quit-all!`, `:qa!` — force close all views.~~
- [x] ~~`:buffer-next`, `:bn`, `:bnext` — next buffer.~~
- [x] ~~`:buffer-previous`, `:bp`, `:bprev` — previous buffer.~~
- [x] ~~`:buffer-close`, `:bc`, `:bclose` — close current buffer.~~
- [x] ~~`:buffer-close!`, `:bc!`, `:bclose!` — force close current buffer.~~
- [ ] `:buffer-close-others`, `:bco` — close other buffers.
- [ ] `:buffer-close-all`, `:bca` — close all buffers without quitting.
- [x] ~~`:new`, `:n` — new scratch buffer.~~
- [x] ~~`:read`, `:r <path>` — insert file contents into buffer.~~
- [x] ~~`:move`, `:mv <path>` — move current buffer and file to a new path.~~
- [x] ~~`:reload`, `:rl` — reload current file from disk.~~
- [x] ~~`:reload-all`, `:rla` — reload all files.~~

Changed from original:
- [ ] Do not require `:bf` / `:bl` as default first/last buffer commands unless adding custom extensions.

### Editing commands

- [x] ~~`:format`, `:fmt` — format current file with formatter/LSP.~~
- [x] ~~`:sort` — sort ranges in selection.~~
- [ ] `:reflow` — hard-wrap current selection.
- [ ] `:indent-style` — set tabs/spaces indentation.
- [ ] `:line-ending` — set CRLF/LF.
- [ ] `:encoding` — set encoding.
- [ ] `:character-info`, `:char` — show character information.
- [ ] `:yank-join [separator]` — yank joined selections.

Changed from original:
- [ ] Do not require `:uniq` as a default command.
- [ ] Do not require `:%s/old/new/g` or `:s/old/new/g` as default Helix commands.

### Configuration commands

- [ ] `:theme [name]` — change/show theme.
- [ ] `:set-option`, `:set <option> <value>` — set runtime option.
- [ ] `:toggle-option`, `:toggle <option>` — toggle runtime option.
- [ ] `:get-option`, `:get <option>` — get runtime option.
- [ ] `:set-language`, `:lang [name]` — set/show buffer language.
- [ ] `:config-open` — open user config.
- [ ] `:config-open-workspace` — open workspace config.
- [ ] `:config-reload` — reload config.
- [ ] `:log-open` — open Helix log.
- [ ] `:tree-sitter-scopes` — show tree-sitter scopes.
- [x] ~~`:tree-sitter-highlight-name` — show highlight scope under cursor.~~
- [x] ~~`:tree-sitter-subtree`, `:ts-subtree` — inspect syntax subtree.~~

### Shell commands

- [ ] `:run-shell-command`, `:sh`, `:!` — run shell command.
- [ ] `:insert-output` — insert command output before selections.
- [ ] `:append-output` — append command output after selections.
- [ ] `:pipe`, `:|` — pipe selections through shell command.
- [ ] `:pipe-to` — pipe selections into command, ignore output.

### Clipboard commands

- [ ] `:clipboard-yank`
- [ ] `:clipboard-yank-join`
- [ ] `:clipboard-paste-after`
- [ ] `:clipboard-paste-before`
- [ ] `:clipboard-paste-replace`
- [ ] `:primary-clipboard-yank`
- [ ] `:primary-clipboard-yank-join`
- [ ] `:primary-clipboard-paste-after`
- [ ] `:primary-clipboard-paste-before`
- [ ] `:primary-clipboard-paste-replace`
- [ ] `:show-clipboard-provider`
- [ ] `:clear-register [register]`

---

## 12. Clipboard and Registers

- [ ] Support default yank register `"`.
- [ ] Support selecting a register with `"<register>`.
- [ ] Support named registers.
- [ ] Support search register `/`.
- [ ] Support system clipboard commands through Space mode and command mode.
- [ ] Support primary clipboard commands separately from system clipboard.
- [x] ~~Support inserting register content in insert mode with `Ctrl-r`.~~

Corrected:
- [ ] Do not model Helix exactly as Vim registers like `0`, `*`, `+` unless intentionally adding compatibility aliases.
- [ ] Prefer Helix commands: `Space-y`, `Space-p`, `Space-P`, `Space-R`, and primary clipboard commands.

---

## 13. Macros

- [ ] `Q` — start/stop recording macro to selected register.
- [ ] `q` — replay macro from selected register.
- [ ] Support selecting macro register with `"<register>`.
- [ ] Support repeat counts before replay.
- [ ] Make macro support experimental/optional if implementing in stages.

Corrected:
- [ ] Do not use Vim-style `q{register}` as the canonical recording command.
- [ ] Do not use `@{register}` as the canonical playback command.
- [ ] Do not use `Q` as "play last macro"; in Helix, `Q` records/stops.

---

## 14. Syntax, Tree-sitter, and Text Objects

- [x] ~~Syntax highlighting using tree-sitter queries.~~
- [x] ~~Language detection by file name, extension, and shebang.~~
- [ ] Per-language indentation settings.
- [ ] Per-language comment tokens.
- [ ] Per-language formatter command.
- [ ] Per-language auto-format option.
- [ ] Tree-sitter text objects:
  - [ ] `ma<object>` — select around object.
  - [ ] `mi<object>` — select inside object.
- [ ] Tree-sitter navigation:
  - [x] ~~`Alt-o` — expand to parent node.~~
  - [x] ~~`Alt-i` — shrink selection.~~
  - [x] ~~`Alt-p` / `Alt-n` — previous/next sibling.~~
  - [x] ~~`Alt-a` — all siblings.~~
  - [x] ~~`Alt-I` — all children.~~
- [ ] Tree-sitter movement:
  - [ ] `]f` / `[f` — next/previous function.
  - [ ] `]t` / `[t` — next/previous type/class.
  - [ ] `]a` / `[a` — next/previous argument/parameter.
  - [ ] `]c` / `[c` — next/previous comment.
  - [ ] `]T` / `[T` — next/previous test.

---

## 15. LSP Features

- [x] ~~LSP server process management. (Multi-server support for C, C++, Rust, Python, Go, TypeScript, JavaScript)~~
- [x] ~~Per-language server configuration. (Default configurations + auto-initialization)~~
- [x] ~~`gd` — goto definition. (Full implementation: request → parse response → navigate)~~
- [x] ~~`gy` — goto type definition. (Full implementation: request → parse response → navigate)~~
- [x] ~~`gr` — goto references. (Full implementation: request → parse response → navigate)~~
- [x] ~~`gi` — goto implementation. (Full implementation: request → parse response → navigate)~~
- [x] ~~`Space-k` — hover. (Full implementation: request → parse response → tooltip display)~~
- [x] ~~Syntax highlighting from LSP semanticTokens. (Delta-decoded parser, token type mapping, dynamic highlighting)~~
- [x] ~~`Space-r` — rename symbol.~~
- [x] ~~`Space-a` — code action.~~
- [x] ~~`Space-s` — document symbols.~~
- [x] ~~`Space-S` — workspace symbols.~~
- [x] ~~`Space-d` — document diagnostics picker.~~
- [x] ~~`Space-D` — workspace diagnostics picker.~~
- [x] ~~`Space-h` — select references to symbol under cursor.~~
- [x] ~~`Ctrl-x` in insert mode — completion menu.~~
- [x] ~~`=` — format selection.~~
- [x] ~~`:format` / `:fmt` — format file.~~
- [x] ~~`:lsp-restart` — restart servers.~~
- [x] ~~`:lsp-stop` — stop servers.~~
- [ ] `:lsp-workspace-command` — workspace commands.
- [x] ~~LSP progress spinner in statusline.~~
- [ ] Diagnostics:
  - [x] ~~Inline diagnostics.~~
  - [x] ~~Gutter diagnostics.~~
  - [x] ~~Statusline diagnostic counts.~~
  - [x] ~~`]d` / `[d` — next/previous diagnostic.~~
  - [x] ~~`]D` / `[D` — last/first diagnostic.~~

---

## 16. Completion Menu and Popups

### Completion menu

- [x] ~~`Ctrl-x` opens completion in insert mode.~~
- [x] ~~`Tab` / `Ctrl-n` / `Down` — next completion item.~~
- [x] ~~`Shift-Tab` / `Ctrl-p` / `Up` — previous completion item.~~
- [x] ~~`Enter` — accept completion.~~
- [x] ~~`Ctrl-c` — reject completion.~~
- [x] ~~Show documentation for selected completion item.~~

### Hover/signature popup

- [x] ~~`Space-k` opens hover popup.~~
- [x] ~~`Ctrl-u` / `Ctrl-d` scroll popup.~~
- [ ] Signature help popup supports:
  - [ ] `Alt-p` — previous signature.
  - [ ] `Alt-n` — next signature.

---

## 17. Configuration

- [ ] Load `config.toml`.
- [ ] Load `languages.toml`.
- [ ] Support workspace-local config.
- [ ] Support key remapping sections:
  - [ ] `[keys.normal]`
  - [ ] `[keys.insert]`
  - [ ] `[keys.select]`
- [ ] Support nested minor-mode keymaps.
- [ ] Support static command mappings.
- [ ] Support typable command mappings.
- [ ] Support macro mappings.
- [ ] Support `no_op` to disable a key.
- [ ] Support special key names:
  - [ ] `backspace`
  - [ ] `space`
  - [ ] `ret`
  - [ ] `left`
  - [ ] `right`
  - [ ] `up`
  - [ ] `down`
  - [ ] `home`
  - [ ] `end`
  - [ ] `pageup`
  - [ ] `pagedown`
  - [ ] `tab`
  - [ ] `del`
  - [ ] `ins`
  - [ ] `esc`
- [ ] Support modifiers:
  - [ ] `C-` for Ctrl
  - [ ] `A-` for Alt
  - [ ] `S-` for Shift
  - [ ] `Meta-` / `Cmd-` / `Win-` for Super where terminal supports it.

### Editor options

- [ ] `line-number`: absolute/relative.
- [ ] `mouse`: enable/disable mouse.
- [ ] `scrolloff`.
- [ ] `scroll-lines`.
- [ ] `cursorline`.
- [ ] `cursorcolumn`.
- [ ] `gutters`.
- [ ] `bufferline`.
- [ ] `color-modes`.
- [ ] `text-width`.
- [ ] `rulers`.
- [ ] `true-color`.
- [ ] `auto-completion`.
- [ ] `path-completion`.
- [ ] `auto-format`.
- [ ] `auto-pairs`.
- [ ] `auto-save`.
- [ ] `search.smart-case`.
- [ ] `search.wrap-around`.
- [ ] `whitespace`.
- [ ] `indent-guides`.
- [ ] `soft-wrap`.
- [ ] `smart-tab`.
- [ ] `inline-diagnostics`.

---

## 18. Statusline

- [ ] Configurable left/center/right statusline sections.
- [ ] Configurable separator.
- [ ] Configurable mode labels:
  - [ ] normal
  - [ ] insert
  - [ ] select
- [ ] Configurable diagnostic severities.
- [ ] Supported statusline elements:
  - [x] ~~mode~~
  - [x] ~~spinner~~
  - [x] ~~file-name~~
  - [ ] file-absolute-path
  - [x] ~~file-base-name~~
  - [x] ~~file-modification-indicator~~
  - [x] ~~file-encoding~~
  - [x] ~~file-line-ending~~
  - [x] ~~file-type~~
  - [x] ~~diagnostics~~
  - [x] ~~workspace-diagnostics~~
  - [x] ~~selections~~
  - [x] ~~primary-selection-length~~
  - [x] ~~position~~
  - [x] ~~position-percentage~~
  - [x] ~~total-line-numbers~~
  - [ ] register
  - [ ] version
- [x] ~~Show macro recording indicator.~~
- [x] ~~Show current language/file type.~~
- [x] ~~Show line ending style.~~
- [x] ~~Show file encoding.~~

Optional extension:
- [x] ~~Current git branch in statusline.~~

---

## 19. Git / Diff

- [ ] Show diff gutter.
- [ ] `]g` / `[g` — next/previous change.
- [ ] `]G` / `[G` — last/first change.
- [ ] `:reset-diff-change`, `:diffget`, `:diffg` — reset diff change at cursor.
- [x] ~~Changed-file picker with `Space-g`.~~

Optional extension:
- [ ] Full diff view.

---

## 20. Mouse

- [ ] Mouse mode option.
- [ ] Click to move primary cursor.
- [ ] Drag to create/extend selection.
- [ ] Scroll wheel support.
- [ ] Middle-click paste if enabled.

---

## 21. Optional Non-Default Extensions

The following features are useful, but they should be marked as extensions, not Helix defaults:

- [ ] Tree file browser.
- [ ] File create/delete/rename UI.
- [x] ~~Integrated terminal.~~
- [ ] First/last buffer commands `:bf` and `:bl`.
- [ ] `:uniq`.
- [ ] Current git branch in statusline.
- [ ] Undo tree visualization.
- [ ] Full diff view.
- [ ] Session persistence.
- [ ] File watcher / hot reload.
- [ ] Plugin system.

---

## Recent Improvements (Session)

### Cursor Movement Fixes
- Fixed vertical movement losing column position - now preserves desired column when moving up/down
- Fixed Select mode auto-resetting selection anchor on every keystroke
- Fixed 'i'/'a' keys inserting the character when entering insert mode

### Evil Theme Enhancement
- Updated to high-contrast colors for better visibility
- Syntax highlighting colors:
  - Keywords: Bright Magenta (1.0, 0.0, 1.0)
  - Strings: Bright Yellow-Orange (1.0, 0.8, 0.0)
  - Numbers: Bright Cyan (0.0, 1.0, 1.0)
  - Comments: Dark Red (0.3, 0.0, 0.0)
  - Functions: Bright Lime Green (0.0, 1.0, 0.0)
  - Foreground: Bright Red (1.0, 0.2, 0.2)
  - Cursor: Pure Red (1.0, 0.0, 0.0)

### LSP Integration
- Implemented LSP server process management with multi-language support
- Added language detection from file extensions
- Integrated syntax highlighting system ready for LSP semantic tokens
- Added LSP goto keybindings (gd/gy/gr/gi) with infrastructure in place
- Supported languages: C, C++, Rust, Python, Go, TypeScript, JavaScript
