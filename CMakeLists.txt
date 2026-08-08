cmake_minimum_required(VERSION 3.16)
project(PiTAK LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(PITAK_BUILD_APP "Build the PiTAK appliance application" ON)
option(PITAK_BUILD_TESTS "Build PiTAK unit tests" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

if(PITAK_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS Quick Qml Sql)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_path(PIGPIO_INCLUDE_DIR pigpio.h REQUIRED)
        find_library(PIGPIO_LIBRARY NAMES pigpio libpigpio)
        if(NOT PIGPIO_LIBRARY)
            message(FATAL_ERROR "Could not find the pigpio library")
        endif()
    endif()

    add_executable(pitak
        main.cpp
        sx126x.cpp
        LoRaManager.cpp
        SecureProtocol.cpp
        PiTakStore.cpp
        PiSatPage.cpp
        pitak.qrc
    )

    target_link_libraries(pitak PRIVATE
        Qt6::Quick
        Qt6::Qml
        Qt6::Sql
        OpenSSL::Crypto
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(pitak PRIVATE ${PIGPIO_LIBRARY})
        target_include_directories(pitak PRIVATE ${PIGPIO_INCLUDE_DIR})
    endif()
    if(UNIX)
        find_package(Threads REQUIRED)
        find_library(RT_LIBRARY rt REQUIRED)
        target_link_libraries(pitak PRIVATE Threads::Threads ${RT_LIBRARY})
    endif()
endif()

if(PITAK_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(secure_protocol_tests
        tests/SecureProtocolTests.cpp
        SecureProtocol.cpp
        PiSatPage.cpp
    )
    target_include_directories(secure_protocol_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(secure_protocol_tests PRIVATE Qt6::Core Qt6::Test OpenSSL::Crypto)
    add_test(NAME secure_protocol_tests COMMAND secure_protocol_tests)
endif()