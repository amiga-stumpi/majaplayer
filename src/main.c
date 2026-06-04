#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <string.h>

#include "amitcp13/bsdsocket.h"

#define MAJA_VERSION "MajaPlayer v0.2 by Marcel Jaehne (c)2026"
#define MAJA_API_URL "http://mods.c64.social/api/random.txt"
#define MAJA_TEMP_FILE "RAM:MajaPlayer.mod"
#define MAJA_SAVE_FILE "MajaPlayer_saved.mod"

#define GUI_MIN_W 260
#define GUI_MIN_H 68
#define API_BUF_SIZE 2048
#define HTTP_BUF_SIZE 2048
#define TITLE_SIZE 96
#define URL_SIZE 256
#define STATUS_SIZE 96

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
static struct Library *SocketBase;

struct ButtonRect {
    WORD x;
    WORD y;
    WORD w;
    WORD h;
    const char *label;
};

struct AppState {
    struct Window *win;
    struct ButtonRect play;
    struct ButtonRect stop;
    struct ButtonRect skip;
    struct ButtonRect save;
    char title[TITLE_SIZE];
    char url[URL_SIZE];
    char status[STATUS_SIZE];
    int have_mod;
};

static struct Amitcp13BsdSockAddrIn g_addr;
static struct Amitcp13BsdFdSet g_rfds;
static struct Amitcp13BsdFdSet g_wfds;
static struct Amitcp13BsdTimeVal g_timeout;
static ULONG g_wait_signals;

static int call_socket(struct Library *base, int domain, int type, int protocol)
{
    register int d0 __asm("d0") = domain;
    register int d1 __asm("d1") = type;
    register int d2 __asm("d2") = protocol;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-30:W)"
        : "+r" (d0), "+r" (d1), "+r" (d2)
        : "r" (a6)
        : "a0", "a1", "cc", "memory");
    return d0;
}

static int call_connect(struct Library *base, int fd,
                        const struct Amitcp13BsdSockAddr *addr, int addrlen)
{
    register int d0 __asm("d0") = fd;
    register const struct Amitcp13BsdSockAddr *a0 __asm("a0") = addr;
    register int d1 __asm("d1") = addrlen;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-54:W)"
        : "+r" (d0), "+r" (a0), "+r" (d1)
        : "r" (a6)
        : "a1", "cc", "memory");
    return d0;
}

static int call_send(struct Library *base, int fd, const void *buf, int len, int flags)
{
    register int d0 __asm("d0") = fd;
    register const void *a0 __asm("a0") = buf;
    register int d1 __asm("d1") = len;
    register int d2 __asm("d2") = flags;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-66:W)"
        : "+r" (d0), "+r" (a0), "+r" (d1), "+r" (d2)
        : "r" (a6)
        : "a1", "cc", "memory");
    return d0;
}

static int call_recv(struct Library *base, int fd, void *buf, int len, int flags)
{
    register int d0 __asm("d0") = fd;
    register void *a0 __asm("a0") = buf;
    register int d1 __asm("d1") = len;
    register int d2 __asm("d2") = flags;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-78:W)"
        : "+r" (d0), "+r" (a0), "+r" (d1), "+r" (d2)
        : "r" (a6)
        : "a1", "cc", "memory");
    return d0;
}

static int call_close_socket(struct Library *base, int fd)
{
    register int d0 __asm("d0") = fd;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-120:W)"
        : "+r" (d0)
        : "r" (a6)
        : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static int call_waitselect(struct Library *base, int nfds,
                           struct Amitcp13BsdFdSet *readfds,
                           struct Amitcp13BsdFdSet *writefds,
                           const struct Amitcp13BsdTimeVal *timeout)
{
    register int d0 __asm("d0") = nfds;
    register ULONG *d1 __asm("d1") = &g_wait_signals;
    register struct Amitcp13BsdFdSet *a0 __asm("a0") = readfds;
    register struct Amitcp13BsdFdSet *a1 __asm("a1") = writefds;
    register struct Amitcp13BsdFdSet *a2 __asm("a2") = 0;
    register const struct Amitcp13BsdTimeVal *a3 __asm("a3") = timeout;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-126:W)"
        : "+r" (d0), "+r" (d1), "+r" (a0), "+r" (a1), "+r" (a2), "+r" (a3)
        : "r" (a6)
        : "cc", "memory");
    return d0;
}

