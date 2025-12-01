#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <cstdio>
#include <ostream>

using namespace std;

// =================== TCPSender ======================


// ***** public function *****

// 生成一个空的msg  
TCPSenderMessage TCPSender::make_empty_message() const
{
  	TCPSenderMessage TCPSdMsg;
  	TCPSdMsg.seqno = Wrap32::wrap(next_seq_, isn_);
	if(should_RST_)
		TCPSdMsg.RST = true;
	if(input_.reader().has_error())
		TCPSdMsg.RST = true;
  	return TCPSdMsg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
//	std::cout << std::endl;
//	std::cerr << "receive!" << std::endl;
//	std::cerr
//      << " next_seq=" << next_seq_ << std::endl
//	  << " SeqFNum=" << SeqFNum_ << std::endl
//      << " RWSize=" << RWSize_ << std::endl
//      << " reader_finished=" << input_.reader().is_finished() << std::endl
//      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl
//	  << " msg.ackno=" << (*msg.ackno).unwrap(isn_, last_ackno_) << std::endl
//	  << " first msg time=" << RT_.get_time() << std::endl
//	  << " first msg RTO=" << RT_.get_RTO() << std::endl;

//	if(window_.size() != 0)
//	{
//		std::cerr
//		  << " window_[0].seqno=" << window_[0].seqno.unwrap(isn_, last_ackno_) << std::endl
//		  << " window_[0]=" << window_[0].payload << std::endl
//		  << " window_[0].FIN=" << window_[0].FIN << std::endl;
//	}

	// 进入单字节探测模式
	if(msg.window_size == 0)
	{
		receive_ZWP_mode_(msg);
	}
	// 进入正常模式
	else if(msg.window_size != 0)
	{
		receive_simple_mode_(msg);
	}
//	std::cout << std::endl;
//	std::cerr << "receive finish!" << std::endl;
//	std::cerr
//      << " next_seq=" << next_seq_ << std::endl
//	  << " SeqFNum=" << SeqFNum_ << std::endl
//      << " RWSize=" << RWSize_ << std::endl
//      << " reader_finished=" << input_.reader().is_finished() << std::endl
//      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl
//	  << " msg.ackno=" << (*msg.ackno).unwrap(isn_, last_ackno_) << std::endl
//	  << " first msg time=" << RT_.get_time() << std::endl
//	  << " first msg RTO=" << RT_.get_RTO() << std::endl;
//	
//	if(window_.size() != 0)
//	{
//		std::cerr
//		  << " window_[0].seqno=" << window_[0].seqno.unwrap(isn_, last_ackno_) << std::endl
//		  << " window_[0]=" << window_[0].payload << std::endl
//		  << " window_[0].FIN=" << window_[0].FIN << std::endl;
//	}
}


void TCPSender::push( const TransmitFunction& transmit )
{
	if(zero_window_probe_ && !is_zero_window_probeing_)
	{
		push_ZWP_mode_(transmit);
	}
	else if(!zero_window_probe_)
	{
		// send SYN
		if(!is_SYNed_)
			push_simple_mode_(transmit);
	
		while(static_cast<int64_t>(RWSize_ - SeqFNum_) > 0 && !is_FINed_ && input_.reader().bytes_buffered() > 0)
			push_simple_mode_(transmit);
	
		// send FIN
		if(!is_FINed_ && RWSize_ - SeqFNum_ > 0 && input_.reader().is_finished() && input_.reader().bytes_buffered() == 0)
			push_simple_mode_(transmit);
	}
	else if(zero_window_probe_ && is_zero_window_probeing_)
	{}
	else 
	{
		std::cerr << "push error !!" << std::endl;
		return ;
	}
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
//	std::cout << std::endl;
//	std::cerr << "tick begin!" << std::endl;
//	std::cerr
//      << " next_seq=" << next_seq_ << std::endl
//	  << " SeqFNum=" << SeqFNum_ << std::endl
//      << " RWSize=" << RWSize_ << std::endl
//      << " reader_finished=" << input_.reader().is_finished() << std::endl
//      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl
//	  << " first msg time=" << RT_.get_time() << std::endl
//	  << " first msg RTO=" << RT_.get_RTO() << std::endl;

	// 单字节探测模式
	if(zero_window_probe_)
		tick_ZWP_mode_(ms_since_last_tick, transmit);
	// 普通模式
	else 
		tick_simple_mode_(ms_since_last_tick, transmit);	
//	std::cout << std::endl;
//	std::cerr << "tick finish!" << std::endl;
//	std::cerr
//      << " next_seq=" << next_seq_ << std::endl
//	  << " SeqFNum=" << SeqFNum_ << std::endl
//      << " RWSize=" << RWSize_ << std::endl
//      << " reader_finished=" << input_.reader().is_finished() << std::endl
//      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl
//	  << " first msg time=" << RT_.get_time() << std::endl
//	  << " first msg RTO=" << RT_.get_RTO() << std::endl;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  	return SeqFNum_;
}

