/* cejson_fuzz.c – ultimate JSON fuzzer with maximum mutation control */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <getopt.h>
#include "cejson.h"

#define NODE_CAP           (1ULL << 20)
#define STACK_CAP          (1ULL << 18)
#define DEFAULT_ITERATIONS 1000000ULL
#define DEFAULT_MAXSIZE    16384
#define DEFAULT_MAX_FLIPS  0          /* 0 = normal mode, >0 = aggressive mutation */

static JsonNode  nodes[NODE_CAP];
static uint32_t  stack[STACK_CAP];
static uint8_t   expecting_key[STACK_CAP];

static uint64_t total_tests = 0;
static uint64_t total_bytes_processed = 0;

static uint64_t rng_state = 0x123456789abcdefULL;
static inline uint64_t xorshift64(void) {
    uint64_t x = rng_state;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    return rng_state = x;
}
static uint32_t rnd32(void) { return (uint32_t)xorshift64(); }
static double   rndf(void)  { return (xorshift64() >> 11) * (1.0 / 9007199254740992.0); }

static void generate_random_json(char *buf, size_t max_len);
static void fuzz_one(const char *json, size_t len);
static void print_progress(uint64_t current, uint64_t total, clock_t start);
static void usage(const char *prog);

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    uint64_t iterations = DEFAULT_ITERATIONS;
    size_t   max_size   = DEFAULT_MAXSIZE;
    uint32_t max_flips  = DEFAULT_MAX_FLIPS;

    int opt;
    while ((opt = getopt(argc, argv, "hi:s:f:")) != -1) {
        switch (opt) {
            case 'i': iterations = strtoull(optarg, NULL, 0); if (!iterations) iterations = DEFAULT_ITERATIONS; break;
            case 's': max_size = strtoull(optarg, NULL, 0); if (max_size < 256) max_size = 256; if (max_size > 1024*1024) max_size = 1024*1024; break;
            case 'f': max_flips = (uint32_t)strtoul(optarg, NULL, 0); break;
            case 'h': default: usage(argv[0]); return 0;
        }
    }

    rng_state = (uint64_t)time(NULL) ^ 0xdeadbeefcafebabeULL;

    const char *mode = max_flips > 0 ? "AGGRESSIVE MUTATION" :
                       iterations >= 10000000ULL ? "huge" : "normal";

    printf("=== cejson.h fuzz tester ===\n");
    printf("Iterations        : %" PRIu64 "\n", iterations);
    printf("Max JSON size     : %zu bytes\n", max_size);
    printf("Max random flips  : %u per document%s\n", max_flips, max_flips > 0 ? " (chaos mode!)" : "");
    printf("Mode              : %s\n", mode);
    printf("Starting...\n");

    clock_t start = clock();
    char *buffer = malloc(max_size + 64);
    if (!buffer) { perror("malloc"); return 1; }

    for (uint64_t i = 1; i <= iterations; ++i) {
        generate_random_json(buffer, max_size);

        size_t len = strlen(buffer);

        /* NEW: Aggressive random corruption mode */
        if (max_flips > 0 && len > 10) {
            uint32_t flips = max_flips;
            if (flips > len / 4) flips = len / 4;
            for (uint32_t f = 0; f < flips; ++f) {
                size_t pos = rnd32() % len;
                /* Flip a byte in a way that's likely to break JSON */
                switch (rnd32() % 6) {
                    case 0: buffer[pos] = (char)rnd32(); break;                    /* random byte */
                    case 1: buffer[pos] = '"'; break;                              /* stray quote */
                    case 2: buffer[pos] = '{'; break;                              /* stray brace */
                    case 3: buffer[pos] = '}'; break;
                    case 4: buffer[pos] = ','; break;
                    case 5: if (pos < len-1) { buffer[pos] = buffer[pos+1]; buffer[pos+1] = buffer[pos]; } break; /* swap */
                }
            }
            /* Re-terminate in case we corrupted the null */
            buffer[len] = '\0';
        }

        /* Every 100th round: pure random garbage */
        if (i % 100 == 0) {
            for (size_t k = 0; k < max_size; ++k) buffer[k] = (char)rnd32();
            size_t garbage_len = 64 + rnd32() % (max_size - 64);
            buffer[garbage_len] = '\0';
            len = garbage_len;
        }

        fuzz_one(buffer, len);
        total_bytes_processed += len;
        total_tests++;

        if (i % 10000 == 0 || i == iterations) {
            print_progress(i, iterations, start);
        }
    }

    free(buffer);

    double secs = (double)(clock() - start) / CLOCKS_PER_SEC;
    double mb_total = total_bytes_processed / (1024.0 * 1024.0);
    double mb_per_sec = secs > 0.0 ? mb_total / secs : 0.0;

    printf("\n\n=== DONE ===\n");
    printf("Total tests       : %" PRIu64 "\n", total_tests);
    printf("Total data parsed : %.2f MB (%" PRIu64 " bytes)\n", mb_total, total_bytes_processed);
    printf("Total time        : %.3f seconds\n", secs);
    printf("Throughput        : %.2f MB/s  (%.2f million tests/sec)\n", mb_per_sec, total_tests / secs / 1e6);
    printf("cejson survived %" PRIu64 " brutally malformed JSONs – UNSTOPPABLE!\n", total_tests);

    return 0;
}