static int call_errno(struct Library *base)
{
    register int d0 __asm("d0");
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-162:W)"
        : "=r" (d0)
        : "r" (a6)
        : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static struct hostent *call_gethostbyname(struct Library *base, const char *name)
{
    register const char *a0 __asm("a0") = name;
    register struct Library *a6 __asm("a6") = base;
    register struct hostent *d0 __asm("d0");

    __asm volatile ("jsr a6@(-210:W)"
        : "=r" (d0), "+r" (a0)
        : "r" (a6)
        : "d1", "a1", "cc", "memory");
    return d0;
}

static int streq_prefix(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

static int str_len(const char *s)
{
    int n = 0;
    while (s && s[n])
        ++n;
    return n;
}

static void str_copy(char *dst, int dst_size, const char *src)
{
    int i = 0;
    if (!dst || dst_size <= 0)
        return;
    if (src) {
        while (src[i] && i < dst_size - 1) {
            dst[i] = src[i];
            ++i;
        }
    }
    dst[i] = 0;
}

static void set_status(struct AppState *app, const char *status)
{
    str_copy(app->status, STATUS_SIZE, status);
}

static UWORD htons16(UWORD v)
{
    return (UWORD)(((v & 0x00ff) << 8) | ((v & 0xff00) >> 8));
}

static int parse_http_url(const char *url, char *host, int host_size,
                          char *path, int path_size, UWORD *port)
{
    const char *p;
    int i;

    if (!streq_prefix(url, "http://"))
        return 0;
    p = url + 7;
    i = 0;
    while (*p && *p != '/' && *p != ':' && i < host_size - 1)
        host[i++] = *p++;
    host[i] = 0;
    if (host[0] == 0)
        return 0;
    *port = 80;
    if (*p == ':') {
        ULONG value = 0;
        ++p;
        if (*p < '0' || *p > '9')
            return 0;
        while (*p >= '0' && *p <= '9') {
            value = value * 10 + (ULONG)(*p - '0');
            if (value > 65535UL)
                return 0;
            ++p;
        }
        if (value == 0)
            return 0;
        *port = (UWORD)value;
    }
    if (*p == 0)
        str_copy(path, path_size, "/");
    else
        str_copy(path, path_size, p);
    return 1;
}

static int send_all(struct Library *base, int fd, const char *buf, int len)
{
    int pos = 0;
    int sent;
    int err;

    while (pos < len) {
        sent = call_send(base, fd, buf + pos, len - pos, 0);
        if (sent > 0) {
            pos += sent;
            continue;
        }
        err = call_errno(base);
        if (err == AMITCP13_EWOULDBLOCK || err == AMITCP13_EAGAIN) {
            AMITCP13_BSD_FD_ZERO(&g_wfds);
            AMITCP13_BSD_FD_SET(fd, &g_wfds);
            g_timeout.tv_sec = 2;
            g_timeout.tv_usec = 0;
            g_wait_signals = 0;
            if (call_waitselect(base, fd + 1, 0, &g_wfds, &g_timeout) > 0)
                continue;
        }
        return 0;
    }
    return 1;
}

static int open_http_socket(const char *url, char *path, int path_size)
{
    char host[80];
    UWORD port;
    struct hostent *he;
    int fd;

    if (!SocketBase)
        return -1;
    if (!parse_http_url(url, host, sizeof(host), path, path_size, &port))
        return -1;
    he = call_gethostbyname(SocketBase, host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0])
        return -1;
    fd = call_socket(SocketBase, AMITCP13_AF_INET, AMITCP13_SOCK_STREAM, AMITCP13_IPPROTO_TCP);
    if (fd < 0)
        return -1;
    memset(&g_addr, 0, sizeof(g_addr));
    g_addr.sin_len = sizeof(g_addr);
    g_addr.sin_family = AMITCP13_AF_INET;
    g_addr.sin_port = htons16(port);
    g_addr.sin_addr.s_addr = *(ULONG *)he->h_addr_list[0];
    if (call_connect(SocketBase, fd, (const struct Amitcp13BsdSockAddr *)&g_addr, sizeof(g_addr)) < 0) {
        call_close_socket(SocketBase, fd);
        return -1;
    }
    return fd;
}

