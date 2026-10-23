cmake_minimum_required(VERSION 3.28)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(PARAMA VERSION 0.1.0 LANGUAGES C)
if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "PARAMA M0-M1 requires Clang/LLD")
endif()
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  message(FATAL_ERROR "Use the x86_64 PARAMA toolchain")
endif()
set(KERNEL_SOURCES
  brahma/main.c
  brahma/arch/x86_64/serial.c
  brahma/core/bhumi/boot_alloc.c
  brahma/core/display/boot_display.c
)
add_executable(brahma.elf ${KERNEL_SOURCES})
target_include_directories(brahma.elf PRIVATE brahma/include third_party)
target_compile_options(brahma.elf PRIVATE
  -std=c2x -O2 -g -mcmodel=kernel -ffreestanding
  -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.
  -ffile-prefix-map=${CMAKE_BINARY_DIR}=.
  -fdebug-compilation-dir=. -fno-stack-protector -fno-pic -fno-pie
  -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-80387
  -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes
)
target_link_options(brahma.elf PRIVATE
  -fuse-ld=lld -nostdlib -static
  -Wl,-T,${CMAKE_SOURCE_DIR}/brahma/arch/x86_64/linker.ld
  -Wl,-z,max-page-size=0x1000 -Wl,--build-id=none
)
set_target_properties(brahma.elf PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  LINK_DEPENDS "${CMAKE_SOURCE_DIR}/brahma/arch/x86_64/linker.ld"
)
