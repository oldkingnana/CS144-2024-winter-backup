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

uint64_t TCPSender::sequence_numbers_in_flight() const // OK
{
  	// Your code here.
  	return {};
}

uint64_t TCPSender::consecutive_retransmissions() const // OK
{
  	// Your code here.
  	return {};
}

void TCPSender::push( const TransmitFunction& transmit )
{
	// Your code here.
	(void)transmit;

TCPSenderMessage TCPSender::make_empty_message() const // OK
{
  	TCPSenderMessage TCPSdMsg;
  	TCPSdMsg.seqno = isn_;
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
