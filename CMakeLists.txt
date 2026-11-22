cmake_minimum_required(VERSION 3.20)

project(AT-D878-Open-Firmware
    VERSION 0.0.1
    DESCRIPTION "Open source firmware for AnyTone AT-D878UVII Plus"
    LANGUAGES C ASM
)

# Firmware hedefi
set(FIRMWARE_TARGET at_d878_fw)

# MCU tanımları
set(MCU_FAMILY GD32F303)
set(MCU_MODEL GD32F303VGT6)
set(MCU_CORE cortex-m4)
set(MCU_FPU fpv4-sp-d16)
set(MCU_FLOAT_ABI hard)
set(MCU_FLASH_SIZE 1048576)   # 1 MB
set(MCU_RAM_SIZE 98304)        # 96 KB
set(MCU_FLASH_ORIGIN 0x08000000)
set(MCU_RAM_ORIGIN 0x20000000)

# Bootloader alanını koru (ilk 16KB üretici bootloader'a ayrılmış)
set(APP_FLASH_ORIGIN 0x08004000)
set(APP_FLASH_SIZE 1032192)    # 1 MB - 16 KB

# Derleme seçenekleri
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_TOOLS "Build host tools" OFF)
option(ENABLE_DEBUG "Enable debug output via UART" ON)
option(ENABLE_GPS "Enable GPS module" ON)
option(ENABLE_BLUETOOTH "Enable Bluetooth module" ON)
option(ENABLE_FM_BROADCAST "Enable FM broadcast receiver" ON)
option(ENABLE_DMR "Enable DMR protocol (requires SCT3258)" ON)
option(ENABLE_M17 "Enable M17 protocol" OFF)
option(ENABLE_APRS "Enable APRS protocol" ON)

# Derleme türü
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Ortak C bayrakları
set(COMMON_C_FLAGS
    -mcpu=${MCU_CORE}
    -mthumb
    -mfpu=${MCU_FPU}
    -mfloat-abi=${MCU_FLOAT_ABI}
    -fdata-sections
    -ffunction-sections
    -fno-common
    -fno-exceptions
    -Wall
    -Wextra
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
    -Wundef
    -fstack-usage
)

# Debug/Release bayrakları
set(DEBUG_FLAGS -Og -g3 -gdwarf-2 -DDEBUG)
set(RELEASE_FLAGS -O2 -DNDEBUG)

# Tanımlar
add_compile_definitions(
    ${MCU_FAMILY}
    ${MCU_MODEL}
    USE_STDPERIPH_DRIVER
    HSE_VALUE=8000000
    $<$<BOOL:${ENABLE_DEBUG}>:DEBUG_UART_ENABLED>
    $<$<BOOL:${ENABLE_GPS}>:GPS_ENABLED>
    $<$<BOOL:${ENABLE_BLUETOOTH}>:BLUETOOTH_ENABLED>
    $<$<BOOL:${ENABLE_DMR}>:DMR_ENABLED>
    $<$<BOOL:${ENABLE_M17}>:M17_ENABLED>
    $<$<BOOL:${ENABLE_APRS}>:APRS_ENABLED>
)

# Include dizinleri
set(FIRMWARE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/hal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/drivers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/radio/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/digital/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/audio/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/gps/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/aprs/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/ui/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/storage/include
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/config/include
)

