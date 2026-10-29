# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

add_subdirectory(config)
add_subdirectory(core)

create_test(
  NAME ComputerLayoutTests
  DEPENDS gui
  SOURCE ComputerLayoutTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
# the dialog's headers include each other relative to the gui library
target_include_directories(ComputerLayoutTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")

create_test(
  NAME LayoutGeometryTests
  DEPENDS gui
  SOURCE LayoutGeometryTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME LoggerTests
  DEPENDS gui
  SOURCE LoggerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