/* ------------------------------------------------------------------ */
static void usage(const char *prog)
{
    printf("Usage: %s [-i iterations] [-s size] [-f flips]\n", prog);
    printf("  -i N    Number of iterations (default: %" PRIu64 ")\n", DEFAULT_ITERATIONS);
    printf("  -s N    Max JSON size in bytes (default: %d)\n", DEFAULT_MAXSIZE);
    printf("  -f N    Max random byte flips per document (0 = normal, >0 = chaos!)\n");
    printf("  -h      Show help\n");
    printf("\nExamples:\n");
    printf("  %s -i 10000000 -s 65536 -f 50    # 10M docs, up to 64KB, 50 random corruptions each\n", prog);
    printf("  %s -f 200                        # pure chaos mode\n", prog);
}

/* ------------------------------------------------------------------ */
static void fuzz_one(const char *json, size_t len)
{
    JsonParser p;
    json_init(&p, nodes, NODE_CAP, stack, STACK_CAP, expecting_key);
    size_t off = 0;
    while (off < len) {
        size_t chunk = 1 + (rnd32() % 127);
        if (chunk > len - off) chunk = len - off;
        if (!json_feed(&p, json + off, chunk)) {
            if (p.error == JSON_ERR_CAPACITY) { printf("JSON string too large\n"); return; }
			//printf("JSON error %s @ %u\n", JsonErrorStr[p.error], p.error_pos);
			//printf("%s\n", p.buffer);
            return;
        } else {
#ifdef DEBUG
			printf("  ====SUSSESS DEBUG====\n");
			json_print_pretty(&p);
			printf("\n=====================\n");
#endif //DEBUG
		}

        off += chunk;
    }
    (void)json_finish(&p);
}

static void print_progress(uint64_t current, uint64_t total, clock_t start)
{
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    double mb_total = total_bytes_processed / (1024.0 * 1024.0);
    double mb_per_sec = elapsed > 0.01 ? mb_total / elapsed : 0.0;
    double percent = 100.0 * current / total;

    int bar_width = 50;
    int filled = (int)(percent / 100.0 * bar_width);
    if (filled > bar_width) filled = bar_width;

    const char spinner[] = "-\\|/";
    int spin_idx = (int)(current / 1000) % 4;

    printf("\r%c [", spinner[spin_idx]);
    for (int i = 0; i < filled; i++) putchar('=');
    for (int i = filled; i < bar_width; i++) putchar(' ');
    printf("] %6.2f%%  %8" PRIu64 "/%" PRIu64 "  %.2f MB/s",
           percent, current, total, mb_per_sec);
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* Stack-safe JSON generator – unchanged */
#define GEN_STACK_CAP 256
typedef enum { GEN_VALUE, GEN_ARRAY, GEN_OBJECT } GenCmd;
typedef struct { GenCmd cmd; char open, close; int items_left; } GenFrame;

static void generate_random_json(char *buf, size_t max_len)
{
    size_t pos = 0;
    GenFrame stack[GEN_STACK_CAP];
    int top = 0;

    if (max_len < 256) { buf[0] = '{'; buf[1] = '}'; buf[2] = '\0'; return; }
    stack[top++] = (GenFrame){ .cmd = GEN_VALUE };

    while (top > 0 && pos < max_len - 128) {
        if (top >= GEN_STACK_CAP) { while (top--) { if (pos < max_len-1) buf[pos++] = '}'; } break; }
        GenFrame *f = &stack[top - 1];

        if (f->cmd == GEN_VALUE) {
            double r = rndf();
            if (r < 0.20) { buf[pos++] = '"'; int len = (int)(rnd32() % 48); /* string body */ for (int i = 0; i < len && pos < max_len - 64; ++i) { if (rndf() < 0.04) { /* escapes */ } else buf[pos++] = (char)(rnd32() & 0x7F); } if (pos < max_len) buf[pos++] = '"'; }
            else if (r < 0.40) { /* number */ if (rnd32()&1) buf[pos++] = '-'; int d = 1 + (int)(rnd32()%12); for (int i = 0; i < d && pos < max_len-32; ++i) buf[pos++] = '0'+(rnd32()%10); /* frac/exp optional */ }
            else if (r < 0.55) { const char *lits[] = {"null","true","false"}; const char *s = lits[rnd32()%3]; while (*s && pos < max_len-32) buf[pos++] = *s++; }
            else { char open = (r < 0.78) ? '[' : '{'; char close = open=='[' ? ']' : '}'; buf[pos++] = open; int items = (int)(rnd32() % 9); if (items == 0 || pos >= max_len - 64) { if (pos < max_len) buf[pos++] = close; top--; continue; } stack[top++] = (GenFrame){ .cmd = open=='[' ? GEN_ARRAY : GEN_OBJECT, .open=open, .close=close, .items_left=items }; continue; }
            top--;
        } else {
            if (f->items_left == 0 || rndf() < 0.07 || pos >= max_len - 64) { if (pos < max_len) buf[pos++] = f->close; top--; }
            else { if (f->cmd == GEN_OBJECT) { buf[pos++] = '"'; int len = 1 + (int)(rnd32()%16); for (int i = 0; i < len && pos < max_len-32; ++i) buf[pos++] = 'a'+(rnd32()%26); if (pos+2 < max_len) { buf[pos++] = '"'; buf[pos++] = ':'; } } stack[top++] = (GenFrame){ .cmd = GEN_VALUE }; f->items_left--; }
        }
        if (top > 0 && pos < max_len - 32) { GenFrame *p = &stack[top-1]; if ((p->cmd == GEN_ARRAY || p->cmd == GEN_OBJECT) && p->items_left > 0) buf[pos++] = ','; }
    }
    while (top > 0 && pos < max_len - 1) buf[pos++] = '}';
    buf[pos] = '\0';
}