uint64_t TCPSender::consecutive_retransmissions() const 
{
  	return CRT_;
}


// ***** simple mode *****

void TCPSender::push_simple_mode_( const TransmitFunction& transmit )
{
	TCPSenderMessage newTCPSdMsg = make_empty_message();

//	std::cout << "push_simple_mode_ !" << std::endl;

//	std::cout << std::endl;
//	std::cout << "push begin!" << std::endl;
//	std::cerr
//	  << " seq_len=" << newTCPSdMsg.sequence_length() << std::endl
//	  << " SYN=" << newTCPSdMsg.SYN << std::endl
//      << " payload=" << newTCPSdMsg.payload.size() << std::endl
//      << " FIN=" << newTCPSdMsg.FIN << std::endl
//      << " next_seq=" << next_seq_ << std::endl
//	  << " SeqFNum=" << SeqFNum_ << std::endl
//      << " RWSize=" << RWSize_ << std::endl
//      << " reader_finished=" << input_.reader().is_finished() << std::endl
//      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl;
//	std::cerr << " payload is " << newTCPSdMsg.payload << std::endl;

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

	if(newTCPSdMsg.sequence_length() == 0)
		return ;
	
	// 更新公共资源
	next_seq_ += newTCPSdMsg.sequence_length();
	SeqFNum_ += newTCPSdMsg.sequence_length();


//	std::cout << std::endl;
//	std::cout << "push finish!" << std::endl;
//	std::cerr
//	  << " seq_len=" << newTCPSdMsg.sequence_length() << std::endl
//	  << " SYN=" << newTCPSdMsg.SYN << std::endl
//      << " payload=" << newTCPSdMsg.payload.size() << std::endl
//      << " FIN=" << newTCPSdMsg.FIN << std::endl
//      << " next_seq=" << next_seq_ << std::endl
//	  << " SeqFNum=" << SeqFNum_ << std::endl
//      << " RWSize=" << RWSize_ << std::endl
//      << " reader_finished=" << input_.reader().is_finished() << std::endl
//      << " bytes_buffered=" << input_.reader().bytes_buffered() << std::endl;
//	std::cerr << " payload is " << newTCPSdMsg.payload << std::endl;
//
//	std::flush(std::cout);
//	std::flush(std::cerr);
//
	// 传输
//	std::cout << std::endl << "send!" << std::endl;

//	std::cout << std::endl;
//	std::cout 
//		<< "msg.SYN = " << newTCPSdMsg.SYN << std::endl
//		<< "msg.payload = " << newTCPSdMsg.payload << std::endl
//		<< "msg.FIN = " << newTCPSdMsg.FIN << std::endl 
//		<< "msg.RST = " << newTCPSdMsg.RST << std::endl;

	transmit(newTCPSdMsg);
	// 在窗口塞入传输的msg
	window_.push_back(newTCPSdMsg);

	if(newTCPSdMsg.RST)
		should_RST_ = false;
}


