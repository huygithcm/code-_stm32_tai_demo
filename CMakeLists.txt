cmake_minimum_required(VERSION 3.24)

project(stm32f103c8_servo_test LANGUAGES C ASM)

add_executable(stm32f103c8_servo_test
    src/startup.S
    src/main.c
)

set_target_properties(stm32f103c8_servo_test PROPERTIES
    SUFFIX ".elf"
)

target_compile_options(stm32f103c8_servo_test PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -O2
    -ffreestanding
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
)

target_link_options(stm32f103c8_servo_test PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103c8.ld
    -nostartfiles
    -nostdlib
    -Wl,--gc-sections
    -Wl,--build-id=none
    -Wl,-Map=${CMAKE_BINARY_DIR}/stm32f103c8_servo_test.map
)

target_link_libraries(stm32f103c8_servo_test PRIVATE gcc)

add_custom_command(TARGET stm32f103c8_servo_test POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:stm32f103c8_servo_test>
        ${CMAKE_BINARY_DIR}/stm32f103c8_servo_test.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:stm32f103c8_servo_test>
    COMMENT "Creating STM32 flash image"
    VERBATIM
)

find_program(ST_FLASH_EXECUTABLE st-flash)
if(ST_FLASH_EXECUTABLE)
    add_custom_target(flash
        COMMAND ${ST_FLASH_EXECUTABLE} --reset write
            ${CMAKE_BINARY_DIR}/stm32f103c8_servo_test.bin 0x08000000
        DEPENDS stm32f103c8_servo_test
        USES_TERMINAL
        COMMENT "Flashing STM32F103C8Tx over ST-Link"
        VERBATIM
    )
else()
    message(STATUS "st-flash not found; the flash target is unavailable")
endif()
