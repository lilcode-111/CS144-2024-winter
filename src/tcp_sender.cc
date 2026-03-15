#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return next_abs_seqno_ - ack_abs_seqno_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  while(true){
    uint64_t flight = next_abs_seqno_ - ack_abs_seqno_;
    uint64_t window = receive_window_size_;
    if(window==0){
      window = 1;
    }
    if(flight >= window){
      return;
    }
    uint64_t available = window -flight;
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap(next_abs_seqno_, isn_);
    if(!syn_sent_&&available){
      msg.SYN = true;
      syn_sent_ = true;
      available -= 1;
    }
    std::size_t payload_size = std::min(static_cast<size_t>(available),TCPConfig::MAX_PAYLOAD_SIZE);
    string data;
    size_t n =min(payload_size,reader().bytes_buffered());
    read(input_.reader(),n,data);
    msg.payload = data;
    available -= data.size();
    if(!fin_sent_&& input_.reader().is_finished()&&available>0){
      fin_sent_ = true;
      msg.FIN = true;
      available -= 1;
    }
    if(!msg.SYN&&msg.payload.empty()&&!msg.FIN){
      return;
    }
    msg.RST = input_.reader().has_error();
    transmit(msg);
    outstanding_.push_back(OutstandingSegment{msg,next_abs_seqno_,msg.sequence_length()});
    next_abs_seqno_ += msg.sequence_length();
    if(!timer_running_){
      timer_running_ = true;
      timer_elapsed_ms_ = 0;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap(next_abs_seqno_, isn_);
  msg.RST = input_.has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  receive_window_size_ = msg.window_size;
  if(msg.RST){
    input_.set_error();
  }

  if(!msg.ackno.has_value()){
    return;
  }
  uint64_t ack_abs = msg.ackno->unwrap(isn_, next_abs_seqno_);
  if(ack_abs>next_abs_seqno_){
    return;
  }
  if(ack_abs<=ack_abs_seqno_){
    return;
  }
  ack_abs_seqno_ = ack_abs;
  while(!outstanding_.empty()&&outstanding_.front().abs_seqno+outstanding_.front().len<=ack_abs_seqno_){
    outstanding_.pop_front();
  }
  current_RTO_ms_ = initial_RTO_ms_;
  consecutive_retransmissions_ = 0;
  timer_elapsed_ms_ = 0;

  timer_running_ = !outstanding_.empty();
  
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
   if(!timer_running_||outstanding_.empty()){
      return;
   }
   timer_elapsed_ms_ +=ms_since_last_tick;
   if(timer_elapsed_ms_>=current_RTO_ms_){
      transmit(outstanding_.front().msg);
      if(receive_window_size_>0){
        consecutive_retransmissions_++;
        current_RTO_ms_*=2;
      }
      timer_elapsed_ms_ = 0;
   }
}
