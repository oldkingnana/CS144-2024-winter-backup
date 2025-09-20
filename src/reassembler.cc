#include "reassembler.hh"
#include "byte_stream.hh"
#include <cstdint>
#include <iterator>
#include <string>
#include <sys/types.h>
#include <iostream>

// debug helpers - 放在文件顶部 includes 之后
// #include <iomanip>

//static void debug_hexdump(const std::string &s, const char *tag, uint64_t index = (uint64_t)-1) {
//    std::cerr << tag;
//    if (index != (uint64_t)-1) std::cerr << " @ index " << index;
//    std::cerr << " len=" << s.size() << " hex=";
//    std::ios oldState(nullptr);
//    oldState.copyfmt(std::cerr);
//    for (unsigned char c : s) {
//        std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)c << " ";
//    }
//    std::cerr.copyfmt(oldState);
//    std::cerr << std::dec << std::endl;
//}

using namespace std;

// v2

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
	tail_index_ = waiting_index_ + output_.writer().available_capacity();
	//cout << "window init: " << "waiting_index->" << waiting_index_ << " tail_index->" << tail_index_ << endl;
	
	uint64_t first_index_buff_1;
	std::string data_buff_1;
	if(!EOFM_(first_index, data, is_last_substring, first_index_buff_1, data_buff_1))
		return ;

	uint64_t first_index_buff_2;
	std::string data_buff_2;
	if(!WF_(first_index_buff_1, data_buff_1, first_index_buff_2, data_buff_2))
		return ;


	if(!CM_.cachin().cache_in(first_index_buff_2, data_buff_2))
		return ;

	CM_.writer().write();

	if(EOFM_.is_EOF() && EOF_end_index_ <= waiting_index_)
		output_.writer().close();
}

uint64_t Reassembler::bytes_pending() const
{
	return size_;
}

bool Reassembler::WindowFilter::operator()(const uint64_t& first_index, 
		                                   const std::string& data,
		                                   uint64_t& first_index_ret, 
										   std::string& data_ret)
{
	uint64_t end_index = first_index + data.length();
	first_index_ret = first_index;
	data_ret = data;
	//cout << "WindowFilter: " << "first_index->" << first_index << " end_index->" << end_index << endl;
	if(!data_ret.empty() && 
	   (tail_index_ <= static_cast<int64_t>(first_index_ret) || 
	   static_cast<int64_t>(first_index_ret + data_ret.length()) <= waiting_index_))
	{
		//cout << "out of the window" << endl;
		//cout << "waiting_index->" << waiting_index_ << " tail_index->" << tail_index_ << endl;
		return false;
	}
	else if(data_ret.empty() && 
	   (tail_index_ <= static_cast<int64_t>(first_index_ret) || 
	   static_cast<int64_t>(first_index_ret + data_ret.length()) < waiting_index_))
	{
		//cout << "out of the window" << endl;
		return false;
	}

	if(waiting_index_ <= static_cast<int64_t>(first_index_ret) &&
	   static_cast<int64_t>(end_index) <= tail_index_)
	{
		//cout << "in the window" << endl;
		return true;
	}

	if(static_cast<int64_t>(first_index_ret) < waiting_index_)
	{
		//cout << "cut left" << endl;
		data_ret = data_ret.substr(waiting_index_ - first_index_ret);
		first_index_ret = waiting_index_;
	}
	
	if(tail_index_ < static_cast<int64_t>(first_index_ret + data_ret.length()))
	{
		//cout << "cut right" << endl;
		data_ret = data_ret.substr(0, tail_index_ - first_index_ret);
	}
	cout << endl;
	return true;
}

void Reassembler::EOFManager::init(uint64_t EOF_first_index, uint64_t EOF_end_index)
{
	is_init_ = true;
	EOF_first_index_ = static_cast<int64_t>(EOF_first_index);
	EOF_end_index_ = static_cast<int64_t>(EOF_end_index);
	//cout << "EOFManager init: " << "EOF_first_index->" << EOF_first_index_ << " EOF_end_index->" << EOF_end_index_ << endl;
}

bool Reassembler::EOFManager::operator()(const uint64_t& first_index,
										 const std::string& data,
										 bool is_last_substring,
										 uint64_t& first_index_ret,
										 std::string& data_ret)
{
	//uint64_t end_index = first_index + data.length();
	first_index_ret = first_index;
	data_ret = data;
	
	//cout << "EOFManager: " << "first_index->" << first_index << " end_index->" << end_index << endl;
	if(is_init_)
	{
		//cout << "EOF_first_index->" << EOF_first_index_ << " EOF_end_index->" << EOF_end_index_ << endl;
	}


	if(!is_init_ && !is_last_substring)
	{
		//cout << "not init!" << endl;
		return true;
	}

	if(!is_init_ && is_last_substring)
	{
		//cout << "begin to init!" << endl;
		init(first_index_ret, first_index_ret + data.length());
	}

	if(data.length() == 0 && cache_size_ == 0)
	{
		//cout << "special, data is empty" << endl;
		output_.writer().close();
		return false;
	}

	if(EOF_end_index_ <= static_cast<int64_t>(first_index_ret))
	{
		//cout << "out of EOF" << endl;
		return false;
	}

	if(EOF_end_index_ < static_cast<int64_t>(first_index_ret + data_ret.length()))
	{
		//cout << "cut because of EOF" << endl;
		data_ret = data_ret.substr(0, EOF_end_index_ - first_index_ret);
	}
	cout << endl;
	return true;
}
		
