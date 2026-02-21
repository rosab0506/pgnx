{
  "targets": [{
    "target_name": "pgnx",
    "sources": [
      "src/addon.cpp",
      "src/connection_pool.cpp",
      "src/connection.cpp",
      "src/listener.cpp"
    ],
    "include_dirs": [
      "<!@(node -p \"require('node-addon-api').include\")"
    ],
    "cflags!": ["-fno-exceptions", "-fno-rtti"],
    "cflags_cc!": ["-fno-exceptions", "-fno-rtti"],
    "cflags_cc": ["-std=c++17", "-fexceptions", "-frtti", "-O3"],
    "defines": ["NAPI_VERSION=8"],
    "conditions": [
      ["OS=='linux'", {
        "include_dirs": [
          "<!@(node -e \"try{process.stdout.write(require('child_process').execSync('pg_config --includedir',{encoding:'utf8'}).trim())}catch(e){process.stdout.write('/usr/include/postgresql')}\")",
          "<!@(node -e \"try{process.stdout.write(require('child_process').execSync('pkg-config --cflags-only-I libpqxx',{encoding:'utf8'}).trim().replace(/-I/g,''))}catch(e){process.stdout.write('/usr/include')}\")"
        ],
        "libraries": [
          "<!@(node -e \"try{process.stdout.write(require('child_process').execSync('pkg-config --libs libpqxx',{encoding:'utf8'}).trim())}catch(e){process.stdout.write('-lpqxx -lpq')}\")",
          "-lssl", "-lcrypto", "-lz", "-lpthread"
        ]
      }],
      ["OS=='mac'", {
        "include_dirs": [
          "<!@(node -e \"try{process.stdout.write(require('child_process').execSync('pg_config --includedir',{encoding:'utf8'}).trim())}catch(e){process.stdout.write('/usr/local/include')}\")",
          "<!@(node -e \"try{process.stdout.write(require('child_process').execSync('pkg-config --cflags-only-I libpqxx',{encoding:'utf8'}).trim().replace(/-I/g,''))}catch(e){process.stdout.write('/usr/local/include')}\")"
        ],
        "libraries": [
          "<!@(node -e \"try{process.stdout.write(require('child_process').execSync('pkg-config --libs libpqxx',{encoding:'utf8'}).trim())}catch(e){process.stdout.write('-lpqxx -lpq')}\")",
          "-lssl", "-lcrypto", "-lz"
        ],
        "xcode_settings": {
          "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
          "GCC_ENABLE_CPP_RTTI": "YES",
          "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
          "MACOSX_DEPLOYMENT_TARGET": "10.15",
          "OTHER_LDFLAGS": [
            "-framework", "CoreFoundation",
            "-framework", "Security"
          ]
        }
      }],
      ["OS=='win'", {
        "include_dirs": [
          "<!(node -e \"process.stdout.write(process.env.PGNX_INCLUDE_DIR||'C:/PostgreSQL/16/include')\")"
        ],
        "libraries": [
          "-l<!(node -e \"process.stdout.write(process.env.PGNX_LIB_DIR||'C:/PostgreSQL/16/lib')\")/libpqxx",
          "-l<!(node -e \"process.stdout.write(process.env.PGNX_LIB_DIR||'C:/PostgreSQL/16/lib')\")/libpq",
          "-lws2_32", "-lsecur32", "-lcrypt32", "-lwldap32",
          "-lssl", "-lcrypto", "-lz"
        ],
        "defines": ["WIN32", "_WIN32"],
        "msvs_settings": {
          "VCCLCompilerTool": {
            "ExceptionHandling": 1,
            "RuntimeTypeInfo": "true",
            "AdditionalOptions": ["/std:c++17", "/EHsc"]
          }
        }
      }]
    ]
  }]
}
