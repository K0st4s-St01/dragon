# Dragon Editor

A modal text editor built with OpenGL 3.3, GLFW, and ImGui.

## Dependencies

- CMake >= 3.10
- pkg-config
- GLFW3 (system)
- OpenGL (system)
- GLAD, ImGui, stb (vendored - see `vendor/README.md`)

## Building

```bash
chmod +x install.sh
./install.sh
```

Or manually:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/dragon_editor
```

## Usage

```bash
dragon_editor                  # empty editor
dragon_editor <file>           # open file
```

## Modal Editing

| Mode        | Key     | Action                    |
|-------------|---------|---------------------------|
| Normal      | `i`     | Enter Insert mode         |
| Normal      | `v`     | Enter Visual mode         |
| Normal      | `:`     | Enter Command mode        |
| Normal      | `h/j/k/l` | Move cursor            |
| Insert      | typing  | Insert text               |
| Insert      | `Backspace` | Delete character      |
| Insert      | `Enter` | New line                  |
| Command     | `w`     | Save file                 |
| Command     | `q`     | Quit                      |
| Command     | `wq`    | Save and quit             |
| Any         | `Esc`   | Return to Normal mode     |

## Shortcuts

- `Ctrl+O` - Open file browser
- `Ctrl+F` - Find and replace
- `Ctrl+S` - Save file

## Project Structure

```
dragon_editor/
  CMakeLists.txt
  install.sh
  src/
    main.c                  Entry point + ImGui loop
    core/
      renderer.c/h          OpenGL renderer
      editor.c/h            Text buffer + editing
      modal.c/h             Modal dialog state
    gui/panels/
      file_browser.c/h      File open dialog
      find_replace.c/h      Find & replace
      settings.c/h          Settings dialog
    shaders/
      basic.vert/frag       Rectangle rendering
      text.vert/frag        Text rendering
  vendor/
    glad/                   OpenGL loader
    imgui/                  Immediate mode GUI
    stb/                    Image/font loading
  include/dragon_editor/    Public headers
  assets/
    fonts/                  Font files
    icons/                  Icon files
  build/                    Build output
```
