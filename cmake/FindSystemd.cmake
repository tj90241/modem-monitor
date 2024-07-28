find_path(LIBSYSTEMD_INCLUDE_DIR sd-bus.h
  /usr/include
  /usr/include/systemd
  /usr/local/include
  /usr/local/include/systemd
)

find_library(LIBSYSTEMD_LIBRARY NAMES systemd REQUIRED)

set(LIBSYSTEMD_FOUND TRUE)
set(LIBSYSTEMD_LIBRARIES ${LIBSYSTEMD_LIBRARY})
message("Found systemd includes: ${LIBSYSTEMD_INCLUDE_DIR}")
message("Found systemd libraries:  ${LIBSYSTEMD_LIBRARIES}")
