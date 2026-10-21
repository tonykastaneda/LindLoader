cmake_minimum_required(VERSION 3.21)
project(LindLoaderUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Link third-party libraries statically too, so LindLoader.exe is one file.
if(MINGW)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Svg)
# HarfBuzz's CMake package always points at its DLL; use the static library instead.
if(MINGW AND TARGET harfbuzz::harfbuzz)
    find_library(HARFBUZZ_STATIC NAMES libharfbuzz.a REQUIRED)
    set_target_properties(harfbuzz::harfbuzz PROPERTIES
        IMPORTED_LOCATION "${HARFBUZZ_STATIC}" IMPORTED_IMPLIB "${HARFBUZZ_STATIC}")
endif()

qt_add_executable(LindLoader WIN32
    main.cpp
    MainWindow.cpp MainWindow.h
    GameLibrary.cpp GameLibrary.h
    GamePropertiesDialog.cpp GamePropertiesDialog.h
    IniFile.cpp IniFile.h
    resources.qrc
)

target_link_libraries(LindLoader PRIVATE Qt6::Widgets Qt6::Concurrent Qt6::Svg)
qt_import_plugins(LindLoader
    INCLUDE Qt6::QSvgPlugin Qt6::QSvgIconPlugin
    EXCLUDE_BY_TYPE imageformats platforminputcontexts
)
if(MINGW)
    target_link_options(LindLoader PRIVATE -static -static-libgcc -static-libstdc++ -s)
    # Dependencies of the static FreeType and OpenSSL that Qt's static build doesn't list itself;
    # they have to come last on the link line.
    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lbz2 -lbrotlidec -lbrotlicommon -lcrypt32 -lws2_32 -lusp10 -lrpcrt4 -ldwrite -lglib-2.0 -lintl -liconv -lpcre2-8 -lgraphite2 -lole32 -lwinmm -lshlwapi -luuid -latomic")
endif()
