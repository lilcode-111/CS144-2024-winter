#include "reassembler.hh"
#include <cstdint>
#include <type_traits>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
  eof_seen_ = true;
  eof_index_ = first_index + data.size();
  }

  uint64_t index = output_.writer().bytes_pushed();
  uint64_t right_bound = index + output_.writer().available_capacity();
  if(first_index <= index){
    if(first_index+data.size()>index){
      uint64_t end = first_index + min(data.size(),right_bound);
      string content = data.substr(index- first_index,end-index);
      output_.writer().push(content);
      uint64_t curr_index = output_.writer().bytes_pushed();
      for(uint64_t i = index;i<curr_index;++i){
        uint64_t slot = i % capacity_;
        if(present_[slot]){
          present_[slot] = false;
          bytes_pending_ --;
        }
      }
      string assembled;
      while(assembled.size() < output_.writer().available_capacity()){
        uint64_t slot = curr_index % capacity_;
        if(!present_[slot]){
          break;
        }
        assembled += buffer_[slot];
        present_[slot] = false;
        ++curr_index;
        --bytes_pending_;
      }
      if(!assembled.empty()){
        output_.writer().push(assembled);
      }
    } 
  } else {
    for(uint64_t i = 0; i<data.size();++i ){
      uint64_t pos = i+first_index;
      if(pos>=right_bound){
        break;
      }
      uint64_t slot = pos %capacity_;
      if(!present_[slot]){
      buffer_[slot] = data[i];
      present_[slot] = true;
      ++bytes_pending_;
      }
    }
  }
  if ( eof_seen_ && output_.writer().bytes_pushed() == eof_index_ ) {   // 这个判断还比较细节
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}
