#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  auto t = static_cast<uint32_t>(n) + zero_point.raw_value_;
  return Wrap32(t);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t n =this->raw_value_ - zero_point.raw_value_;
  uint64_t t = (checkpoint& 0xffffffff00000000ULL) + n;
  auto ret = t;
  const uint64_t MOD = 1ULL << 32;
  auto dist = []( uint64_t a, uint64_t b ) -> uint64_t {
        return a > b ? a - b : b - a;
    };
  if(dist(t+MOD,checkpoint)<dist(t,checkpoint)){
    return ret = t +MOD;
  } 
  if(t>=MOD&&dist(t-MOD, checkpoint) < dist(t, checkpoint)) {
    return ret = t - MOD;
  }
  return ret;
}