const int64_t& Reassembler::EOFManager::get_EOF_first() const
{
	return EOF_first_index_;
}
const int64_t& Reassembler::EOFManager::get_EOF_end() const
{
	return EOF_end_index_;
}
		
bool Reassembler::EOFManager::is_EOF() const
{
	return is_init_;
}

Reassembler::writer_& Reassembler::CacheManager::writer()
{
	return static_cast<writer_&>(*this);
}

Reassembler::cache_in_& Reassembler::CacheManager::cachin()
{
	return static_cast<cache_in_&>(*this);
}

bool Reassembler::writer_::write()
{
	while(!cache_.empty())
	{
		//cout << "begin to write!" << endl;
		if(cache_.empty())
		{
			//cout << "cache is empty!" << endl;
			return false;
		}
		
		if(static_cast<int64_t>(std::get<0>(*cache_.begin())) != waiting_index_)
		{
			//cout << "the first cache's first index is not waiting_index" << endl;
			return false;
		}
	
		//cout << "get " << endl;
		uint64_t cur_data_len = std::get<2>(*cache_.begin()).length();
		std::string push_str = std::get<2>(*cache_.begin()).substr
			(0, output_.writer().available_capacity() > cur_data_len ? cur_data_len : output_.writer().available_capacity());
		std::string leave_str = std::get<2>(*cache_.begin()).substr
			(output_.writer().available_capacity() > cur_data_len ? cur_data_len : output_.writer().available_capacity());
		//cout << "push " << endl;
		output_.writer().push(push_str);
		size_ -= std::get<2>(*cache_.begin()).length();
		waiting_index_ += push_str.length();
		cache_.erase(cache_.begin());
		if(!leave_str.empty())
		{
			//cout << "leave_str.length: " << leave_str.length() << endl;
			size_ += leave_str.length();
			cache_.insert(std::tuple<int64_t, int64_t, std::string>(waiting_index_, waiting_index_ + leave_str.length(), leave_str));
			//cout << "insert back" << endl;
	
		}
	}
	cout << endl;
	return true;
}

bool Reassembler::cache_in_::cache_in(uint64_t first_index, std::string data)
{
	//cout << "begin to cache in" << endl;
	uint64_t end_index = first_index + data.length();
	//cout << "first_index->" << first_index << " end_index->" << end_index << endl;
	for(auto it = cache_.begin(); it != cache_.end(); )
	{
		uint64_t cur_first_index = std::get<0>(*it);	
		uint64_t cur_end_index = std::get<1>(*it);
		//cout << "cur_first_index->" << cur_first_index << " cur_end_index->" << cur_end_index << endl;

		// 新字符串被已有字符串包含
		if(cur_first_index <= first_index && 
		   end_index <= cur_end_index)
		{
			return false;
		}
		// 新字符串包含已有字符串
		else if(first_index <= cur_first_index && 
				cur_end_index <= end_index)
		{
			//cout << "新字符串包含已有字符串" << endl;
			size_ -= get<2>(*it).length();
			it = cache_.erase(it);
		}
		// 新字符串在已有字符串左侧且部分重叠
		else if(first_index < cur_first_index && 
				cur_first_index < end_index && 
				end_index < cur_end_index)
		{
		 	//cout << "新字符串在已有字符串左侧且部分重叠" << endl;
			data = data + get<2>(*it).substr(end_index - cur_first_index);
			end_index = first_index + data.length();
			size_ -= get<2>(*it).length();
			cache_.erase(it);
			break;
		}
		// 新字符串在已有字符串右侧且部分重叠
		else if(cur_first_index < first_index && 
				first_index < cur_end_index &&
				cur_end_index < end_index)
		{
			//cout << "新字符串在已有字符串右侧且部分重叠" << endl;
			data = get<2>(*it) + data.substr(cur_end_index - first_index);
			first_index = cur_first_index;
			size_ -= get<2>(*it).length();
			it = cache_.erase(it);
		}
		// 新字符串在已有字符串左侧且不重叠
		else if(end_index <= cur_first_index)
		{
		 	//cout << "新字符串在已有字符串左侧且不重叠" << endl;
			break;
		}
		// 新字符串在已有字符串右侧且不重叠
		else
		{
			//cout << "新字符串在已有字符串右侧且不重叠" << endl;
			it++;
		}
	}
	cache_.insert(std::tuple<int64_t, int64_t, std::string>(first_index, end_index, data));	
	size_ += data.length();
	//cout << "insert: " << "first_index->" << first_index << " end_index->" << end_index << endl;
	return true;
}



























// v1

