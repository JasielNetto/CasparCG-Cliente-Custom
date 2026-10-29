# Núcleo de execução — C++17 puro, sem Qt. Usado pelo Core e compilado isoladamente nos testes
# (tools/tests/engine).
cmake_minimum_required(VERSION 3.16)
project(Engine LANGUAGES CXX)

add_library(engine STATIC
    AmcpFormat.cpp AmcpFormat.h
    ClipEndDetector.h
    Conjunto.cpp Conjunto.h
    Item.h
    PlayoutPort.h
    Resource.cpp Resource.h
    Sequence.cpp Sequence.h
)
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(engine PUBLIC cxx_std_17)
set_target_properties(engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Fontes em UTF-8 (textos em português).
if (MSVC)
    target_compile_options(engine PRIVATE /utf-8)
endif()
