rm -rf build/
cmake -S . -B build  -DCMAKE_TOOLCHAIN_FILE=cmake/raspberrypi-aarch64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)