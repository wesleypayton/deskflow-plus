# SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
# SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: (C) 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

if(UNIX AND NOT APPLE)
  find_program(HELP2MAN help2man)
  if(NOT HELP2MAN)
    message(STATUS "Man page tool (help2man) not found, man pages will not be generated")
  endif()
endif()

function(generate_app_man TARGET NAME)
  if(HELP2MAN)
    add_custom_command(
      TARGET ${target} POST_BUILD
        COMMAND QT_QPA_PLATFORM=minimal PATH=$<TARGET_FILE_DIR:${target}>:${PATH} ${HELP2MAN}
          --name ${NAME}
          --include ${CMAKE_SOURCE_DIR}/src/apps/res/manpage.txt
          --no-info
          ${target}
          -o $<TARGET_FILE_DIR:${target}>/${target}.1
    )
    install(
      FILES $<TARGET_FILE_DIR:${target}>/${target}.1
      DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
  endif()
endfunction()

set(WIN32_PRE_EXCLUDE_REGEXES
  "api-ms-win-.*"
  "ext-ms-.*"
  "^hvsifiletrust\\.dll$"
  "^icu.*\\.dll$"
)

# match any capitalisation, so Windows' own DLLs are never packaged however their path is spelled
set(WIN32_POST_EXCLUDE_REGEXES ".*[Ss][Yy][Ss][Tt][Ee][Mm]32.*" ".*[Ss][Yy][Ss][Nn][Aa][Tt][Ii][Vv][Ee].*")

set(WIN32_RUNTIME_DEPENDENCY_SET_ARGS)
if(WIN32)
  set(_runtime_dirs)
  # Runtime DLL fallback for layouts like vcpkg
  if(DEFINED OPENSSL_EXE_DIR AND EXISTS "${OPENSSL_EXE_DIR}")
    list(APPEND _runtime_dirs "${OPENSSL_EXE_DIR}")
  elseif(DEFINED OPENSSL_ROOT_DIR AND EXISTS "${OPENSSL_ROOT_DIR}/bin")
    list(APPEND _runtime_dirs "${OPENSSL_ROOT_DIR}/bin")
  endif()
  # the Visual C++ runtime is found in the system folder, and then left out by the exclusion above;
  # without this it can't be resolved at all and packaging fails. a 32-bit CMake (such as the one
  # Visual Studio ships) sees the 32-bit SysWOW64 under the name System32, so look in Sysnative too
  file(TO_CMAKE_PATH "$ENV{SystemRoot}" _windows_dir)
  list(APPEND _runtime_dirs "${_windows_dir}/Sysnative" "${_windows_dir}/System32")
  set(WIN32_RUNTIME_DEPENDENCY_SET_ARGS DIRECTORIES ${_runtime_dirs})
endif()

add_subdirectory(deskflow-core)
add_subdirectory(deskflow-daemon) #Only used on windows
add_subdirectory(deskflow-gui)
