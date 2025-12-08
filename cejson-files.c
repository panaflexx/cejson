/* test_ultra_json.c - heap-based parser for >2GB JSON files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "cejson.h"

#define INITIAL_NODE_CAP   (4096ULL * 4096)   /* 16M nodes to start (~512MB) */
#define INITIAL_STACK_CAP  (4096ULL * 4096)     /* 4M stack depth (~64MB) */

int main(int argc, char **argv)
{
    srand(time(NULL));

    bool dump_json = false;
    bool network_emulation = false;
    bool verbose = false;

    /* Parse options */
    int arg_start = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            dump_json = true;
            arg_start++;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
            arg_start++;
        } else if (strcmp(argv[i], "-nw") == 0) {
            network_emulation = true;
            arg_start++;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Usage: %s [-d] [-nw] <file1.json> [file2.json ...]\n", argv[0]);
            fprintf(stderr, "  -d     dump (pretty-print) parsed JSON\n");
            fprintf(stderr, "  -nw    network emulation: 8–4096 byte chunks\n");
            fprintf(stderr, "  -v     verbose output\n");
            fprintf(stderr, "  (default: full-speed single-pass parsing)\n");
            return 1;
        } else {
            break;
        }
    }

    if (arg_start >= argc) {
        fprintf(stderr, "Usage: %s [-v] [-d] [-nw] <file1.json> [file2.json ...]\n", argv[0]);
        return 1;
    }

    /* Heap-allocated buffers - will grow dynamically if needed */
    JsonNode  *nodes = NULL;
    uint32_t  *stack = NULL;
    uint8_t   *expecting_key_stack = NULL;

    /* Allocate initial capacity */
    nodes = malloc(INITIAL_NODE_CAP * sizeof(JsonNode));
    stack = malloc(INITIAL_STACK_CAP * sizeof(uint32_t));
    expecting_key_stack = malloc(INITIAL_STACK_CAP * sizeof(uint8_t));

    if (!nodes || !stack || !expecting_key_stack) {
        fprintf(stderr, "Failed to allocate initial parser buffers\n");
        free(nodes); free(stack); free(expecting_key_stack);
        return 1;
    }

    for (int i = arg_start; i < argc; i++) {
        const char *filename = argv[i];

        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            printf("Failed to open %s\n", filename);
            continue;
        }

        fseek(fp, 0, SEEK_END);
        size_t total_len = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (total_len == 0) {
            printf("Empty file: %s\n", filename);
            fclose(fp);
            continue;
        }

        char *full_json = malloc(total_len + 1);
        if (!full_json) {
            printf("Malloc failed for %s (%zu bytes)\n", filename, total_len);
            fclose(fp);
            continue;
        }

        size_t read_len = fread(full_json, 1, total_len, fp);
        fclose(fp);
        if (read_len != total_len) {
            printf("Read failed for %s\n", filename);
            free(full_json);
            continue;
        }
        full_json[total_len] = '\0';

        JsonParser p;
        json_init(&p,
                  nodes, INITIAL_NODE_CAP,
                  stack, INITIAL_STACK_CAP,
                  expecting_key_stack);

        clock_t start = clock();

        size_t offset = 0;
        while (offset < total_len) {
            size_t remaining = total_len - offset;
            size_t chunk_size = network_emulation
                ? (8 + (rand() % (4096 - 8 + 1)))
                : (1024 * 1024);  /* 1MB chunks in full-speed mode */

            if (chunk_size > remaining) chunk_size = remaining;

            if (!json_feed(&p, full_json + offset, chunk_size)) {
                if (p.error) {
                    printf("Parse error %d at pos %u in %s\n",
                           p.error, p.error_pos, filename);
                    free(full_json);
                    goto next_file;
                }
            }
            offset += chunk_size;
        }

        if (!json_finish(&p)) {
            printf("JSON incomplete or invalid in %s\n", filename);
            free(full_json);
            continue;
        }

        clock_t end = clock();
        double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
        double mb = total_len / (1024.0 * 1024.0);
        double speed = cpu_time > 0.0 ? mb / cpu_time : 0.0;

        p.buffer = full_json;
        p.buf_len = total_len;

		if(verbose)
			fprintf(stderr, "Parsed %s to %llu nodes | %.2f MB/s (%.3f sec) [%s]\n",
				   filename, (unsigned long long)p.nodes_len,
				   speed, cpu_time,
				   network_emulation ? "network emulation" : "full speed");

        if (dump_json) {
            json_print_pretty(&p);
        }

        free(full_json);
    next_file:;
    }

    /* Clean up heap allocations */
    free(nodes);
    free(stack);
    free(expecting_key_stack);

    return 0;
}

void __builtin_unreachable(void) {
    for (;;) ;
}
