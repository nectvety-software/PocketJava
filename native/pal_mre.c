#include "pal_mre.h"

#include "ls30/api.h"
#include "ls30/events.h"
#include "../build/nanojava-reference/src/vm/vm.h"
#include "../build/nanojava-reference/src/util/zip.h"
#include "../build/nanojava-reference/src/midp/midp.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MRE_EVENT_QUEUE 32
#define JAVA_STACK_SLOTS 4096
#define JAVA_MAX_FRAMES 256

static volatile int64_t g_epoch_ms;
static volatile int g_running;
static volatile int g_timer_id = -1;
static volatile int g_queue_head;
static volatile int g_queue_tail;
static NjEvent g_queue[MRE_EVENT_QUEUE];
static int g_layer = -1;
static int g_screen_w;
static int g_screen_h;
static uint16_t *g_frame;
static Zip *g_classlib;
static Zip *g_app;

static int64_t now_ms(void)
{
    return g_epoch_ms;
}

int64_t pal_time_ms(void) { return now_ms(); }
int64_t pal_epoch_ms(void) { return now_ms(); }

void pal_console_write(const char *s, int len)
{
    char text[160];
    int n;
    if (!s || len <= 0) return;
    n = len < (int)sizeof(text) - 1 ? len : (int)sizeof(text) - 1;
    memcpy(text, s, (size_t)n);
    text[n] = 0;
    ls30_log_info(text);
}

static int map_key(int key)
{
    switch (key) {
    case LS30_KEY_UP: return -1;
    case LS30_KEY_DOWN: return -2;
    case LS30_KEY_LEFT: return -3;
    case LS30_KEY_RIGHT: return -4;
    case LS30_KEY_OK: return -5;
    case LS30_KEY_LSK: return -6;
    case LS30_KEY_RSK: return -7;
    case LS30_KEY_CLEAR: return -8;
    case LS30_KEY_BACK: return -9;
    default: return key;
    }
}

static void queue_event(int type, int a, int b)
{
    int tail = g_queue_tail;
    int next = (tail + 1) % MRE_EVENT_QUEUE;
    if (next == g_queue_head) return;
    g_queue[tail].type = type;
    g_queue[tail].a = a;
    g_queue[tail].b = b;
    g_queue_tail = next;
}

static void on_key(int event, int keycode)
{
    if (event == LS30_KEY_EVENT_DOWN || event == LS30_KEY_EVENT_LONG_PRESS ||
        event == LS30_KEY_EVENT_REPEAT)
        queue_event(EV_KEY_DOWN, map_key(keycode), 0);
    else if (event == LS30_KEY_EVENT_UP)
        queue_event(EV_KEY_UP, map_key(keycode), 0);
}

static void on_pen(int event, int x, int y)
{
    if (event == LS30_KEY_EVENT_UP)
        queue_event(EV_POINTER_UP, x, y);
    else if (event == LS30_KEY_EVENT_DOWN)
        queue_event(EV_POINTER_DOWN, x, y);
    else
        queue_event(EV_POINTER_DRAG, x, y);
}

static void on_system(int message, int param)
{
    (void)param;
    if (message == LS30_MSG_QUIT) vm.exit_requested = true;
}

static void on_timer(int id)
{
    (void)id;
    if (!g_running) return;
    g_epoch_ms += 20;
    if (vm_step()) {
        g_running = 0;
        if (g_timer_id >= 0) ls30_timer_stop(g_timer_id);
        g_timer_id = -1;
        vm_shutdown();
        midp_shutdown();
        if (g_layer >= 0) ls30_layer_delete(g_layer);
        g_layer = -1;
        g_frame = NULL;
        g_app = NULL;
        g_classlib = NULL;
    }
}

bool pal_poll_event(NjEvent *ev)
{
    int head;
    if (!ev) return false;
    head = g_queue_head;
    if (head == g_queue_tail) return false;
    *ev = g_queue[head];
    g_queue_head = (head + 1) % MRE_EVENT_QUEUE;
    return true;
}

void pal_wait(int64_t until_ms)
{
    (void)until_ms;
    /* MRE owns the event loop; the periodic VM timer yields between slices. */
}

void pal_screen_size(int *w, int *h)
{
    if (w) *w = g_screen_w;
    if (h) *h = g_screen_h;
}

