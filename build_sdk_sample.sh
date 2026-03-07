export PATH="/home/work/ax/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin:$PATH"

cmake -B build;cmake --build build
cd plugins
cmake -B build;cmake --build build
cd ../
