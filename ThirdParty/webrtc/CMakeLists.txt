cmake_minimum_required (VERSION 3.3) # IN_LIST

project(webrtc)

# Default to debug build
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

# Search path for CMake include files.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Opus REQUIRED)
find_package(JPEG REQUIRED)
find_package(LibVpx REQUIRED)

# Collect all sources not belonging to tests or specific architectures.
macro(add_sources SRCS BASE_PATH)
set(EXCLUDES ".*unittest.*" ".*test.*" ".*tools.*" ".*win.*" ".*mac.*" ".*ios.*" ".*android.*" ".*arm.*" ".*mips.*" ".*neon.*" ".*aec_dump.*" ".*openmax.*" ".*H264.*")
file(GLOB_RECURSE UNFILTERED_SRCS "${BASE_PATH}/*.c" "${BASE_PATH}/*.cc")
foreach(SRC ${UNFILTERED_SRCS})
set(IS_EXCLUDED FALSE)
foreach (EXCLUDE ${EXCLUDES})
if ("${SRC}" MATCHES "${EXCLUDE}")
set(IS_EXCLUDED TRUE)
endif()
endforeach()
if (NOT ${IS_EXCLUDED})
list(APPEND ${SRCS} ${SRC})
endif()
endforeach()
endmacro()

# Collect all paths containing header files.
macro(add_includes INCLUDE_DIRS BASE_PATH)
set(EXCLUDES ".*unittest.*" ".*test.*" ".*tools.*" ".*win.*" ".*mac.*" ".*ios.*" ".*android.*" ".*arm.*" ".*mips.*" ".*neon.*" ".*aec_dump.*" ".*openmax.*" ".*H264.*")
file(GLOB_RECURSE INCLUDES "${BASE_PATH}/*.h" "${BASE_PATH}/*.hpp")
foreach (INCLUDE ${INCLUDES})
set(IS_EXCLUDED FALSE)
foreach (EXCLUDE ${EXCLUDES})
if (${INCLUDE} MATCHES ${EXCLUDE})
set(IS_EXCLUDED TRUE)
endif()
endforeach()
if (NOT ${IS_EXCLUDED})
get_filename_component(INCLUDE_DIR ${INCLUDE} DIRECTORY)
if (NOT "${INCLUDE_DIR}" IN_LIST ${INCLUDE_DIRS})
list(APPEND ${INCLUDE_DIRS} ${INCLUDE_DIR})
endif()
endif()
endforeach()
endmacro()

add_sources(common_audio_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/common_audio")
add_includes(common_audio_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/common_audio")
list(APPEND common_audio_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/common_audio/signal_processing/get_hanning_window.c")

add_sources(common_audio_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/common_video")
add_includes(common_audio_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/common_video")

add_sources(modules_audio_coding_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_coding")
add_includes(modules_audio_coding_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_coding")
list(APPEND modules_audio_coding_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_coding/codecs/ilbc/window32_w32.c")

add_sources(modules_audio_conference_mixer_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_conference_mixer")
add_includes(modules_audio_conference_mixer_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_conference_mixer")

add_sources(modules_audio_device_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_device")
add_includes(modules_audio_device_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_device")
list(APPEND modules_audio_device_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_device/linux/pulseaudiosymboltable_linux.cc")

add_sources(modules_audio_processing_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_processing")
add_includes(modules_audio_processing_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_processing")

add_sources(modules_bitrate_controller_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/bitrate_controller")
add_includes(modules_bitrate_controller_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/bitrate_controller")

add_sources(modules_media_file_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/media_file")
add_includes(modules_media_file_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/media_file")

add_sources(modules_pacing_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/pacing")
add_includes(modules_pacing_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/pacing")

add_sources(modules_remote_bitrate_estimator_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/remote_bitrate_estimator")
add_includes(modules_remote_bitrate_estimator_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/remote_bitrate_estimator")

add_sources(modules_rtp_rtcp_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/rtp_rtcp")
add_includes(modules_rtp_rtcp_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/rtp_rtcp")

add_sources(modules_udp_transport_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/udp_transport")
add_includes(modules_udp_transport_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/udp_transport")

add_sources(modules_utility_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/utility")
add_includes(modules_utility_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/utility")

add_sources(modules_video_capture_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_capture")
add_includes(modules_video_capture_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_capture")
list(REMOVE_ITEM modules_video_capture_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_capture/external/device_info_external.cc")
list(REMOVE_ITEM modules_video_capture_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_capture/external/video_capture_external.cc")

add_sources(modules_video_coding_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_coding")
add_includes(modules_video_coding_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_coding")

add_sources(modules_video_processing_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_processing")
add_includes(modules_video_processing_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_processing")

add_sources(modules_video_render_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_render")
add_includes(modules_video_render_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules/video_render")

add_sources(video_engine_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/video_engine")
add_includes(video_engine_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/video_engine")

add_sources(voice_engine_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/voice_engine")
add_includes(voice_engine_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/voice_engine")

add_sources(system_wrappers_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers")
add_includes(system_wrappers_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers")
list(REMOVE_ITEM system_wrappers_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers/source/cpu_no_op.cc")
list(REMOVE_ITEM system_wrappers_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers/source/data_log_no_op.cc")
list(REMOVE_ITEM system_wrappers_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers/source/logging_no_op.cc")
list(REMOVE_ITEM system_wrappers_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers/source/trace_impl_no_op.cc")

add_library(${PROJECT_NAME} SHARED
	${common_audio_SRCS} ${common_video_SRCS}
	${video_engine_SRCS} ${voice_engine_SRCS} ${modules_audio_coding_SRCS} ${modules_audio_device_SRCS}
	${modules_audio_conference_mixer_SRCS} ${modules_audio_processing_SRCS} ${modules_bitrate_controller_SRCS}
	${modules_media_file_SRCS} ${modules_pacing_SRCS} ${modules_rtp_rtcp_SRCS} ${modules_remote_bitrate_estimator_SRCS}
	${modules_udp_transport_SRCS} ${modules_utility_SRCS}
	${modules_video_capture_SRCS} ${modules_video_coding_SRCS} ${modules_video_processing_SRCS} ${modules_video_render_SRCS}
	${system_wrappers_SRCS}
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/simple_command_line_parser.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/test/testsupport/fileutils.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/test/testsupport/metrics/video_metrics.cc")
target_include_directories(${PROJECT_NAME} PRIVATE "${gtest_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${gmock_SOURCE_DIR}/include")
target_compile_definitions(${PROJECT_NAME} PRIVATE WEBRTC_LINUX WEBRTC_SVNREVISION="git" WEBRTC_NS_FLOAT)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../libyuv/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/modules/interface")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")
target_include_directories(${PROJECT_NAME} PRIVATE ${OPUS_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBVPX_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${common_audio_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${common_video_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_audio_coding_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_audio_conference_mixer_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_audio_device_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_audio_processing_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_bitrate_controller_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_media_file_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_pacing_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_remote_bitrate_estimator_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_rtp_rtcp_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_udp_transport_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_utility_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_video_capture_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_video_coding_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_video_processing_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${modules_video_render_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${video_engine_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${voice_engine_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${system_wrappers_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${OPUS_LIBRARIES} yuv_shared ${JPEG_LIBRARIES} ${LIBVPX_LIBRARIES})

