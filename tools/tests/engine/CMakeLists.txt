# Testes do núcleo (sem Qt):
#   cmake -S tools/tests/engine -B build-engine-tests && cmake --build build-engine-tests && ctest --test-dir build-engine-tests --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(EngineTests LANGUAGES CXX)

option(ENGINE_SANITIZERS "Compila com AddressSanitizer/UBSan (GCC/Clang)" ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../src/Engine ${CMAKE_CURRENT_BINARY_DIR}/engine)

add_executable(engine_test engine_test.cpp)
target_link_libraries(engine_test PRIVATE engine)

if (NOT MSVC)
    target_compile_options(engine PRIVATE -Wall -Wextra -Werror)
    target_compile_options(engine_test PRIVATE -Wall -Wextra -Werror)
    if (ENGINE_SANITIZERS)
        foreach(target engine engine_test)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
else()
    target_compile_options(engine PRIVATE /W4 /WX /utf-8)
    target_compile_options(engine_test PRIVATE /W4 /WX /utf-8)
endif()

enable_testing()
add_test(NAME engine_test COMMAND engine_test)
