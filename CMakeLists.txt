cmake_minimum_required(VERSION 3.1)

project(qtmir)

set(QTMIR_VERSION_MAJOR 0)
set(QTMIR_VERSION_MINOR 2)
set(QTMIR_VERSION_PATCH 0)

if(${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
   message(FATAL_ERROR "In-tree build attempt detected, aborting. Set your build dir outside your source dir, delete CMakeCache.txt and CMakeFiles/ from source root and try again.")
endif()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package (ECM 1.7.0 QUIET NO_MODULE)
if (ECM_FOUND)
    # Provides us with -DECM_ENABLE_SANITIZERS='X'
    # Where X can be address, thread, memory, leak, undefined
    include("${ECM_MODULE_DIR}/ECMEnableSanitizers.cmake")
endif()

# add custom cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -fno-strict-aliasing -Werror -Wextra")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

set (CMAKE_CXX_STANDARD 14)

if ("${CMAKE_CXX_COMPILER}" MATCHES "clang")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-return-type-c-linkage -Wno-mismatched-tags -Wno-inconsistent-missing-override -Wno-implicit-exception-spec-mismatch -Wno-unknown-pragmas")
endif()


find_package(CoverageReport)
#####################################################################
# Enable code coverage calculation with gcov/gcovr/lcov
# Usage:
#  * Switch build type to coverage (use ccmake or cmake-gui)
#  * Invoke make, make test, make coverage (or ninja if you use that backend)
#  * Find html report in subdir coveragereport
#  * Find xml report feasible for jenkins in coverage.xml
#####################################################################
if(cmake_build_type_lower MATCHES coverage)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage" )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage" )
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} --coverage" )
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage" )
    ENABLE_COVERAGE_REPORT(TARGETS ${SHELL_APP} FILTER /usr/include ${CMAKE_SOURCE_DIR}/tests/* ${CMAKE_BINARY_DIR}/*)
endif()

# Make sure we have all the needed symbols
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,defs")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-z,defs")

# Static C++ checks
add_custom_target(cppcheck COMMAND cppcheck --enable=all -q --error-exitcode=2
                                       ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)

include(FindPkgConfig)
find_package(Qt5Core 5.4 REQUIRED)
find_package(Qt5DBus 5.4 REQUIRED)
find_package(Qt5Gui 5.4 REQUIRED)
find_package(Qt5Qml 5.4 REQUIRED)
find_package(Qt5Quick 5.4 REQUIRED)
find_package(Qt5Sensors 5.4 REQUIRED)
find_package(Qt5Test 5.4 REQUIRED)

find_package(Threads REQUIRED)

pkg_check_modules(MIRSERVER mirserver>=0.26 REQUIRED)
pkg_check_modules(MIRCLIENT mirclient>=0.26 REQUIRED)
pkg_check_modules(MIRRENDERERGLDEV mir-renderer-gl-dev>=0.26 REQUIRED)
pkg_check_modules(MIRAL miral>=1.1.0 REQUIRED)

pkg_check_modules(XKBCOMMON xkbcommon REQUIRED)
pkg_check_modules(GLIB glib-2.0 REQUIRED)
pkg_check_modules(PROCESS_CPP process-cpp REQUIRED)
pkg_check_modules(UBUNTU_APP_LAUNCH ubuntu-app-launch-3 REQUIRED)
pkg_check_modules(URL_DISPATCHER url-dispatcher-1)
pkg_check_modules(EGL egl)
pkg_check_modules(GIO gio-2.0)
pkg_check_modules(GIO_UNIX gio-unix-2.0)
pkg_check_modules(LTTNG lttng-ust)
pkg_check_modules(GSETTINGS_QT REQUIRED gsettings-qt)
pkg_check_modules(QTDBUSTEST libqtdbustest-1 REQUIRED)
pkg_check_modules(QTDBUSMOCK libqtdbusmock-1 REQUIRED)
pkg_check_modules(APPLICATION_API REQUIRED unity-shell-application=27)
pkg_check_modules(CGMANAGER libcgmanager REQUIRED)
pkg_check_modules(CONTENT_HUB libcontent-hub>=0.2 REQUIRED)
pkg_check_modules(VALGRIND valgrind REQUIRED)

include_directories(SYSTEM ${APPLICATION_API_INCLUDE_DIRS})


# Use the fast string builder
add_definitions(-DQT_USE_QSTRINGBUILDER)

# We expect this to be set via debian/rules for GLES builds
if ("${USE_OPENGLES}" STREQUAL 1)
    message(STATUS "Qt5 determined to be compiled with GLES support")
    pkg_check_modules(GLESv2 glesv2)
    add_definitions(-DQT_USING_GLES)
    include_directories (SYSTEM ${GLESv2_INCLUDE_DIRS})
    set (GL_LIBRARIES "${GLESv2_LIBRARIES}")
# Because qt/gl and qt/gles are not parallel installable we have a difficulty
# building qtmir-desktop on armhf. We would like to link against opengl and bind the
# opengl API but Qt is already linked against the ES APIs. So in this setup we link against OpenGLES
# to accomodate Qt but bind the OpenGL API anyway. This is technically broken though
# I suspect Mesa may allow it ~racarr.
elseif("${USE_OPENGL_BUT_LINK_AGAINST_OPENGLES}")
    message(STATUS "Linking against OpenGL ES but binding OpenGL API")
    pkg_check_modules(GLESv2 glesv2)
    add_definitions(-DQT_USING_GL)
    include_directories (SYSTEM ${GLESv2_INCLUDE_DIRS})
    set (GL_LIBRARIES "${GLESv2_LIBRARIES}")
else()
    message(STATUS "Qt5 determined to be compiled with OpenGL support")
    pkg_check_modules(GL gl)
    add_definitions(-DQT_USING_OPENGL)
    include_directories (SYSTEM ${GL_INCLUDE_DIRS})
endif()

# Standard install paths
include(GNUInstallDirs)


# Determine QPA plugin install path
# FIXME(greyback) there must be a better way than calling a private macro to set a property.
# I'm manually calling a macro from Qt5GuiConfig.cmake, but I don't understand why it isn't being called.
_populate_Gui_plugin_properties("Gui" "PLATFORMS" "platforms")
get_target_property(Qt5Gui_QPA_Plugin_Path Qt5::Gui IMPORTED_LOCATION_PLATFORMS)

# Set QML module install path
set(QML_MODULE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/qt5/qml")
set(QTMIR_DATA_DIR ${CMAKE_INSTALL_DATADIR}/qtmir)


# Customisations for the build type
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_lower)

if(cmake_build_type_lower MATCHES relwithdebinfo) # workaround for http://public.kitware.com/Bug/view.php?id=14696
    add_definitions(-DQT_NO_DEBUG)
endif()
if ("${CMAKE_BUILD_TYPE}" STREQUAL "release" OR "${CMAKE_BUILD_TYPE}" STREQUAL "relwithdebinfo")
    option(Werror "Treat warnings as errors" ON)
else()
    option(Werror "Treat warnings as errors" OFF)
endif()

if (Werror)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()


# Mir uses boost, which does not like Qt defining macros named "signals" and "slots"
add_definitions(-DQT_NO_KEYWORDS)

# Tests
if (NO_TESTS)
    message(STATUS "Tests disabled")
else()
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

# add subdirectories to build
add_subdirectory(src)
add_subdirectory(demos)
add_subdirectory(benchmarks)
