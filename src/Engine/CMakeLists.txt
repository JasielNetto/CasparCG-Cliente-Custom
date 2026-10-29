# Núcleo de execução — C++17 puro, sem Qt. Pode ser compilado isoladamente para testes
# (tools/tests/engine) e, futuramente, ligado ao Core/Widgets.
cmake_minimum_required(VERSION 3.16)
project(Engine LANGUAGES CXX)

add_library(engine STATIC
    Resource.cpp
    Conjunto.cpp
    Sequence.cpp
)
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(engine PUBLIC cxx_std_17)
