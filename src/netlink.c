/*
 * src/netlink.c: Kernel network management helper functions
 *
 * modem-monitor: A WWAN modem monitoring and control daemon
 * Copyright (C) 2024, Tyler J. Stachecki
 *
 * This file is subject to the terms and conditions defined in
 * 'LICENSE', which is part of this source code package.
 */

#include "mm_log.h"
#include "mm_netlink.h"

#include <libnl3/netlink/netlink.h>
#include <libnl3/netlink/route/addr.h>
#include <libnl3/netlink/route/link.h>
#include <libnl3/netlink/route/route.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_NETLINK_ADDRS 126U
#define WWAN_INTERFACE_NAME "mhi_hwip0"

static int allocate_ipv4_addrs(struct mm_netlink *);
static int allocate_ipv6_addrs(struct mm_netlink *);
static int allocate_wg0_resources(struct mm_netlink *);

static void collect_nonlink_addrs(struct nl_object *, void *);
static int ensure_interface_state(struct nl_sock *, struct rtnl_link *, bool);

struct mm_netlink_addrs {
    struct rtnl_addr *list[MAX_NETLINK_ADDRS];
    size_t count, allocated;
};

int allocate_ipv4_addrs(struct mm_netlink *mm_nl) {
    uint32_t s_addr = 0;

    if ((mm_nl->default_route_addr4 = nl_addr_build(AF_INET,
            &s_addr, sizeof(s_addr))) == NULL) {
        perror("nl_addr_build");
        return -1;
    }

    nl_addr_set_prefixlen(mm_nl->default_route_addr4, 0);

    if ((mm_nl->gateway_addr4 = nl_addr_build(AF_INET,
            &s_addr, sizeof(s_addr))) == NULL) {
        perror("nl_addr_build");
    }

    else {
        nl_addr_set_prefixlen(mm_nl->gateway_addr4, 32);

        if ((mm_nl->nl_wwan_addr4 = nl_addr_build(AF_INET,
                &s_addr, sizeof(s_addr))) == NULL) {
            perror("nl_addr_build");
        }

        else {
            if ((mm_nl->default_route4 = rtnl_route_alloc()) == NULL) {
                perror("rtnl_route_alloc");
            }

            else {
                struct rtnl_route *default_route = mm_nl->default_route4;
                struct rtnl_addr *addr;
                int status;

                rtnl_route_set_family(default_route, AF_INET);
                rtnl_route_set_dst(default_route, mm_nl->default_route_addr4);
                rtnl_route_set_scope(default_route, RT_SCOPE_UNIVERSE);
                rtnl_route_set_pref_src(default_route, mm_nl->nl_wwan_addr4);
                rtnl_route_set_protocol(default_route, RTPROT_STATIC);
                rtnl_route_set_table(default_route, RT_TABLE_MAIN);
                rtnl_route_set_type(default_route, RTN_UNICAST);

                if ((mm_nl->wwan_addr4 = rtnl_addr_alloc()) == NULL) {
                    perror("rtnl_addr_alloc");
                }

                else if ((status = rtnl_addr_set_local(mm_nl->wwan_addr4,
                        mm_nl->nl_wwan_addr4))) {
                    MM_LOG("%srtnl_addr_set_local: %s\n", nl_geterror(status));
                }

                else {
                    addr = mm_nl->wwan_addr4;

                    rtnl_addr_set_ifindex(addr, mm_nl->wwan_ifindex);
                    rtnl_addr_set_link(addr, mm_nl->wwan_link_v4);
                    rtnl_addr_set_scope(addr, RT_SCOPE_UNIVERSE);

                    return 0;
                }

                rtnl_route_put(mm_nl->default_route4);
            }

            nl_addr_put(mm_nl->nl_wwan_addr4);
        }

        nl_addr_put(mm_nl->gateway_addr4);
    }

    nl_addr_put(mm_nl->default_route_addr4);
    return -1;
}

