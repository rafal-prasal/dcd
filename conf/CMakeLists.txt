set(dcd_config_files
    console.allow
    dcd.users
    dcd.banned
    dcd.conf
    dcd.motd
    dcd.welcome
    nicks.allow
    dcd.penalties
    dcd.rules
    dcd.hublinks
    dcd.usercommands
)

install(FILES ${dcd_config_files} DESTINATION "${DCD_CONFIGDIR}")

install(PROGRAMS ${CMAKE_BINARY_DIR}/dcd.adduser DESTINATION ${CMAKE_INSTALL_FULL_SBINDIR})
