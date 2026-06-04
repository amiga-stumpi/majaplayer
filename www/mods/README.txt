Copy ProTracker-compatible .mod files into this directory or into subdirectories.
Spaces in directory and file names are supported; generated API URLs are percent-encoded.

Then run locally:

  cd /opt/majaplayer/www
  python3 tools/generate_api.py

On the real server:

  cd /var/www/amigamods
  python3 tools/generate_api.py --mods-dir /var/www/amigamods/mods

The generated API will appear in api/list.txt and api/random.txt.
