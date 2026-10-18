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

# ---------------------------------------------------------------------------
# Translations: i18n/ubmeshcore_<lang>.ts -> .qm, embedded as :/i18n/*.qm.
# main.cpp loads the one matching the phone's system language.
# ---------------------------------------------------------------------------
find_package(Qt5LinguistTools REQUIRED)
file(GLOB UBMESHCORE_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/i18n/ubmeshcore_*.ts)
qt5_add_translation(UBMESHCORE_QM_FILES ${UBMESHCORE_TS_FILES})
set(UBMESHCORE_I18N_QRC ${CMAKE_CURRENT_BINARY_DIR}/i18n.qrc)
set(_i18n_entries "")
foreach(_qm ${UBMESHCORE_QM_FILES})
    get_filename_component(_qm_name ${_qm} NAME)
    string(APPEND _i18n_entries "    <file alias=\"i18n/${_qm_name}\">${_qm}</file>\n")
endforeach()
file(WRITE ${UBMESHCORE_I18N_QRC}
     "<RCC>\n  <qresource prefix=\"/\">\n${_i18n_entries}  </qresource>\n</RCC>\n")
# The .qrc references generated .qm files, so they must exist before rcc runs.
add_custom_target(ubmeshcore_translations DEPENDS ${UBMESHCORE_QM_FILES})

add_executable(ubmeshcore
    src/main.cpp
    src/bluetoothmanager.cpp
    src/bluetoothmanager.h
    src/companionprotocol.cpp
    src/companionprotocol.h
    src/contactmodel.cpp
    src/contactmodel.h
    src/messagestore.cpp
    src/messagestore.h
    qml.qrc
    ${UBMESHCORE_I18N_QRC})
add_dependencies(ubmeshcore ubmeshcore_translations)

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
