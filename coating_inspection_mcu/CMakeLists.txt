cmake_minimum_required(VERSION 3.21)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake" CACHE FILEPATH "Arm toolchain")
endif()
project(STM32F407 LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
option(COATING_DEBUG_INJECTION "Enable UART1 diagnostic fault/input injection" OFF)

# Cortex-M4F, Thumb, AAPCS base calling convention; all dependencies build from source.
set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp)
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/STM32F407VE_FLASH.ld")

# GNU startup
set(STARTUP_SOURCES
    Core/Startup/startup_stm32f407xx.S
)

# Application/User/Core
set(CORE_SOURCES
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/freertos.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
    Core/Src/stm32f4xx_hal_timebase_tim.c
)

# Drivers/STM32F4xx_HAL_Driver
set(HAL_SOURCES
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c
)

# Drivers/CMSIS
set(SYSTEM_SOURCES
    Core/Src/system_stm32f4xx.c
)

# Bsp
set(BSP_SOURCES
    Bsp/UART/bsp_UART.c
    Bsp/Board/bsp_board.c
    Bsp/Reset/bsp_reset.c
    Bsp/RS485/bsp_rs485.c
    Bsp/Watchdog/bsp_watchdog.c
    Bsp/UART/syscalls.c
)

include(cmake/pure_modules.cmake)
set(MODULE_SOURCES ${PURE_MODULE_SOURCES}
    Application/app_runtime.c
    Device/Output/output_manager.c)

# Middlewares/FreeRTOS
set(FREERTOS_SOURCES
    Middlewares/Third_Party/FreeRTOS/Source/croutine.c
    Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
    Middlewares/Third_Party/FreeRTOS/Source/list.c
    Middlewares/Third_Party/FreeRTOS/Source/queue.c
    Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
    Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    Middlewares/Third_Party/FreeRTOS/Source/timers.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
)

add_executable(STM32F407
    ${STARTUP_SOURCES}
    ${CORE_SOURCES}
    ${HAL_SOURCES}
    ${SYSTEM_SOURCES}
    ${BSP_SOURCES}
    ${MODULE_SOURCES}
    ${FREERTOS_SOURCES}
)
set_target_properties(STM32F407 PROPERTIES SUFFIX ".elf" LINK_DEPENDS "${LINKER_SCRIPT}")
target_compile_definitions(STM32F407 PRIVATE USE_HAL_DRIVER STM32F407xx)
target_compile_definitions(STM32F407 PRIVATE COATING_DEBUG_INJECTION=$<BOOL:${COATING_DEBUG_INJECTION}>)
target_include_directories(STM32F407 PRIVATE
    Core/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
    Bsp/UART
    Bsp/Board
    Bsp/Reset
    Module/Diagnostics
    Bsp/RS485
    Bsp/Watchdog
    Application
    ${PURE_MODULE_INCLUDES}
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
)

target_compile_options(STM32F407 PRIVATE
    ${MCU_FLAGS} -g3 -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-fshort-enums>
)
# Size optimization for application/drivers; original RTOS file overrides were O0.
set_source_files_properties(${CORE_SOURCES} ${HAL_SOURCES} ${SYSTEM_SOURCES} ${BSP_SOURCES} ${MODULE_SOURCES}
    PROPERTIES COMPILE_OPTIONS "-Os")
set_source_files_properties(${FREERTOS_SOURCES} PROPERTIES COMPILE_OPTIONS "-O0")
target_link_options(STM32F407 PRIVATE
    ${MCU_FLAGS} -nostartfiles "-T${LINKER_SCRIPT}"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/STM32F407.map"
    -Wl,--gc-sections -Wl,--print-memory-usage
)
target_link_libraries(STM32F407 PRIVATE c m)
add_custom_command(TARGET STM32F407 POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:STM32F407> "${CMAKE_CURRENT_BINARY_DIR}/STM32F407.hex"
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:STM32F407> "${CMAKE_CURRENT_BINARY_DIR}/STM32F407.bin"
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:STM32F407>
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/STM32F407.hex" "${CMAKE_CURRENT_BINARY_DIR}/STM32F407.bin"
    VERBATIM
)
