cmake_minimum_required(VERSION 3.16.3)

project(AudioRecording VERSION 1.0.0)

set(SOURCE_DIR src)
set(HEADERS_DIR ${SOURCE_DIR}/headers)

aux_source_directory(${SOURCE_DIR} SRC_FILES)

set(TARGET_INC_DIRS ${HEADERS_DIR})

set(TARGET_LINK_LIBS
    asound
)

add_executable(audiorecording ${SRC_FILES})

target_include_directories(audiorecording PRIVATE ${TARGET_INC_DIRS})

target_link_libraries(audiorecording ${TARGET_LINK_LIBS})
