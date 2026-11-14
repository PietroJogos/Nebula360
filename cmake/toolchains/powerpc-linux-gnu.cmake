# Compilação cruzada para PowerPC 32-bit big-endian (Linux), usada para
# verificar o código em uma CPU com a mesma ordem de bytes (big-endian) e o
# mesmo tamanho de ponteiro (32 bits) do Xbox 360. NÃO gera executáveis de
# Xbox 360: serve para rodar os testes no qemu-ppc.
#
#   cmake -S . -B build-ppc -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/powerpc-linux-gnu.cmake \
#         -DNEBULA_WITH_SDL2=OFF
#   cmake --build build-ppc && ctest --test-dir build-ppc
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR powerpc)
set(CMAKE_C_COMPILER powerpc-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER powerpc-linux-gnu-g++)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-ppc)
set(CMAKE_FIND_ROOT_PATH /usr/powerpc-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
