cmake_minimum_required(VERSION 3.14)

project(MediaOrganizer VERSION 0.1 LANGUAGES CXX)

if(${MSVC_TOOLSET_VERSION} LESS 140)
    message(FATAL_ERROR "MSVC 2015 or greater toolset required")
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Quick REQUIRED Concurrent)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Quick REQUIRED Concurrent)

set(PROJECT_SOURCES
        main.cpp
        qml.qrc

        ThumbsProvider.h ThumbsProvider.cpp

        ThumbsModel.h ThumbsModel.cpp
        FavoritFoldersModel.h FavoritFoldersModel.cpp

        DirProcessor.h DirProcessor.cpp
        BrowsingHistory.h BrowsingHistory.cpp
        ThumbsDiskCacher.h ThumbsDiskCacher.cpp

        ThumbGenerators/VideoThumbnailGeneratorWin32.h ThumbGenerators/VideoThumbnailGeneratorWin32.cpp

        ThumbData.h
        FavoriteFolderData.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MediaOrganizer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET MediaOrganizer APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(MediaOrganizer SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(MediaOrganizer
          ${PROJECT_SOURCES}
        )
    endif()
endif()

target_compile_definitions(MediaOrganizer
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(MediaOrganizer
  PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick)

set_target_properties(MediaOrganizer PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_import_qml_plugins(MediaOrganizer)
    qt_finalize_executable(MediaOrganizer)
endif()

target_link_libraries(MediaOrganizer PRIVATE Qt5::Concurrent Kernel32 mfplat mfreadwrite mfuuid propsys shell32 d2d1 winmm)
