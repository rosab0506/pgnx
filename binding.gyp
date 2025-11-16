{
  "targets": [{
    "target_name": "pgnx",
    "sources": ["src/addon.cpp", "src/connection_pool.cpp", "src/connection.cpp", "src/listener.cpp"],
    "include_dirs": [
      "<!@(node -p \"require('node-addon-api').include\")"
    ],
    "cflags!": ["-fno-exceptions", "-fno-rtti"],
    "cflags_cc!": ["-fno-exceptions", "-fno-rtti"],
    "cflags_cc": ["-std=c++17", "-fexceptions", "-frtti"],
    "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
    "conditions": [
      ["OS=='win'", {
        "include_dirs": [
          "C:/vcpkg/installed/x64-windows/include"
        ],
        "libraries": [
          "C:/vcpkg/installed/x64-windows/lib/pqxx.lib",
          "C:/vcpkg/installed/x64-windows/lib/libpq.lib"
        ],
        "msvs_settings": {
          "VCCLCompilerTool": {
            "ExceptionHandling": 1,
            "RuntimeTypeInfo": "true",
            "AdditionalOptions": ["/std:c++17"]
          }
        }
      }],
      ["OS=='linux'", {
        "include_dirs": [
          "/usr/include/postgresql",
          "/usr/local/include"
        ],
        "libraries": ["-lpqxx", "-lpq", "-lpthread"],
        "cflags_cc": ["-O3"]
      }],
      ["OS=='mac'", {
        "include_dirs": [
          "<!@(brew --prefix)/include",
          "<!@(brew --prefix libpqxx)/include",
          "<!@(brew --prefix postgresql@16)/include"
        ],
        "libraries": ["-lpqxx", "-lpq"],
        "library_dirs": [
          "<!@(brew --prefix)/lib",
          "<!@(brew --prefix libpqxx)/lib",
          "<!@(brew --prefix postgresql@16)/lib"
        ],
        "xcode_settings": {
          "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
          "GCC_ENABLE_CPP_RTTI": "YES",
          "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
          "MACOSX_DEPLOYMENT_TARGET": "10.15"
        }
      }]
    ]
  }]
}
