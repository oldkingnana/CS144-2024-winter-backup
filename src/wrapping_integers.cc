#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{	
	return Wrap32(n) + zero_point.raw_value_;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
	uint64_t left_low = 0;
	uint64_t right_low = 0;
	if(raw_value_ >= zero_point.raw_value_)
	{
		left_low = static_cast<uint64_t>(raw_value_) - zero_point.raw_value_;
		right_low = static_cast<uint64_t>(raw_value_) - zero_point.raw_value_ + 0xFFFFFFFFULL + 1ULL;
	}
	else 
	{
		left_low = (((static_cast<uint64_t>(raw_value_) + 0xFFFFFFFFULL) + 1ULL) - zero_point.raw_value_) & 0x00000000FFFFFFFFULL;
		right_low = left_low + 0xFFFFFFFFULL + 1ULL;
	}
	
	uint64_t checkpoint_low = checkpoint & 0x00000000FFFFFFFFULL;
	uint64_t checkpoint_low_lwindow = (checkpoint_low > 0x80000000ULL) ? (checkpoint_low - 0x80000000ULL) : 0ULL;
	uint64_t checkpoint_low_rwindow = checkpoint_low + 0x80000000ULL;

	if(checkpoint_low_rwindow <= left_low)
	{
		checkpoint_low += 0xFFFFFFFFULL + 1ULL;
		checkpoint_low_lwindow = checkpoint_low - 0x80000000ULL;
		checkpoint_low_rwindow = checkpoint_low + 0x80000000ULL;
//		cout << "checkpoint_low -> " << checkpoint_low << endl;
//		cout << "checkpoint_low_lwindow -> " << checkpoint_low_lwindow << endl;
//		cout << "checkpoint_low_rwindow -> " << checkpoint_low_rwindow << endl;
	}

//	cout << "checkpoint: " << checkpoint << endl;
//	cout << "zero_point: " << zero_point.raw_value_ << endl;
//	cout << "raw_value_: " << raw_value_ << endl;
//	cout << "left_low: " << left_low << endl;
//	cout << "right_low: " << right_low << endl;
//	cout << "checkpoint_low: " << checkpoint_low << endl;
//	cout << "checkpoint_low_lwindow: " << checkpoint_low_lwindow << endl;
//	cout << "checkpoint_low_rwindow: " << checkpoint_low_rwindow << endl;
	
	if(checkpoint_low_lwindow <= left_low && left_low < checkpoint_low_rwindow)
		return left_low + (checkpoint > checkpoint_low ? (checkpoint - checkpoint_low) : 0);		
	else
		return right_low + (checkpoint > checkpoint_low ? (checkpoint - checkpoint_low) : 0);		
	
	return {};
}
