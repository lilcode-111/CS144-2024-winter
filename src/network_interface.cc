#include <cstdint>
#include <iostream>

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  ,datagrams_received_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
    uint32_t next_hop_ip = next_hop.ipv4_numeric();
    auto it = arp_table_.find(next_hop_ip);
    if(it!=arp_table_.end()){
      EthernetFrame frame;
      frame.header.src = ethernet_address_;  
      frame.header.dst = it->second.ethernet_address;
      frame.header.type = EthernetHeader::TYPE_IPv4;
      frame.payload = serialize(dgram);
      transmit(frame);
      return;
    }
    waiting_dgram[next_hop_ip].push(dgram);
    if(!arp_request_timer.count(next_hop_ip)){
      ARPMessage  arp;
      arp.opcode = ARPMessage::OPCODE_REQUEST;
      arp.sender_ethernet_address = ethernet_address_;
      arp.sender_ip_address = ip_address_.ipv4_numeric();
      arp.target_ip_address = next_hop_ip;
      arp.target_ethernet_address = {};

      EthernetFrame frame;
      frame.header.src = ethernet_address_;
      frame.header.dst = ETHERNET_BROADCAST;
      frame.header.type = EthernetHeader::TYPE_ARP;
      frame.payload = serialize(arp);
      transmit(frame);
      arp_request_timer[next_hop_ip] = 5000;
    }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if(frame.header.dst != ethernet_address_&&frame.header.dst!=ETHERNET_BROADCAST){
    return;
  }
  if(frame.header.type==EthernetHeader::TYPE_IPv4){
    InternetDatagram dgram ;
    Parser parser { frame.payload };
    dgram.parse(parser);
    datagrams_received_.push(dgram);
    return;
  }
  if(frame.header.type != EthernetHeader::TYPE_ARP){
    return;
  }
  ARPMessage msg;
  Parser parser{frame.payload};
  msg.parse(parser);
  if (parser.has_error()) {
    return;
  } 
  arp_table_[msg.sender_ip_address] = {msg.sender_ethernet_address,30000};
  arp_request_timer.erase(msg.sender_ip_address);
  auto it = waiting_dgram.find(msg.sender_ip_address);
  if(it!=waiting_dgram.end()){
    while (!it->second.empty()) {
      EthernetFrame out;
      out.header.src = ethernet_address_;
      out.header.dst = msg.sender_ethernet_address;
      out.header.type = EthernetHeader::TYPE_IPv4;
      out.payload = serialize(it->second.front());
      transmit(out);
      it->second.pop();
    }
    waiting_dgram.erase(it);
  }

  if(msg.opcode == ARPMessage::OPCODE_REQUEST&&msg.target_ip_address == ip_address_.ipv4_numeric()){
    ARPMessage reply;
    reply.opcode = ARPMessage::OPCODE_REPLY;
    reply.sender_ethernet_address = ethernet_address_;
    reply.sender_ip_address = ip_address_.ipv4_numeric();
    reply.target_ethernet_address = msg.sender_ethernet_address;
    reply.target_ip_address = msg.sender_ip_address;
    
    EthernetFrame out;
    out.header.src = ethernet_address_;
    out.header.dst = msg.sender_ethernet_address;
    out.header.type = EthernetHeader::TYPE_ARP;
    out.payload = serialize(reply);
    transmit(out);
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for(auto it=arp_table_.begin();it!=arp_table_.end();){
    if(it->second.ttl_ms<=ms_since_last_tick){
      it = arp_table_.erase(it);
    } else {
      it->second.ttl_ms-=ms_since_last_tick;
      ++it;
    }
  }
  for(auto it= arp_request_timer.begin();it!=arp_request_timer.end();){
    if(it->second<=ms_since_last_tick){
      it = arp_request_timer.erase(it);
    } else {
      it->second-=ms_since_last_tick;
      ++it;
    }
  }
}