int allocate_ipv6_addrs(struct mm_netlink *mm_nl) {
    static const uint32_t s_addr[4] = {0, 0, 0, 0};

    if ((mm_nl->default_route_addr6 = nl_addr_build(AF_INET6,
            &s_addr, sizeof(s_addr))) == NULL) {
        perror("nl_addr_build");
        return -1;
    }

    nl_addr_set_prefixlen(mm_nl->default_route_addr6, 0);

    if ((mm_nl->gateway_addr6 = nl_addr_build(AF_INET6,
            &s_addr, sizeof(s_addr))) == NULL) {
        perror("nl_addr_build");
    }

    else {
        nl_addr_set_prefixlen(mm_nl->gateway_addr6, 128);

        if ((mm_nl->nl_wwan_addr6 = nl_addr_build(AF_INET6,
                &s_addr, sizeof(s_addr))) == NULL) {
            perror("nl_addr_build");
        }

        else {
            nl_addr_set_prefixlen(mm_nl->nl_wwan_addr6, 64);

            if ((mm_nl->default_route6 = rtnl_route_alloc()) == NULL) {
                perror("rtnl_route_alloc");
            }

            else {
                struct rtnl_route *default_route = mm_nl->default_route6;
                struct rtnl_addr *addr;
                int status;

                rtnl_route_set_family(default_route, AF_INET6);
                rtnl_route_set_dst(default_route, mm_nl->default_route_addr6);
                rtnl_route_set_scope(default_route, RT_SCOPE_UNIVERSE);
                rtnl_route_set_protocol(default_route, RTPROT_STATIC);
                rtnl_route_set_table(default_route, RT_TABLE_MAIN);
                rtnl_route_set_type(default_route, RTN_UNICAST);

                if ((mm_nl->wwan_addr6 = rtnl_addr_alloc()) == NULL) {
                    perror("rtnl_addr_alloc");
                }

                else if ((status = rtnl_addr_set_local(mm_nl->wwan_addr6,
                        mm_nl->nl_wwan_addr6))) {
                    MM_LOG("%srtnl_addr_set_local: %s\n", nl_geterror(status));
                }

                else {
                    addr = mm_nl->wwan_addr6;

                    rtnl_addr_set_ifindex(addr, mm_nl->wwan_ifindex);
                    rtnl_addr_set_link(addr, mm_nl->wwan_link_v6);
                    rtnl_addr_set_scope(addr, RT_SCOPE_UNIVERSE);

                    return 0;
                }

                rtnl_route_put(mm_nl->default_route6);
            }

            nl_addr_put(mm_nl->nl_wwan_addr6);
        }

        nl_addr_put(mm_nl->gateway_addr6);
    }

    nl_addr_put(mm_nl->default_route_addr6);
    return -1;
}

int allocate_wg0_resources(struct mm_netlink *mm_nl) {
    uint32_t s_addr;

    s_addr = 0x01010A0A; // 10.10.1.1: wg0 gateway
    if ((mm_nl->wg0_gateway_address = nl_addr_build(AF_INET,
            &s_addr, sizeof(s_addr))) == NULL) {
        perror("nl_addr_build");
        return -1;
    }

    nl_addr_set_prefixlen(mm_nl->wg0_gateway_address, 32);

    s_addr = 0x02010A0A; // 10.10.1.2: wg0 interface address
    if ((mm_nl->wg0_self_address = nl_addr_build(AF_INET,
            &s_addr, sizeof(s_addr))) == NULL) {
        perror("nl_addr_build");
        return -1;
    }

    else {
        nl_addr_set_prefixlen(mm_nl->wg0_self_address, 32);

        s_addr = 0x02020A0A; // 10.10.2.2: apt server
        if ((mm_nl->wg0_tgt_address = nl_addr_build(AF_INET,
                &s_addr, sizeof(s_addr))) == NULL) {
            perror("nl_addr_build");
            return -1;
        }

        else {
            nl_addr_set_prefixlen(mm_nl->wg0_tgt_address, 32);

            if ((mm_nl->wg0_tgt_route = rtnl_route_alloc()) == NULL) {
                perror("rtnl_route_alloc");
            }

            else {
                rtnl_route_set_family(mm_nl->wg0_tgt_route, AF_INET);
                rtnl_route_set_scope(mm_nl->wg0_tgt_route, RT_SCOPE_UNIVERSE);
                rtnl_route_set_protocol(mm_nl->wg0_tgt_route, RTPROT_STATIC);
                rtnl_route_set_table(mm_nl->wg0_tgt_route, RT_TABLE_MAIN);
                rtnl_route_set_type(mm_nl->wg0_tgt_route, RTN_UNICAST);

                rtnl_route_set_dst(mm_nl->wg0_tgt_route,
                        mm_nl->wg0_tgt_address);

                rtnl_route_set_pref_src(mm_nl->wg0_tgt_route,
                        mm_nl->wg0_self_address);

                if ((mm_nl->wg0_tgt_nexthop = rtnl_route_nh_alloc()) == NULL) {
                    perror("rtnl_route_nh_alloc");
                }

                else {
                    rtnl_route_nh_set_gateway(mm_nl->wg0_tgt_nexthop,
                            mm_nl->wg0_gateway_address);

                    rtnl_route_add_nexthop(mm_nl->wg0_tgt_route,
                            mm_nl->wg0_tgt_nexthop);

                    return 0;
                }

                rtnl_route_put(mm_nl->wg0_tgt_route);
            }

            nl_addr_put(mm_nl->wg0_tgt_address);
        }

        nl_addr_put(mm_nl->wg0_self_address);
    }

    nl_addr_put(mm_nl->wg0_gateway_address);
    return -1;
}

