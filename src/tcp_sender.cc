#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

// ============== RetransmissionTimer =================

bool RetransmissionTimer::is_timeout() const 
{
	return ms_time_ > RTO_;
}

void RetransmissionTimer::time_plus(const uint64_t& ms_since_last_tick)
{
	ms_time_ += ms_since_last_tick;	
}


// =================== TCPSender ======================

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  	return SeqFNum_;
}

uint64_t TCPSender::consecutive_retransmissions() const 
{
  	return CRT_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
	if(input_.reader().bytes_buffered() == 0 && next_seq_ != isn_)
		return ;

	// send
	TCPSenderMessage newTCPSdMsg = make_empty_message();
	newTCPSdMsg.payload = input_.reader().peek().substr(0, 200);
	input_.reader().pop(200);
	if(next_seq_ == isn_)
	{
		newTCPSdMsg.SYN = true;
		next_seq_ += 1;
		SeqFNum_ += 1;
	}
	transmit(newTCPSdMsg);
	window_.emplace_back(newTCPSdMsg, RetransmissionTimer(initial_RTO_ms_));

	// update public resource
	next_seq_ += newTCPSdMsg.payload.length();
	SeqFNum_ += newTCPSdMsg.payload.length();
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  	TCPSenderMessage TCPSdMsg;
  	TCPSdMsg.seqno = next_seq_;
  	return TCPSdMsg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  	// Your code here.
  	(void)msg;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  	// Your code here.
  	(void)ms_since_last_tick;
  	(void)transmit;
  	(void)initial_RTO_ms_;
}
