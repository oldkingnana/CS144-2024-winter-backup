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

void RetransmissionTimer::time_reset()
{
	ms_time_ = 0;
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

//void TCPSender::push_(const TransmitFunction& transmit, TCPSenderMessage newTCPSdMsg) // private
//{
//
//}

void TCPSender::push( const TransmitFunction& transmit )
{
	if(input_.reader().bytes_buffered() == 0 && next_seq_ != isn_)
		return ;

	// send
	TCPSenderMessage newTCPSdMsg = make_empty_message();

	// 向msg塞入足量的数据
	if(RWSize_ == 0)
	{
		newTCPSdMsg.payload = input_.reader().peek().substr(0, 200);
		input_.reader().pop(200);
	}
	else if(RWSize_ <= 1452) 
	{
		newTCPSdMsg.payload = input_.reader().peek().substr(0, RWSize_);
		input_.reader().pop(RWSize_);
	}
	else 
	{
		newTCPSdMsg.payload = input_.reader().peek().substr(0, 1452);
		input_.reader().pop(1452);
	}

	// 判断是否是第一个包,第一个包需要塞入SYN
	if(next_seq_ == isn_)
	{
		newTCPSdMsg.SYN = true;
		next_seq_ += 1;
		SeqFNum_ += 1;
	}

	// 传输
	transmit(newTCPSdMsg);
	// 在窗口塞入传输的msg
	window_.push_back(newTCPSdMsg);

	// 更新公共资源
	next_seq_ += newTCPSdMsg.payload.length();
	SeqFNum_ += newTCPSdMsg.payload.length();
}

// 生成一个空的msg  
TCPSenderMessage TCPSender::make_empty_message() const
{
  	TCPSenderMessage TCPSdMsg;
  	TCPSdMsg.seqno = next_seq_;
  	return TCPSdMsg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
	if(window_.size() == 0)
		return ;

	// 如果已经有内容被接收了
	if(msg.ackno > next_seq_ - SeqFNum_)
	{
		// 重置公共资源
		CRT_ = 0;
		RTO_ = initial_RTO_ms_;
		SeqFNum_ -= msg.ackno - window_[0].seqno; 
		RWSize_ = static_cast<uint64_t>(msg.window_size);

		// 重置时间
		RT.time_reset();

		// 将已经接收的msg从窗口移除
		for(auto it = window_.begin() ; it.seqno + it.payload.length() - 1 < msg.ackno; )
			it = window_.erase(it);
	}
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
	if(window_.size() == 0)
		return ;

	// 时间流逝
	RT.time_plus(ms_since_last_tick);
	
	// 超时重传
	if(RT.is_timeout())
		retransmit(transmit, window_[0]);	
}

void TCPSender::retransmit(const TransmitFunction& transmit, TCPSenderMessage TCPSdMsg)
{
	// 重传
	transmit(TCPSdMsg);
	// 公共资源更新
	CRT_++;
	RTO_ *= 2;
}






