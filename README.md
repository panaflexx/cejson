# cejson
Fast zero-copy JSON parser in C

Examples:
**$ ./bin/cejson-files -v ~/src/node/node-v20.19.5/deps/v8/tools/unittests/testdata/*.json**

Parsed test1.json to 23 nodes (4096 allocated) | 12.87 MB/s (0.000 sec) | alloc: 4096 nodes [full speed]

Parsed test2.json to 25 nodes (4096 allocated) | 31.35 MB/s (0.000 sec) | alloc: 4096 nodes [full speed]

Parsed test3.json to 33 nodes (4096 allocated) | 106.49 MB/s (0.000 sec) | alloc: 4096 nodes [full speed]



**$ ./bin/cejson-files --help**

Usage: ./bin/cejson-files [-d] [-nw] [-v] <file1.json> [file2.json ...]

 -d  dump pretty-printed JSON
 
 -nw network emulation (8–4096 byte chunks)
 
 -v  verbose output
 