void collect_nonlink_addrs(struct nl_object *object, void *data) {
    struct mm_netlink_addrs *addrs = (struct mm_netlink_addrs *) data;
    struct rtnl_addr *addr = (struct rtnl_addr *) object;

    if (rtnl_addr_get_scope(addr) == RT_SCOPE_LINK) {
        return;
    }

    if (addrs->allocated < MAX_NETLINK_ADDRS) {
        addrs->list[addrs->allocated++] = addr;
    }

    addrs->count++;
}

int ensure_interface_state(struct nl_sock *nl, struct rtnl_link *link,
        bool request_up) {
    bool iface_is_up;
    int status;

    iface_is_up = !!(rtnl_link_get_flags(link) & IFF_UP);
    status = 0;

    if ((request_up && !iface_is_up) || (!request_up && iface_is_up)) {
        struct rtnl_link *change;

        if ((change = rtnl_link_alloc()) == NULL) {
            perror("rtnl_link_alloc");
            status = -1;
        }

        else {
            if (request_up) {
                rtnl_link_set_flags(change, IFF_UP);
            }

            else {
                rtnl_link_unset_flags(change, IFF_UP);
            }

            if ((status = rtnl_link_change(nl, link, change, 0))) {
                MM_LOG("%srtnl_link_change: %s\n", nl_geterror(status));
            }

            rtnl_link_put(change);
        }
    }

    return status;
}

int mm_netlink_add_v4_address(struct mm_netlink *mm_nl,
        uint32_t s_addr, int prefix_length) {
    int status;

    nl_addr_set_binary_addr(mm_nl->nl_wwan_addr4, &s_addr, sizeof(s_addr));
    nl_addr_set_prefixlen(mm_nl->nl_wwan_addr4, prefix_length);
    rtnl_addr_set_prefixlen(mm_nl->wwan_addr4, prefix_length);

    if ((status = rtnl_addr_add(mm_nl->nl, mm_nl->wwan_addr4, 0))) {
        MM_LOG("%srtnl_addr_add: %s\n", nl_geterror(status));
    }

    return status;
}

int mm_netlink_add_v6_address(struct mm_netlink *mm_nl,
        const struct in6_addr *address, int prefix_length) {
    int status;

    nl_addr_set_binary_addr(mm_nl->nl_wwan_addr6, address, sizeof(*address));
    nl_addr_set_prefixlen(mm_nl->nl_wwan_addr6, prefix_length);
    rtnl_addr_set_prefixlen(mm_nl->wwan_addr6, prefix_length);

    if ((status = rtnl_addr_add(mm_nl->nl, mm_nl->wwan_addr6, 0))) {
        MM_LOG("%srtnl_addr_add: %s\n", nl_geterror(status));
    }

    return status;
}

