set(dcd_sources
    conf.c
    console.c
    main.c
    match.c
    misc.c
    net.c
    penalties.c
    cmd.c
    cmd.user.c
    cmd.opt_line.c
    cmd.dc.c
    cmd.conf.c
    compatibility.c
    cmd.udp.c
    log.c
)

find_package(OpenSSL REQUIRED)

add_executable(dcd ${dcd_sources})

target_include_directories(dcd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}
)

target_compile_definitions(dcd PRIVATE
    CONFIGDIR="${DCD_CONFIGDIR}"
    DEFAULT_USER="${DEFAULT_USER}"
    DEFAULT_GROUP="${DEFAULT_GROUP}"
    PACKAGE="dcd"
    VERSION="${PROJECT_VERSION}"
    PACKAGE_NAME="dcd"
    PACKAGE_VERSION="${PROJECT_VERSION}"
    BUILD_TIME="${BUILD_TIME}"
)

target_link_libraries(dcd PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)

# Install binary to sbin
install(TARGETS dcd DESTINATION ${CMAKE_INSTALL_FULL_SBINDIR})

# Install adduser script
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dcd.adduser.in ${CMAKE_BINARY_DIR}/dcd.adduser @ONLY)
install(PROGRAMS ${CMAKE_BINARY_DIR}/dcd.adduser DESTINATION ${CMAKE_INSTALL_FULL_SBINDIR})
