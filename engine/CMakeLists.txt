cmake_minimum_required(VERSION 3.22.1)
project(videoEngineCodec VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/main/cpp/*.cpp)
file(GLOB SRC_FILES_COMPUTE ${CMAKE_SOURCE_DIR}/src/main/cpp/compute/*.cpp)
file(GLOB SRC_FILES_IMGUI ${CMAKE_SOURCE_DIR}/src/main/cpp/imgui/*.cpp)

add_library(${PROJECT_NAME} SHARED
        ${SRC_FILES}
        ${SRC_FILES_COMPUTE}
        ${SRC_FILES_IMGUI}
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c

)

find_library(ANDROID_LIB android)
find_library(ANDROID_LOG log)
find_library(VULKAN vulkan)
find_library(JNIGRAPHICS_LIB jnigraphics)
find_library(MEDIANDK_LIB mediandk)
find_library(AUDIONDK_LIB aaudio )

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/src/main/include
        ${ANDROID_NDK}/sources/android/native_app_glue
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${ANDROID_LIB}
        ${ANDROID_LOG}
        ${VULKAN}
        ${JNIGRAPHICS_LIB}
        ${MEDIANDK_LIB}
        ${AUDIONDK_LIB}
)