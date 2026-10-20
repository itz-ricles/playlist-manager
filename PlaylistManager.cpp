// Music Playlist Manager - the array logic, written in C++.
// Data Structures (INF 311): another application of arrays.
//
// Compiled to WebAssembly (playlist_manager.wasm) so it runs in the browser.
// The web page (index.html + app.js) only draws the interface; every array
// operation below is C++.
//
// Build:  clang --target=wasm32 -O2 -nostdlib -ffreestanding -fno-builtin \
//               -Wl,--no-entry -o playlist_manager.wasm PlaylistManager.cpp

#define EXPORT(name) __attribute__((export_name(name)))

constexpr int MAX_SONGS = 50;   // fixed capacity of the arrays
constexpr int TEXT_LEN  = 40;

// ---- The arrays ------------------------------------------------------------
static char titles[MAX_SONGS][TEXT_LEN];    // titles[i], artists[i], durations[i]
static char artists[MAX_SONGS][TEXT_LEN];   // all describe the same song
static int  durations[MAX_SONGS];           // length in seconds
static int  songCount = 0;                  // slots currently in use
static int  current   = 0;                  // index of the song now playing

// Small arrays the page uses to hand text to C++
static char titleBuffer[TEXT_LEN];
static char artistBuffer[TEXT_LEN];

static unsigned int seed = 2463534242u;     // for shuffle

// ---- Helpers (no standard library is available in WebAssembly here) --------
static char lower(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static void copyText(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0' && i < TEXT_LEN - 1) { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}

// Does 'text' contain 'part' (ignoring upper/lower case)?
static bool contains(const char* text, const char* part) {
    for (int s = 0; text[s] != '\0'; s++) {
        int k = 0;
        while (part[k] != '\0' && text[s + k] != '\0' &&
               lower(text[s + k]) == lower(part[k])) k++;
        if (part[k] == '\0') return true;
    }
    return false;
}

static void copyRow(int from, int to) {
    copyText(titles[to], titles[from]);
    copyText(artists[to], artists[from]);
    durations[to] = durations[from];
}

static void swapRows(int a, int b) {
    char t1[TEXT_LEN], t2[TEXT_LEN];
    int d = durations[a];
    copyText(t1, titles[a]);  copyText(t2, artists[a]);
    copyRow(b, a);
    copyText(titles[b], t1);  copyText(artists[b], t2);  durations[b] = d;
}

static unsigned int nextRandom() {          // xorshift: tiny pseudo-random numbers
    seed ^= seed << 13;  seed ^= seed >> 17;  seed ^= seed << 5;
    return seed;
}

// INSERT at position pos: shift later songs right to make room, O(n).
// pos == songCount means "add to the end", which shifts nothing: O(1).
// Returns 0 = added, 1 = playlist full, 2 = empty title, 3 = bad length
static int insertAt(int pos, int seconds) {
    if (songCount == MAX_SONGS) return 1;
    if (titleBuffer[0] == '\0') return 2;
    if (seconds < 1 || seconds > 7200) return 3;
    for (int i = songCount; i > pos; i--) copyRow(i - 1, i);
    copyText(titles[pos], titleBuffer);
    copyText(artists[pos], artistBuffer[0] ? artistBuffer : "Unknown");
    durations[pos] = seconds;
    if (songCount > 0 && pos <= current) current++;   // keep the same song playing
    songCount++;
    return 0;
}

extern "C" {

// ---- Operations the page can call ------------------------------------------
EXPORT("add_song")   int add_song(int seconds)  { return insertAt(songCount, seconds); }
EXPORT("play_next")  int play_next(int seconds) { return insertAt(songCount == 0 ? 0 : current + 1, seconds); }

// DELETE: shift every later song one step left, O(n)
EXPORT("remove_song") int remove_song(int i) {
    if (i < 0 || i >= songCount) return 0;
    for (int k = i; k < songCount - 1; k++) copyRow(k + 1, k);
    songCount--;
    if (i < current) current--;
    if (current >= songCount) current = 0;            // wrapped past the end
    return 1;
}

// SWAP neighbours to reorder the playlist: O(1)
EXPORT("move_up") int move_up(int i) {
    if (i <= 0 || i >= songCount) return 0;
    swapRows(i - 1, i);
    if (current == i) current = i - 1; else if (current == i - 1) current = i;
    return 1;
}
EXPORT("move_down") int move_down(int i) {
    if (i < 0 || i >= songCount - 1) return 0;
    swapRows(i, i + 1);
    if (current == i) current = i + 1; else if (current == i + 1) current = i;
    return 1;
}

// NEXT / PREVIOUS: the % operator wraps the index around the array, O(1)
EXPORT("next_song") void next_song() { if (songCount > 0) current = (current + 1) % songCount; }
EXPORT("prev_song") void prev_song() { if (songCount > 0) current = (current - 1 + songCount) % songCount; }
EXPORT("set_current") void set_current(int i) { if (i >= 0 && i < songCount) current = i; }

// LINEAR SEARCH by title or artist: O(n). Returns the index or -1.
EXPORT("find_song") int find_song() {
    if (titleBuffer[0] == '\0') return -1;
    for (int i = 0; i < songCount; i++)
        if (contains(titles[i], titleBuffer) || contains(artists[i], titleBuffer)) return i;
    return -1;
}

// SHUFFLE (Fisher-Yates): the playing song moves to the top, the rest are
// swapped into random positions. O(n)
EXPORT("set_seed") void set_seed(unsigned int s) { seed = s ? s : 1; }
EXPORT("shuffle") void shuffle() {
    if (songCount < 2) return;
    swapRows(0, current);
    current = 0;
    for (int i = songCount - 1; i >= 2; i--) {
        int j = 1 + (int)(nextRandom() % (unsigned int)i);   // j is in 1..i
        swapRows(i, j);
    }
}

// TRAVERSAL: add up every length, O(n)
EXPORT("total_seconds") int total_seconds() {
    int sum = 0;
    for (int i = 0; i < songCount; i++) sum += durations[i];
    return sum;
}

// ---- Read-only access for drawing the page ---------------------------------
EXPORT("song_count")   int song_count()   { return songCount; }
EXPORT("current_index") int current_index() { return current; }
EXPORT("max_songs")    int max_songs()    { return MAX_SONGS; }
EXPORT("text_len")     int text_len()     { return TEXT_LEN; }
EXPORT("get_title")    const char* get_title(int i)  { return titles[i]; }
EXPORT("get_artist")   const char* get_artist(int i) { return artists[i]; }
EXPORT("get_duration") int get_duration(int i)       { return durations[i]; }
EXPORT("title_buffer")  char* title_buffer()  { return titleBuffer; }
EXPORT("artist_buffer") char* artist_buffer() { return artistBuffer; }

}  // extern "C"