# Kaynak dosyaları topla
file(GLOB_RECURSE CORE_SOURCES    CONFIGURE_DEPENDS firmware/core/src/*.c)
file(GLOB_RECURSE HAL_SOURCES     CONFIGURE_DEPENDS firmware/hal/src/*.c)
file(GLOB_RECURSE DRIVER_SOURCES  CONFIGURE_DEPENDS firmware/drivers/src/*.c)
file(GLOB_RECURSE RADIO_SOURCES   CONFIGURE_DEPENDS firmware/radio/src/*.c)
file(GLOB_RECURSE AUDIO_SOURCES   CONFIGURE_DEPENDS firmware/audio/src/*.c)
file(GLOB_RECURSE UI_SOURCES      CONFIGURE_DEPENDS firmware/ui/src/*.c)
file(GLOB_RECURSE STORAGE_SOURCES CONFIGURE_DEPENDS firmware/storage/src/*.c)
file(GLOB_RECURSE CONFIG_SOURCES  CONFIGURE_DEPENDS firmware/config/src/*.c)

set(FIRMWARE_SOURCES
    ${CORE_SOURCES}
    ${HAL_SOURCES}
    ${DRIVER_SOURCES}
    ${RADIO_SOURCES}
    ${AUDIO_SOURCES}
    ${UI_SOURCES}
    ${STORAGE_SOURCES}
    ${CONFIG_SOURCES}
)

# Koşullu modüller
if(ENABLE_GPS)
    file(GLOB_RECURSE GPS_SOURCES CONFIGURE_DEPENDS firmware/gps/src/*.c)
    list(APPEND FIRMWARE_SOURCES ${GPS_SOURCES})
endif()

if(ENABLE_DMR OR ENABLE_M17)
    file(GLOB_RECURSE DIGITAL_SOURCES CONFIGURE_DEPENDS firmware/digital/src/*.c)
    list(APPEND FIRMWARE_SOURCES ${DIGITAL_SOURCES})
endif()

if(ENABLE_APRS)
    file(GLOB_RECURSE APRS_SOURCES CONFIGURE_DEPENDS firmware/aprs/src/*.c)
    list(APPEND FIRMWARE_SOURCES ${APRS_SOURCES})
endif()

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/firmware/core/gd32f303vgt6.ld)

# Firmware hedefi
if(NOT FIRMWARE_SOURCES)
    message(FATAL_ERROR "Firmware source list is empty")
endif()

add_executable(${FIRMWARE_TARGET} ${FIRMWARE_SOURCES})
target_include_directories(${FIRMWARE_TARGET} PRIVATE ${FIRMWARE_INCLUDES})
target_compile_options(${FIRMWARE_TARGET} PRIVATE ${COMMON_C_FLAGS})
target_compile_options(${FIRMWARE_TARGET} PRIVATE
    "$<$<CONFIG:Debug>:-Og>"
    "$<$<CONFIG:Debug>:-g3>"
    "$<$<CONFIG:Debug>:-gdwarf-2>"
    "$<$<CONFIG:Debug>:-DDEBUG>"
    "$<$<CONFIG:Release>:-O2>"
    "$<$<CONFIG:Release>:-DNDEBUG>"
)

target_link_options(${FIRMWARE_TARGET} PRIVATE
    "-mcpu=${MCU_CORE}"
    "-mthumb"
    "-mfpu=${MCU_FPU}"
    "-mfloat-abi=${MCU_FLOAT_ABI}"
    "-T${LINKER_SCRIPT}"
    "-Wl,--gc-sections"
    "-Wl,--print-memory-usage"
    "-Wl,-Map=${FIRMWARE_TARGET}.map"
    "--specs=nano.specs"
    "--specs=nosys.specs"
    "-nostartfiles"
)
target_link_libraries(${FIRMWARE_TARGET} PRIVATE m c nosys)

if(NOT CMAKE_OBJCOPY)
    find_program(CMAKE_OBJCOPY NAMES arm-none-eabi-objcopy REQUIRED)
endif()
if(NOT CMAKE_SIZE)
    find_program(CMAKE_SIZE NAMES arm-none-eabi-size REQUIRED)
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(FIRMWARE_BIN ${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_TARGET}.bin)
set(FIRMWARE_HEX ${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_TARGET}.hex)
set(FIRMWARE_PACKAGE_DIR ${CMAKE_CURRENT_BINARY_DIR}/package)

add_custom_command(TARGET ${FIRMWARE_TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${FIRMWARE_TARGET}> ${FIRMWARE_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${FIRMWARE_TARGET}> ${FIRMWARE_HEX}
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${FIRMWARE_TARGET}>
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/firmware_analyzer/make_firmware_pkg.py
            ${FIRMWARE_BIN} -o ${FIRMWARE_PACKAGE_DIR}
    COMMENT "Generating firmware binaries and update package"
    VERBATIM
)

message(STATUS "")
message(STATUS "=== AT-D878 Open Firmware Build Configuration ===")
message(STATUS "MCU:           ${MCU_MODEL}")
message(STATUS "Core:          ${MCU_CORE}")
message(STATUS "Flash:         ${MCU_FLASH_SIZE} bytes")
message(STATUS "RAM:           ${MCU_RAM_SIZE} bytes")
message(STATUS "Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "GPS:           ${ENABLE_GPS}")
message(STATUS "Bluetooth:     ${ENABLE_BLUETOOTH}")
message(STATUS "DMR:           ${ENABLE_DMR}")
message(STATUS "M17:           ${ENABLE_M17}")
message(STATUS "APRS:          ${ENABLE_APRS}")
message(STATUS "Debug:         ${ENABLE_DEBUG}")
message(STATUS "=================================================")
message(STATUS "")
