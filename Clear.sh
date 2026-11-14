rm -rf build
rm build_log.txt
cmake -B build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
