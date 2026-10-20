# Music Playlist Manager (C++ arrays)

INF 311 Data Structures: another application of arrays.

- `PlaylistManager.cpp`: the main file. Parallel arrays for titles, artists and lengths, plus add / insert / remove / move / next / previous / find / shuffle, all in C++.
- `playlist_manager.wasm`: PlaylistManager.cpp compiled to WebAssembly.
- `index.html`, `style.css`, `app.js`: the page that draws the interface and calls the C++ functions.

## Rebuild the .wasm

    clang --target=wasm32 -O2 -nostdlib -ffreestanding -fno-builtin -Wl,--no-entry -o playlist_manager.wasm PlaylistManager.cpp

## Run locally

Browsers will not load a .wasm from a file:// address, so use a small local server:

    python3 -m http.server 8000

then open http://localhost:8000.

## Deploy

Upload this folder as a static site (Vercel, Render static site, GitHub Pages). No build step is needed.
