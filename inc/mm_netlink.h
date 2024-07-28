/*
 * inc/mm_netlink.h: Kernel network management helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#ifndef MM_NETLINK_H
#define MM_NETLINK_H

#include <libnl3/netlink/netlink.h>
#include <netinet/in.h>

#include <stdbool.h>
#include <stdint.h>

struct mm_netlink {
    struct nl_sock *nl;
    struct nl_cache *link_cache_v4;
    struct nl_cache *link_cache_v6;
    struct rtnl_link *wwan_link_v4;
    struct rtnl_link *wwan_link_v6;
    struct rtnl_link *wg0_link;
    struct nl_cache *addr_cache;
    struct rtnl_addr *addr_filter;
    struct rtnl_nexthop *wwan_nexthop;
    struct rtnl_route *default_route4;
    struct nl_addr *default_route_addr4;
    struct nl_addr *gateway_addr4;
    struct nl_addr *nl_wwan_addr4;
    struct rtnl_addr *wwan_addr4;
    struct rtnl_route *default_route6;
    struct nl_addr *default_route_addr6;
    struct nl_addr *gateway_addr6;
    struct nl_addr *nl_wwan_addr6;
    struct rtnl_addr *wwan_addr6;
    struct nl_addr *wg0_gateway_address;
    struct nl_addr *wg0_self_address;
    struct nl_addr *wg0_tgt_address;
    struct rtnl_route *wg0_tgt_route;
    struct rtnl_nexthop *wg0_tgt_nexthop;
    int wwan_ifindex;
    int wg0_ifindex;
};

int mm_netlink_add_v4_address(struct mm_netlink *, uint32_t, int);
int mm_netlink_add_v6_address(struct mm_netlink *,
        const struct in6_addr *, int);

int mm_netlink_change_v4_default_gateway(struct mm_netlink *, uint32_t,
        uint32_t);

int mm_netlink_change_v6_default_gateway(struct mm_netlink *,
        const struct in6_addr *, const struct in6_addr *, int);

int mm_netlink_ensure_v4_configuration_is_applied(struct mm_netlink *,
        uint32_t, int, uint32_t);


int mm_netlink_ensure_wg0_interface_state(struct mm_netlink *, bool);
int mm_netlink_ensure_wg0_routes_are_applied(struct mm_netlink *);
int mm_netlink_ensure_wwan_interface_state(struct mm_netlink *, bool);
int mm_netlink_reload_address_cache(struct mm_netlink *);
int mm_netlink_reload_link_cache(struct mm_netlink *);

int mm_netlink_addr_flush(struct mm_netlink *);
int mm_netlink_initialize(struct mm_netlink *);
void mm_netlink_shutdown(struct mm_netlink *);

#endif
