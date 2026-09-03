set(TIO_SOURCES
    poll.cpp
    waker.cpp
    net/tcp_listener.cpp
    net/tcp_stream.cpp
    net/udp_socket.cpp
    unix/unix_listener.cpp
    unix/unix_stream.cpp
    unix/unix_datagram.cpp
    unix/pipe.cpp
)

if(TIO_BACKEND STREQUAL "epoll")
    list(APPEND TIO_SOURCES
        sys/unix_/epoll_selector.cpp
        sys/unix_/eventfd_waker.cpp
    )
elseif(TIO_BACKEND STREQUAL "kqueue")
    list(APPEND TIO_SOURCES
        sys/unix_/kqueue_selector.cpp
        sys/unix_/kqueue_waker.cpp
    )
endif()

add_library(tio STATIC ${TIO_SOURCES})
add_library(tio::tio ALIAS tio)

target_include_directories(tio PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(tio PUBLIC cxx_std_26)

if(TIO_BACKEND STREQUAL "epoll")
    target_compile_definitions(tio PUBLIC TIO_BACKEND_EPOLL)
elseif(TIO_BACKEND STREQUAL "kqueue")
    target_compile_definitions(tio PUBLIC TIO_BACKEND_KQUEUE)
elseif(TIO_BACKEND STREQUAL "io_uring")
    target_compile_definitions(tio PUBLIC TIO_BACKEND_IO_URING)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS tio
    EXPORT tioTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/tio
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT tioTargets
    FILE tioTargets.cmake
    NAMESPACE tio::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tio
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/tioConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/tioConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/tioConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tio
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tioConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/tioConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tio
)
