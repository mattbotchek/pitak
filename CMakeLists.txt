cmake_minimum_required(VERSION 3.16)
project(PiTAK LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS 
    Quick 
    Qml
    VirtualKeyboard
)

add_executable(pitak
    main.cpp
    sx126x.cpp
    LoRaManager.cpp
    pitak.qrc
)

target_link_libraries(pitak PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::VirtualKeyboard
    /usr/local/lib/libpigpiod_if2.so
    rt
    pthread
)