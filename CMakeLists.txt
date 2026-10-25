cmake_minimum_required(VERSION 3.16)

project(dcd
	VERSION 1.1.1
	LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

set(DCD_ENABLE_LTO OFF CACHE BOOL "Enable link-time optimization")
set(DCD_ENABLE_TSAN OFF CACHE BOOL "Instrument the dcd binary with ThreadSanitizer")
set(DCD_ENABLE_ASAN OFF CACHE BOOL "Instrument the dcd binary with AddressSanitizer")

set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE "${PROJECT_NAME}")
set(VERSION "${PROJECT_VERSION}")
set(prefix "${CMAKE_INSTALL_PREFIX}")

set(DCD_CONFIG_DIR "${CMAKE_INSTALL_PREFIX}/etc/${PACKAGE_NAME}" CACHE PATH
	"Directory where the dcd configuration files are stored")
set(CONFIGDIR "${DCD_CONFIG_DIR}")
set(DEFAULT_USER "daemon" CACHE STRING "Default user dcd runs as")
set(DEFAULT_GROUP "daemon" CACHE STRING "Default group dcd runs as")
set(LOG_DIR "/var/log/${PACKAGE_NAME}" CACHE PATH "Directory with dcd log files")

string(TIMESTAMP BUILD_TIME "%a %b %d %H:%M:%S UTC %Y" UTC)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# daemon sources without main(), shared between the dcd binary and the tests
set(DCD_CORE_SOURCES
	src/conf.c
	src/console.c
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
	src/sha512.c
	src/stats.c
		src/log.c)

add_library(dcd_obj OBJECT ${DCD_CORE_SOURCES})

target_include_directories(dcd_obj PUBLIC ${CMAKE_SOURCE_DIR} src)

target_compile_definitions(dcd_obj PUBLIC
	"-DPACKAGE_NAME=\"${PACKAGE_NAME}\""
	"-DPACKAGE_VERSION=\"${PROJECT_VERSION}\""
	"-DCONFIGDIR=\"${DCD_CONFIG_DIR}\""
	"-DDEFAULT_USER=\"${DEFAULT_USER}\""
	"-DDEFAULT_GROUP=\"${DEFAULT_GROUP}\""
	"-DBUILD_TIME=\"${BUILD_TIME}\"")

target_compile_options(dcd_obj PRIVATE -Wall -fno-strict-aliasing)

target_link_libraries(dcd_obj PUBLIC OpenSSL::Crypto)

if(DCD_ENABLE_LTO)
	target_compile_options(dcd_obj PRIVATE -flto)
endif()

add_executable(dcd src/main.c)
target_link_libraries(dcd PRIVATE dcd_obj)

if(DCD_ENABLE_LTO)
	target_link_options(dcd PRIVATE -flto)
endif()
target_link_libraries(dcd PRIVATE Threads::Threads)

if(DCD_ENABLE_TSAN)
	# ThreadSanitizer: -O1 keeps reports readable; -g gives source lines.
	# drop the Release -O2 so the sanitizer's -O1 is not overridden.
	set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG")
	target_compile_options(dcd_obj PRIVATE -fsanitize=thread -g -O1)
	target_compile_options(dcd PRIVATE -fsanitize=thread -g -O1)
	target_link_options(dcd PRIVATE -fsanitize=thread)
endif()

if(DCD_ENABLE_ASAN)
	set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG")
	target_compile_options(dcd_obj PRIVATE -fsanitize=address -g -O1 -fno-omit-frame-pointer)
	target_compile_options(dcd PRIVATE -fsanitize=address -g -O1 -fno-omit-frame-pointer)
	target_link_options(dcd PRIVATE -fsanitize=address)
endif()

configure_file(README.in ${CMAKE_BINARY_DIR}/README @ONLY)
configure_file(conf/dcd.conf.in ${CMAKE_BINARY_DIR}/conf/dcd.conf @ONLY)
configure_file(src/dcd.adduser.in ${CMAKE_BINARY_DIR}/src/${PACKAGE_NAME}.adduser @ONLY)
configure_file(contrib/logrotate.dcd.in ${CMAKE_BINARY_DIR}/contrib/logrotate.${PACKAGE_NAME} @ONLY)
configure_file(contrib/PLD/${PACKAGE_NAME}.spec.in ${CMAKE_BINARY_DIR}/contrib/PLD/${PACKAGE_NAME}.spec @ONLY)
configure_file(contrib/PLD/${PACKAGE_NAME}.init.in ${CMAKE_BINARY_DIR}/contrib/PLD/${PACKAGE_NAME}.init @ONLY)
configure_file(contrib/MandrakeLinux/${PACKAGE_NAME}.spec.in ${CMAKE_BINARY_DIR}/contrib/MandrakeLinux/${PACKAGE_NAME}.spec @ONLY)
configure_file(contrib/MandrakeLinux/${PACKAGE_NAME}.init.in ${CMAKE_BINARY_DIR}/contrib/MandrakeLinux/${PACKAGE_NAME}.init @ONLY)

install(TARGETS dcd RUNTIME DESTINATION sbin)

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

option(DCD_BUILD_TESTS "Build unit and performance tests" ON)

if(DCD_BUILD_TESTS)
	enable_testing()

	# globals/stubs are compiled into each test binary directly: they provide
	# the src/main.c-owned symbols that dcd_obj references, and plain objects
	# avoid static-library link-order issues.
	set(DCD_TEST_HELPERS tests/globals.c tests/stubs.c)

	set(DCD_UNIT_TESTS
		test_cmd_dict
		test_match
		test_misc
		test_myinfo
		test_sha512
		test_stats)

	foreach(tst ${DCD_UNIT_TESTS})
		add_executable(${tst} tests/${tst}.c ${DCD_TEST_HELPERS})
		target_link_libraries(${tst} PRIVATE dcd_obj Threads::Threads)
		add_test(NAME ${tst} COMMAND ${tst})
	endforeach()

	set(DCD_PERF_TESTS
		bench_cmd_dict
		bench_add2buf
		bench_match
		bench_nick2id
		bench_sha512)

	foreach(tst ${DCD_PERF_TESTS})
		add_executable(${tst} tests/${tst}.c ${DCD_TEST_HELPERS})
		target_link_libraries(${tst} PRIVATE dcd_obj Threads::Threads)
		add_test(NAME ${tst} COMMAND ${tst})
		set_tests_properties(${tst} PROPERTIES LABELS "perf" TIMEOUT 180)
	endforeach()
endif()
