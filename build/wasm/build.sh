#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT

openssl_version=3.2.1
curl -fsSL --retry 3 \
  "https://github.com/openssl/openssl/releases/download/openssl-${openssl_version}/openssl-${openssl_version}.tar.gz" \
  -o "$workdir/openssl.tar.gz"
tar -xzf "$workdir/openssl.tar.gz" -C "$workdir"

(
  cd "$workdir/openssl-${openssl_version}"
  CC=emcc AR=emar RANLIB=emranlib ./Configure linux-generic32 \
    no-asm no-async no-dso no-engine no-shared no-tests no-threads \
    --prefix="$workdir/openssl-install" --openssldir=/etc/ssl
  make -s -j4 build_libs
  make -s install_dev
)

mkdir -p "$workdir/objects" bin
for source in src/third-party/zlib/*.c \
  src/third-party/minizip/ioapi.c \
  src/third-party/minizip/zip.c \
  src/third-party/minizip/unzip.c; do
  emcc -O3 -Wno-unused-result \
    -Isrc -Isrc/third-party/zlib \
    -c "$source" -o "$workdir/objects/$(basename "${source%.c}").o"
done

version=${VERSION:-0.0.0-dev}
em++ -std=c++11 -O3 -Wno-unused-result \
  -DZSIGN_VERSION="$version" \
  -Isrc -Isrc/common -Isrc/third-party/zlib \
  -I"$workdir/openssl-install/include" \
  src/*.cpp src/common/*.cpp "$workdir"/objects/*.o \
  -L"$workdir/openssl-install/lib" -lssl -lcrypto \
  -sENVIRONMENT=web,worker,node -sFORCE_FILESYSTEM=1 -lnodefs.js \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createZsignModule -sINVOKE_RUN=0 \
  -sEXPORTED_RUNTIME_METHODS='["callMain","FS","NODEFS","HEAPU8","stackSave","stackRestore"]' \
  -sDYNAMIC_EXECUTION=0 -sALLOW_MEMORY_GROWTH=1 \
  --post-js build/wasm/post.js \
  -o bin/zsign.mjs
cp build/wasm/node-cli.cjs bin/zsign.js
# Keep existing dependency notices with the shared runtime.
cp LICENSE bin/LICENSE
cp "$workdir/openssl-${openssl_version}/LICENSE.txt" bin/OPENSSL-LICENSE.txt
mkdir -p bin/bundled-notices
cp src/third-party/zlib/zlib.h src/third-party/minizip/{zip.h,unzip.h,crypt.h} bin/bundled-notices/
