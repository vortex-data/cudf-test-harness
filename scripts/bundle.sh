#!/bin/bash
# Bundle cudf-test-harness with all its shared library dependencies
# Creates a self-contained tarball that can be extracted and run anywhere
# (as long as NVIDIA drivers are installed)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"
BINARY="${BUILD_DIR}/cudf-test-harness"

if [[ ! -f "$BINARY" ]]; then
    echo "Error: Binary not found at $BINARY"
    echo "Please build the project first: cd build && cmake .. && make"
    exit 1
fi

# Create bundle directory
BUNDLE_DIR="${BUILD_DIR}/bundle"
BUNDLE_NAME="cudf-test-harness-$(uname -m)"
BUNDLE_PATH="${BUNDLE_DIR}/${BUNDLE_NAME}"

rm -rf "$BUNDLE_PATH"
mkdir -p "$BUNDLE_PATH/lib"

echo "Collecting dependencies for $BINARY..."

# Install the binary so CMake applies the bundle RPATH.
cmake --install "$BUILD_DIR" --prefix "$BUNDLE_PATH" --component harness

# Get all shared library dependencies (excluding system libraries we expect to exist)
# We keep: libcu*, libnv*, librmm*, libcudf*, libarrow*, libstdc++, etc.
# We exclude: libc, libm, libpthread, libdl, librt, ld-linux, libgcc_s (basic system libs)
# We also exclude: libcuda.so (NVIDIA driver - must be on target system)

get_deps() {
    ldd "$1" 2>/dev/null | grep "=>" | awk '{print $3}' | grep -v "^$" || true
}

copy_lib() {
    local lib="$1"
    local basename=$(basename "$lib")

    # Skip libraries that should come from the target system
    case "$basename" in
        libc.so*|libm.so*|libpthread.so*|libdl.so*|librt.so*|ld-linux*|libgcc_s.so*)
            echo "  Skipping system lib: $basename"
            return
            ;;
        libcuda.so*|libnvidia*.so*)
            echo "  Skipping driver lib: $basename"
            return
            ;;
    esac

    if [[ -f "$lib" && ! -f "$BUNDLE_PATH/lib/$basename" ]]; then
        echo "  Copying: $basename"
        cp -L "$lib" "$BUNDLE_PATH/lib/"
    fi
}

# Recursively collect all dependencies
declare -A processed
collect_deps() {
    local file="$1"

    for dep in $(get_deps "$file"); do
        if [[ -n "$dep" && -f "$dep" && -z "${processed[$dep]:-}" ]]; then
            processed[$dep]=1
            copy_lib "$dep"
            collect_deps "$dep"
        fi
    done
}

collect_deps "$BINARY"

# Create wrapper script that sets up the environment
cat > "$BUNDLE_PATH/cudf-test-harness.sh" << 'WRAPPER_EOF'
#!/bin/bash
# Wrapper script to run cudf-test-harness with bundled libraries

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

exec "${SCRIPT_DIR}/cudf-test-harness" "$@"
WRAPPER_EOF
chmod +x "$BUNDLE_PATH/cudf-test-harness.sh"

# Create tarball
echo ""
echo "Creating tarball..."
cd "$BUNDLE_DIR"
tar -czvf "${BUNDLE_NAME}.tar.gz" "$BUNDLE_NAME"

echo ""
echo "Bundle created: ${BUNDLE_DIR}/${BUNDLE_NAME}.tar.gz"
echo ""
echo "Usage on target machine:"
echo "  tar -xzf ${BUNDLE_NAME}.tar.gz"
echo "  cd ${BUNDLE_NAME}"
echo "  ./cudf-test-harness check your-library.so"
echo ""
echo "Requirements on target machine:"
echo "  - NVIDIA GPU driver installed"
echo "  - Compatible Linux kernel and glibc"
