if (ENV{USER})
    set (USERNAME $ENV{USER})
else (ENV{USER})
    set (USERNAME "user")
endif (ENV{USER})

if (${USERNAME} STREQUAL "root")
    set (INSTALLDIR ${INSTALLDIR_ROOT})
else (${USERNAME} STREQUAL "root")
    set (INSTALLDIR ${INSTALLDIR_USER})
endif (${USERNAME} STREQUAL "root")

file (INSTALL DESTINATION ${INSTALLDIR}
      TYPE FILE
      FILES ${FILE})