int mm_netlink_change_v4_default_gateway(struct mm_netlink *mm_nl,
        uint32_t wwan_addr, uint32_t gateway_addr) {
    int status;

    nl_addr_set_prefixlen(mm_nl->nl_wwan_addr4, 32);
    nl_addr_set_binary_addr(mm_nl->nl_wwan_addr4, &wwan_addr,
            sizeof(wwan_addr));

    nl_addr_set_binary_addr(mm_nl->gateway_addr4, &gateway_addr,
            sizeof(gateway_addr));

    rtnl_route_nh_set_gateway(mm_nl->wwan_nexthop, mm_nl->gateway_addr4);
    rtnl_route_add_nexthop(mm_nl->default_route4, mm_nl->wwan_nexthop);

    if ((status = rtnl_route_add(mm_nl->nl, mm_nl->default_route4,
            NLM_F_CREATE | NLM_F_REPLACE))) {
        MM_LOG("%srtnl_route_add: %s\n", nl_geterror(status));
    }

    /*
     * Instead of trying to manage reference counts, it's easier to just always
     * keep them at zero. Then we can just free the nexthop when shutting down.
     */
    rtnl_route_nh_set_gateway(mm_nl->wwan_nexthop, NULL);
    rtnl_route_remove_nexthop(mm_nl->default_route4, mm_nl->wwan_nexthop);
    return status;
}

int mm_netlink_change_v6_default_gateway(struct mm_netlink *mm_nl,
        const struct in6_addr *wwan_addr,
        const struct in6_addr *gateway_addr,
        int prefix_length) {
    int status;

    nl_addr_set_prefixlen(mm_nl->nl_wwan_addr6, prefix_length);
    nl_addr_set_binary_addr(mm_nl->nl_wwan_addr6, wwan_addr, sizeof(*wwan_addr));
    nl_addr_set_binary_addr(mm_nl->gateway_addr6, gateway_addr,
        sizeof(*gateway_addr));

    rtnl_route_nh_set_gateway(mm_nl->wwan_nexthop, mm_nl->gateway_addr6);
    rtnl_route_add_nexthop(mm_nl->default_route6, mm_nl->wwan_nexthop);

    if ((status = rtnl_route_add(mm_nl->nl, mm_nl->default_route6,
            NLM_F_CREATE | NLM_F_REPLACE))) {
        MM_LOG("%srtnl_route_add: %s\n", nl_geterror(status));
    }

    /*
     * Instead of trying to manage reference counts, it's easier to just always
     * keep them at zero. Then we can just free the nexthop when shutting down.
     */
    rtnl_route_nh_set_gateway(mm_nl->wwan_nexthop, NULL);
    rtnl_route_remove_nexthop(mm_nl->default_route6, mm_nl->wwan_nexthop);
    return status;
}

int mm_netlink_ensure_v4_configuration_is_applied(struct mm_netlink *mm_nl,
        uint32_t address, int prefix_length, uint32_t gateway_address) {
    struct mm_netlink_addrs addrs;
    bool found_address;
    int status = 0;
    size_t i;

    /* Dump a list of addresses on the WWAN interface. */
    addrs.count = addrs.allocated = 0;

    rtnl_addr_set_family(mm_nl->addr_filter, AF_INET);
    nl_cache_foreach_filter(mm_nl->addr_cache,
            (struct nl_object *) (mm_nl->addr_filter), collect_nonlink_addrs,
            &addrs);

    if (addrs.allocated < addrs.count) {
        MM_LOG("%smm_netlink_ensure_v4_confiuration_is_applied: "
                ">%u addresses returned?\n", MAX_NETLINK_ADDRS);

        status = -1;
    }

    /* Remove any addresses which should no longer be present. */
    for (i = 0, found_address = false; i < addrs.allocated; i++) {
        struct nl_addr *nl_addr = rtnl_addr_get_local(addrs.list[i]);

        if (rtnl_addr_get_prefixlen(addrs.list[i]) == prefix_length &&
                nl_addr_get_len(nl_addr) == sizeof(address)) {
            uint32_t binary_address;

            memcpy(&binary_address, nl_addr_get_binary_addr(nl_addr),
                    sizeof(binary_address));

            if (binary_address == address) {
                found_address = true;
                continue;
            }
        }

        if ((status = rtnl_addr_delete(mm_nl->nl, addrs.list[i], 0))) {
            MM_LOG("%srtnl_addr_delete: %s\n", nl_geterror(status));
        }
    }

    /* Add the address if missing and provision the default route. */
    if (!found_address) {
        status = mm_netlink_add_v4_address(mm_nl, address, prefix_length);
    }

    if (!status) {
        status = mm_netlink_change_v4_default_gateway(mm_nl, address,
                gateway_address);
    }

    return status;
}

int mm_netlink_ensure_wg0_interface_state(struct mm_netlink *mm_nl,
        bool request_up) {
    return ensure_interface_state(mm_nl->nl, mm_nl->wg0_link, request_up);
}

