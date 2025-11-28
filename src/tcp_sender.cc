#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <cstdio>
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

uint64_t TCPSender::RetransmissionTimer::get_time()
{
	return ms_time_;
}

uint64_t TCPSender::RetransmissionTimer::get_RTO()
{
	return RTO_;
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
	// send SYN
	if(!is_SYNed_)
		push_single_msg_(transmit);

	while(static_cast<int64_t>(RWSize_ - SeqFNum_) > 0 && !is_FINed_ && input_.reader().bytes_buffered() > 0)
		push_single_msg_(transmit);

	// send FIN
	if(!is_FINed_ && RWSize_ - SeqFNum_ > 0 && input_.reader().is_finished() && input_.reader().bytes_buffered() == 0)
		push_single_msg_(transmit);
}

//void TCPSender::push_SYN_(const TransmitFunction& transmit)
//{
//	TCPSenderMessage newTCPSdMsg = make_empty_message();
//
//	// 更新公共资源
//	newTCPSdMsg.SYN = true;
//	next_seq_ += 1;
//	SeqFNum_ += 1;
//	is_SYNed_ = true;
//
//	transmit(newTCPSdMsg);
//	// 在窗口塞入传输的msg
//	window_.push_back(newTCPSdMsg);
//}
//
//void TCPSender::push_FIN_(const TransmitFunction& transmit)
//{
//	TCPSenderMessage newTCPSdMsg = make_empty_message();
//
//	// 更新公共资源
//	newTCPSdMsg.FIN = true;
//	next_seq_ += 1;
//	SeqFNum_ += 1;
//	is_FINed_ = true;
//
//	transmit(newTCPSdMsg);
//	// 在窗口塞入传输的msg
//	window_.push_back(newTCPSdMsg);
//}

