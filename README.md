# MajaPlayer

MajaPlayer is a small AmigaOS 1.3 Workbench MOD player front-end for the
mods.c64.social MOD archive service.

Version:

    MajaPlayer v0.3 by Marcel Jaehne (c)2026

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

- Play: fetches a random MOD, downloads it, loads it into Chip RAM, and starts embedded ptplayer playback.
- Stop: stops embedded playback and frees the loaded module memory.
- Skip: stops the current MOD, fetches another random MOD, and starts it.
- Download: saves the currently downloaded MOD file.
- Title line: shows the currently loaded MOD title.

## Playback

MajaPlayer embeds the public-domain MiniMod/ptplayer replay code by Harry
Sintonen and Frank Wille. Playback runs from a CIA timer interrupt, so the
Workbench GUI remains responsive while music is playing.

The current v0.3 loader keeps the complete MOD in Chip RAM for simplicity.
This is intentionally conservative for the first embedded playback milestone.

## Build

    make clean && make

Output:

    build/MajaPlayer

## Debug build

For API/download troubleshooting:

    make clean && make debug

The debug build writes fetch diagnostics to:

    RAM:MajaPlayer_debug.log
