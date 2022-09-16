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
        Src/main.cpp
        Qml/qml.qrc

        Src/ThumbsProviderInterface.h
        Src/ThumbsProvider.h Src/ThumbsProvider.cpp

        Src/ThumbsModel.h Src/ThumbsModel.cpp
        Src/FavoritFoldersModel.h Src/FavoritFoldersModel.cpp

        Src/DirProcessor.h Src/DirProcessor.cpp
        Src/ThumbsDiskCacher.h Src/ThumbsDiskCacher.cpp
        Src/DiskOperations.h Src/DiskOperations.cpp

        Src/ThumbGenerators/VideoThumbnailGeneratorInterface.h
        Src/ThumbGenerators/VideoThumbnailGeneratorWin32.h Src/ThumbGenerators/VideoThumbnailGeneratorWin32.cpp
        Src/ThumbGenerators/VideoThumbnailGeneratorFactory.h

        Src/ThumbData.h
        Src/FavoriteFolderData.h
        Src/ThumbsViewState.h Src/ThumbsViewState.cpp

        Src/Browsing/BrowsingBase.h Src/Browsing/BrowsingBase.cpp
        Src/Browsing/BrowsingHistory.h Src/Browsing/BrowsingHistory.cpp
        Src/Browsing/BrowsingBackForward.h Src/Browsing/BrowsingBackForward.cpp
        Src/Browsing/BrowsingParentFolder.h Src/Browsing/BrowsingParentFolder.cpp
        Src/Browsing/BrowsingPrevNextFolder.h Src/Browsing/BrowsingPrevNextFolder.cpp
        Src/Browsing/BrowsingNavigation.h

        Src/ComponentsMediator.h Src/ComponentsMediator.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MediaOrganizer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(MediaOrganizer
      ${PROJECT_SOURCES}
    )
endif()

target_compile_definitions(MediaOrganizer
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(MediaOrganizer
  PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Quick)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_import_qml_plugins(MediaOrganizer)
    qt_finalize_executable(MediaOrganizer)
endif()

target_link_libraries(MediaOrganizer PRIVATE Qt${QT_VERSION_MAJOR}::Concurrent Kernel32 mfplat mfreadwrite mfuuid propsys shell32 d2d1 winmm)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    add_library(mediaorganizer-test
            Src/DirProcessor.h Src/DirProcessor.cpp
    )
    target_link_libraries(mediaorganizer-test PUBLIC
        Qt${QT_VERSION_MAJOR}::Concurrent
        Qt${QT_VERSION_MAJOR}::Gui
    )
    add_subdirectory(test)
endif()
