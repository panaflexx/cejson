/* test_ultra_json.c - heap-based parser with smart pre-allocation */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "cejson.h"

static inline uint64_t json_estimate_node_count(uint64_t input_bytes)
{
    if (input_bytes == 0) return 64;

    uint64_t nodes = input_bytes / 11;  /* ~11 bytes/node worst-case (dense like citylots) */

    if (nodes < 64) nodes = 64;
    nodes += nodes / 5;                 /* +20% headroom */
    nodes = (nodes + 4095ULL) & ~4095ULL;  /* Round up to 4K boundary */

    return nodes;
}

int main(int argc, char **argv)
{
    srand(time(NULL));
    bool dump_json = false;
    bool network_emulation = false;
    bool verbose = false;

    /* Parse options */
    int arg_start = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) { dump_json = true; arg_start++; }
        else if (strcmp(argv[i], "-v") == 0) { verbose = true; arg_start++; }
        else if (strcmp(argv[i], "-nw") == 0) { network_emulation = true; arg_start++; }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Usage: %s [-d] [-nw] [-v] <file1.json> [file2.json ...]\n", argv[0]);
            fprintf(stderr, " -d  dump pretty-printed JSON\n");
            fprintf(stderr, " -nw network emulation (8–4096 byte chunks)\n");
            fprintf(stderr, " -v  verbose output\n");
            return 1;
        } else break;
    }

    if (arg_start >= argc) {
        fprintf(stderr, "Usage: %s [-v] [-d] [-nw] <file1.json> [file2.json ...]\n", argv[0]);
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
        long file_size_l = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (file_size_l <= 0) {
            printf("Empty or invalid file: %s\n", filename);
            fclose(fp);
            continue;
        }
        uint64_t total_len = (uint64_t)file_size_l;

        /* Smart pre-allocation based on file size */
        uint64_t estimated_nodes = json_estimate_node_count(total_len);
        uint64_t node_cap  = estimated_nodes;
        uint64_t stack_cap = estimated_nodes / 8 + 1024;  /* Stack depth rarely > nodes/8 */

        JsonNode *nodes = malloc(node_cap * sizeof(JsonNode));
        uint32_t *stack = malloc(stack_cap * sizeof(uint32_t));
        uint8_t  *expecting_key_stack = malloc(stack_cap * sizeof(uint8_t));

        if (!nodes || !stack || !expecting_key_stack) {
            fprintf(stderr, "Failed to allocate parser buffers for %s (~%llu nodes)\n",
                    filename, (unsigned long long)estimated_nodes);
            free(nodes); free(stack); free(expecting_key_stack);
            fclose(fp);
            continue;
        }

        char *full_json = malloc(total_len + 1);
        if (!full_json) {
            printf("Malloc failed for %s (%llu bytes)\n", filename, (unsigned long long)total_len);
            free(nodes); free(stack); free(expecting_key_stack);
            fclose(fp);
            continue;
        }

        size_t read_len = fread(full_json, 1, total_len, fp);
        fclose(fp);

        if (read_len != total_len) {
            printf("Read failed for %s\n", filename);
            free(full_json); free(nodes); free(stack); free(expecting_key_stack);
            continue;
        }
        full_json[total_len] = '\0';

        JsonParser p = {0,0};
        json_init(&p, nodes, node_cap, stack, stack_cap, expecting_key_stack);

        clock_t start = clock();
        size_t offset = 0;

        while (offset < total_len) {
            size_t remaining = total_len - offset;
            size_t chunk_size = network_emulation
                ? (8 + (rand() % (4096 - 8 + 1)))
                : remaining;  /* Full speed: one big chunk (or remaining) */

            if (chunk_size > remaining) chunk_size = remaining;

            if (!json_feed(&p, full_json + offset, chunk_size)) {
                if (p.error) {
                    printf("Parse error %s at pos %llu in %s\n",
                           JsonErrorStr[p.error], p.error_pos, filename);
                    break;
                }
            }
            offset += chunk_size;
        }

        bool parse_ok = false;
        if (!p.error) {
            parse_ok = json_finish(&p);
            if (!parse_ok) {
                printf("JSON incomplete or invalid in %s\n", filename);
            }
        }

        clock_t end = clock();
        double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
        double mb = total_len / (1024.0 * 1024.0);
        double speed = cpu_time > 0.0 ? mb / cpu_time : 0.0;

        if (parse_ok && verbose) {
            fprintf(stderr, "Parsed %s to %llu nodes (%llu allocated) | %.2f MB/s (%.3f sec) | alloc: %llu nodes [%s]\n",
                    filename,
                    (unsigned long long)p.nodes_len,
					estimated_nodes,
                    speed, cpu_time,
                    (unsigned long long)node_cap,
                    network_emulation ? "net emu" : "full speed");
        }

        if (parse_ok && dump_json) {
			StringBuf sb;
			bool result = stringbuf_init(&sb, (uint64_t)(p.buf_len * 2));
			if(result) {
				json_serialize(&p, false, &sb);
				printf("%s\n",  stringbuf_cstr(&sb));
				stringbuf_free(&sb);
			}
        }

        free(full_json);
        free(nodes);
        free(stack);
        free(expecting_key_stack);
    }

    return 0;
}
