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
          "-lC:/vcpkg/installed/x64-windows/lib/pqxx.lib",
          "-lC:/vcpkg/installed/x64-windows/lib/libpq.lib"
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
          "/opt/homebrew/include",
          "/opt/homebrew/opt/libpqxx/include",
          "/opt/homebrew/opt/postgresql@14/include",
          "/usr/local/include",
          "/usr/local/opt/libpqxx/include",
          "/usr/local/opt/postgresql/include"
        ],
        "libraries": ["-lpqxx", "-lpq"],
        "library_dirs": [
          "/opt/homebrew/lib",
          "/opt/homebrew/opt/libpqxx/lib",
          "/opt/homebrew/opt/postgresql@14/lib",
          "/usr/local/lib",
          "/usr/local/opt/libpqxx/lib",
          "/usr/local/opt/postgresql/lib"
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
