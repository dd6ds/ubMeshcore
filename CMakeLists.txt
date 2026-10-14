cmake_minimum_required(VERSION 3.16)
project(ubmeshcore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required: bluetoothmanager.h contains Q_OBJECT classes.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# DBus is needed to read device names straight from org.bluez - Qt 5.12's
# QBluetoothDeviceDiscoveryAgent drops names that BlueZ resolves late.
find_package(Qt5 COMPONENTS Core Gui Qml Quick Bluetooth DBus REQUIRED)

add_executable(ubmeshcore
    src/main.cpp
    src/bluetoothmanager.cpp
    src/bluetoothmanager.h
    src/companionprotocol.cpp
    src/companionprotocol.h
    qml.qrc)

target_include_directories(ubmeshcore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(ubmeshcore
    Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::Bluetooth Qt5::DBus)

# ---------------------------------------------------------------------------
# Optional Rust core library (src/rust). Enable with -DUSE_RUST_CORE=ON once
# the meshcore-info-display path dependencies are reachable inside the
# Clickable container.
# ---------------------------------------------------------------------------
option(USE_RUST_CORE "Link the ubmeshcore_core Rust cdylib" OFF)
if(USE_RUST_CORE)
    set(RUST_LIB "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/target/aarch64-unknown-linux-gnu/release/libubmeshcore_core.so")
    target_link_libraries(ubmeshcore "${RUST_LIB}")
    set_target_properties(ubmeshcore PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
    install(FILES "${RUST_LIB}" DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif()

# QML files are compiled into the binary via qml.qrc, so they are deliberately
# NOT installed separately - that only bloated the click package with a second,
# easily stale copy.
install(TARGETS ubmeshcore RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES manifest.json DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ubmeshcore.desktop DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ubmeshcore.apparmor DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ubmeshcore.png DESTINATION ${CMAKE_INSTALL_PREFIX})
