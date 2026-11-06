cmake_minimum_required(VERSION 3.24)

project(stm32f103c8_sample LANGUAGES C ASM)

add_executable(stm32f103c8_sample
    src/startup.S
    src/main.c
)

set_target_properties(stm32f103c8_sample PROPERTIES
    SUFFIX ".elf"
)

target_compile_options(stm32f103c8_sample PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -O2
    -ffreestanding
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
)

target_link_options(stm32f103c8_sample PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103c8.ld
    -nostartfiles
    -nostdlib
    -Wl,--gc-sections
    -Wl,--build-id=none
    -Wl,-Map=${CMAKE_BINARY_DIR}/stm32f103c8_sample.map
)

target_link_libraries(stm32f103c8_sample PRIVATE gcc)

add_custom_command(TARGET stm32f103c8_sample POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:stm32f103c8_sample>
        ${CMAKE_BINARY_DIR}/stm32f103c8_sample.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:stm32f103c8_sample>
    COMMENT "Creating STM32 flash image"
    VERBATIM
)

find_program(ST_FLASH_EXECUTABLE st-flash REQUIRED)

add_custom_target(flash
    COMMAND ${ST_FLASH_EXECUTABLE} --reset write
        ${CMAKE_BINARY_DIR}/stm32f103c8_sample.bin 0x08000000
    DEPENDS stm32f103c8_sample
    USES_TERMINAL
    COMMENT "Flashing STM32F103C8Tx over ST-Link"
    VERBATIM
)
