cmake_minimum_required(VERSION 3.7)

if (NOT ESP_PLATFORM)
  project(castspeaker LANGUAGES C)
endif ()

option(BUILD_TESTS "Build tests (in test/castspeaker_test)" ON)
option(PACKAGE_PACKED "Pack UDP package" OFF)

# Configure paths
if (NOT DEFINED CMAKE_INSTALL_BINDIR)
  SET(CMAKE_INSTALL_BINDIR bin CACHE
      PATH "Output directory for binary files")
endif ()

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
  SET(CMAKE_INSTALL_LIBDIR lib CACHE PATH "Output directory for libraries")
endif ()

if (NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
  SET(CMAKE_INSTALL_INCLUDEDIR include CACHE
      PATH "Output directory for header files")
endif ()

if (NOT ESP_PLATFORM)
  set(INCLUDE_DIRS
      "${CMAKE_SOURCE_DIR}"
      "${CMAKE_INSTALL_INCLUDEDIR}")
  include_directories(${INCLUDE_DIRS})

  configure_file(config.h.in config.h)

  if (BUILD_TESTS)
    add_subdirectory(test)
  endif ()


  if (PACKAGE_PACKED)
    add_definitions(-DPACKAGE_PACKED)
  endif ()

  include(GNUInstallDirs)

endif ()

############################# speaker start ####################################
set(SPEAKER_SOURCES
    "speaker/speaker_receiver.c"
    "speaker/speaker_multicast.c")
set(SPEAKER_HEADERS
    "speaker/speaker_receiver.h"
    "speaker/speaker_multicast.h")

set(SPEAKER_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
    common
    Threads::Threads)

if (NOT ESP_PLATFORM)
  set(SPEAKRE_EXE_NAME castspeaker)

  list(APPEND SPEAKER_SOURCES
      speaker/speaker.c
      speaker/output/raw.c
      )
  list(APPEND SPEAKER_HEADERS
      speaker/speaker.h
      speaker/output/raw.h
      )
  find_package(Threads REQUIRED)
  find_package(PkgConfig)

  # find pulseaudio
  option(PULSEAUDIO_ENABLE "Enable PulseAudio" OFF)
  if (PULSEAUDIO_ENABLE)
    pkg_check_modules(PULSEAUDIO libpulse-simple)
    if (PULSEAUDIO_FOUND)
      include_directories(${PULSEAUDIO_INCLUDE_DIRS})
      target_link_directories(${SPEAKRE_EXE_NAME} PUBLIC ${PULSEAUDIO_LIBRARY_DIRS})
      list(APPEND SERVER_LIBRARIES ${PULSEAUDIO_LIBRARIES})
      target_sources(${SPEAKRE_EXE_NAME} PRIVATE output/pulseaudio.c)
    else ()
      set(PULSEAUDIO_ENABLE OFF)
    endif ()
  endif ()

  # find ALSA
  option(ALSA_ENABLE "Enable ALSA" OFF)
  if (ALSA_ENABLE)
    pkg_check_modules(PC_ALSA alsa)
    if (PC_ALSA_FOUND)
      include_directories(${PC_ALSA_INCLUDEDIR})
      list(APPEND SERVER_LIBRARIES ${PC_ALSA_LIBRARIES})
      target_sources(${SPEAKRE_EXE_NAME} PRIVATE output/alsa.c)
    else ()
      set(ALSA_ENABLE OFF)
    endif ()
  endif ()


  # find libpcap
  option(PCAP_ENABLE "Enable PCAP" OFF)
  if (PCAP_ENABLE)
    pkg_check_modules(PC_PCAP libpcap)
    if (PC_PCAP_FOUND)
      include_directories(${PC_PCAP_INCLUDEDIR})
      list(APPEND SERVER_LIBRARIES ${PC_PCAP_LIBRARIES})
      target_sources(${SPEAKRE_EXE_NAME} PRIVATE input/pcap.c)
    else ()
      set(PCAP_ENABLE OFF)
    endif ()
  endif ()

  add_subdirectory(common)

  add_executable(${SPEAKRE_EXE_NAME} ${SPEAKER_SOURCES} ${SPEAKER_HEADERS})

  target_link_libraries(${SPEAKRE_EXE_NAME} ${SPEAKER_LIBRARIES})

  target_include_directories(${SPEAKRE_EXE_NAME} PUBLIC "${PROJECT_BINARY_DIR}")

  include(GNUInstallDirs)
  install(TARGETS ${SPEAKRE_EXE_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif ()
########################## speaker end #################################

if (ESP32_PLATFORM)
  set(COMPONENT_ADD_INCLUDEDIRS ${SPEAKER_HEADERS})

  set(COMPONENT_SRCS ${SPEAKER_SOURCES})

  register_component()
endif ()
