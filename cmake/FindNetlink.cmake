find_path(LIBNL_INCLUDE_DIR netlink/netlink.h
  /usr/include
  /usr/include/libnl3
  /usr/local/include
  /usr/local/include/libnl3
)

find_library(LIBNL_LIBRARY NAMES nl nl-3 REQUIRED)
find_library(LIBNL_ROUTE_LIBRARY NAMES nl-route nl-route-3 REQUIRED)
#find_library(LIBNL_NETFILTER_LIBRARY NAMES nl-nf nl-nf-3 REQUIRED)
#find_library(LIBNL_GENL_LIBRARY NAMES nl-genl nl-genl-3 REQUIRED)

set(LIBNL_FOUND TRUE)
set(LIBNL_LIBRARIES ${LIBNL_LIBRARY} ${LIBNL_ROUTE_LIBRARY})
message("Found netlink includes: ${LIBNL_INCLUDE_DIR}")
message("Found netlink libraries:  ${LIBNL_LIBRARIES}")