static int send_http_get(int fd, const char *host_path_url, const char *path)
{
    char host[80];
    char dummy_path[16];
    UWORD port;
    char req[384];
    int pos = 0;

    if (!parse_http_url(host_path_url, host, sizeof(host), dummy_path, sizeof(dummy_path), &port))
        return 0;
    pos = 0;
#define ADDTXT(t) do { const char *q = (t); while (*q && pos < (int)sizeof(req) - 1) req[pos++] = *q++; } while (0)
    ADDTXT("GET ");
    ADDTXT(path);
    ADDTXT(" HTTP/1.0\r\nHost: ");
    ADDTXT(host);
    ADDTXT("\r\nConnection: close\r\n\r\n");
#undef ADDTXT
    req[pos] = 0;
    return send_all(SocketBase, fd, req, pos);
}

static int find_header_end(char *buf, int len)
{
    int i;
    for (i = 0; i + 3 < len; ++i) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
            return i + 4;
    }
    for (i = 0; i + 1 < len; ++i) {
        if (buf[i] == '\n' && buf[i + 1] == '\n')
            return i + 2;
    }
    return -1;
}

static int recv_wait(int fd, char *buf, int len)
{
    int r;
    int err;

    AMITCP13_BSD_FD_ZERO(&g_rfds);
    AMITCP13_BSD_FD_SET(fd, &g_rfds);
    g_timeout.tv_sec = 20;
    g_timeout.tv_usec = 0;
    g_wait_signals = 0;
    r = call_waitselect(SocketBase, fd + 1, &g_rfds, 0, &g_timeout);
    if (r <= 0)
        return r;
    if (!AMITCP13_BSD_FD_ISSET(fd, &g_rfds))
        return 0;
    r = call_recv(SocketBase, fd, buf, len, 0);
    if (r < 0) {
        err = call_errno(SocketBase);
        if (err == AMITCP13_EWOULDBLOCK || err == AMITCP13_EAGAIN)
            return 0;
    }
    return r;
}

static int http_get_small(const char *url, char *out, int out_size)
{
    char path[256];
    char buf[HTTP_BUF_SIZE];
    int fd;
    int used = 0;
    int header_done = 0;
    int r;
    int off;
    int copy;

    fd = open_http_socket(url, path, sizeof(path));
    if (fd < 0)
        return 0;
    if (!send_http_get(fd, url, path)) {
        call_close_socket(SocketBase, fd);
        return 0;
    }
    while (used < out_size - 1) {
        r = recv_wait(fd, buf, sizeof(buf));
        if (r <= 0)
            break;
        off = 0;
        if (!header_done) {
            off = find_header_end(buf, r);
            if (off < 0)
                continue;
            header_done = 1;
        }
        copy = r - off;
        if (copy > out_size - 1 - used)
            copy = out_size - 1 - used;
        if (copy > 0) {
            memcpy(out + used, buf + off, copy);
            used += copy;
        }
    }
    out[used] = 0;
    call_close_socket(SocketBase, fd);
    return used > 0;
}

static int http_download_file(const char *url, const char *filename)
{
    char path[256];
    char buf[HTTP_BUF_SIZE];
    BPTR fh;
    int fd;
    int header_done = 0;
    int r;
    int off;

    fd = open_http_socket(url, path, sizeof(path));
    if (fd < 0)
        return 0;
    if (!send_http_get(fd, url, path)) {
        call_close_socket(SocketBase, fd);
        return 0;
    }
    fh = Open((STRPTR)filename, MODE_NEWFILE);
    if (!fh) {
        call_close_socket(SocketBase, fd);
        return 0;
    }
    while (1) {
        r = recv_wait(fd, buf, sizeof(buf));
        if (r == 0)
            break;
        if (r < 0) {
            Close(fh);
            call_close_socket(SocketBase, fd);
            return 0;
        }
        off = 0;
        if (!header_done) {
            off = find_header_end(buf, r);
            if (off < 0)
                continue;
            header_done = 1;
        }
        if (r - off > 0) {
            if (Write(fh, buf + off, r - off) != r - off) {
                Close(fh);
                call_close_socket(SocketBase, fd);
                return 0;
            }
        }
    }
    Close(fh);
    call_close_socket(SocketBase, fd);
    return header_done;
}

