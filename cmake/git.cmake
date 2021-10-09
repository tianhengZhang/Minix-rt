# SPDX-License-Identifier: Apache-2.0

#.rst:
# git.cmake
# ---------
# If the user didn't already define BUILD_VERSION then try to initialize
# it with the output of "git describe". Warn but don't error if
# everything fails and leave BUILD_VERSION undefined.
#
# See also: independent and more static ``KERNEL_VERSION_*`` in
# ``version.cmake`` and ``kernel_version.h``


# https://cmake.org/cmake/help/latest/module/FindGit.html
find_package(Git QUIET)
if(NOT DEFINED BUILD_VERSION AND GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --abbrev=12 --always
    WORKING_DIRECTORY                ${MINIX_RT_BASE}
    OUTPUT_VARIABLE                  BUILD_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE                   stderr
    RESULT_VARIABLE                  return_code
  )
  if(return_code)
    message(STATUS "git describe failed: ${stderr}")
  elseif(NOT "${stderr}" STREQUAL "")
    message(STATUS "git describe warned: ${stderr}")
  endif()
endif()
