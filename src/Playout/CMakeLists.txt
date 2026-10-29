# Playout: ponte entre o nucleo (src/Engine) e o CasparCG real (AMCP e OSC).
# Usada pelo cliente antigo (Core) e pelo CasparClient. Nao depende de banco de dados nem de widgets.
cmake_minimum_required(VERSION 3.16)
project(playout VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

qt_add_library(playout
    STATIC
    AmcpPlayoutPort.cpp AmcpPlayoutPort.h
    OscLayerMonitor.cpp OscLayerMonitor.h
)
add_external_dependencies(playout)

target_include_directories(playout PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Caspar
)

target_link_libraries(playout PUBLIC
    caspar
    engine

    Qt::Core
    Qt::Network
)

# Fontes em UTF-8.
if (MSVC)
    target_compile_options(playout PRIVATE /utf-8)
endif()
