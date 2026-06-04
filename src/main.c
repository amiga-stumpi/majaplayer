#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <string.h>

#include "amitcp13/bsdsocket.h"

#define MAJA_VERSION "MajaPlayer v0.6 by Marcel Jaehne (c)2026"
#define MAJA_API_URL "http://mods.c64.social/api/random.php"
#define MAJA_STATIC_RANDOM_URL "http://mods.c64.social/api/random.txt"
#define MAJA_LIST_URL "http://mods.c64.social/api/list.txt"
#define MAJA_TEMP_FILE "RAM:MajaPlayer.mod"
#define MAJA_SAVE_FILE "MajaPlayer_saved.mod"

#define GUI_MIN_W 260
#define GUI_MIN_H 68
#define API_BUF_SIZE 2048
#define LIST_LINE_SIZE 384
#define HTTP_BUF_SIZE 2048
#define TITLE_SIZE 96
#define URL_SIZE 256
#define STATUS_SIZE 96
#define MEMORY_RESERVE_BYTES 65536UL
#define PTH_ORDERLIST 952
#define PTH_SIZEOF 1084
#define PATTERN_SIZE 1024

#ifndef MAJAPLAYER_DEBUG
#define MAJAPLAYER_DEBUG 0
#endif

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
    ULONG mod_size;
    int have_mod;
};

static struct Amitcp13BsdSockAddrIn g_addr;
static struct Amitcp13BsdFdSet g_rfds;
static struct Amitcp13BsdFdSet g_wfds;
static struct Amitcp13BsdTimeVal g_timeout;
static ULONG g_wait_signals;

extern LONG maja_pt_install(void);
extern void maja_pt_start(APTR module, APTR samples);
extern void maja_pt_stop(void);
static LONG g_one;
static int g_so_error;
static int g_so_error_len;
static char g_http_host[80];
static char g_http_path[256];
static char g_http_dummy_path[16];
static char g_http_req[384];
static char g_http_buf[HTTP_BUF_SIZE];
static char g_api_buf[API_BUF_SIZE];
static char g_list_line[LIST_LINE_SIZE];
static char g_selected_line[LIST_LINE_SIZE];
static char g_file_buf[1024];
static UBYTE g_mod_header[PTH_SIZEOF];
static char g_last_error[STATUS_SIZE];
static APTR g_mod_mem;
static APTR g_sample_mem;
static ULONG g_mod_size;
static ULONG g_sample_size;
static int g_player_active;
static struct FileInfoBlock g_fib;
static ULONG g_rand_state;

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

static int call_getsockopt(struct Library *base, int fd, int level, int optname,
                           void *optval, int *optlen)
{
    register int d0 __asm("d0") = fd;
    register int d1 __asm("d1") = level;
    register int d2 __asm("d2") = optname;
    register void *a0 __asm("a0") = optval;
    register int *a1 __asm("a1") = optlen;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-96:W)"
        : "+r" (d0), "+r" (d1), "+r" (d2), "+r" (a0), "+r" (a1)
        : "r" (a6)
        : "cc", "memory");
    return d0;
}

static int call_ioctl(struct Library *base, int fd, ULONG request, void *argp)
{
    register int d0 __asm("d0") = fd;
    register ULONG d1 __asm("d1") = request;
    register void *a0 __asm("a0") = argp;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-114:W)"
        : "+r" (d0), "+r" (d1), "+r" (a0)
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

static void debug_write(const char *text)
{
#if MAJAPLAYER_DEBUG
    BPTR fh;
    if (!text)
        return;
    fh = Open((STRPTR)"RAM:MajaPlayer_debug.log", MODE_READWRITE);
    if (!fh)
        fh = Open((STRPTR)"RAM:MajaPlayer_debug.log", MODE_NEWFILE);
    if (!fh)
        return;
    Seek(fh, 0, OFFSET_END);
    Write(fh, (APTR)text, str_len(text));
    Write(fh, (APTR)"\n", 1);
    Close(fh);
#else
    (void)text;
#endif
}

