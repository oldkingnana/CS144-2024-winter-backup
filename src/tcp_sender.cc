#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <ostream>

using namespace std;

// ============== RetransmissionTimer =================

bool TCPSender::RetransmissionTimer::is_timeout()
{
	return ms_time_ >= RTO_;
}

void TCPSender::RetransmissionTimer::time_plus(const uint64_t& ms_since_last_tick)
{
	ms_time_ += ms_since_last_tick;	
}

void TCPSender::RetransmissionTimer::time_reset()
{
	ms_time_ = 0;
}

void TCPSender::RetransmissionTimer::RTO_reset(uint64_t initial_RTO_ms_)
{
	RTO_ = initial_RTO_ms_;
}


void TCPSender::RetransmissionTimer::RTO_multi()
{
	RTO_ *= 2;
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
//	if(input_.reader().bytes_buffered() == 0 && Wrap32::wrap(next_seq_, isn_) != isn_)
//		return ;

	// send
	TCPSenderMessage newTCPSdMsg = make_empty_message();

	// 判断是否是第一个包,第一个包需要塞入SYN
	if(Wrap32::wrap(next_seq_, isn_) == isn_)
	{
		newTCPSdMsg.SYN = true;
		next_seq_ += 1;
		SeqFNum_ += 1;
	}
	
	// 向msg塞入足量的数据
	if(RWSize_ == 0)
	{
		newTCPSdMsg.payload = input_.reader().peek().substr(0, 200);
		input_.reader().pop(200);
	}
	else if(RWSize_ - SeqFNum_ <= 1452) 
	{
		newTCPSdMsg.payload = input_.reader().peek().substr(0, RWSize_ - SeqFNum_);
		input_.reader().pop(RWSize_ - SeqFNum_);
	}
	else 
	{
		newTCPSdMsg.payload = input_.reader().peek().substr(0, 1452);
		input_.reader().pop(1452);
	}
	
	// 更新公共资源
	next_seq_ += newTCPSdMsg.payload.length();
	SeqFNum_ += newTCPSdMsg.payload.length();

	// FIN检查 
	if(input_.reader().is_finished() && input_.reader().bytes_buffered() == 0 && RWSize_ - SeqFNum_ - newTCPSdMsg.payload.length() > 0)
	{
		std::cout << std::endl;
		std::cout << "FIN check" << std::endl;
		std::cout << "next_seq_: " << next_seq_ << std::endl;

		newTCPSdMsg.FIN = true;
		next_seq_ += 1;
		SeqFNum_ += 1;

		std::cout << "msg.FIN: " << newTCPSdMsg.FIN << std::endl;
	}

	std::cout << std::endl;
	std::cerr
	  << " seq_len=" << newTCPSdMsg.sequence_length() << std::endl
	  << " SYN=" << newTCPSdMsg.SYN << std::endl
      << " payload=" << newTCPSdMsg.payload.size() << std::endl
      << " FIN=" << newTCPSdMsg.FIN << std::endl
      << " next_seq=" << next_seq_ << std::endl
	  << " SeqFNum=" << SeqFNum_ << std::endl
      << " RWSize=" << RWSize_ << std::endl
      << " reader_finished=" << input_.reader().is_finished() << std::endl
      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl;

	std::cerr << " payload is " << newTCPSdMsg.payload << std::endl;

	// 无效发送检查
	if(newTCPSdMsg.sequence_length() == 0)
	{
		std::cout << std::endl;
		std::cout << "send none" << std::endl;
		return ;
	}
	// 传输
	std::cout << std::endl << "send!" << std::endl;
	transmit(newTCPSdMsg);
	// 在窗口塞入传输的msg
	window_.push_back(newTCPSdMsg);

}

// 生成一个空的msg  
TCPSenderMessage TCPSender::make_empty_message() const
{
  	TCPSenderMessage TCPSdMsg;
  	TCPSdMsg.seqno = Wrap32::wrap(next_seq_, isn_);
  	return TCPSdMsg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
	std::cout << std::endl;
	std::cerr << "receive!" << std::endl;
	std::cerr
      << " next_seq=" << next_seq_ << std::endl
	  << " SeqFNum=" << SeqFNum_ << std::endl
      << " RWSize=" << RWSize_ << std::endl
      << " reader_finished=" << input_.reader().is_finished() << std::endl
      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl
	  << " msg.ackno=" << (*msg.ackno).unwrap(isn_, last_ackno_) << std::endl
	  << " window_[0].seqno=" << window_[0].seqno.unwrap(isn_, last_ackno_)
	  << std::endl;

	RWSize_ = static_cast<uint64_t>(msg.window_size);

	// 如果已经有内容被接收了
	if((*msg.ackno).unwrap(isn_, last_ackno_) > next_seq_ - SeqFNum_)
	{
		// 重置公共资源
		CRT_ = 0;
		RT_.RTO_reset(initial_RTO_ms_);
		SeqFNum_ -= (*msg.ackno).unwrap(isn_, last_ackno_) - window_[0].seqno.unwrap(isn_, last_ackno_); 

		// 重置时间
		RT_.time_reset();

		// 将已经接收的msg从窗口移除
		for(auto it = window_.begin() ; it != window_.end() && it->seqno.unwrap(isn_, last_ackno_) + it->payload.length() < (*msg.ackno).unwrap(isn_, last_ackno_) + static_cast<uint32_t>(1); )
			it = window_.erase(it);

		last_ackno_ = (*msg.ackno).unwrap(isn_, last_ackno_);
	}
	
	std::cout << std::endl;
	std::cerr << "receive finish!" << std::endl;
	std::cerr
      << " next_seq=" << next_seq_ << std::endl
	  << " SeqFNum=" << SeqFNum_ << std::endl
      << " RWSize=" << RWSize_ << std::endl
      << " reader_finished=" << input_.reader().is_finished() << std::endl
      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
	if(window_.size() == 0)
		return ;

	// 时间流逝
	RT_.time_plus(ms_since_last_tick);
	
	// 超时重传
	if(RT_.is_timeout())
		retransmit(transmit, window_[0]);	
}

void TCPSender::retransmit(const TransmitFunction& transmit, TCPSenderMessage TCPSdMsg)
{
	// 重传
	transmit(TCPSdMsg);
	// 公共资源更新
	CRT_++;
	RT_.RTO_multi();
	RT_.time_reset();
}









