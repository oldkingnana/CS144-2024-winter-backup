#include "byte_stream.hh"
#include <cstdint>
#include <iostream>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) , buffer_({}) {}

bool Writer::is_closed() const
{
 	return close_;
}

void Writer::push( string data )
{
	// safe check
	if(static_cast<uint64_t>(data.length()) > available_capacity()) 
	{
		if(available_capacity() != 0)
		{
			bytes_pushed_ += data.substr(0, available_capacity()).length();
			buffer_ += data.substr(0, available_capacity());	
		}
	}
	else 
	{
		if(!buffer_.length())
		{
			bytes_pushed_ += data.length();
			buffer_ = std::move(data);	
		}
		else
		{
			bytes_pushed_ += data.length();
			buffer_ += data;
		}
	}

	return;
}

void Writer::close()
{
	close_ = true;
}

uint64_t Writer::available_capacity() const
{
	return capacity_ - buffer_.length();
}

uint64_t Writer::bytes_pushed() const
{
	return bytes_pushed_;
}

bool Reader::is_finished() const
{
	return close_ && !static_cast<bool>(bytes_buffered()); 
}

uint64_t Reader::bytes_popped() const
{
  	return bytes_popped_;
}

string_view Reader::peek() const
{
	return std::string_view(buffer_.c_str(), buffer_.length() < READ_BYTES_SIZE ? buffer_.length() : READ_BYTES_SIZE);
}

void Reader::pop( uint64_t len )
{
	// safe check 
	if(buffer_.length() < len)
		bytes_popped_ += buffer_.length();
	else 
		bytes_popped_ += len;
	buffer_.erase(0, len);
}

uint64_t Reader::bytes_buffered() const
{
  	return buffer_.length();
}