void pal_present(const uint16_t *px, int w, int h)
{
    int x, y;
    if (!px || !g_frame || w != g_screen_w || h != g_screen_h) return;
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            uint16_t c = px[y * w + x]; /* RGB555, bit 15 set */
            unsigned r = c & 31u, g = (c >> 5) & 31u, b = (c >> 10) & 31u;
            g_frame[y * w + x] = (uint16_t)((r << 11) | ((g << 1) << 5) | b);
        }
    }
    ls30_flush(&g_layer, 1);
}

void pal_soft_labels(const char *left, const char *right)
{
    (void)left;
    (void)right;
}

bool pal_audio_available(void) { return false; }
void pal_audio_song(int slot, const NjaEvent *ev, uint32_t n, uint32_t start_ms, int volume)
{ (void)slot; (void)ev; (void)n; (void)start_ms; (void)volume; }
void pal_audio_pcm(int slot, const int16_t *pcm, uint32_t samples, uint32_t rate,
                   uint32_t start, int volume)
{ (void)slot; (void)pcm; (void)samples; (void)rate; (void)start; (void)volume; }
void pal_audio_stop(int slot) { (void)slot; }
void pal_audio_volume(int slot, int volume) { (void)slot; (void)volume; }

const char *pal_data_dir(void) { return NULL; }

void pal_fatal(const char *msg)
{
    if (msg) ls30_log_error(msg);
    vm.exit_requested = true;
}

int pocketjava_mre_start(const char *midlet_class, const void *jar, unsigned jar_size,
                         const void *classlib, unsigned classlib_size,
                         size_t heap_size)
{
    VMOptions opt;
    const char *args[2];
    int stack_marker;
    int sw = ls30_screen_width(), sh = ls30_screen_height();

    if (g_running || !jar || !jar_size || !classlib || !classlib_size) return -1;
    if (sw <= 0 || sh <= 0 || sw > 480 || sh > 640) return -2;
    g_screen_w = sw;
    g_screen_h = sh;
    g_layer = ls30_layer_create(0, 0, sw, sh, LS30_NO_TRANS_COLOR);
    if (g_layer < 0) return -3;
    g_frame = (uint16_t *)ls30_layer_buffer(g_layer);
    if (!g_frame) { ls30_layer_delete(g_layer); g_layer = -1; return -4; }
    memset(g_frame, 0, (size_t)sw * (size_t)sh * sizeof(*g_frame));
    ls30_flush(&g_layer, 1);

    g_queue_head = g_queue_tail = 0;
    g_epoch_ms = 0;
    g_running = 1;
    ls30_on_key(on_key);
    ls30_on_pen(on_pen);
    ls30_on_system(on_system);

    memset(&opt, 0, sizeof(opt));
    opt.heap_size = heap_size;
    opt.stack_slots = JAVA_STACK_SLOTS;
    opt.max_frames = JAVA_MAX_FRAMES;
    heap_set_stack_base(&stack_marker);
    if (!vm_init(&opt)) { g_running = 0; return -5; }
    midp_init();
    g_classlib = zip_open_mem((const uint8_t *)classlib, classlib_size);
    g_app = zip_open_mem((const uint8_t *)jar, jar_size);
    if (!g_classlib || !g_app) { g_running = 0; vm_shutdown(); return -6; }
    res_set_system(g_classlib);
    res_add_app_zip(g_app);
    args[0] = "-midlet";
    args[1] = midlet_class ? midlet_class : "";
    if (!vm_boot(args, 2)) { g_running = 0; vm_shutdown(); midp_shutdown(); return -7; }
    vm_poll_hook = midp_poll;
    g_timer_id = ls30_timer_start(20, on_timer);
    if (g_timer_id < 0) { g_running = 0; vm_shutdown(); midp_shutdown(); return -8; }
    return 0;
}

void pocketjava_mre_stop(void)
{
    g_running = 0;
    if (g_timer_id >= 0) ls30_timer_stop(g_timer_id);
    g_timer_id = -1;
    vm_shutdown();
    midp_shutdown();
    if (g_layer >= 0) ls30_layer_delete(g_layer);
    g_layer = -1;
    g_frame = NULL;
    g_app = NULL;
    g_classlib = NULL;
}