void TCPSender::receive_simple_mode_(const TCPReceiverMessage& msg)
{
//	std::cout << std::endl;
//	std::cout << "receive_simple_mode_ !" << std::endl;
//	std::cout << "msg.ackno = " << (*msg.ackno).unwrap(isn_, last_ackno_) << std::endl;
//	std::cout << "msg.window_size = " << msg.window_size << std::endl;
//	std::cout << "msg.RST = " << msg.RST << std::endl;

	if(msg.RST)
		input_.reader().set_error();

	// 如果上一个包是单字节探测包
	if(zero_window_probe_ == true)
	{
//		std::cerr << std::endl << "quit zero_window_probe !" << std::endl;
		RWSize_ = static_cast<uint64_t>(msg.window_size);
		SeqFNum_ -= 1;
		zero_window_probe_ = false;
		is_zero_window_probeing_ = false;
		// 重置时间
		RT_.time_reset();
		zero_window_probe_msg_.pop_back();
		last_ackno_ = (*msg.ackno).unwrap(isn_, last_ackno_);
	}
	// 如果上一个包不是单字节探测包
	else
	{
		RWSize_ = static_cast<uint64_t>(msg.window_size);
		uint64_t ackno_u64 = (*msg.ackno).unwrap(isn_, last_ackno_);

		// ACK合法性判断
		// 超了--无效
		if(ackno_u64 > next_seq_)
		{}
		// 如果已经有内容被接收了
		else if(ackno_u64 > next_seq_ - SeqFNum_)
		{
			// 将已经接收的msg从窗口移除
			for(auto it = window_.begin() ; it != window_.end() ; )
			{
				uint64_t seqno_u64 = it->seqno.unwrap(isn_, last_ackno_);
	
				if(seqno_u64 + it->sequence_length() <= ackno_u64)
					it = window_.erase(it);
			//	else if(seqno_u64 < ackno_u64 && ackno_u64 < seqno_u64 + it->sequence_length())
			//	{
			//		// 鲁棒性优化
			//		uint64_t need_del_len = ackno_u64 - seqno_u64;
	
			//		if(need_del_len > 0)
			//		{
			//			it->payload = it->payload.substr(need_del_len);
			//			it->seqno = it->seqno + need_del_len;	
			//		}
			//		
			//		if(need_del_len == 1 && it->FIN == true)
			//		{
			//			it->FIN = false;
			//			need_del_len -= 1;
			//			it->seqno = it->seqno + 1;
			//		}
	
			//		break;
			//	}
				else 
					break;
			}
			// 重置公共资源
			CRT_ = 0;
			RT_.RTO_reset(initial_RTO_ms_);
			SeqFNum_ -= ackno_u64 - last_ackno_; 
	
			// 重置时间
			RT_.time_reset();
			
			last_ackno_ = (*msg.ackno).unwrap(isn_, last_ackno_);
		}
		// 太少--无效
		else 
		{}
	}
	zero_window_probe_ = false;
}

void TCPSender::tick_simple_mode_(uint64_t ms_since_last_tick, const TransmitFunction& transmit)
{
	if(window_.size() == 0)
		return ;
	
	// 时间流逝
	RT_.time_plus(ms_since_last_tick);
	
	// 超时重传
	if(RT_.is_timeout())
		retransmit_simple_mode_(transmit, window_[0]);	
}

void TCPSender::retransmit_simple_mode_(const TransmitFunction& transmit, TCPSenderMessage TCPSdMsg)
{
	// 重传
	transmit(TCPSdMsg);
	// 公共资源更新
	CRT_++;
	RT_.RTO_multi();
	RT_.time_reset();
}


// ***** zero window probe mode *****

