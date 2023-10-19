#!/bin/bash

GMP_VERSION="gmp-6.1.2"
GMP_DIR="gmp"

MSIEVE_VERSION="msieve153_src"
_MSIEVE_PATH="Msieve%20v1.53"
MSIEVE_DIR="msieve"

DIST_DIR="dist"
mkdir -p "${DIST_DIR}"

DEBUG=0
CLEAN=0
BUILD_GMP=1
BUILD_MSIEVE=1

while [ $# -gt 0 ]; do
  if [[ $1 == "help" ]]; then
    echo "usage: $0 help"
    echo "       $0 debug|clean|gmp|msieve"
    exit 0
  fi
  if [[ $1 == "debug" ]]; then
    DEBUG=1
  fi
  if [[ $1 == "clean" ]]; then
    CLEAN=1
    BUILD_GMP=$(( ${BUILD_GMP} - 1 ))
    BUILD_MSIEVE=$(( ${BUILD_MSIEVE} - 1 ))
  fi
  if [[ $1 == "gmp" ]] && [ ${BUILD_GMP} -le 1 ]; then
    BUILD_GMP=$(( ${BUILD_GMP} + 1 ))
  fi
  if [[ $1 == "msieve" ]] && [ ${BUILD_MSIEVE} -le 1 ]; then
    BUILD_MSIEVE=$(( ${BUILD_MSIEVE} + 1 ))
  fi
  shift
done

# Configure build tools
export CC=emcc
export CXX=emcc

# =========
# Build GMP
# =========
function build_gmp() {
  # Clean old build artifacts
  if [ -d "${GMP_DIR}" ]; then
    rm -rf "${GMP_DIR}"
  fi

  echo "================="
  echo "=== Build GMP ==="
  echo "================="
  echo ""

  # Download sources
  if [ ! -f ${GMP_VERSION}.tar.lz ]; then
    curl -L -O "https://gmplib.org/download/gmp/${GMP_VERSION}.tar.lz"
  fi

  ## Extract sources
  mkdir "${GMP_DIR}"
  tar xf "${GMP_VERSION}.tar.lz" --strip-components=1 --directory="${GMP_DIR}"
  pushd "${GMP_DIR}" || exit 1

  # Configure environment
  export LDFLAGS="-s ALLOW_MEMORY_GROWTH=1"
  export CFLAGS="-Os"

  # Compile gmp
  emconfigure ./configure "CCAS=gcc -c" --disable-assembly --disable-shared || exit 1
  emmake make -j 16 all NO_ZLIB=1 || exit 1

  popd
}

# ============
# Build msieve
# ============
function build_msieve() {
  # Clean old build artifacts
  if [ -d "${MSIEVE_DIR}" ]; then
    rm -rf "${MSIEVE_DIR}"
  fi

  echo "===================="
  echo "=== Build msieve ==="
  echo "===================="
  echo ""

  # Download sources
  if [ ! -f ${MSIEVE_VERSION}.tar.gz ]; then
    curl -L -O "https://sourceforge.net/projects/msieve/files/msieve/${_MSIEVE_PATH}/${MSIEVE_VERSION}.tar.gz"
  fi

  # Extract sources
  mkdir "${MSIEVE_DIR}"
  tar xf "${MSIEVE_VERSION}.tar.gz" --strip-components=1 --directory="${MSIEVE_DIR}"
  pushd "${MSIEVE_DIR}" || exit 1

  # Configure environment
  export GMP_INCLUDES="../${GMP_DIR}"
  export GMP_LIBS="${GMP_INCLUDES}/.libs"

  export LDFLAGS="\
    -s ENVIRONMENT='web,worker'\
    -s EXPORT_NAME='msieve'\
    -s FILESYSTEM=1\
    -s MODULARIZE=1\
    -s EXPORTED_FUNCTIONS=\"['_sieve','_main']\"\
    -s EXPORTED_RUNTIME_METHODS=\"['callMain','ccall','cwrap']\"\
    -s INVOKE_RUN=0\
    -s EXIT_RUNTIME=1\
    -s EXPORT_NAME='EmscrJSR_msieve'\
    -s EXPORT_ES6=0\
    -s USE_ES6_IMPORT_META=0\
    -s ALLOW_MEMORY_GROWTH=1\
    --js-library ../msieve_lib.js\
    -L${GMP_LIBS}"
  export CFLAGS="-I${GMP_INCLUDES} -Os"

  if [ ${DEBUG} -gt 0 ]; then
    export LDFLAGS="${LDFLAGS} -s ASSERTIONS=1" # For logging purposes.
  fi

  # Apply patches
  patch -up1 Makefile ../msieve-makefile.patch
  patch -up1 --input=../msieve-callbacks.patch

  # Copy main code
  cp ../msieve.c demo.c

  # Compile msieve
  emmake make -j 16 all CC=${CC} NO_ZLIB=1 || exit 1
  mv msieve msieve.js

  popd

  # Copy binaries to dist folder
  cp ${MSIEVE_DIR}/msieve.js ${DIST_DIR}/msieve.js
  cp ${MSIEVE_DIR}/msieve.wasm ${DIST_DIR}/msieve.wasm
}

# Select build targets
if [ ${CLEAN} -gt 0 ]; then
  echo "Clean build files"
  rm -rf "${GMP_DIR}" "${GMP_VERSION}.tar.lz"
  rm -rf "${MSIEVE_DIR}" "${MSIEVE_VERSION}.tar.gz"
fi

if [ ${BUILD_GMP} -gt 0 ] && [ ${BUILD_GMP} -ge ${BUILD_MSIEVE} ]; then
  build_gmp
fi

if [ ${BUILD_MSIEVE} -gt 0 ] && [ ${BUILD_MSIEVE} -ge ${BUILD_GMP} ]; then
  build_msieve
fi
