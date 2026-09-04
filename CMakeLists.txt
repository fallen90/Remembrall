cmake_minimum_required(VERSION 3.24)
project(remembrall VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Keep the default at C++17 so sherpa-onnx deps (openfst / ssentencepiece) compile.
# Remembrall targets opt into C++20 via target_compile_features below.

if(NOT WIN32)
  message(FATAL_ERROR "Remembrall targets Windows 10 build 20348+ only (WASAPI process loopback).")
endif()

option(LTA_BUILD_TESTS "Build unit tests" ON)
option(LTA_BUILD_CAPTURE_POC "Build capture POC console harness" ON)
option(LTA_FETCH_SHERPA "Fetch and build sherpa-onnx" ON)
set(LTA_GITHUB_REPO "fallen90/Remembrall" CACHE STRING "GitHub owner/repo for auto-update")

# Embed semver into the binary for updater comparisons.
set(LTA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(LTA_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(LTA_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(LTA_VERSION_STRING "${PROJECT_VERSION}")

add_compile_definitions(
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _CRT_SECURE_NO_WARNINGS
  LTA_VERSION_MAJOR=${LTA_VERSION_MAJOR}
  LTA_VERSION_MINOR=${LTA_VERSION_MINOR}
  LTA_VERSION_PATCH=${LTA_VERSION_PATCH}
)
# String macros need escaped quotes for the preprocessor.
add_compile_definitions(
  "LTA_VERSION_STRING=\"${LTA_VERSION_STRING}\""
  "LTA_GITHUB_REPO=\"${LTA_GITHUB_REPO}\""
)

include(FetchContent)

# Microsoft WIL (COM / RAII helpers)
set(WIL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WIL_BUILD_PACKAGING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  wil
  GIT_REPOSITORY https://github.com/microsoft/wil.git
  GIT_TAG v1.0.240803.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(wil)

if(LTA_FETCH_SHERPA)
  set(SHERPA_ONNX_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_ENABLE_CHECK OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_ENABLE_PORTAUDIO OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_ENABLE_WEBSOCKET OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_ENABLE_TTS OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_ENABLE_BINARY OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_BUILD_C_API_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SHERPA_ONNX_ENABLE_C_API ON CACHE BOOL "" FORCE)
  # Shared libs avoid sherpa's forced /MT static-CRT clash with our /MD app.
  set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

  FetchContent_Declare(
    sherpa-onnx
    GIT_REPOSITORY https://github.com/k2-fsa/sherpa-onnx.git
    GIT_TAG v1.10.46
    GIT_SHALLOW TRUE
  )
  FetchContent_GetProperties(sherpa-onnx)
  if(NOT sherpa-onnx_POPULATED)
    FetchContent_Populate(sherpa-onnx)
    # sherpa-onnx CMakeLists appends ${CMAKE_SOURCE_DIR}/cmake, which points at
    # *this* project when embedded. Prepend the real sherpa cmake dirs first.
    list(PREPEND CMAKE_MODULE_PATH
      "${sherpa-onnx_SOURCE_DIR}/cmake"
      "${sherpa-onnx_SOURCE_DIR}/cmake/Modules")
    add_subdirectory(${sherpa-onnx_SOURCE_DIR} ${sherpa-onnx_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
endif()

set(LTA_COMMON_SOURCES
  src/config/configuration.cpp
  src/system/logger.cpp
  src/system/diagnostics.cpp
  src/system/memory_monitor.cpp
  src/system/process_monitor.cpp
  src/system/version.cpp
  src/system/http_client.cpp
  src/system/auto_updater.cpp
  src/audio/audio_format.cpp
  src/audio/ring_buffer.cpp
  src/audio/resampler.cpp
  src/audio/discord_locator.cpp
  src/audio/process_loopback.cpp
  src/audio/device_change_monitor.cpp
  src/asr/zipformer_recognizer.cpp
  src/transcript/transcript_manager.cpp
  src/app/application.cpp
  src/app/state_machine.cpp
)

set(LTA_UI_SOURCES
  src/ui/main_window.cpp
  src/ui/transcript_view.cpp
  src/ui/status_bar.cpp
  src/ui/diagnostics_view.cpp
)

add_library(lta_core STATIC ${LTA_COMMON_SOURCES})
target_compile_features(lta_core PUBLIC cxx_std_20)
target_include_directories(lta_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# WIL may export WIL::WIL or just WIL depending on version.
if(TARGET WIL::WIL)
  target_link_libraries(lta_core PUBLIC WIL::WIL)
elseif(TARGET WIL)
  target_link_libraries(lta_core PUBLIC WIL)
else()
  target_include_directories(lta_core PUBLIC ${wil_SOURCE_DIR}/include)
endif()

target_link_libraries(lta_core PUBLIC
  ole32
  uuid
  mmdevapi
  avrt
  propsys
  psapi
  winmm
  winhttp
  shell32
)

if(LTA_FETCH_SHERPA)
  if(TARGET sherpa-onnx-cxx-api)
    target_link_libraries(lta_core PUBLIC sherpa-onnx-cxx-api)
  elseif(TARGET sherpa-onnx-c-api)
    target_link_libraries(lta_core PUBLIC sherpa-onnx-c-api)
  else()
    message(FATAL_ERROR "sherpa-onnx C++ API target not found after FetchContent")
  endif()
  target_compile_definitions(lta_core PUBLIC LTA_HAS_SHERPA=1)
endif()

add_executable(Remembrall WIN32
  src/main.cpp
  src/app/remembrall.rc
  ${LTA_UI_SOURCES}
)
target_compile_features(Remembrall PRIVATE cxx_std_20)
target_include_directories(Remembrall PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app
)
target_link_libraries(Remembrall PRIVATE
  lta_core
  d2d1
  dwrite
  windowscodecs
  shell32
  comctl32
  dwmapi
)
target_compile_definitions(Remembrall PRIVATE LTA_APP=1)

# Copy sherpa/onnxruntime DLLs next to the exe for portable runs.
if(LTA_FETCH_SHERPA AND WIN32)
  add_custom_command(TARGET Remembrall POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
      "$<TARGET_FILE_DIR:Remembrall>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_BINARY_DIR}/bin"
      "$<TARGET_FILE_DIR:Remembrall>"
    VERBATIM
  )
endif()

# Ensure the resource compiler finds remembrall.ico next to the .rc file.
set_source_files_properties(src/app/remembrall.rc PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/app
)

if(LTA_BUILD_CAPTURE_POC)
  add_executable(remembrall-capture-poc
    src/tools/capture_poc_main.cpp
  )
  target_compile_features(remembrall-capture-poc PRIVATE cxx_std_20)
  target_link_libraries(remembrall-capture-poc PRIVATE lta_core)
endif()

if(LTA_BUILD_TESTS)
  enable_testing()
  add_executable(lta_tests
    tests/test_ring_buffer.cpp
    tests/test_transcript_manager.cpp
    tests/test_audio_format.cpp
    tests/test_version.cpp
    tests/test_main.cpp
  )
  target_compile_features(lta_tests PRIVATE cxx_std_20)
  target_link_libraries(lta_tests PRIVATE lta_core)
  add_test(NAME lta_tests COMMAND lta_tests)
endif()

install(TARGETS Remembrall RUNTIME DESTINATION bin)
