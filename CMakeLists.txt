cmake_minimum_required(VERSION 3.21)

project(ParallelFileAnalyzer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(ParallelFileAnalyzer
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/FileAnalyzer.h
    src/FileAnalyzer.cpp
    src/FileAnalysisResult.h
    src/AnalysisWorker.h
    src/AnalysisWorker.cpp
)

target_link_libraries(ParallelFileAnalyzer PRIVATE Qt6::Core Qt6::Widgets)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(FileAnalyzerTests
        tests/FileAnalyzerTests.cpp
        src/FileAnalyzer.h
        src/FileAnalyzer.cpp
        src/FileAnalysisResult.h
    )
    target_link_libraries(FileAnalyzerTests PRIVATE Qt6::Core)
    target_include_directories(FileAnalyzerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME FileAnalyzerTests COMMAND FileAnalyzerTests)
endif()
