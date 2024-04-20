#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
    const uint8_t prefix_length,
    const optional<Address> next_hop,
    const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
        << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    RouteItem item;
    item.route_prefix = route_prefix;
    item.prefix_length = prefix_length;
    item.next_hop = next_hop;
    item.interface_idx = interface_num;
    _route_table.push_back(item);
    // Your code here.
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram& dgram) {
    int longest_prefix_len = INT32_MIN;
    RouteItem match_route;
    uint32_t ip = dgram.header().dst;
    //匹配最长前缀的路由项
    for (auto& route : _route_table) {
        uint32_t offset = (route.prefix_length == 0) ? 0 : 0xffffffff << (32 - route.prefix_length);
        //匹配成功
        if ((ip & offset) == (route.route_prefix & offset) && longest_prefix_len < route.prefix_length) {
            longest_prefix_len = route.prefix_length;
            match_route = route;
        }
    }
    //匹配失败
    if (longest_prefix_len == INT32_MIN) {
        return;
    }
    if (dgram.header().ttl <= 1) {
        return;
    }
    dgram.header().ttl--;
    Address addr = Address::from_ipv4_numeric(dgram.header().dst);
    interface(match_route.interface_idx).send_datagram(dgram, match_route.next_hop.value_or(addr));
    return;
// Your code here.
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto& interface : _interfaces) {
        auto& queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
