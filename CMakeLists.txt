cmake_minimum_required(VERSION 3.16)

project(appMediaPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavfilter libavformat libavutil libswresample libswscale)
pkg_check_modules(PULSE REQUIRED libpulse-simple libpulse)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(${PROJECT_NAME} 
    main.cpp
    src/core/MediaCore.cpp
    src/core/AudioEngine.cpp
)

qt_add_qml_module(${PROJECT_NAME}
    URI MediaPlayer
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/quickitem/VideoDisplay.h
        src/quickitem/VideoDisplay.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/quickitem
    ${FFMPEG_INCLUDE_DIRS}
    ${PULSE_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    ${FFMPEG_LIBRARIES}
    ${PULSE_LIBRARIES}
)

target_link_options(${PROJECT_NAME} PRIVATE ${FFMPEG_LDFLAGS} ${PULSE_LDFLAGS})
target_compile_options(${PROJECT_NAME} PRIVATE ${FFMPEG_CFLAGS} ${PULSE_CFLAGS})