int mm_netlink_ensure_wg0_routes_are_applied(struct mm_netlink *mm_nl) {
    uint32_t s_addr;
    int status;

    s_addr = 0x02020A0A; // 10.10.2.2/32: apt server
    nl_addr_set_prefixlen(mm_nl->wg0_tgt_address, 32);
    nl_addr_set_binary_addr(mm_nl->wg0_tgt_address, &s_addr, sizeof(s_addr));

    if ((status = rtnl_route_add(mm_nl->nl, mm_nl->wg0_tgt_route,
            NLM_F_CREATE | NLM_F_REPLACE))) {
        MM_LOG("%srtnl_route_add: %s\n", nl_geterror(status));
    }

    else {
        s_addr = 0x00030A0A; // 10.10.3.0/24: vrf-ops network
        nl_addr_set_prefixlen(mm_nl->wg0_tgt_address, 24);
        nl_addr_set_binary_addr(mm_nl->wg0_tgt_address, &s_addr,
                sizeof(s_addr));

        if ((status = rtnl_route_add(mm_nl->nl, mm_nl->wg0_tgt_route,
                NLM_F_CREATE | NLM_F_REPLACE))) {
            MM_LOG("%srtnl_route_add: %s\n", nl_geterror(status));
        }
    }

    return status;
}

int mm_netlink_ensure_wwan_interface_state(struct mm_netlink *mm_nl,
        bool request_up) {
    return ensure_interface_state(mm_nl->nl, mm_nl->wwan_link_v4, request_up);
}

int mm_netlink_addr_flush(struct mm_netlink *mm_nl) {
    struct mm_netlink_addrs addrs;
    int status;
    size_t i;

    if ((status = mm_netlink_reload_address_cache(mm_nl))) {
        return status;
    }

    /* Collect IPv4 and IPv6 addresses to delete */
    addrs.count = addrs.allocated = 0;

    rtnl_addr_set_family(mm_nl->addr_filter, AF_INET);
    nl_cache_foreach_filter(mm_nl->addr_cache,
            (struct nl_object *) (mm_nl->addr_filter), collect_nonlink_addrs,
            &addrs);

    rtnl_addr_set_family(mm_nl->addr_filter, AF_INET6);
    nl_cache_foreach_filter(mm_nl->addr_cache,
            (struct nl_object *) (mm_nl->addr_filter), collect_nonlink_addrs,
            &addrs);

    if (addrs.allocated < addrs.count) {
        MM_LOG("%smm_netlink_flush: >%u addresses returned?\n",
                MAX_NETLINK_ADDRS);

        status = -1;
    }

    for (i = 0; i < addrs.allocated; i++) {
        if ((status = rtnl_addr_delete(mm_nl->nl, addrs.list[i], 0))) {
            MM_LOG("%srtnl_addr_delete: %s\n", nl_geterror(status));
        }
    }

    return status;
}

