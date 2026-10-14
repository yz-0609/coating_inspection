set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(TOOLCHAIN_PREFIX arm-none-eabi CACHE STRING "gnu arm embedded toolchain prefix")

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_AR ${TOOLCHAIN_PREFIX}-ar CACHE FILEPATH "archiver executable")
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}-objcopy CACHE FILEPATH "objcopy executable")
set(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}-objdump CACHE FILEPATH "objdump executable")
set(CMAKE_NM ${TOOLCHAIN_PREFIX}-nm CACHE FILEPATH "nm executable")
set(CMAKE_READELF ${TOOLCHAIN_PREFIX}-readelf CACHE FILEPATH "readelf executable")
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}-size CACHE FILEPATH "size executable")

# Control optimization at target/source scope; do not implicitly define NDEBUG.
foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_C_FLAGS_${configuration} "" CACHE STRING "C configuration flags" FORCE)
    set(CMAKE_ASM_FLAGS_${configuration} "" CACHE STRING "ASM configuration flags" FORCE)
endforeach()
