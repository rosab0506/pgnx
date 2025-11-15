{
  "targets": [{
    "target_name": "pgn",
    "sources": ["src/addon.cpp", "src/connection_pool.cpp", "src/connection.cpp", "src/listener.cpp"],
    "include_dirs": [
      "<!@(node -p \"require('node-addon-api').include\")",
      "/usr/include/postgresql",
      "/usr/local/include"
    ],
    "libraries": ["-lpqxx", "-lpq", "-lpthread"],
    "cflags!": ["-fno-exceptions", "-fno-rtti"],
    "cflags_cc!": ["-fno-exceptions", "-fno-rtti"],
    "cflags_cc": ["-std=c++17", "-O3", "-fexceptions", "-frtti"]
  }]
}