//
//void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
//{
//	if(is_closed_.first != -1 && is_closed_.second < static_cast<int64_t>(first_index + data.length()))
//		return ;
//	
//	if(is_closed_.first != -1 && static_cast<int64_t>(first_index + data.length()) == is_closed_.second && !is_last_substring)
//		return ;
//
//	if(is_closed_.first == -1 && is_last_substring)
//	{
//		is_closed_ = {first_index, first_index + data.length()};
//		if(cach_.empty() && first_index == waiting_index_ && data.length() == 0)
//			output_.writer().close();
//	}
//
//	//cout << "insert: " << data << " @ index " << first_index << endl;
//	// 窗口更新
//	end_window_index_ = waiting_index_ + output_.writer().available_capacity();
//	//cout << "window update: " << "end_window_index->" << end_window_index_ << endl;
//	// 窗口未命中
//	if(first_index >= end_window_index_)
//		return ;
//	if(first_index + data.length() <= waiting_index_)
//		return ;
//
//	// 窗口命中	
//	// 默认判断等待包一定不匹配
//	// cut & cache in
//	cut_cache_in(first_index, data); 
//
//	// 缓存检查
//	while(cach_.begin() != cach_.end() && cach_.begin()->first == waiting_index_)
//	{
//		// write
//		auto cur_first_index = cach_.begin()->first;
//		//cout << "cach check!" << endl;
//		// debug_hexdump(cach_.begin()->second, "DEBUG: about_to_push", cach_.begin()->first);
//		output_.writer().push(cach_.begin()->second);
//		//cout << "write cach begin it to output: " << cach_.begin()->second << endl;
//		waiting_index_ = cur_first_index + cach_.begin()->second.length();
//		//cout << "change waiting_index to: " << waiting_index_ << endl;
//		size_ -= cach_.begin()->second.length();
//		cach_.erase(cach_.begin());
//		if(is_closed_.first != -1 && is_closed_.second <= static_cast<int64_t>(waiting_index_))
//		{
//			cout << "close!" << endl;
//			output_.writer().close();
//			break;
//		}
//	}
//	//if(cach_.empty())
//		//cout << "cach_ is empty" << endl;
//	//else 
//		//cout << "cach_ begin: " << "first_index->" << cach_.begin()->first << " data->" << cach_.begin()->second << endl;
//
//}
//
//uint64_t Reassembler::bytes_pending() const
//{
//	//cout << "end_window_index->" << end_window_index_ << " waiting_index_->" << waiting_index_ << "size_->" << size_ << endl;
//	return size_;
//}
//
//void Reassembler::cut_cache_in(uint64_t first_index, std::string data)
//{
//	//cout << "cut cache in:" << endl;
//
//	if(first_index < waiting_index_)
//	{
//		//cout << "cut head because of window" << endl;
//		data = data.substr(waiting_index_ - first_index);
//		first_index = waiting_index_;
//	}
//	if(first_index + data.length() > end_window_index_)
//	{
//		//cout << "cut tail because of window" << endl;
//		data = data.substr(0, end_window_index_ - first_index);
//	}
//	for(auto it = cach_.begin(); it != cach_.end();)
//	{
//		//cout << "string in cache: " << it->second << "@ " << "index " << it->first << endl;
//		auto cur_first_index = it->first;
//		auto cur_end_index = it->first + it->second.length();
//		
//		if(cur_first_index <= first_index && 
//		   first_index + data.length() <= cur_end_index)
//			return ;
//
//		if(cur_first_index <= first_index && 
//		   first_index < cur_end_index && 
//		   cur_end_index <= first_index + data.length())
//		{
//			//cout << "merge with " << it->second << " as head" << endl;
//			data = it->second.substr(0, first_index - cur_first_index) + data;
//			first_index = it->first;
//			size_ -= it->second.length();
//			it = cach_.erase(it);	
//		}
//		else if(first_index <= cur_first_index && 
//				cur_first_index < first_index + data.length() && 
//				first_index + data.length() <= cur_end_index) 
//		{
//			//cout << "merge with " << it->second << " as tail" << endl;
//			data = data + it->second.substr(it->second.length() - (cur_end_index - (first_index + data.length())));
//			size_ -= it->second.length();
//			it = cach_.erase(it);
//		}
//		else if(first_index < cur_first_index && cur_end_index < first_index + data.length()) 
//		{
//			//cout << "erase " << it->second << endl;
//			size_ -= it->second.length();
//			it = cach_.erase(it);
//		}
//		else if(first_index == cur_end_index)
//		{
//			//cout << "next" << endl;
//			it++;
//			continue;
//		}
//		else if(first_index + data.length() == cur_first_index) 
//			break;
//		else 
//			it++;
//	}
//	//debug_hexdump(data, "DEBUG: before cache_in", first_index);
//	cach_[first_index] = data;
//	size_ += data.length();
//	//cout << "cache in: " << "first_index->" << first_index << " data->" << data << endl;
//	//debug_hexdump(data, "DEBUG: cache_in", first_index);
//}
//
//

