#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${AUTTOJETTY_BUILD_DIR:-${project_dir}/build}"

executable="${build_dir}/AuttoJetty"
needs_build=false
if [[ ! -x "${executable}" ]]; then
    needs_build=true
elif ldd "${executable}" 2>/dev/null | grep -q "not found"; then
    echo "AuttoJetty is linked against libraries that are no longer installed."
    needs_build=true
fi

if [[ "${needs_build}" == true ]]; then
    cmake_args=(-S "${project_dir}" -B "${build_dir}")

    # CUDA 13.2 does not support Fedora's default GCC 16 yet.
    if [[ -x /usr/bin/g++-15 ]]; then
        cmake_args+=(
            -DCMAKE_CXX_COMPILER=/usr/bin/g++-15
            -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-15
        )
    elif [[ -x /usr/local/cuda/bin/nvcc ]]; then
        echo "Error: rebuilding this CUDA project requires GCC 15." >&2
        echo "Install it with: sudo dnf install gcc15 gcc15-c++" >&2
        exit 1
    fi

    echo "Configuring and building AuttoJetty..."
    cmake "${cmake_args[@]}"
    cmake --build "${build_dir}" --parallel
fi

# The AI policy is loaded and saved relative to the process working directory.
cd -- "${build_dir}"
exec "${executable}" "$@"
