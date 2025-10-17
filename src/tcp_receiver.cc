#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
	if(message.SYN == false && SYN == 0)
		reassembler_.reader().set_error();
	if(message.SYN && SYN == 0)
	{
		if(reassembler_.writer().is_closed() && FIN == 1 && SYN == 1)
		{
			FIN--;
			SYN--;
		}
		zero_point = message.seqno;
		SYN++;
	}
	if(message.SYN && SYN_before == 0)
		SYN_before++;
	if(SYN)
	{
		uint64_t old_bytes_pending = reassembler_.writer().bytes_pushed();
//		cout << "first_index" << message.seqno.unwrap(zero_point, checkpoint) << endl;
//		cout << "checkpoint" << checkpoint << endl;
		if(message.SYN)
			reassembler_.insert(message.seqno.unwrap(zero_point, checkpoint), message.payload, message.FIN);
		else
			reassembler_.insert(message.seqno.unwrap(zero_point, checkpoint) - SYN, message.payload, message.FIN);
		uint64_t new_bytes_pending = reassembler_.writer().bytes_pushed();
		checkpoint += new_bytes_pending - old_bytes_pending;
		if(message.FIN)
			need_FIN++;
		if(reassembler_.bytes_pending() == 0 && FIN == 0 && need_FIN == 1)
		{
			FIN++;
			need_FIN--;
		}
	}
}

TCPReceiverMessage TCPReceiver::send() const
{
	TCPReceiverMessage ret_msg;
	ret_msg.window_size = reassembler_.writer().available_capacity() > UINT16_MAX ? UINT16_MAX : reassembler_.writer().available_capacity()	;
	if(SYN_before)
		ret_msg.ackno = Wrap32::wrap(checkpoint + SYN + FIN, zero_point);
	else 
		ret_msg.ackno = {};
	if(reassembler_.reader().has_error())
		ret_msg.RST = 1;
	else 
		ret_msg.RST = {};
	return ret_msg;
}
