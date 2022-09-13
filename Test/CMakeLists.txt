cmake_minimum_required(VERSION 3.5)

project(MediaOrganizerTests LANGUAGES CXX)

enable_testing()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Test Gui REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test Gui REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(test_name
    DirProcessorTest
)
    add_executable(${test_name} ${test_name}/${test_name}.cpp)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Src/
    )
    target_link_libraries(${test_name} PRIVATE
        mediaorganizer-test
        Qt${QT_VERSION_MAJOR}::Test
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
target_compile_definitions(DirProcessorTest PRIVATE TEST_DATADIR="${CMAKE_CURRENT_SOURCE_DIR}/DirProcessorTest/test_data/")

add_custom_target(run-tests
    ${CMAKE_CTEST_COMMAND} . -V
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)



