#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

#include <deque>

#include <iostream>

class TCPSender
{
public:
  	/* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  	TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
	, isn_( isn )
	, initial_RTO_ms_( initial_RTO_ms )
	, CRT_(0)
	, SeqFNum_(0)
	, next_seq_(isn.unwrap(isn, 0))
	// , RTO_(initial_RTO_ms)
	, RWSize_(0)
	, last_ackno_(0)
	, is_FINed_(false)
	, is_SYNed_(false)
	, zero_window_probe_(false)
	, window_({})
	, RT_(initial_RTO_ms_)
	{
		//std::cout << "TCPSender init!" << std::endl;
	}

	// friend bool RetransmissionTimer::is_timeout();

  	/* Generate an empty TCPSenderMessage */
  	TCPSenderMessage make_empty_message() const;

  	/* Receive and process a TCPReceiverMessage from the peer's receiver */
  	void receive( const TCPReceiverMessage& msg );

  	/* Type of the `transmit` function that the push and tick methods can use to send messages */
  	using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  	/* Push bytes from the outbound stream */
  	void push( const TransmitFunction& transmit );

  	/* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  	void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  	// Accessors
  	uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  	uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
    Writer& writer() { return input_.writer(); }
    const Writer& writer() const { return input_.writer(); }
  
    // Access input stream reader, but const-only (can't read from outside)
    const Reader& reader() const { return input_.reader(); }

private: 
	void retransmit(const TransmitFunction& transmit, TCPSenderMessage TCPSdMsg);

	//void push_(const TransmitFunction& transmit, TCPSenderMessage newTCPSdMsg);
  	void push_single_msg_( const TransmitFunction& transmit );

	void push_SYN_(const TransmitFunction& transmit );

	void push_FIN_(const TransmitFunction& transmit );

	class RetransmissionTimer
	{
	public:
		RetransmissionTimer(uint64_t initial_RTO_ms_)
		: ms_time_(0)
		, RTO_(initial_RTO_ms_)
		{}
	
		~RetransmissionTimer()
		{}
	
		bool is_timeout(); 
	
		void time_plus(const uint64_t& ms_since_last_tick);
	
		void time_reset();

		void RTO_reset(uint64_t initial_RTO_ms_);

		void RTO_multi();

		uint64_t get_time();

		uint64_t get_RTO();

	private:
		uint64_t ms_time_;
		uint64_t RTO_; // 超时标准时间
	};

private:
    // Variables initialized in constructor
    ByteStream input_;
    Wrap32 isn_;
    uint64_t initial_RTO_ms_;

	uint64_t CRT_; // consecutive retransmission times  最近超时次数
	uint64_t SeqFNum_; // seq in flight number  正在传输的字节数

	uint64_t next_seq_; // 下一个包的seq
	uint64_t RWSize_; // 接收方窗口大小
	uint64_t last_ackno_; // 最近一次接收的ACK的绝对位置

	bool is_FINed_;
	bool is_SYNed_;

	bool zero_window_probe_;

	std::deque<TCPSenderMessage> window_; // 窗口
	RetransmissionTimer RT_; // 重传计时器
};
