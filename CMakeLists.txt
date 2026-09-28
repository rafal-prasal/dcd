cmake_minimum_required(VERSION 3.10)

project(dcd
	VERSION 1.1.1
	LANGUAGES C)

set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE "${PROJECT_NAME}")
set(VERSION "${PROJECT_VERSION}")
set(prefix "${CMAKE_INSTALL_PREFIX}")

set(DCD_CONFIG_DIR "${CMAKE_INSTALL_PREFIX}/etc/${PACKAGE_NAME}" CACHE PATH
	"Directory where the dcd configuration files are stored")
set(DEFAULT_USER "daemon" CACHE STRING "Default user dcd runs as")
set(DEFAULT_GROUP "daemon" CACHE STRING "Default group dcd runs as")
set(LOG_DIR "/var/log/${PACKAGE_NAME}" CACHE PATH "Directory with dcd log files")

string(TIMESTAMP BUILD_TIME "%a %b %d %H:%M:%S UTC %Y" UTC)

find_package(Threads REQUIRED)

add_executable(dcd
	src/conf.c
	src/console.c
	src/main.c
	src/match.c
	src/misc.c
	src/net.c
	src/penalties.c
	src/cmd.c
	src/cmd.user.c
	src/cmd.opt_line.c
	src/cmd.dc.c
	src/cmd.conf.c
	src/compatibility.c
	src/cmd.udp.c
		src/log.c)

target_include_directories(dcd PRIVATE ${CMAKE_SOURCE_DIR} src)

target_compile_definitions(dcd PRIVATE
	"-DPACKAGE_NAME=\"${PACKAGE_NAME}\""
	"-DPACKAGE_VERSION=\"${PACKAGE_VERSION}\""
	"-DCONFIGDIR=\"${DCD_CONFIG_DIR}\""
	"-DDEFAULT_USER=\"${DEFAULT_USER}\""
	"-DDEFAULT_GROUP=\"${DEFAULT_GROUP}\""
	"-DBUILD_TIME=\"${BUILD_TIME}\"")

target_compile_options(dcd PRIVATE -Wall)
target_link_libraries(dcd PRIVATE Threads::Threads)

configure_file(README.in ${CMAKE_BINARY_DIR}/README @ONLY)
configure_file(conf/dcd.conf.in ${CMAKE_BINARY_DIR}/conf/dcd.conf @ONLY)
configure_file(src/dcd.adduser.in ${CMAKE_BINARY_DIR}/src/${PACKAGE_NAME}.adduser @ONLY)
configure_file(contrib/logrotate.dcd.in ${CMAKE_BINARY_DIR}/contrib/logrotate.${PACKAGE_NAME} @ONLY)
configure_file(contrib/PLD/${PACKAGE_NAME}.spec.in ${CMAKE_BINARY_DIR}/contrib/PLD/${PACKAGE_NAME}.spec @ONLY)
configure_file(contrib/PLD/${PACKAGE_NAME}.init.in ${CMAKE_BINARY_DIR}/contrib/PLD/${PACKAGE_NAME}.init @ONLY)
configure_file(contrib/MandrakeLinux/${PACKAGE_NAME}.spec.in ${CMAKE_BINARY_DIR}/contrib/MandrakeLinux/${PACKAGE_NAME}.spec @ONLY)
configure_file(contrib/MandrakeLinux/${PACKAGE_NAME}.init.in ${CMAKE_BINARY_DIR}/contrib/MandrakeLinux/${PACKAGE_NAME}.init @ONLY)

install(TARGETS dcd DESTINATION sbin)

install(PROGRAMS ${CMAKE_BINARY_DIR}/src/${PACKAGE_NAME}.adduser DESTINATION sbin)

install(DIRECTORY DESTINATION ${DCD_CONFIG_DIR} MESSAGE_NEVER)

set(DCD_CONF_FILES
	console.allow dcd.users dcd.banned dcd.conf dcd.motd dcd.welcome
	nicks.allow dcd.penalties dcd.rules dcd.hublinks dcd.usercommands)

foreach(conf_file ${DCD_CONF_FILES})
	if(conf_file STREQUAL "dcd.conf")
		set(src_file ${CMAKE_BINARY_DIR}/conf/${conf_file})
	else()
		set(src_file ${CMAKE_SOURCE_DIR}/conf/${conf_file})
	endif()
	if(conf_file STREQUAL "dcd.users" OR conf_file STREQUAL "console.allow")
		set(file_perms OWNER_READ OWNER_WRITE GROUP_READ)
	else()
		set(file_perms OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
	endif()
	install(FILES ${src_file} DESTINATION ${DCD_CONFIG_DIR}
		PERMISSIONS ${file_perms})
endforeach()

install(FILES doc/dcd.2 doc/dcd.console.2 DESTINATION share/man/man2)
install(FILES doc/pl/dcd.1 doc/pl/dcd.console.1 DESTINATION share/man/pl/man1)