static void debug_reset(void)
{
#if MAJAPLAYER_DEBUG
    BPTR fh;
    fh = Open((STRPTR)"RAM:MajaPlayer_debug.log", MODE_NEWFILE);
    if (fh) {
        Write(fh, (APTR)"MajaPlayer debug start\n", 23);
        Close(fh);
    }
#endif
}

static void debug_i(const char *prefix, int value)
{
#if MAJAPLAYER_DEBUG
    char num[16];
    char line[96];
    int pos = 0;
    int i;
    int neg;
    ULONG v;

    if (!prefix)
        prefix = "";
    while (*prefix && pos < (int)sizeof(line) - 1)
        line[pos++] = *prefix++;
    neg = value < 0;
    v = neg ? (ULONG)(-value) : (ULONG)value;
    i = sizeof(num) - 1;
    num[i] = 0;
    do {
        --i;
        num[i] = (char)('0' + (v % 10UL));
        v /= 10UL;
    } while (v && i > 0);
    if (neg && i > 0)
        num[--i] = '-';
    while (num[i] && pos < (int)sizeof(line) - 1)
        line[pos++] = num[i++];
    line[pos] = 0;
    debug_write(line);
#else
    (void)prefix;
    (void)value;
#endif
}

static void set_last_error(const char *status)
{
    str_copy(g_last_error, STATUS_SIZE, status);
    debug_write(status);
}

static void set_status(struct AppState *app, const char *status)
{
    str_copy(app->status, STATUS_SIZE, status);
}

static ULONG parse_ulong_value(const char *s)
{
    ULONG value = 0;

    if (!s)
        return 0;
    while (*s >= '0' && *s <= '9') {
        value = value * 10UL + (ULONG)(*s - '0');
        ++s;
    }
    return value;
}

