cmake_minimum_required(VERSION 3.0)
project(jni)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(../../http-parser ${CMAKE_CURRENT_BINARY_DIR}/http-parser)

find_package(JNI REQUIRED)
include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})

set(SOURCE_FILES
        com_adguard_http_parser_HttpMessage.h
        com_adguard_http_parser_HttpMessage_HttpHeaderField.h
        com_adguard_http_parser_NativeParser.h
        com_adguard_http_parser_NativeParser_Callbacks.h
        callbacks.h
        callbacks.cpp
        parser.cpp
        com_adguard_http_parser_NativeLogger.h
        logger.cpp)


add_library(httpparser-jni SHARED ${SOURCE_FILES})
target_link_libraries(httpparser-jni httpparser-c)