static const char *field_value(char *body, const char *field)
{
    char *p = body;
    int flen = str_len(field);

    while (*p) {
        if (streq_prefix(p, field) && p[flen] == '=')
            return p + flen + 1;
        while (*p && *p != '\n')
            ++p;
        if (*p == '\n')
            ++p;
    }
    return 0;
}

static void copy_line(char *dst, int dst_size, const char *src)
{
    int i = 0;
    while (src && src[i] && src[i] != '\r' && src[i] != '\n' && i < dst_size - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static int fetch_random_mod_info(struct AppState *app)
{
    char api[API_BUF_SIZE];
    const char *title;
    const char *url;

    if (!http_get_small(MAJA_API_URL, api, sizeof(api))) {
        set_status(app, "API failed");
        return 0;
    }
    if (!streq_prefix(api, "OK")) {
        set_status(app, "API returned error");
        return 0;
    }
    title = field_value(api, "TITLE");
    url = field_value(api, "URL");
    if (!url) {
        set_status(app, "API has no URL");
        return 0;
    }
    copy_line(app->url, URL_SIZE, url);
    if (title)
        copy_line(app->title, TITLE_SIZE, title);
    else
        str_copy(app->title, TITLE_SIZE, "Unknown MOD");
    return 1;
}

static int copy_file(const char *src, const char *dst)
{
    BPTR in;
    BPTR out;
    char buf[1024];
    LONG r;

    in = Open((STRPTR)src, MODE_OLDFILE);
    if (!in)
        return 0;
    out = Open((STRPTR)dst, MODE_NEWFILE);
    if (!out) {
        Close(in);
        return 0;
    }
    while ((r = Read(in, buf, sizeof(buf))) > 0) {
        if (Write(out, buf, r) != r) {
            Close(out);
            Close(in);
            return 0;
        }
    }
    Close(out);
    Close(in);
    return r == 0;
}

static void launch_player(void)
{
    Execute((STRPTR)"Run >NIL: MiniMod RAM:MajaPlayer.mod", 0, 0);
}

static void stop_player(void)
{
    Execute((STRPTR)"Break NAME MiniMod", 0, 0);
}

static void layout(struct AppState *app)
{
    WORD w = app->win->Width;
    WORD y = 32;
    WORD bw = 74;
    WORD gap = 6;
    WORD x;

    if (w < GUI_MIN_W)
        w = GUI_MIN_W;
    x = 8;
    app->play.x = x;
    app->play.y = y;
    app->play.w = bw;
    app->play.h = 16;
    app->play.label = "Play";
    x += bw + gap;
    app->stop.x = x;
    app->stop.y = y;
    app->stop.w = bw;
    app->stop.h = 16;
    app->stop.label = "Stop";
    x += bw + gap;
    app->skip.x = x;
    app->skip.y = y;
    app->skip.w = bw;
    app->skip.h = 16;
    app->skip.label = "Skip";
    x += bw + gap;
    app->save.x = x;
    app->save.y = y;
    app->save.w = w - x - 8;
    if (app->save.w < bw)
        app->save.w = bw;
    app->save.h = 16;
    app->save.label = "Download";
}

static void draw_button(struct Window *win, const struct ButtonRect *b)
{
    struct RastPort *rp = win->RPort;

    SetAPen(rp, 1);
    Move(rp, b->x, b->y);
    Draw(rp, b->x + b->w, b->y);
    Draw(rp, b->x + b->w, b->y + b->h);
    Draw(rp, b->x, b->y + b->h);
    Draw(rp, b->x, b->y);
    Move(rp, b->x + 8, b->y + 12);
    Text(rp, (STRPTR)b->label, str_len(b->label));
}

static void redraw(struct AppState *app)
{
    struct RastPort *rp = app->win->RPort;
    WORD w = app->win->Width;
    WORD h = app->win->Height;

    if (w < GUI_MIN_W)
        w = GUI_MIN_W;
    if (h < GUI_MIN_H)
        h = GUI_MIN_H;
    SetAPen(rp, 0);
    RectFill(rp, 2, 10, w - 3, h - 3);
    SetAPen(rp, 1);
    SetBPen(rp, 0);
    SetDrMd(rp, JAM1);
    Move(rp, 8, 20);
    Text(rp, (STRPTR)"Title:", 6);
    Move(rp, 58, 20);
    Text(rp, (STRPTR)app->title, str_len(app->title));
    draw_button(app->win, &app->play);
    draw_button(app->win, &app->stop);
    draw_button(app->win, &app->skip);
    draw_button(app->win, &app->save);
    Move(rp, 8, h - 8);
    Text(rp, (STRPTR)app->status, str_len(app->status));
}

static int hit(const struct ButtonRect *b, WORD x, WORD y)
{
    return x >= b->x && x <= b->x + b->w && y >= b->y && y <= b->y + b->h;
}

static void do_play(struct AppState *app)
{
    app->have_mod = 0;
    set_status(app, "Fetching random MOD...");
    redraw(app);
    if (!fetch_random_mod_info(app)) {
        redraw(app);
        return;
    }
    set_status(app, "Downloading MOD...");
    redraw(app);
    if (!http_download_file(app->url, MAJA_TEMP_FILE)) {
        set_status(app, "Download failed");
        redraw(app);
        return;
    }
    app->have_mod = 1;
    set_status(app, "Playing");
    redraw(app);
    launch_player();
}

static void do_stop(struct AppState *app)
{
    stop_player();
    set_status(app, "Stopped");
    redraw(app);
}

static void do_skip(struct AppState *app)
{
    stop_player();
    set_status(app, "Skipping...");
    redraw(app);
    do_play(app);
}

static void do_save(struct AppState *app)
{
    if (!app->have_mod) {
        set_status(app, "No MOD loaded");
        redraw(app);
        return;
    }
    if (copy_file(MAJA_TEMP_FILE, MAJA_SAVE_FILE))
        set_status(app, "Saved MajaPlayer_saved.mod");
    else
        set_status(app, "Save failed");
    redraw(app);
}

static int init_libraries(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 0);
    if (!IntuitionBase || !GfxBase || !SocketBase)
        return 0;
    return 1;
}

static void close_libraries(void)
{
    if (SocketBase)
        CloseLibrary(SocketBase);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
}

static int open_app_window(struct AppState *app)
{
    struct NewWindow nw;

    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = 24;
    nw.TopEdge = 24;
    nw.Width = 320;
    nw.Height = 74;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
               WFLG_SIZEGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE;
    nw.FirstGadget = 0;
    nw.CheckMark = 0;
    nw.Title = (UBYTE *)MAJA_VERSION;
    nw.Screen = 0;
    nw.BitMap = 0;
    nw.MinWidth = GUI_MIN_W;
    nw.MinHeight = GUI_MIN_H;
    nw.MaxWidth = 640;
    nw.MaxHeight = 256;
    nw.Type = WBENCHSCREEN;

    app->win = OpenWindow(&nw);
    return app->win != 0;
}

int main(void)
{
    struct AppState app;
    ULONG sigmask;
    int done = 0;

    memset(&app, 0, sizeof(app));
    str_copy(app.title, TITLE_SIZE, "No module loaded");
    str_copy(app.status, STATUS_SIZE, "Ready");

    if (!init_libraries()) {
        close_libraries();
        return 20;
    }
    if (!open_app_window(&app)) {
        close_libraries();
        return 20;
    }
    layout(&app);
    redraw(&app);
    sigmask = 1UL << app.win->UserPort->mp_SigBit;

    while (!done) {
        struct IntuiMessage *msg;
        Wait(sigmask);
        while ((msg = (struct IntuiMessage *)GetMsg(app.win->UserPort)) != 0) {
            ULONG cls = msg->Class;
            WORD mx = msg->MouseX;
            WORD my = msg->MouseY;
            UWORD code = msg->Code;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                done = 1;
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(app.win);
                layout(&app);
                redraw(&app);
                EndRefresh(app.win, TRUE);
            } else if (cls == IDCMP_NEWSIZE) {
                layout(&app);
                redraw(&app);
            } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                if (hit(&app.play, mx, my))
                    do_play(&app);
                else if (hit(&app.stop, mx, my))
                    do_stop(&app);
                else if (hit(&app.skip, mx, my))
                    do_skip(&app);
                else if (hit(&app.save, mx, my))
                    do_save(&app);
            }
        }
    }

    CloseWindow(app.win);
    close_libraries();
    return 0;
}