static void append_number(char *dst, int dst_size, ULONG value)
{
    char tmp[12];
    int i = 0;
    int pos;

    pos = str_len(dst);
    do {
        tmp[i++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    } while (value && i < (int)sizeof(tmp));
    while (i > 0 && pos < dst_size - 1)
        dst[pos++] = tmp[--i];
    dst[pos] = 0;
}

static void build_memory_status(char *dst, int dst_size)
{
    ULONG fast_k = AvailMem(MEMF_FAST) / 1024UL;
    ULONG chip_k = AvailMem(MEMF_CHIP) / 1024UL;

    str_copy(dst, dst_size, "Ready Fast ");
    append_number(dst, dst_size, fast_k);
    str_copy(dst + str_len(dst), dst_size - str_len(dst), "K Chip ");
    append_number(dst, dst_size, chip_k);
    str_copy(dst + str_len(dst), dst_size - str_len(dst), "K");
}

static int module_size_allowed(ULONG size)
{
    ULONG public_avail;

    if (size == 0)
        return 1;
    public_avail = AvailMem(MEMF_PUBLIC);
    if (public_avail <= MEMORY_RESERVE_BYTES)
        return 0;
    /* RAM: download plus split player allocation coexist briefly. */
    if (size > (public_avail - MEMORY_RESERVE_BYTES) / 2UL)
        return 0;
    return 1;
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

static int wait_for_connect(struct Library *base, int fd)
{
    int r;

    AMITCP13_BSD_FD_ZERO(&g_wfds);
    AMITCP13_BSD_FD_SET(fd, &g_wfds);
    g_timeout.tv_sec = 20;
    g_timeout.tv_usec = 0;
    g_wait_signals = 0;
    r = call_waitselect(base, fd + 1, 0, &g_wfds, &g_timeout);
    debug_i("Connect WaitSelect ret=", r);
    if (r <= 0)
        return 0;
    if (!AMITCP13_BSD_FD_ISSET(fd, &g_wfds))
        return 0;
    g_so_error = -1;
    g_so_error_len = sizeof(g_so_error);
    if (call_getsockopt(base, fd, AMITCP13_SOL_SOCKET, AMITCP13_SO_ERROR,
                        &g_so_error, &g_so_error_len) < 0) {
        debug_i("SO_ERROR getsockopt errno=", call_errno(base));
        set_last_error("Connect check failed");
        return 0;
    }
    debug_i("SO_ERROR value=", g_so_error);
    if (g_so_error == 0)
        return 1;
    if (g_so_error == AMITCP13_ECONNREFUSED)
        set_last_error("Connect refused");
    else if (g_so_error == AMITCP13_ETIMEDOUT)
        set_last_error("Connect timeout");
    else
        set_last_error("Connect error");
    return 0;
}

static int open_http_socket(const char *url, char *path, int path_size)
{
    UWORD port;
    struct hostent *he;
    int fd;

    if (!SocketBase) {
        set_last_error("bsdsocket missing");
        return -1;
    }
    debug_write("HTTP parse URL");
    if (!parse_http_url(url, g_http_host, sizeof(g_http_host), path, path_size, &port)) {
        set_last_error("Bad URL");
        return -1;
    }
    debug_write("DNS start");
    he = call_gethostbyname(SocketBase, g_http_host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        debug_i("DNS errno=", call_errno(SocketBase));
        set_last_error("DNS failed");
        return -1;
    }
    debug_write("DNS ok");
    fd = call_socket(SocketBase, AMITCP13_AF_INET, AMITCP13_SOCK_STREAM, AMITCP13_IPPROTO_TCP);
    debug_i("Socket fd=", fd);
    if (fd < 0) {
        debug_i("Socket errno=", call_errno(SocketBase));
        set_last_error("Socket failed");
        return -1;
    }
    g_one = 1;
    debug_i("Ioctl ret=", call_ioctl(SocketBase, fd, AMITCP13_FIONBIO, &g_one));
    memset(&g_addr, 0, sizeof(g_addr));
    g_addr.sin_len = sizeof(g_addr);
    g_addr.sin_family = AMITCP13_AF_INET;
    g_addr.sin_port = port;
    g_addr.sin_addr.s_addr = *(ULONG *)he->h_addr_list[0];
    debug_write("Connect start");
    if (call_connect(SocketBase, fd, (const struct Amitcp13BsdSockAddr *)&g_addr, sizeof(g_addr)) < 0) {
        int err = call_errno(SocketBase);
        debug_i("Connect errno=", err);
        if (err != AMITCP13_EINPROGRESS && err != AMITCP13_EALREADY) {
            call_close_socket(SocketBase, fd);
            set_last_error("Connect failed");
            return -1;
        }
        if (!wait_for_connect(SocketBase, fd)) {
            call_close_socket(SocketBase, fd);
            if (!g_last_error[0])
                set_last_error("Connect timeout");
            return -1;
        }
    }
    debug_write("Connect ok");
    return fd;
}

static int send_http_get(int fd, const char *host_path_url, const char *path)
{
    UWORD port;
    int pos = 0;

    if (!parse_http_url(host_path_url, g_http_host, sizeof(g_http_host), g_http_dummy_path, sizeof(g_http_dummy_path), &port))
        return 0;
    pos = 0;
#define ADDTXT(t) do { const char *q = (t); while (*q && pos < (int)sizeof(g_http_req) - 1) g_http_req[pos++] = *q++; } while (0)
    ADDTXT("GET ");
    ADDTXT(path);
    ADDTXT(" HTTP/1.0\r\nHost: ");
    ADDTXT(g_http_host);
    ADDTXT("\r\nConnection: close\r\n\r\n");
#undef ADDTXT
    g_http_req[pos] = 0;
    debug_i("HTTP request bytes=", pos);
    return send_all(SocketBase, fd, g_http_req, pos);
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
    debug_i("Recv WaitSelect ret=", r);
    if (r <= 0)
        return r;
    if (!AMITCP13_BSD_FD_ISSET(fd, &g_rfds))
        return 0;
    r = call_recv(SocketBase, fd, buf, len, 0);
    debug_i("Recv ret=", r);
    if (r < 0) {
        err = call_errno(SocketBase);
        debug_i("Recv errno=", err);
        if (err == AMITCP13_EWOULDBLOCK || err == AMITCP13_EAGAIN)
            return 0;
    }
    return r;
}

static int http_get_small(const char *url, char *out, int out_size)
{
    int fd;
    int used = 0;
    int header_done = 0;
    int r;
    int off;
    int copy;

    fd = open_http_socket(url, g_http_path, sizeof(g_http_path));
    if (fd < 0)
        return 0;
    if (!send_http_get(fd, url, g_http_path)) {
        set_last_error("HTTP send failed");
        call_close_socket(SocketBase, fd);
        return 0;
    }
    while (used < out_size - 1) {
        r = recv_wait(fd, g_http_buf, sizeof(g_http_buf));
        if (r <= 0)
            break;
        off = 0;
        if (!header_done) {
            off = find_header_end(g_http_buf, r);
            if (off < 0)
                continue;
            header_done = 1;
        }
        copy = r - off;
        if (copy > out_size - 1 - used)
            copy = out_size - 1 - used;
        if (copy > 0) {
            memcpy(out + used, g_http_buf + off, copy);
            used += copy;
        }
    }
    out[used] = 0;
    debug_i("HTTP body bytes=", used);
    call_close_socket(SocketBase, fd);
    if (used <= 0) {
        set_last_error("HTTP no data");
        return 0;
    }
    return 1;
}

static int http_download_file(const char *url, const char *filename)
{
    BPTR fh;
    int fd;
    int header_done = 0;
    int r;
    int off;

    fd = open_http_socket(url, g_http_path, sizeof(g_http_path));
    if (fd < 0)
        return 0;
    if (!send_http_get(fd, url, g_http_path)) {
        call_close_socket(SocketBase, fd);
        return 0;
    }
    fh = Open((STRPTR)filename, MODE_NEWFILE);
    if (!fh) {
        call_close_socket(SocketBase, fd);
        return 0;
    }
    while (1) {
        r = recv_wait(fd, g_http_buf, sizeof(g_http_buf));
        if (r == 0)
            break;
        if (r < 0) {
            Close(fh);
            call_close_socket(SocketBase, fd);
            return 0;
        }
        off = 0;
        if (!header_done) {
            off = find_header_end(g_http_buf, r);
            if (off < 0)
                continue;
            header_done = 1;
        }
        if (r - off > 0) {
            if (Write(fh, g_http_buf + off, r - off) != r - off) {
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

static ULONG next_random(void)
{
    struct DateStamp ds;

    if (!g_rand_state) {
        DateStamp(&ds);
        g_rand_state = ((ULONG)ds.ds_Days << 16) ^
                       ((ULONG)ds.ds_Minute << 4) ^
                       (ULONG)ds.ds_Tick ^
                       (ULONG)(APTR)&ds;
        if (!g_rand_state)
            g_rand_state = 0x13572468UL;
    }
    g_rand_state = g_rand_state * 1103515245UL + 12345UL;
    return g_rand_state;
}

static int parse_list_line(char *line, char *title, int title_size, char *url, int url_size, ULONG *size_out)
{
    char *p;
    char *size_src;
    char *path;
    char *title_src;
    int pos;

    if (!line || line[0] == '#' || line[0] == 0)
        return 0;
    p = line;
    while (*p && *p != '|')
        ++p;
    if (*p != '|')
        return 0;
    ++p;
    size_src = p;
    while (*p && *p != '|')
        ++p;
    if (*p != '|')
        return 0;
    *p++ = 0;
    if (size_out)
        *size_out = parse_ulong_value(size_src);
    path = p;
    while (*p && *p != '|')
        ++p;
    if (*p != '|')
        return 0;
    *p++ = 0;
    title_src = p;
    copy_line(title, title_size, title_src);
    if (title[0] == 0)
        str_copy(title, title_size, "Unknown MOD");

    pos = 0;
#define ADDURL(t) do { const char *q = (t); while (*q && pos < url_size - 1) url[pos++] = *q++; } while (0)
    if (streq_prefix(path, "http://"))
        ADDURL(path);
    else {
        ADDURL("http://mods.c64.social");
        ADDURL(path);
    }
#undef ADDURL
    url[pos] = 0;
    return url[0] != 0;
}

static int consider_list_line(char *line, ULONG *count)
{
    char tmp[LIST_LINE_SIZE];
    char title[TITLE_SIZE];
    char url[URL_SIZE];

    str_copy(tmp, sizeof(tmp), line);
    if (!parse_list_line(tmp, title, sizeof(title), url, sizeof(url), 0))
        return 0;
    ++(*count);
    if ((next_random() % *count) == 0)
        str_copy(g_selected_line, sizeof(g_selected_line), line);
    return 1;
}

static int fetch_random_mod_from_list(struct AppState *app)
{
    int fd;
    int header_done = 0;
    int r;
    int off;
    int i;
    int line_pos = 0;
    ULONG count = 0;

    g_selected_line[0] = 0;
    debug_write("LIST fetch start");
    fd = open_http_socket(MAJA_LIST_URL, g_http_path, sizeof(g_http_path));
    if (fd < 0)
        return 0;
    if (!send_http_get(fd, MAJA_LIST_URL, g_http_path)) {
        set_last_error("LIST send failed");
        call_close_socket(SocketBase, fd);
        return 0;
    }
    while (1) {
        r = recv_wait(fd, g_http_buf, sizeof(g_http_buf));
        if (r == 0)
            break;
        if (r < 0) {
            call_close_socket(SocketBase, fd);
            set_last_error("LIST recv failed");
            return 0;
        }
        off = 0;
        if (!header_done) {
            off = find_header_end(g_http_buf, r);
            if (off < 0)
                continue;
            header_done = 1;
        }
        for (i = off; i < r; ++i) {
            char c = g_http_buf[i];
            if (c == '\r')
                continue;
            if (c == '\n') {
                g_list_line[line_pos] = 0;
                consider_list_line(g_list_line, &count);
                line_pos = 0;
            } else if (line_pos < LIST_LINE_SIZE - 1) {
                g_list_line[line_pos++] = c;
            }
        }
    }
    if (line_pos > 0) {
        g_list_line[line_pos] = 0;
        consider_list_line(g_list_line, &count);
    }
    call_close_socket(SocketBase, fd);
    debug_i("LIST entries=", (int)count);
    if (!header_done || count == 0 || !g_selected_line[0]) {
        set_last_error("LIST empty");
        return 0;
    }
    if (!parse_list_line(g_selected_line, app->title, TITLE_SIZE, app->url, URL_SIZE, &app->mod_size)) {
        set_last_error("LIST parse failed");
        return 0;
    }
    debug_write("LIST selected");
    return 1;
}

static int parse_random_api_response(struct AppState *app)
{
    const char *title;
    const char *size;
    const char *url;

    if (!streq_prefix(g_api_buf, "OK")) {
        set_status(app, "API returned error");
        return 0;
    }
    title = field_value(g_api_buf, "TITLE");
    size = field_value(g_api_buf, "SIZE");
    app->mod_size = parse_ulong_value(size);
    url = field_value(g_api_buf, "URL");
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

static int fetch_random_mod_info(struct AppState *app)
{
    debug_write("random.php fetch start");
    if (http_get_small(MAJA_API_URL, g_api_buf, sizeof(g_api_buf))) {
        if (parse_random_api_response(app))
            return 1;
    }

    debug_write("random.php failed, fallback list.txt");
    if (fetch_random_mod_from_list(app))
        return 1;

    debug_write("LIST failed, fallback random.txt");
    if (!http_get_small(MAJA_STATIC_RANDOM_URL, g_api_buf, sizeof(g_api_buf))) {
        set_status(app, g_last_error[0] ? g_last_error : "API failed");
        return 0;
    }
    return parse_random_api_response(app);
}

static int copy_file(const char *src, const char *dst)
{
    BPTR in;
    BPTR out;
    LONG r;

    in = Open((STRPTR)src, MODE_OLDFILE);
    if (!in)
        return 0;
    out = Open((STRPTR)dst, MODE_NEWFILE);
    if (!out) {
        Close(in);
        return 0;
    }
    while ((r = Read(in, g_file_buf, sizeof(g_file_buf))) > 0) {
        if (Write(out, g_file_buf, r) != r) {
            Close(out);
            Close(in);
            return 0;
        }
    }
    Close(out);
    Close(in);
    return r == 0;
}

static void free_loaded_mod(void)
{
    if (g_mod_mem) {
        FreeMem(g_mod_mem, g_mod_size);
        g_mod_mem = 0;
        g_mod_size = 0;
    }
    if (g_sample_mem) {
        FreeMem(g_sample_mem, g_sample_size);
        g_sample_mem = 0;
        g_sample_size = 0;
    }
}

static void stop_player(void)
{
    if (g_player_active) {
        maja_pt_stop();
        g_player_active = 0;
    }
    free_loaded_mod();
}

static LONG file_size(const char *filename)
{
    BPTR lock;
    LONG size = -1;

    lock = Lock((STRPTR)filename, ACCESS_READ);
    if (!lock)
        return -1;
    if (Examine(lock, &g_fib))
        size = g_fib.fib_Size;
    UnLock(lock);
    return size;
}

static int load_mod_for_player(const char *filename)
{
    BPTR fh;
    LONG size;
    LONG got;
    UBYTE *h;
    int i;
    UBYTE max_pattern = 0;
    ULONG header_pattern_size;
    ULONG sample_size;

    stop_player();
    size = file_size(filename);
    if (size <= PTH_SIZEOF)
        return 0;

    fh = Open((STRPTR)filename, MODE_OLDFILE);
    if (!fh)
        return 0;
    got = Read(fh, g_mod_header, PTH_SIZEOF);
    if (got != PTH_SIZEOF) {
        Close(fh);
        return 0;
    }

    for (i = 0; i < 128; ++i) {
        if (g_mod_header[PTH_ORDERLIST + i] > max_pattern)
            max_pattern = g_mod_header[PTH_ORDERLIST + i];
    }
    header_pattern_size = (ULONG)PTH_SIZEOF + ((ULONG)max_pattern + 1UL) * (ULONG)PATTERN_SIZE;
    if (header_pattern_size >= (ULONG)size) {
        Close(fh);
        return 0;
    }
    sample_size = (ULONG)size - header_pattern_size;

    if (AvailMem(MEMF_PUBLIC) <= header_pattern_size + MEMORY_RESERVE_BYTES) {
        Close(fh);
        return 0;
    }
    if (AvailMem(MEMF_CHIP) <= sample_size + MEMORY_RESERVE_BYTES / 2UL) {
        Close(fh);
        return 0;
    }

    g_mod_mem = AllocMem(header_pattern_size, MEMF_FAST);
    if (!g_mod_mem)
        g_mod_mem = AllocMem(header_pattern_size, MEMF_PUBLIC);
    if (!g_mod_mem) {
        Close(fh);
        return 0;
    }
    g_mod_size = header_pattern_size;
    g_sample_mem = AllocMem(sample_size, MEMF_CHIP);
    if (!g_sample_mem) {
        Close(fh);
        free_loaded_mod();
        return 0;
    }
    g_sample_size = sample_size;

    h = (UBYTE *)g_mod_mem;
    for (i = 0; i < PTH_SIZEOF; ++i)
        h[i] = g_mod_header[i];
    got = Read(fh, h + PTH_SIZEOF, header_pattern_size - PTH_SIZEOF);
    if (got != (LONG)(header_pattern_size - PTH_SIZEOF)) {
        Close(fh);
        free_loaded_mod();
        return 0;
    }
    got = Read(fh, g_sample_mem, sample_size);
    Close(fh);
    if (got != (LONG)sample_size) {
        free_loaded_mod();
        return 0;
    }
    return 1;
}

static int launch_player(void)
{
    if (!g_mod_mem)
        return 0;
    if (!maja_pt_install())
        return 0;
    maja_pt_start(g_mod_mem, g_sample_mem);
    g_player_active = 1;
    return 1;
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
    g_last_error[0] = 0;
    debug_reset();
    set_status(app, "Fetching random MOD...");
    redraw(app);
    if (!fetch_random_mod_info(app)) {
        redraw(app);
        return;
    }
    if (!module_size_allowed(app->mod_size)) {
        set_status(app, "MOD too large");
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
    set_status(app, "Loading MOD...");
    redraw(app);
    if (!load_mod_for_player(MAJA_TEMP_FILE)) {
        set_status(app, "MOD load failed");
        redraw(app);
        return;
    }
    if (!launch_player()) {
        set_status(app, "Player init failed");
        redraw(app);
        return;
    }
    set_status(app, "Playing");
    redraw(app);
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
    build_memory_status(app.status, STATUS_SIZE);

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

    stop_player();
    CloseWindow(app.win);
    close_libraries();
    return 0;
}
