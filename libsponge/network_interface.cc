#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress& ethernet_address, const Address& ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
        << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram& dgram, const Address& next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.payload() = std::move(dgram.serialize());
    if (_arp_table.count(next_hop_ip)) {
        frame.header().dst = _arp_table[next_hop_ip].mac;
        _frames_out.push(frame);
    }
    else {
        if (_waiting_reply_arp.count(next_hop.ipv4_numeric())) {
            return;
        }
        //广播查找mac地址
        ARPMessage arp_msg;
        arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
        arp_msg.sender_ethernet_address = _ethernet_address;
        arp_msg.target_ethernet_address = { 0,0,0,0,0,0 };
        arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
        arp_msg.target_ip_address = next_hop.ipv4_numeric();
        EthernetFrame arp_frame;
        arp_frame.header().type = EthernetHeader::TYPE_ARP;
        arp_frame.header().src = _ethernet_address;
        arp_frame.header().dst = ETHERNET_BROADCAST;
        arp_frame.payload() = std::move(arp_msg.serialize());
        waiting_arp_item warp_item;
        warp_item.ttl = 5000;
        warp_item.frame = arp_frame;
        _waiting_reply_arp[next_hop.ipv4_numeric()] = warp_item;
        _waiting_reply_arp[next_hop.ipv4_numeric()].datagram_queue.push(std::move(dgram));
        _frames_out.push(arp_frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame& frame) {
    //数据链路层帧的地址要么是广播地址，要么是自身的mac地址
    if (frame.header().dst != ETHERNET_BROADCAST &&
        frame.header().dst != _ethernet_address) {
        return nullopt;
    }
    //ipv4数据报
    else if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ip_datagram;
        if (ip_datagram.parse(frame.payload()) == ParseResult::NoError) {
            return ip_datagram;
        }
        else {
            return nullopt;
        }
    }
    else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) != ParseResult::NoError) {
            return nullopt;
        }
        _arp_table[arp_msg.sender_ip_address].mac = arp_msg.sender_ethernet_address;
        _arp_table[arp_msg.sender_ip_address].ttl = 30 * 1000; // active mappings last 30 seconds
        //learn a mapping from the "sender" fields, and send an ARP reply.
        if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
            ARPMessage reply;
            reply.opcode = ARPMessage::OPCODE_REPLY;
            reply.sender_ethernet_address = _ethernet_address;
            reply.sender_ip_address = _ip_address.ipv4_numeric();
            reply.target_ethernet_address = arp_msg.sender_ethernet_address;
            reply.target_ip_address = arp_msg.sender_ip_address;
            EthernetFrame arp_frame;
            arp_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_frame.header().src = _ethernet_address;
            arp_frame.header().dst = arp_msg.sender_ethernet_address;
            arp_frame.payload() = move(reply.serialize());
            _frames_out.push(arp_frame);
        }
        //如果学习到了ip就发送之前的ipdatagram
        if (_waiting_reply_arp.count(arp_msg.sender_ip_address)) {
            auto& q = _waiting_reply_arp[arp_msg.sender_ip_address].datagram_queue;
            while (!q.empty()) {
                EthernetFrame frame2;
                frame2.header().type = EthernetHeader::TYPE_IPv4;
                frame2.header().src = _ethernet_address;
                frame2.payload() = std::move(q.front()).serialize();
                frame2.header().dst = _arp_table[arp_msg.sender_ip_address].mac;
                _frames_out.push(frame2);
                q.pop();
            }
        }
        _waiting_reply_arp.erase(arp_msg.sender_ip_address);
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _timer += ms_since_last_tick;
    std::vector<uint32_t> delete_ip;
    //arp_table更新
    for (auto& [ip, item] : _arp_table) {
        if (item.ttl > ms_since_last_tick) {
            item.ttl -= ms_since_last_tick;
        }
        else
        {
            delete_ip.push_back(ip);
        }
    }
    for (auto ip : delete_ip) {
        _arp_table.erase(ip);
    }
    //超过5000ms重新发送arp广播
    for (auto& [ip, w_arp_item] : _waiting_reply_arp) {
        if (w_arp_item.ttl > ms_since_last_tick) {
            w_arp_item.ttl -= ms_since_last_tick;
        }
        else {
            _frames_out.push(w_arp_item.frame);
            w_arp_item.ttl = 5000;
        }
    }
}
