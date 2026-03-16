#include "router.hh"
#include "address.hh"
#include "ipv4_datagram.hh"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  _routes.push_back({route_prefix,prefix_length,next_hop,interface_num});
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for(auto interface:_interfaces){
    auto &queue = interface->datagrams_received();
    while(!queue.empty()){
      InternetDatagram dgram =std::move(queue.front());
      queue.pop();
      const uint32_t dst = dgram.header.dst;
      const RouteEntry* best_match = nullptr;
      for(auto & route:_routes){
        uint32_t mask;
        if(route.prefix_length == 0){
          mask = 0;
        } else {
          mask = 0xFFFFFFFF << (32-route.prefix_length);
        }
        if((dst&mask) == (route.route_prefix&mask)){
          if(best_match == nullptr || route.prefix_length>best_match->prefix_length){
            best_match = &route;
          }
        }
      }
      if(best_match == nullptr){
        continue;
      }
      if(dgram.header.ttl<=1){
        continue;
      }
      dgram.header.ttl--;
      dgram.header.compute_checksum();

      Address next = best_match->next_hop.has_value()?best_match->next_hop.value():Address::from_ipv4_numeric(dgram.header.dst);
      _interfaces.at(best_match->interface_num) ->send_datagram(dgram,next);
      }
  }
}
