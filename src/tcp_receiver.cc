#include "tcp_receiver.hh"
#include "byte_stream.hh"
#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
    if(message.RST){
      reader().set_error();
    }
    if(!syn_){
      if(!message.SYN){
        return;
      } 
      syn_ = true;
      zero_point_ = message.seqno;
    }
    uint64_t checkpoint = writer().bytes_pushed() + 1;   //这个其实就是写到了的地方的绝对序列
    auto abs_seqno  = message.seqno.unwrap(zero_point_, checkpoint);
    uint64_t stream_index = abs_seqno + message.SYN - 1;
    reassembler_.insert(stream_index, message.payload, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg = {};
  msg.RST = reader().has_error();
  msg.window_size = static_cast<uint16_t>(min<uint64_t>(writer().available_capacity(),65535));
  if(syn_){
    uint64_t ack_abs = 1+ writer().bytes_pushed();
    if(writer().is_closed()){
      ack_abs+=1;
    }
    msg.ackno = Wrap32::wrap(ack_abs, zero_point_);
  }
  return msg;
}
