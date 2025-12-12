/* cejson_test_suite.c
   Comprehensive test suite for cejson.h
   Compile with:
       gcc -Wall -Wextra -O2 cejson_test_suite.c -o test_cejson -lm
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include "cejson.h"

#define NODE_CAP  65536
#define STACK_CAP 4096

static JsonNode nodes[NODE_CAP];
static uint32_t stack[STACK_CAP];
static uint8_t expecting_key[STACK_CAP];

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name) void name(void)
#define RUN_TEST(name) do { printf("*** Running %s... \n", #name); name(); } while(0)
#define ASSERT(cond, test) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("FAIL %s at %s:%d\n", test, __FILE__, __LINE__); \
        tests_failed++; \
		json_print_pretty(&p); \
    } else { \
        printf("PASS %s %s:%d %s\n", test, __FILE__, __LINE__, __FUNCTION__); \
    } \
} while(0)

static bool parse_full(const char* json, JsonParser* p)
{
    json_init(p, nodes, NODE_CAP, stack, STACK_CAP, expecting_key);

    size_t len = strlen(json);
    size_t pos = 0;
    while (pos < len) {
        size_t chunk = 1 + (rand() % 128);  // random chunks 1..128
        if (chunk > len - pos) chunk = len - pos;
        if (!json_feed(p, json + pos, chunk)) {
#ifdef DEBUG
			fprintf(stderr, "Failed parse [%s @ %u]: %s\n", JsonErrorStr[p->error], p->error_pos, json);
#endif
            return false;
		}
        pos += chunk;
    }
    p->buffer = json;  // final buffer for json_str()
    return json_finish(p);
}

static void test_null()
{
    JsonParser p;
	printf("TEST: test_null\n");

    ASSERT(parse_full("null", &p), "literal null");
    ASSERT(p.nodes_len == 1, "nodes_len == 1");
    ASSERT(p.nodes[0].type == JSON_NULL, "JSON_NULL test");
    ASSERT(p.nodes[0].len == 4, "Node wise == 4");
}

static void test_true_false()
{
    JsonParser p;
	printf("TEST: test_true_false\n");

    ASSERT(parse_full("true", &p), "literanl true");
    ASSERT(p.nodes[0].type == JSON_TRUE, "node type == true");

    ASSERT(parse_full(" false ", &p), "literal false");
    ASSERT(p.nodes[0].type == JSON_FALSE, "node type == false");
}

static void test_numbers()
{
    JsonParser p;
	printf("TEST: test_numbers\n");

    ASSERT(parse_full("0", &p), "literal 0");
    ASSERT(p.nodes[0].type == JSON_NUMBER_INT, "type is int");

    ASSERT(parse_full("-123456", &p), "literal number");
    ASSERT(p.nodes[0].type == JSON_NUMBER_INT, "type is int");

    ASSERT(parse_full("3.14159", &p), "literal float");
    ASSERT(p.nodes[0].type == JSON_NUMBER_FLOAT, "float value");

    ASSERT(parse_full("1e10", &p), "e float number");
    ASSERT(p.nodes[0].type == JSON_NUMBER_FLOAT, "type is floag");

    ASSERT(parse_full("-0.5e-3", &p), "scientific number");
    ASSERT(p.nodes[0].type == JSON_NUMBER_FLOAT, "type is float");
}

static void test_strings()
{
	size_t tmplen = 1024;
	char tmp[tmplen];

    JsonParser p;
    ASSERT(parse_full("\"hello world\"", &p), "literal string");
    ASSERT(p.nodes[0].type == JSON_STRING, "type is string");
    ASSERT( (strcmp(json_str(&p, &p.nodes[0], tmp, tmplen), "hello world") == 0), "strncmp test");
	//printf("json_str == [%s]\n", json_str(&p, &p.nodes[0], tmp, tmplen) );

    // escapes
    const char* esc = "\"\\\"\\\\/\\b\\f\\n\\r\\t\\u0020\"";
    ASSERT(parse_full(esc, &p), "literal escaped values");
    ASSERT( (strcmp(json_str(&p, &p.nodes[0], tmp, tmplen), "\\\"\\\\/\\b\\f\\n\\r\\t\\u0020") == 0), "strncmp test");
	//printf("json_str == [%s]\n", json_str(&p, &p.nodes[0], tmp, tmplen) );
}

static void test_empty_containers()
{
    JsonParser p;
    ASSERT(parse_full("{}", &p), "empty object");
    ASSERT(p.nodes[0].type == JSON_OBJECT, "type json object");
    ASSERT(p.nodes[0].children == 0, "0 children");

    ASSERT(parse_full("[]", &p), "empty array");
    ASSERT(p.nodes[0].type == JSON_ARRAY, "type json array");
    ASSERT(p.nodes[0].children == 0, "0 children array");
}

static void test_simple_object()
{
    JsonParser p;
    ASSERT(parse_full("{\"a\":1,\"b\":true,\"c\":null}", &p), "simple object");
    ASSERT(p.nodes_len == 7, "nodes len == 7");  // { "a" 1 "b" true "c" null }
    ASSERT(p.nodes[0].type == JSON_OBJECT, "type object");
    ASSERT(p.nodes[1].type == JSON_STRING, "type string");
    ASSERT(p.nodes[2].type == JSON_NUMBER_INT, "type int");
    ASSERT(p.nodes[3].type == JSON_STRING, "type string");
    ASSERT(p.nodes[4].type == JSON_TRUE, "type true");
    ASSERT(p.nodes[5].type == JSON_STRING, "type string");
    ASSERT(p.nodes[6].type == JSON_NULL, "type null");
}

static void test_nested()
{
    const char* json = "{\"user\":{\"name\":\"Alice\",\"age\":30,\"active\":true},\"tags\":[]}";
    JsonParser p;
    ASSERT(parse_full(json, &p), "nested objects");
    ASSERT(p.nodes_len == 11, "wrong nodes len");
}

static void test_array_of_primitives()
{
    JsonParser p;
    ASSERT(parse_full("[1, 2.5, true, false, null, \"hi\"]", &p), "array of primatives");
    ASSERT(p.nodes_len == 7, "");
    ASSERT(p.nodes[0].type == JSON_ARRAY, "");
    ASSERT(p.nodes[1].type == JSON_NUMBER_INT, "");
    ASSERT(p.nodes[2].type == JSON_NUMBER_FLOAT, "");
    ASSERT(p.nodes[3].type == JSON_TRUE, "");
    ASSERT(p.nodes[4].type == JSON_FALSE, "");
    ASSERT(p.nodes[5].type == JSON_NULL, "");
    ASSERT(p.nodes[6].type == JSON_STRING, "");
}

static void test_error_detection()
{
    JsonParser p;
    ASSERT(!parse_full("{", &p), "Should fail: Incomplete");           // incomplete
    ASSERT(!parse_full("{\"a\":}", &p), "Should fail: missing value");     // missing value
    //ASSERT(!parse_full("[1,]", &p), "Should fail: Trailing comma");        // trailing comma
    ASSERT(!parse_full("trux", &p), "Should fail: Bad literal");        // bad literal
    ASSERT(!parse_full("\"key\":", &p), "Should fail: Bad literal");        // bad literal
    ASSERT(!parse_full("\"\\q\"", &p), "Should fail: Invalid escape");     // invalid escape
    ASSERT(!parse_full("1.", &p), "Should fail: incomplete float");          // dot without digit
    ASSERT(!parse_full("1e", &p), "Should fail: incomplete e float");          // e without digit
}

static void test_value_extraction()
{
    JsonParser p;
    ASSERT(parse_full("{\"score\": 98.6, \"passed\": true, \"id\": 123}", &p), "");

    for (uint32_t i = 0; i < p.nodes_len; i++) {
        JsonNode* n = &p.nodes[i];
        if (n->type == JSON_NUMBER_FLOAT) {
            double v;
            ASSERT(json_as_f64(&p, n, &v), "");
            ASSERT(v == 98.6, "");
        }
        if (n->type == JSON_TRUE) {
            ASSERT(n->type == JSON_TRUE, "");
        }
        if (n->type == JSON_NUMBER_INT) {
            int64_t v;
            ASSERT(json_as_i64(&p, n, &v), "");
            ASSERT(v == 123, "");
        }
    }
}

static void test_real_world_files()
{
    const char* files[] = {
        "test1.json", "test2.json", "test3.json", "test4.json",
        "test5.json", "test6.json", "test7.json", "test8.json",
        NULL
    };

    for (int i = 0; files[i]; i++) {
        FILE* f = fopen(files[i], "rb");
        if (!f) continue;

        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = malloc(len + 1);
        fread(buf, 1, len, f);
        buf[len] = '\0';
        fclose(f);

        JsonParser p;
        bool ok = parse_full(buf, &p);
        printf("File %s: %s (%llu nodes)\n",
               files[i], ok ? "OK" : "FAIL", p.nodes_len);
        ASSERT(ok, "ok");
        free(buf);
    }
}

static void create_tree_test()
{
	JsonParser p;
    json_init(&p, nodes, NODE_CAP, stack, STACK_CAP, expecting_key);

	JsonNode* root = json_create_object(&p);
	JsonNode* name = json_create_string(&p, "Alice");
	JsonNode* age = json_create_int(&p, 30);
	json_object_set(&p, root, name, age);
	json_print_pretty(&p);  
	json_free_tree(&p, root);
}


int main(void)
{
    printf("=== cejson.h Test Suite ===\n");

    RUN_TEST(test_null);
    RUN_TEST(test_true_false);
    RUN_TEST(test_numbers);
    RUN_TEST(test_strings);
    RUN_TEST(test_empty_containers);
    RUN_TEST(test_simple_object);
    RUN_TEST(test_nested);
    RUN_TEST(test_array_of_primitives);
    RUN_TEST(test_error_detection);
    RUN_TEST(test_value_extraction);
    RUN_TEST(test_real_world_files);
    RUN_TEST(create_tree_test);

    printf("============================\n");
    printf("Tests run: %d | Failed: %d\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
