#include "reassembler.hh"
#include "byte_stream.hh"
#include <cstdint>
#include <iterator>
#include <string>
#include <sys/types.h>
#include <iostream>

// debug helpers - 放在文件顶部 includes 之后
#include <iomanip>

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

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
	if(is_closed_.first != -1 && is_closed_.second < static_cast<int64_t>(first_index + data.length()))
		return ;
	
	if(is_closed_.first != -1 && static_cast<int64_t>(first_index + data.length()) == is_closed_.second && !is_last_substring)
		return ;

	if(is_closed_.first == -1 && is_last_substring)
	{
		is_closed_ = {first_index, first_index + data.length()};
		if(cach_.empty() && first_index == waiting_index_ && data.length() == 0)
			output_.writer().close();
	}

	//cout << "insert: " << data << " @ index " << first_index << endl;
	// 窗口更新
	end_window_index_ = waiting_index_ + output_.writer().available_capacity();
	//cout << "window update: " << "end_window_index->" << end_window_index_ << endl;
	// 窗口未命中
	if(first_index >= end_window_index_)
		return ;
	if(first_index + data.length() <= waiting_index_)
		return ;

	// 窗口命中	
	// 默认判断等待包一定不匹配
	// cut & cache in
	cut_cache_in(first_index, data); 

	// 缓存检查
	while(cach_.begin() != cach_.end() && cach_.begin()->first == waiting_index_)
	{
		// write
		auto cur_first_index = cach_.begin()->first;
		//cout << "cach check!" << endl;
		// debug_hexdump(cach_.begin()->second, "DEBUG: about_to_push", cach_.begin()->first);
		output_.writer().push(cach_.begin()->second);
		//cout << "write cach begin it to output: " << cach_.begin()->second << endl;
		waiting_index_ = cur_first_index + cach_.begin()->second.length();
		//cout << "change waiting_index to: " << waiting_index_ << endl;
		size_ -= cach_.begin()->second.length();
		cach_.erase(cach_.begin());
		if(is_closed_.first != -1 && is_closed_.second <= static_cast<int64_t>(waiting_index_))
		{
			cout << "close!" << endl;
			output_.writer().close();
			break;
		}
	}
	//if(cach_.empty())
		//cout << "cach_ is empty" << endl;
	//else 
		//cout << "cach_ begin: " << "first_index->" << cach_.begin()->first << " data->" << cach_.begin()->second << endl;

}

uint64_t Reassembler::bytes_pending() const
{
	//cout << "end_window_index->" << end_window_index_ << " waiting_index_->" << waiting_index_ << "size_->" << size_ << endl;
	return size_;
}

void Reassembler::cut_cache_in(uint64_t first_index, std::string data)
{
	//cout << "cut cache in:" << endl;

	if(first_index < waiting_index_)
	{
		//cout << "cut head because of window" << endl;
		data = data.substr(waiting_index_ - first_index);
		first_index = waiting_index_;
	}
	if(first_index + data.length() > end_window_index_)
	{
		//cout << "cut tail because of window" << endl;
		data = data.substr(0, end_window_index_ - first_index);
	}
	for(auto it = cach_.begin(); it != cach_.end();)
	{
		//cout << "string in cache: " << it->second << "@ " << "index " << it->first << endl;
		auto cur_first_index = it->first;
		auto cur_end_index = it->first + it->second.length();
		
		if(cur_first_index <= first_index && 
		   first_index + data.length() <= cur_end_index)
			return ;

		if(cur_first_index <= first_index && 
		   first_index < cur_end_index && 
		   cur_end_index <= first_index + data.length())
		{
			//cout << "merge with " << it->second << " as head" << endl;
			data = it->second.substr(0, first_index - cur_first_index) + data;
			first_index = it->first;
			size_ -= it->second.length();
			it = cach_.erase(it);	
		}
		else if(first_index <= cur_first_index && 
				cur_first_index < first_index + data.length() && 
				first_index + data.length() <= cur_end_index) 
		{
			//cout << "merge with " << it->second << " as tail" << endl;
			data = data + it->second.substr(it->second.length() - (cur_end_index - (first_index + data.length())));
			size_ -= it->second.length();
			it = cach_.erase(it);
		}
		else if(first_index < cur_first_index && cur_end_index < first_index + data.length()) 
		{
			//cout << "erase " << it->second << endl;
			size_ -= it->second.length();
			it = cach_.erase(it);
		}
		else if(first_index == cur_end_index)
		{
			//cout << "next" << endl;
			it++;
			continue;
		}
		else if(first_index + data.length() == cur_first_index) 
			break;
		else 
			it++;
	}
	//debug_hexdump(data, "DEBUG: before cache_in", first_index);
	cach_[first_index] = data;
	size_ += data.length();
	//cout << "cache in: " << "first_index->" << first_index << " data->" << data << endl;
	//debug_hexdump(data, "DEBUG: cache_in", first_index);
}



