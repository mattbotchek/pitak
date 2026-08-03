cmake_minimum_required(VERSION 3.16)
project(PiTAK LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS 
    Quick 
    Qml
)

find_library(PIGPIO_LIBRARY NAMES pigpio libpigpio)
if(NOT PIGPIO_LIBRARY)
    find_library(PIGPIOD_IF2_LIBRARY NAMES pigpiod_if2 libpigpiod_if2)
    if(NOT PIGPIOD_IF2_LIBRARY)
        message(FATAL_ERROR "Could not find a pigpio library (tried pigpio and pigpiod_if2)")
    endif()
    set(PIGPIO_LINK_LIBRARY ${PIGPIOD_IF2_LIBRARY})
else()
    set(PIGPIO_LINK_LIBRARY ${PIGPIO_LIBRARY})
endif()

add_executable(pitak
    main.cpp
    sx126x.cpp
    LoRaManager.cpp
    pitak.qrc
)

target_link_libraries(pitak PRIVATE
    Qt6::Quick
    Qt6::Qml
    ${PIGPIO_LINK_LIBRARY}
    rt
    pthread
)