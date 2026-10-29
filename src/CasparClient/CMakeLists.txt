# CasparClient: cliente novo de playout (Qt Widgets). Usa o nucleo (engine), a ponte com o CasparCG
# (playout), o AMCP (caspar) e o OSC (osc). Nao usa o banco de dados nem os widgets do cliente antigo.
cmake_minimum_required(VERSION 3.16)
project(casparclient VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets)

qt_add_executable(casparclient WIN32 MACOSX_BUNDLE
    ConnectionManager.cpp ConnectionManager.h
    ExecutionController.cpp ExecutionController.h
    Main.cpp
    MainWindow.cpp MainWindow.h
    MediaLibraryWidget.cpp MediaLibraryWidget.h
    ResourcePanel.cpp ResourcePanel.h
    RundownModel.cpp RundownModel.h
    RundownStore.cpp RundownStore.h
    RundownWidget.cpp RundownWidget.h
    StationDialog.cpp StationDialog.h
    StationStore.cpp StationStore.h
)
add_external_dependencies(casparclient)

target_include_directories(casparclient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Caspar
    ${CMAKE_CURRENT_SOURCE_DIR}/../Osc
)

target_link_libraries(casparclient PRIVATE
    caspar
    engine
    osc
    playout

    Qt::Core
    Qt::Gui
    Qt::Network
    Qt::Widgets
)

# Fontes em UTF-8 (textos da interface em portugues).
if (MSVC)
    target_compile_options(casparclient PRIVATE /utf-8)
endif()

# Mesmo diretorio de saida que o pacote do Windows copia (build\Release).
set_target_properties(casparclient PROPERTIES
    OUTPUT_NAME "casparclient"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN"
)

install(TARGETS casparclient
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
