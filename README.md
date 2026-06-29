# Dragon Editor

A modal text editor built in C with OpenGL 3.3, GLFW, tree-sitter, LSP support,
and a custom immediate-mode UI.

## Features

- Modal editing with Normal, Insert, Select, and Command modes.
- Helix-inspired selections, multiple cursors, text objects, surround commands,
  macros, jumplist navigation, and split windows.
- File, buffer, jumplist, changed-file, settings, plugin, symbol, diagnostic,
  command-palette, and LSP result panels.
- Embedded terminal opened with `Ctrl+~` or `Space T`.
- tree-sitter syntax and structural selection support.
- LSP diagnostics, hover, completion, goto, references, rename, code actions,
  and formatting.
- Project and user configuration through `dragon.toml`.

## Documentation

See [docs/USER_GUIDE.md](docs/USER_GUIDE.md) for the full command reference,
panel controls, terminal controls, LSP/tree-sitter workflows, configuration,
themes, and plugin setup.

## Dependencies

- CMake >= 3.10
- pkg-config
- GLFW3 (system)
- OpenGL (system)
- tree-sitter (system)
- GLAD, stb, tomlc99 (vendored - see `vendor/README.md`)

## Building

```bash
chmod +x install.sh
./install.sh --no-install
```

Or manually:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/dragon_editor
```

## Installing

```bash
./install.sh --test
```

Useful installer options:

- `--deps` installs system dependencies for supported distros.
- `--prefix <dir>` changes the install prefix.
- `--no-install` builds without installing.
- `--debug` builds a Debug configuration.
- `--clean` removes the build directory before configuring.

The CMake install target installs `dragon_editor` and a sample config at
`share/dragon/dragon.toml.example` under the selected prefix.

## Testing

```bash
./test.sh
```

The test wrapper configures a Debug build, builds `test_all`, and runs CTest.

## Usage

```bash
dragon_editor                  # empty editor
dragon_editor <file>           # open file
```

## Quick Start

| Mode        | Key     | Action                    |
|-------------|---------|---------------------------|
| Normal      | `i`     | Enter Insert mode         |
| Normal      | `v`     | Enter Select mode         |
| Normal      | `:`     | Enter Command mode        |
| Normal      | `h/j/k/l` | Move cursor            |
| Insert      | typing  | Insert text               |
| Insert      | `Backspace` | Delete character      |
| Insert      | `Enter` | New line                  |
| Command     | `w`     | Save file                 |
| Command     | `q`     | Quit                      |
| Command     | `wq`    | Save and quit             |
| Command     | `Tab`   | Accept completion         |
| Any         | `Esc`   | Return to Normal mode     |

Common shortcuts:

- `:w` - Save file
- `/` - Find
- `Space f` - File browser at workspace root
- `Ctrl+~` - Toggle terminal
- `Space` - Open the Space menu
- `Space ?` - Command palette
- `Space b` - Buffer picker
- `Space d` / `Space D` - Document/workspace diagnostics
- `Space s` / `Space S` - Document/workspace symbols

## Configuration

Dragon reads `./dragon.toml`, falling back to `~/.config/dragon/dragon.toml`.
Configured languages can add extensions, indentation/comment settings,
tree-sitter parser paths, formatter commands, and LSP commands without
recompiling.

Plugins are TOML manifests declared with `[[plugin]]`. Use `:plugins` for the
plugin manager, or `:plugin-enable <name>`, `:plugin-disable <name>`, and
`:plugin-toggle <name>` from command mode. Runtime plugin toggles are persisted
per workspace in `.dragon/plugins.state` and reapplied on startup and
`:config-reload`.

Command mode completes command names, themes, plugins, workspace-relative file
paths for file commands, and open buffers for `:b`, `:buffer`, and
`:buffer-close`.

Built-in themes include `dragon`, `ember`, `glacier`, and `black+`. Use
`:theme <name>` to apply one.

## Project Structure

```
dragon_editor/
  CMakeLists.txt
  install.sh
  test.sh
  dragon.toml                Sample project configuration
  docs/
    USER_GUIDE.md            User guide and command reference
  src/
    main.c                  Entry point
    core/
      app.c                 Application lifecycle and event loop
      renderer.c            OpenGL renderer
      input.c               Modal key handling and command mode
    gui/panels/
      file_browser.c/h      File open dialog
      find_replace.c/h      Find & replace
      settings.c/h          Settings dialog
      terminal.c            Embedded terminal panel
    editor/
      document.c            Text editing, search, syntax, language registry
      lsp.c                 LSP transport and protocol parsing
      treesitter.c          tree-sitter parser integration
  vendor/
    glad/                   OpenGL loader
    tomlc99/                TOML parser
    stb/                    Image/font loading
  include/dragon_editor/    Public headers
  build/                    Build output
```
