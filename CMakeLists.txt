cmake_minimum_required(VERSION 3.16)

include( ${CMAKE_CURRENT_SOURCE_DIR}/cmake/QrtrViewers-version.cmake )
project( QrtrViewers VERSION ${QrtrViewers_VERSION} LANGUAGES CXX )

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MACOSX_RPATH 1)
list( APPEND CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")



set(CMAKE_AUTOMOC ON)

find_package(Qt6       REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
find_package(OpenGL    REQUIRED)
find_package(Coin      REQUIRED)
find_package(Quarter   REQUIRED)
# -----------------------------------------------------------------------------
# Coin
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# The reusable viewer library
# -----------------------------------------------------------------------------
add_library(QrtrViewers SHARED
    QrtrViewers/QuarterExaminerViewer.h
    src/QuarterExaminerViewer.cpp
    QrtrViewers/ThumbWheel.h
    src/ThumbWheel.cpp
)
target_include_directories(QrtrViewers PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


target_link_libraries(QrtrViewers PUBLIC
    Quarter::Quarter
    OpenGL::GL)

set_target_properties( QrtrViewers PROPERTIES
   VERSION ${PROJECT_VERSION}
   SOVERSION ${PROJECT_VERSION_MAJOR} )

install(TARGETS QrtrViewers
    EXPORT QrtrViewersTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    QrtrViewers/QuarterExaminerViewer.h
    QrtrViewers/ThumbWheel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/QrtrViewers
)

set(QRTRVIEWERS_INSTALL_CMAKEDIR
    ${CMAKE_INSTALL_LIBDIR}/cmake/QrtrViewers
)

configure_package_config_file(
    cmake/QrtrViewersConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/QrtrViewersConfig.cmake
    INSTALL_DESTINATION ${QRTRVIEWERS_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/QrtrViewersConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT QrtrViewersTargets
    FILE QrtrViewersTargets.cmake
    NAMESPACE QrtrViewers::
    DESTINATION ${QRTRVIEWERS_INSTALL_CMAKEDIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/QrtrViewersConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/QrtrViewersConfigVersion.cmake
    DESTINATION ${QRTRVIEWERS_INSTALL_CMAKEDIR}
)

# -----------------------------------------------------------------------------
# "Hello, Cone" test program
# -----------------------------------------------------------------------------
add_executable(sierpinski test/sierpinski.cpp)
target_link_libraries(sierpinski PRIVATE QrtrViewers)
target_include_directories(sierpinski PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cone test/cone.cpp)
target_link_libraries(cone PRIVATE QrtrViewers)
target_include_directories(cone PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

