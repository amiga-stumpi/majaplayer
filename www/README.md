# MajaPlayer MOD Server

Static HTTP content for an AmigaOS 1.3 friendly MOD download service.

Target domain:

```text
http://mods.c64.social/
```

The real web server can copy this whole `www/` directory to its nginx document root.
The design intentionally avoids HTTPS-only behavior, redirects, compression, JSON, PHP,
and CGI so a small Amiga HTTP client can parse it easily.

## Layout

```text
www/
  index.txt              Human-readable service description
  api/
    random.txt           Current random module metadata
    list.txt             Full module metadata list
  mods/
    put .mod files here
  tools/
    generate_api.py      Regenerates api/list.txt and api/random.txt
  nginx/
    mods.c64.social.conf Example nginx server block
```

## API Format

All API files are plain ASCII key/value text with LF line endings.

`/api/random.txt` example:

```text
OK
ID=stardust_memories
TITLE=stardust memories
SIZE=217846
PATH=/mods/stardust_memories.mod
URL=http://mods.c64.social/mods/stardust_memories.mod
```

`/api/list.txt` contains one module per line:

```text
ID|SIZE|PATH|TITLE
```

Example:

```text
stardust_memories|217846|/mods/stardust_memories.mod|stardust memories
```

## Updating the API

Copy `.mod` files into the web server's `mods/` directory. Unterordner and spaces
in directory/file names are supported; API URLs are percent-encoded automatically.

For this local template:

```sh
cd /opt/majaplayer/www
python3 tools/generate_api.py
```

On the real nginx server using `/var/www/amigamods` as document root:

```sh
cd /var/www/amigamods
python3 tools/generate_api.py --mods-dir /var/www/amigamods/mods
```

Optional size limit:

```sh
python3 tools/generate_api.py --mods-dir /var/www/amigamods/mods --max-size 500000
```

This filters out modules larger than the limit and is useful for small Amiga RAM setups.

## Amiga Client Flow

1. GET `http://mods.c64.social/api/random.txt`
2. Parse `SIZE=`, `PATH=` or `URL=`
3. Check the file fits available memory/RAM disk
4. GET the MOD file with HTTP/1.0
5. Save to `RAM:` or load into memory
6. Play using MiniMod/ptplayer

## nginx Notes

Use the example in `nginx/mods.c64.social.conf` as a starting point. The important
parts are:

- HTTP on port 80
- document root points at this `www/` directory
- no forced HTTPS redirect
- no gzip for API or MOD files
- `.mod` files served as `application/octet-stream`
- subdirectories and spaces in MOD paths are okay; API paths use `%20` etc.