int mm_netlink_initialize(struct mm_netlink *mm_nl) {
    int status;

    if ((mm_nl->nl = nl_socket_alloc()) == NULL) {
        perror("nl_socket_alloc");
        return -1;
    }

    if ((status = nl_connect(mm_nl->nl, NETLINK_ROUTE))) {
        MM_LOG("%snl_connect: %s\n", nl_geterror(status));
        nl_socket_free(mm_nl->nl);
        return status;
    }

    if ((status = rtnl_link_alloc_cache(mm_nl->nl, AF_INET,
            &mm_nl->link_cache_v4))) {
        MM_LOG("%srtnl_link_alloc_cache: %s\n", nl_geterror(status));
        nl_socket_free(mm_nl->nl);
        return status;
    }

    if ((status = rtnl_link_alloc_cache(mm_nl->nl, AF_INET6,
            &mm_nl->link_cache_v6))) {
        MM_LOG("%srtnl_link_alloc_cache: %s\n", nl_geterror(status));
        nl_cache_free(mm_nl->link_cache_v4);
        nl_socket_free(mm_nl->nl);
        return status;
    }

    if ((mm_nl->wwan_link_v4 = rtnl_link_get_by_name(mm_nl->link_cache_v4,
            WWAN_INTERFACE_NAME)) == NULL) {
        MM_LOG("%s%s\n", "rtnl_link_get_by_name: "
                "No such interface: "WWAN_INTERFACE_NAME);

        nl_cache_free(mm_nl->link_cache_v6);
        nl_cache_free(mm_nl->link_cache_v4);
        nl_socket_free(mm_nl->nl);
        return -1;
    }

    if ((mm_nl->wwan_link_v6 = rtnl_link_get_by_name(mm_nl->link_cache_v6,
            WWAN_INTERFACE_NAME)) == NULL) {
        MM_LOG("%s%s\n", "rtnl_link_get_by_name: "
                "No such interface: "WWAN_INTERFACE_NAME);

        rtnl_link_put(mm_nl->wwan_link_v4);
        nl_cache_free(mm_nl->link_cache_v6);
        nl_cache_free(mm_nl->link_cache_v4);
        nl_socket_free(mm_nl->nl);
        return -1;
    }

    if ((mm_nl->wg0_link = rtnl_link_get_by_name(
            mm_nl->link_cache_v4, "wg0")) == NULL) {
        MM_LOG("%s%s\n", "rtnl_link_get_by_name: No such interface: wg0");

        rtnl_link_put(mm_nl->wwan_link_v6);
        rtnl_link_put(mm_nl->wwan_link_v4);
        nl_cache_free(mm_nl->link_cache_v6);
        nl_cache_free(mm_nl->link_cache_v4);
        nl_socket_free(mm_nl->nl);
        return -1;
    }

    mm_nl->wg0_ifindex = rtnl_link_get_ifindex(mm_nl->wg0_link);

    if ((mm_nl->wwan_ifindex = rtnl_link_get_ifindex(mm_nl->wwan_link_v4)) !=
            rtnl_link_get_ifindex(mm_nl->wwan_link_v6)) {
        MM_LOG("%s%s\n", "mm_netlink_initialize: ifindex mismatch");
        status = -1;
    }

    else if ((status = rtnl_addr_alloc_cache(mm_nl->nl, &mm_nl->addr_cache))) {
        MM_LOG("%srtnl_addr_alloc_cache: %s\n", nl_geterror(status));
    }

    /* Save some repeated free statements with a bit of if/else nesting... */
    else {
        if ((mm_nl->addr_filter = rtnl_addr_alloc()) == NULL) {
            perror("rtnl_addr_alloc");
            status = -1;
        }

        else {
            rtnl_addr_set_ifindex(mm_nl->addr_filter, mm_nl->wwan_ifindex);

            if ((mm_nl->wwan_nexthop = rtnl_route_nh_alloc()) == NULL) {
                perror("rtnl_route_nh_alloc");
                status = -1;
            }

            else {
                rtnl_route_nh_set_ifindex(mm_nl->wwan_nexthop,
                        mm_nl->wwan_ifindex);

                if (!(status = allocate_ipv4_addrs(mm_nl))) {
                    if (!(status = allocate_ipv6_addrs(mm_nl))) {
                        if (!(status = allocate_wg0_resources(mm_nl))) {
                            return 0;
                        }

                        rtnl_addr_put(mm_nl->wwan_addr6);
                        rtnl_route_put(mm_nl->default_route6);
                        nl_addr_put(mm_nl->nl_wwan_addr6);
                        nl_addr_put(mm_nl->gateway_addr6);
                        nl_addr_put(mm_nl->default_route_addr6);
                    }

                    rtnl_addr_put(mm_nl->wwan_addr4);
                    rtnl_route_put(mm_nl->default_route4);
                    nl_addr_put(mm_nl->nl_wwan_addr4);
                    nl_addr_put(mm_nl->gateway_addr4);
                    nl_addr_put(mm_nl->default_route_addr4);
                }

                rtnl_route_nh_free(mm_nl->wwan_nexthop);
            }

            rtnl_addr_put(mm_nl->addr_filter);
        }

        nl_cache_free(mm_nl->addr_cache);
    }

    rtnl_link_put(mm_nl->wwan_link_v6);
    rtnl_link_put(mm_nl->wwan_link_v4);
    nl_cache_free(mm_nl->link_cache_v6);
    nl_cache_free(mm_nl->link_cache_v4);
    nl_socket_free(mm_nl->nl);
    return status;
}

int mm_netlink_reload_address_cache(struct mm_netlink *mm_nl) {
    int status;

    if ((status = nl_cache_refill(mm_nl->nl, mm_nl->addr_cache))) {
        MM_LOG("%snl_cache_refill: %s\n", nl_geterror(status));
    }

    return status;
}

