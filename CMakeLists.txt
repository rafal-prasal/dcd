cmake_minimum_required(VERSION 3.10)
project(dcd VERSION 1.1.1 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CheckFunctionExists)
include(CheckIncludeFile)
include(GNUInstallDirs)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

# Required: pthreads
find_package(Threads REQUIRED)

# Check for functions
check_function_exists(daemon HAVE_DAEMON)
check_function_exists(strsep HAVE_STRSEP)

# Check for header files
check_include_file(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file(errno.h HAVE_ERRNO_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(netinet/in.h HAVE_NETINET_IN_H)
check_include_file(pthread.h HAVE_PTHREAD_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_file(sys/signal.h HAVE_SYS_SIGNAL_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(unistd.h HAVE_UNISTD_H)

# Check for libraries
include(CheckLibraryExists)
check_library_exists(c bind "" HAVE_LIBC_BIND)
if(NOT HAVE_LIBC_BIND)
    check_library_exists(socket bind "" HAVE_LIBSOCKET)
endif()

check_library_exists(c inet_aton "" HAVE_INET_ATON_C)
if(NOT HAVE_INET_ATON_C)
    check_library_exists(resolv inet_aton "" HAVE_INET_ATON_RESOLV)
endif()

check_library_exists(c inet_ntoa "" HAVE_INET_NTOA_C)
if(NOT HAVE_INET_NTOA_C)
    check_library_exists(nsl inet_ntoa "" HAVE_LIBNSL)
endif()

# Config directory (default: /etc/dcd)
set(DCD_CONFIGDIR "/etc/dcd" CACHE STRING "Configuration directory for dcd")

# Default user and group
set(DEFAULT_USER "daemon" CACHE STRING "Default user for dcd")
set(DEFAULT_GROUP "daemon" CACHE STRING "Default group for dcd")

# Build time
string(TIMESTAMP BUILD_TIME "%Y-%m-%dT%H:%M:%S" UTC)

# Generate config.h
configure_file(config.h.in config.h @ONLY)

add_subdirectory(src)
add_subdirectory(conf)
add_subdirectory(doc)
