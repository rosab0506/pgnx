#!/bin/bash
set -e

PLATFORM=$1
ARCH=$2
INSTALL_PREFIX=${3:-/tmp/pgsql}

echo "=== Building pgnx native dependencies ==="
echo "Platform: $PLATFORM"
echo "Architecture: $ARCH"
echo "Install prefix: $INSTALL_PREFIX"

NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Build PostgreSQL 16.1 (for libpq)
echo "Building PostgreSQL 16.1..."
cd /tmp
if [ ! -f "postgresql-16.1.tar.gz" ]; then
    wget -q https://ftp.postgresql.org/pub/source/v16.1/postgresql-16.1.tar.gz
fi
if [ ! -d "postgresql-16.1" ]; then
    tar xzf postgresql-16.1.tar.gz
fi

cd postgresql-16.1

if [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "x64" ]; then
    CFLAGS="-fPIC -O3" ./configure \
        --prefix=$INSTALL_PREFIX \
        --without-readline \
        --without-icu \
        --with-openssl
elif [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "arm64" ]; then
    CC=aarch64-linux-gnu-gcc CFLAGS="-fPIC -O3" ./configure \
        --prefix=$INSTALL_PREFIX \
        --host=aarch64-linux-gnu \
        --without-readline \
        --without-icu \
        --with-openssl \
        --with-includes="/usr/aarch64-linux-gnu/include" \
        --with-libraries="/usr/aarch64-linux-gnu/lib"
elif [ "$PLATFORM" = "win32" ]; then
    CC=x86_64-w64-mingw32-gcc ./configure \
        --prefix=$INSTALL_PREFIX \
        --host=x86_64-w64-mingw32 \
        --without-readline \
        --without-icu \
        --without-zlib
elif [ "$PLATFORM" = "darwin" ]; then
    OPENSSL_PREFIX=$(brew --prefix openssl@3 2>/dev/null || echo "/usr/local/opt/openssl@3")
    CFLAGS="-fPIC -O3" ./configure \
        --prefix=$INSTALL_PREFIX \
        --without-readline \
        --without-icu \
        --with-openssl \
        --with-includes=${OPENSSL_PREFIX}/include \
        --with-libraries=${OPENSSL_PREFIX}/lib
fi

make -C src/interfaces/libpq -j$NPROC
make -C src/interfaces/libpq install
make -C src/bin/pg_config install
make -C src/include install
make -C src/common -j$NPROC
make -C src/port -j$NPROC

# Build libpqxx 7.10.4 (requires CMake)
echo "Building libpqxx 7.10.4..."
cd /tmp
if [ ! -f "7.10.4.tar.gz" ]; then
    wget -q https://github.com/jtv/libpqxx/archive/refs/tags/7.10.4.tar.gz
fi
if [ ! -d "libpqxx-7.10.4" ]; then
    tar xzf 7.10.4.tar.gz
fi

cd libpqxx-7.10.4
rm -rf build
mkdir build && cd build

CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_FLAGS=-fPIC \
    -DBUILD_SHARED_LIBS=OFF \
    -DSKIP_BUILD_TEST=ON \
    -DBUILD_DOC=OFF \
    -DPostgreSQL_INCLUDE_DIR=$INSTALL_PREFIX/include \
    -DPostgreSQL_LIBRARY=$INSTALL_PREFIX/lib/libpq.a \
    -DPostgreSQL_TYPE_INCLUDE_DIR=$INSTALL_PREFIX/include"

if [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "arm64" ]; then
    CMAKE_ARGS="$CMAKE_ARGS \
        -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
        -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64"
elif [ "$PLATFORM" = "win32" ]; then
    CMAKE_ARGS="$CMAKE_ARGS \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_SYSTEM_PROCESSOR=AMD64"
fi

cmake .. $CMAKE_ARGS
cmake --build . -j$NPROC
cmake --install .

echo "=== Build complete ==="
echo "Libraries installed to: $INSTALL_PREFIX"
echo "  Include: $INSTALL_PREFIX/include"
echo "  Lib:     $INSTALL_PREFIX/lib"