int mm_netlink_reload_link_cache(struct mm_netlink *mm_nl) {
    int status;

    mm_nl->wwan_ifindex = 0;
    rtnl_link_put(mm_nl->wwan_link_v4);
    mm_nl->wwan_link_v4 = NULL;

    if ((status = nl_cache_refill(mm_nl->nl, mm_nl->link_cache_v4))) {
        MM_LOG("%snl_cache_refill: %s\n", nl_geterror(status));
        return status;
    }

    if ((mm_nl->wwan_link_v4 = rtnl_link_get_by_name(mm_nl->link_cache_v4,
            WWAN_INTERFACE_NAME)) == NULL) {
        return -1;
    }

    rtnl_link_put(mm_nl->wwan_link_v6);
    mm_nl->wwan_link_v6 = NULL;

    if ((status = nl_cache_refill(mm_nl->nl, mm_nl->link_cache_v6))) {
        MM_LOG("%snl_cache_refill: %s\n", nl_geterror(status));
        return status;
    }

    if ((mm_nl->wwan_link_v6 = rtnl_link_get_by_name(mm_nl->link_cache_v6,
            WWAN_INTERFACE_NAME)) == NULL) {
        MM_LOG("%s%s\n", "rtnl_link_get_by_name: "
                "No such interface: "WWAN_INTERFACE_NAME);

        return -1;
    }

    if ((mm_nl->wwan_ifindex = rtnl_link_get_ifindex(mm_nl->wwan_link_v4)) !=
            rtnl_link_get_ifindex(mm_nl->wwan_link_v6)) {
        MM_LOG("%s%s\n", "mm_netlink_reload_link_cache: ifindex mismatch");
        return -1;
    }

    rtnl_addr_set_ifindex(mm_nl->addr_filter, mm_nl->wwan_ifindex);
    rtnl_route_nh_set_ifindex(mm_nl->wwan_nexthop, mm_nl->wwan_ifindex);

    mm_nl->wg0_ifindex = 0;
    rtnl_link_put(mm_nl->wg0_link);
    mm_nl->wg0_link = NULL;

    if ((mm_nl->wg0_link = rtnl_link_get_by_name(
            mm_nl->link_cache_v4, "wg0")) == NULL) {
        MM_LOG("%s%s\n", "rtnl_link_get_by_name: No such interface: wg0");
        return -1;
    }

    mm_nl->wg0_ifindex = rtnl_link_get_ifindex(mm_nl->wg0_link);
    return 0;
}

void mm_netlink_shutdown(struct mm_netlink *mm_nl) {
    rtnl_route_nh_set_gateway(mm_nl->wg0_tgt_nexthop, NULL);
    rtnl_route_remove_nexthop(mm_nl->wg0_tgt_route, mm_nl->wg0_tgt_nexthop);
    rtnl_route_nh_free(mm_nl->wg0_tgt_nexthop);
    rtnl_route_put(mm_nl->wg0_tgt_route);
    nl_addr_put(mm_nl->wg0_tgt_address);
    nl_addr_put(mm_nl->wg0_self_address);
    nl_addr_put(mm_nl->wg0_gateway_address);
    rtnl_addr_put(mm_nl->wwan_addr6);
    rtnl_route_put(mm_nl->default_route6);
    nl_addr_put(mm_nl->nl_wwan_addr6);
    nl_addr_put(mm_nl->gateway_addr6);
    nl_addr_put(mm_nl->default_route_addr6);
    rtnl_addr_put(mm_nl->wwan_addr4);
    rtnl_route_put(mm_nl->default_route4);
    nl_addr_put(mm_nl->nl_wwan_addr4);
    nl_addr_put(mm_nl->gateway_addr4);
    nl_addr_put(mm_nl->default_route_addr4);
    rtnl_route_nh_free(mm_nl->wwan_nexthop);
    rtnl_addr_put(mm_nl->addr_filter);
    nl_cache_free(mm_nl->addr_cache);
    rtnl_link_put(mm_nl->wg0_link);
    rtnl_link_put(mm_nl->wwan_link_v6);
    rtnl_link_put(mm_nl->wwan_link_v4);
    nl_cache_free(mm_nl->link_cache_v6);
    nl_cache_free(mm_nl->link_cache_v4);
    nl_socket_free(mm_nl->nl);
}
