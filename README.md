# MajaPlayer

MajaPlayer is a small AmigaOS 1.3 Workbench MOD player front-end for the
mods.c64.social MOD archive service.

Version:

    MajaPlayer v0.8 by Marcel Jaehne (c)2026

## Scope

- Kickstart/Workbench 1.3 compatible Intuition window
- 68000 compatible
- Network and file transfer buffers are static to avoid small Workbench stack crashes
- Uses `bsdsocket.library` for HTTP downloads
- Fetches server-side random metadata from `http://mods.c64.social/api/random.php` when available
- Falls back to local random selection from `api/list.txt` and then static `api/random.txt`
- Downloads the current MOD to:
  - `RAM:MajaPlayer.mod`
- Download button saves the current MOD using the current MOD title as filename

## GUI

The window is dynamically resizable and the controls are recalculated on size changes.

- Play: fetches a random MOD, downloads it, loads it into Chip RAM, and starts embedded ptplayer playback.
- Stop: stops embedded playback and frees the loaded module memory.
- Skip: stops the current MOD, chooses another random entry from `list.txt`, downloads it, and starts it.
- Download: saves the currently downloaded MOD file.
- Autoplay: when enabled, starts a new random download automatically after the current MOD reaches its song end.
- Title line: shows the currently loaded MOD title.

## Playback

MajaPlayer embeds the public-domain MiniMod/ptplayer replay code by Harry
Sintonen and Frank Wille. Playback runs from a CIA timer interrupt, so the
Workbench GUI remains responsive while music is playing.

The loader keeps header and pattern data in Fast/Slow RAM when available, and sample data in Chip RAM for Paula DMA. Autoplay uses the embedded ptplayer song-end flag and does not depend on a fixed playback timeout.

## Build

    make clean && make

Output:

    build/MajaPlayer

## Debug build

For API/download troubleshooting:

    make clean && make debug

The debug build writes fetch diagnostics to:

    RAM:MajaPlayer_debug.log

## Random selection

`api/random.php` performs server-side random selection from `api/list.txt` and returns the same plain text format as `random.txt`. If PHP is not enabled on the web server, MajaPlayer falls back to loading `api/list.txt` and selecting a random module locally on the Amiga.

## Memory use

On startup MajaPlayer shows available Fast/Slow RAM and Chip RAM in the status
line. Before downloading a MOD it checks the advertised `SIZE=` and skips files
that are too large for the currently available memory.

For playback, header and pattern data are allocated with Fast/Slow RAM priority
(`MEMF_FAST`) and fall back to public memory if needed. Sample data is allocated
in Chip RAM because Paula must be able to DMA it.

## Saved filenames

The Download button stores `RAM:MajaPlayer.mod` under a filename derived from the current MOD title. Spaces and Amiga path separators are replaced with underscores.
