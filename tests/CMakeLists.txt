find_package(GTest CONFIG REQUIRED)

add_executable(remc1-tests
    test_main.cpp
    test_layout.cpp
    test_layout_names.cpp
    test_platform.cpp
    test_render.cpp
    test_data.cpp
    support/test_support.cpp
)
target_include_directories(remc1-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(remc1-tests PRIVATE REMC1_GOLDENS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/goldens")
target_link_libraries(remc1-tests PRIVATE remc1_engine GTest::gtest)
if(MSVC)
    target_compile_options(remc1-tests PRIVATE /wd4200 /wd4103)
endif()

include(GoogleTest)
# Tests are cheap to enumerate; PRE_TEST avoids running the exe at build time.
gtest_discover_tests(remc1-tests DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# Tier 3: replay tests (need REMC1_DATA_DIR; otherwise reported as skipped)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    file(GLOB REMC1_REPLAYS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/replays/*.txt)
    foreach(replay ${REMC1_REPLAYS})
        get_filename_component(replay_name ${replay} NAME_WE)
        add_test(NAME replay.${replay_name}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/replay_test.py
                    --exe $<TARGET_FILE:remc1> --replay ${replay})
        set_tests_properties(replay.${replay_name} PROPERTIES SKIP_RETURN_CODE 77 LABELS replay TIMEOUT 1200)
    endforeach()
endif()
