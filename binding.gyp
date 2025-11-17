{
  "targets": [{
    "target_name": "pgnx",
    "sources": ["src/addon.cpp", "src/connection_pool.cpp", "src/connection.cpp", "src/listener.cpp"],
    "include_dirs": [
      "<!@(node -p \"require('node-addon-api').include\")",
      "/tmp/pgnx-deps/include"
    ],
    "cflags!": ["-fno-exceptions", "-fno-rtti"],
    "cflags_cc!": ["-fno-exceptions", "-fno-rtti"],
    "cflags_cc": ["-std=c++17", "-fexceptions", "-frtti", "-O3"],
    "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
    "libraries": [
      "/tmp/pgnx-deps/lib/libpqxx.a",
      "/tmp/pgnx-deps/lib/libpq.a",
      "/tmp/postgresql-16.1/src/common/libpgcommon.a",
      "/tmp/postgresql-16.1/src/port/libpgport.a",
      "-lssl",
      "-lcrypto",
      "-lz",
      "-lpthread"
    ]
  }]
}