void TCPSender::push_ZWP_mode_(const TransmitFunction& transmit )
{
	TCPSenderMessage newTCPSdMsg = make_empty_message();

//	std::cout << "push_ZWP_mode_ !" << std::endl;

	if(window_.size())
	{
		auto it = window_.begin();
	
		// 从window获取一个字节的有效数据
		// 别忘记从window删掉被获取的那个数据
		if(it->SYN == true)
		{
			newTCPSdMsg.SYN = true;
			it->SYN = false;
		}
		else if(window_[0].payload.length() != 0)
		{
			newTCPSdMsg.payload = it->payload.substr(0, 1);
			it->payload = it->payload.substr(0, 1);
		}
		else if(window_[0].FIN == true)
		{	
			newTCPSdMsg.FIN = true;
			it->FIN = false;
		}
		else
		{
		//	std::cerr << "push_one_byte error!" << std::endl;	
		}

		// window_[0]有效检查
		if(window_[0].sequence_length() == 0)
			window_.erase(window_.begin());
	}
	else // window_.size() == 0
	{
		if(is_SYNed_ == false)
		{
			newTCPSdMsg.SYN = true;
			is_SYNed_ = true;
		}
		else if(input_.reader().bytes_buffered())
		{
			newTCPSdMsg.payload = input_.reader().peek().substr(0, 1);
			input_.reader().pop(1);
		}
		else if(input_.reader().is_finished() && !is_FINed_)
		{
			newTCPSdMsg.FIN = true;
			is_FINed_ = true;
		}
		else 
		{
			//std::cerr << "push_one_byte error!" << std::endl;
		}

		if(newTCPSdMsg.sequence_length() != 1)
			return ;
		
		next_seq_ += 1;
		SeqFNum_ += 1;
	}
	is_zero_window_probeing_ = true;
	
//	std::cout << std::endl;
//	std::cout 
//		<< "msg.SYN = " << newTCPSdMsg.SYN << std::endl
//		<< "msg.payload = " << newTCPSdMsg.payload << std::endl
//		<< "msg.FIN = " << newTCPSdMsg.FIN << std::endl 
//		<< "msg.RST = " << newTCPSdMsg.RST << std::endl;

	transmit(newTCPSdMsg);
	zero_window_probe_msg_.push_back(newTCPSdMsg);
	if(newTCPSdMsg.RST)
		should_RST_ = false;
	if(zero_window_probe_msg_.size() > 1)
	{
	//	std::cerr << "error! zero_window_probe_msg_ have too many msg!" << std::endl;
		return ;
	}
}

void TCPSender::receive_ZWP_mode_(const TCPReceiverMessage& msg)
{
	if(msg.RST)
		input_.reader().set_error();
	
	if(zero_window_probe_)
	{
		is_zero_window_probeing_ = false;
		SeqFNum_ -= 1;
		// 重置时间
		RT_.time_reset();
		zero_window_probe_msg_.pop_back();
		last_ackno_ = (*msg.ackno).unwrap(isn_, last_ackno_);
	}
	else 
	{
	//	std::cerr << "zero_window_probe !" << std::endl;
		RWSize_ = 0;
		zero_window_probe_ = true;
		
		uint64_t ackno_u64 = (*msg.ackno).unwrap(isn_, last_ackno_);
		
		// ACK合法性判断
		// 超了--无效
		if(ackno_u64 > next_seq_)
		{}
		// 如果已经有内容被接收了
		else if(ackno_u64 > next_seq_ - SeqFNum_)
		{
			// 将已经接收的msg从窗口移除
			for(auto it = window_.begin() ; it != window_.end() ; )
			{
				uint64_t seqno_u64 = it->seqno.unwrap(isn_, last_ackno_);
	
				if(seqno_u64 + it->sequence_length() <= ackno_u64)
					it = window_.erase(it);
			//	else if(seqno_u64 < ackno_u64 && ackno_u64 < seqno_u64 + it->sequence_length())
			//	{
			//		// 鲁棒性优化
			//		uint64_t need_del_len = ackno_u64 - seqno_u64;
	
			//		if(need_del_len > 0)
			//		{
			//			it->payload = it->payload.substr(need_del_len);
			//			it->seqno = it->seqno + need_del_len;	
			//		}
			//		
			//		if(need_del_len == 1 && it->FIN == true)
			//		{
			//			it->FIN = false;
			//			need_del_len -= 1;
			//			it->seqno = it->seqno + 1;
			//		}
	
			//		break;
			//	}
				else 
					break;
			}
			// 重置公共资源
			CRT_ = 0;
			RT_.RTO_reset(initial_RTO_ms_);
			SeqFNum_ -= ackno_u64 - last_ackno_; 

			// 重置时间
			RT_.time_reset();
			
			last_ackno_ = (*msg.ackno).unwrap(isn_, last_ackno_);
		}
		// 太少--无效
		else 
		{}
	}
}

void TCPSender::tick_ZWP_mode_(uint64_t ms_since_last_tick, const TransmitFunction& transmit)
{
	if(zero_window_probe_msg_.size() == 0)
		return ;
	// 时间流逝
	RT_.time_plus(ms_since_last_tick);
	if(RT_.is_timeout())
		retransmit_ZWP_mode_(transmit, zero_window_probe_msg_[0]);
}

void TCPSender::retransmit_ZWP_mode_(const TransmitFunction& transmit, TCPSenderMessage TCPSdMsg)
{
	transmit(TCPSdMsg);
	RT_.time_reset();
}


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

