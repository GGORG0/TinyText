# TinyText

TinyText (TText for short) is a really small text ~~editor~~ (only viewer for now) written entirely in C with the goal of not requiring any external dependencies.

## Keymap

| **Shortcut** | **Function** |
|---|---|
| **CTRL+Q** | Exit |
| **CTRL+C** | Exit |
| **Arrow keys** | Moving the cursor |
| **Page up** | Move the cursor 10 lines up |
| **Page down** | Move the cursor 10 lines down |
| **Home** | Move the cursor to the start of the line |
| **End** | Move the cursor to the end of the line |

## Command-line usage

`ttext [file]`

## Building

**Requirements:**

- The ususal `build-essential`/`base-devel`/whatever (you need make and GCC)

**Compiling:**

- Run `make` or `make all` to compile everything
- `make debug` to compile only the debug version
- `make release` to compile only the release versions
- `make clean` to remove all compiled binaries
- `make run` to run the debug version

**Running:**

- Run the `ttext` binary for the debug version
- Run the `ttext_release` binary for the dinamically-linked release version
- Run the `ttext_release_static` binary for the statically-linked release version

**Installing:**

soon