void TCPSender::push_single_msg_( const TransmitFunction& transmit )
{
	TCPSenderMessage newTCPSdMsg = make_empty_message();

	std::cout << std::endl;
	std::cout << "push begin!" << std::endl;
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

	// std::cout << "flag" << std::endl;

	if(!is_SYNed_)
	{
		newTCPSdMsg.SYN = true;
		is_SYNed_ = true;
	}

	// 向msg塞入足量的数据
	if(0 < RWSize_ - SeqFNum_ - newTCPSdMsg.sequence_length() && RWSize_ - SeqFNum_ -newTCPSdMsg.sequence_length() <= 1000) 
	{
		newTCPSdMsg.payload = input_.reader().peek().substr(0, RWSize_ - SeqFNum_);
		input_.reader().pop(RWSize_ - SeqFNum_);
	}
	else if(1000 < RWSize_ - SeqFNum_ - newTCPSdMsg.sequence_length()) 
	{
		newTCPSdMsg.payload = input_.reader().peek().substr(0, 1000);
		input_.reader().pop(1000);
	}

	if(!is_FINed_ && RWSize_ - SeqFNum_ - newTCPSdMsg.sequence_length() > 0 && input_.reader().is_finished() && input_.reader().bytes_buffered() == 0)
	{
		newTCPSdMsg.FIN = true;
		is_FINed_ = true;
	}

	// 更新公共资源
	next_seq_ += newTCPSdMsg.sequence_length();
	SeqFNum_ += newTCPSdMsg.sequence_length();

	if(newTCPSdMsg.sequence_length() == 0)
		return ;

	std::cout << std::endl;
	std::cout << "push finish!" << std::endl;
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

	std::flush(std::cout);
	std::flush(std::cerr);

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
	  << " first msg time=" << RT_.get_time() << std::endl
	  << " first msg RTO=" << RT_.get_RTO() << std::endl;

	if(window_.size() != 0)
	{
		std::cerr
		  << " window_[0].seqno=" << window_[0].seqno.unwrap(isn_, last_ackno_) << std::endl
		  << " window_[0]=" << window_[0].payload << std::endl;
	}


	  RWSize_ = static_cast<uint64_t>(msg.window_size);
		
	uint64_t ackno_u64 = (*msg.ackno).unwrap(isn_, last_ackno_);
	
	// ACK合法性判断
	// 超了--无效
	if(ackno_u64 > next_seq_)
	{}
	// 如果已经有内容被接收了
	else if(ackno_u64 > next_seq_ - SeqFNum_)
	{
		uint64_t first_seq = window_[0].seqno.unwrap(isn_, last_ackno_);

		// 将已经接收的msg从窗口移除
		for(auto it = window_.begin() ; it != window_.end() ; )
		{
			uint64_t seqno_u64 = it->seqno.unwrap(isn_, last_ackno_);

			if(seqno_u64 + it->sequence_length() <= ackno_u64)
				it = window_.erase(it);
			else if(seqno_u64 < ackno_u64 && ackno_u64 < seqno_u64 + it->sequence_length())
			{
				// 鲁棒性优化
				uint64_t need_del_len = ackno_u64 - seqno_u64;

				if(it->FIN == true)
				{
					it->FIN = false;
					need_del_len -= 1;
					it->seqno = it->seqno + 1;
				}
				
				if(need_del_len > 0)
				{
					it->payload = it->payload.substr(need_del_len);
					it->seqno = it->seqno + need_del_len;	
				}

				break;
			}
			else 
				break;
		}

		// 重置公共资源
		CRT_ = 0;
		RT_.RTO_reset(initial_RTO_ms_);
		SeqFNum_ -= ackno_u64 - first_seq; 

		// 重置时间
		RT_.time_reset();
		
		last_ackno_ = (*msg.ackno).unwrap(isn_, last_ackno_);
	}
	// 太少--无效
	else 
	{}
	
	std::cout << std::endl;
	std::cerr << "receive finish!" << std::endl;
	std::cerr
      << " next_seq=" << next_seq_ << std::endl
	  << " SeqFNum=" << SeqFNum_ << std::endl
      << " RWSize=" << RWSize_ << std::endl
      << " reader_finished=" << input_.reader().is_finished() << std::endl
      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl
	  << " msg.ackno=" << (*msg.ackno).unwrap(isn_, last_ackno_) << std::endl
	  << " first msg time=" << RT_.get_time() << std::endl
	  << " first msg RTO=" << RT_.get_RTO() << std::endl;
	
	if(window_.size() != 0)
	{
		std::cerr
		  << " window_[0].seqno=" << window_[0].seqno.unwrap(isn_, last_ackno_) << std::endl
		  << " window_[0]=" << window_[0].payload << std::endl;
	}
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
	std::cout << std::endl;
	std::cerr << "tick begin!" << std::endl;
	std::cerr
      << " next_seq=" << next_seq_ << std::endl
	  << " SeqFNum=" << SeqFNum_ << std::endl
      << " RWSize=" << RWSize_ << std::endl
      << " reader_finished=" << input_.reader().is_finished() << std::endl
      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl
	  << " first msg time=" << RT_.get_time() << std::endl
	  << " first msg RTO=" << RT_.get_RTO() << std::endl;


	if(window_.size() == 0)
		return ;

	// 时间流逝
	RT_.time_plus(ms_since_last_tick);
	
	// 超时重传
	if(RT_.is_timeout())
		retransmit(transmit, window_[0]);	
	
	std::cout << std::endl;
	std::cerr << "tick finish!" << std::endl;
	std::cerr
      << " next_seq=" << next_seq_ << std::endl
	  << " SeqFNum=" << SeqFNum_ << std::endl
      << " RWSize=" << RWSize_ << std::endl
      << " reader_finished=" << input_.reader().is_finished() << std::endl
      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl
	  << " first msg time=" << RT_.get_time() << std::endl
	  << " first msg RTO=" << RT_.get_RTO() << std::endl;
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









