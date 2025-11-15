#include <napi.h>
#include "connection.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return Connection::Init(env, exports);
}

NODE_API_MODULE(pgn, Init)
