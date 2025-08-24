#include "byte_stream.hh"
#include <cstdint>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return close_;
}

void Writer::push( string data )
{
  if ( close_ ) {
    return;
  }
  uint64_t remain = available_capacity();
  uint64_t write_count = std::min( remain, data.size() );
  buffer_.insert( buffer_.end(), data.begin(), data.begin() + write_count );
  bytes_write += write_count;
  return;
}

void Writer::close()
{
  close_ = true;
  return;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_write;
}

bool Reader::is_finished() const
{
  return close_ && buffer_.empty();
}

uint64_t Reader::bytes_popped() const
{
  return bytes_read;
}

string_view Reader::peek() const
{
  return string_view( buffer_.data(), buffer_.size() );
}

void Reader::pop( uint64_t len )
{
  uint64_t pop_count = min( len, buffer_.size() );
  buffer_.erase( buffer_.begin(), buffer_.begin() + pop_count );
  bytes_read += pop_count;
  return;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}
