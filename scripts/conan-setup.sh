#!/bin/bash
set -e

PLATFORM=$1
ARCH=$2
BUILD_DIR=${3:-/tmp/pgnx-build}

echo "=== Conan Setup for pgnx ==="
echo "Platform: $PLATFORM"
echo "Architecture: $ARCH"
echo "Build directory: $BUILD_DIR"

# Install Conan if not present
if ! command -v conan &> /dev/null; then
    echo "Installing Conan..."
    pip3 install conan
fi

# Initialize Conan profile if needed
conan profile detect --force || true

# Create build directory
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Install dependencies via Conan
echo "Installing dependencies via Conan..."
conan install $GITHUB_WORKSPACE \
    --build=missing \
    -s build_type=Release \
    -s compiler.cppstd=17 \
    -o "*:shared=False"

# Source the generated paths
if [ -f "conan_paths.sh" ]; then
    source conan_paths.sh
fi

# Now build PostgreSQL and libpqxx
echo "Building PostgreSQL..."
cd /tmp
if [ ! -d "postgresql-16.1" ]; then
    wget -q https://ftp.postgresql.org/pub/source/v16.1/postgresql-16.1.tar.gz
    tar xzf postgresql-16.1.tar.gz
fi

cd postgresql-16.1

# Platform-specific configuration
if [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "x64" ]; then
    CFLAGS="-fPIC -O3" ./configure \
        --prefix=/tmp/pgsql \
        --without-readline \
        --with-openssl \
        --with-includes="$CONAN_INCLUDE_DIRS" \
        --with-libraries="$(echo $CONAN_LIB_DIRS | tr ':' ' ')"
elif [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "arm64" ]; then
    CC=aarch64-linux-gnu-gcc CFLAGS="-fPIC -O3" ./configure \
        --prefix=/tmp/pgsql \
        --host=aarch64-linux-gnu \
        --without-readline \
        --without-icu \
        --with-openssl \
        --with-includes="$CONAN_INCLUDE_DIRS:/usr/aarch64-linux-gnu/include" \
        --with-libraries="$(echo $CONAN_LIB_DIRS | tr ':' ' '):/usr/aarch64-linux-gnu/lib"
elif [ "$PLATFORM" = "win32" ]; then
    CC=x86_64-w64-mingw32-gcc ./configure \
        --prefix=/tmp/pgsql \
        --host=x86_64-w64-mingw32 \
        --without-readline \
        --without-icu \
        --without-zlib
fi

make -C src/interfaces/libpq -j$(nproc)
make -C src/interfaces/libpq install
make -C src/bin/pg_config install
make -C src/include install
make -C src/common -j$(nproc)
make -C src/port -j$(nproc)

# Build libpqxx
echo "Building libpqxx..."
cd /tmp
if [ ! -d "libpqxx-7.8.1" ]; then
    wget -q https://github.com/jtv/libpqxx/archive/refs/tags/7.8.1.tar.gz
    tar xzf 7.8.1.tar.gz
fi

cd libpqxx-7.8.1

if [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "x64" ]; then
    CXX="g++ -std=c++17" CXXFLAGS="-fPIC -O3" ./configure \
        --prefix=/tmp/pgsql \
        --enable-shared=no \
        --with-postgres-include=/tmp/pgsql/include \
        --with-postgres-lib=/tmp/pgsql/lib
elif [ "$PLATFORM" = "linux" ] && [ "$ARCH" = "arm64" ]; then
    CXX="aarch64-linux-gnu-g++ -std=c++17" CXXFLAGS="-fPIC -O3" ./configure \
        --prefix=/tmp/pgsql \
        --host=aarch64-linux-gnu \
        --enable-shared=no \
        --with-postgres-include=/tmp/pgsql/include \
        --with-postgres-lib=/tmp/pgsql/lib
elif [ "$PLATFORM" = "win32" ]; then
    CXX="x86_64-w64-mingw32-g++ -std=c++17" CXXFLAGS="-fPIC -O3" ./configure \
        --prefix=/tmp/pgsql \
        --host=x86_64-w64-mingw32 \
        --enable-shared=no \
        --with-postgres-include=/tmp/pgsql/include \
        --with-postgres-lib=/tmp/pgsql/lib
fi

make -j$(nproc)
make install

echo "=== Conan setup complete ==="
echo "Libraries installed to: /tmp/pgsql"
