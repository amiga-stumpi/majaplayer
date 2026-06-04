# MajaPlayer

MajaPlayer is a small AmigaOS 1.3 Workbench MOD player front-end for the
mods.c64.social MOD archive service.

Version:

    MajaPlayer v0.2 by Marcel Jaehne (c)2026

## Scope

- Kickstart/Workbench 1.3 compatible Intuition window
- 68000 compatible
- Network and file transfer buffers are static to avoid small Workbench stack crashes
- Uses `bsdsocket.library` for HTTP downloads
- Fetches a random MOD from:
  - `http://mods.c64.social/api/random.txt`
- Downloads the current MOD to:
  - `RAM:MajaPlayer.mod`
- Optional Save button copies the current MOD to:
  - `MajaPlayer_saved.mod`

## GUI

- Play: fetches a random MOD, downloads it, and starts playback through an
  external MiniMod command if available.
- Stop: sends a best-effort DOS `Break` command for the external player.
- Skip: stops the current external player best-effort, fetches another random MOD, and starts it.
- Download: saves the currently downloaded MOD file.
- Title line: shows the currently loaded MOD title.

## Playback note

The v0.2 player core is intentionally isolated. It currently expects a
`MiniMod` executable in the command path and launches it with:

    Run >NIL: MiniMod RAM:MajaPlayer.mod

The next step is replacing this wrapper with an embedded OS1.3-safe MOD replay
core so Stop can control playback directly.

## Build

    make clean && make

Output:

    build/MajaPlayer

## Debug build

For API/download troubleshooting:

    make clean && make debug

The debug build writes fetch diagnostics to:

    RAM:MajaPlayer_debug.log
