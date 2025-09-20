#pragma once

#include "byte_stream.hh"
#include <cstdint>
#include <functional>
#include <set>
#include <tuple>
#include <utility>


class Reassembler
{
public:
	// Construct Reassembler to write into given ByteStream.
	explicit Reassembler( ByteStream&& output ) 
		: output_( std::move( output ) )
		, waiting_index_(0)
		, tail_index_(output_.writer().available_capacity())
		, EOF_first_index_(-1)
		, EOF_end_index_(-1)
		, size_(0)
		, WF_(waiting_index_, tail_index_)
		, CM_(output_, waiting_index_, tail_index_, size_)
		, EOFM_(output_, size_, EOF_first_index_, EOF_end_index_)
	{}		

    // 拷贝构造
    Reassembler(const Reassembler& other)
        : output_(other.output_)
        , waiting_index_(other.waiting_index_)
        , tail_index_(other.tail_index_)
        , EOF_first_index_(other.EOF_first_index_)
        , EOF_end_index_(other.EOF_end_index_)
        , size_(other.size_)
        , WF_(waiting_index_, tail_index_)
        , CM_(output_, waiting_index_, tail_index_, size_)
        , EOFM_(output_, size_, EOF_first_index_, EOF_end_index_) 
	{}

    // 移动构造
    Reassembler(Reassembler&& other) noexcept
        : output_(std::move(other.output_))
        , waiting_index_(other.waiting_index_)
        , tail_index_(other.tail_index_)
        , EOF_first_index_(other.EOF_first_index_)
        , EOF_end_index_(other.EOF_end_index_)
        , size_(other.size_)
        , WF_(waiting_index_, tail_index_)
        , CM_(output_, waiting_index_, tail_index_, size_)
        , EOFM_(output_, size_, EOF_first_index_, EOF_end_index_) 
	{}


		//, cach_()
		//, waiting_index_(0)
		//, end_window_index_(output_.writer().available_capacity())
		//, size_(0)
		//, is_closed_({-1, -1})
	//{}
	
	/*
	 * Insert a new substring to be reassembled into a ByteStream.
	 *   `first_index`: the index of the first byte of the substring
	 *   `data`: the substring itself
	 *   `is_last_substring`: this substring represents the end of the stream
	 *   `output`: a mutable reference to the Writer
	 *
	 * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
	 * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
	 * learns the next byte in the stream, it should write it to the output.
	 *
	 * If the Reassembler learns about bytes that fit within the stream's available capacity
	 * but can't yet be written (because earlier bytes remain unknown), it should store them
	 * internally until the gaps are filled in.
	 *
	 * The Reassembler should discard any bytes that lie beyond the stream's available capacity
	 * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
	 *
	 * The Reassembler should close the stream after writing the last byte.
	 */
	void insert( uint64_t first_index, std::string data, bool is_last_substring );
	
	// How many bytes are stored in the Reassembler itself?
	uint64_t bytes_pending() const;
	
  	// Access output stream reader
  	Reader& reader() { return output_.reader(); }
  	const Reader& reader() const { return output_.reader(); }

	// Access output stream writer, but const-only (can't write from outside)
	const Writer& writer() const { return output_.writer(); }

private:
	// v2
	
	// class 
	
	class WindowFilter
	{
	public:
		WindowFilter(const int64_t& waiting_index, const int64_t& tail_index)
			: waiting_index_(waiting_index)
			, tail_index_(tail_index)
		{}

		bool operator()(const uint64_t& first_index, const std::string& data, 
				        uint64_t& first_index_ret, std::string& data_ret);

	private:
		const int64_t& waiting_index_;
		const int64_t& tail_index_;
	};

	class EOFManager
	{
	public:
		EOFManager(ByteStream& output, const int64_t& cache_size, int64_t& EOF_first_index, int64_t& EOF_end_index)
			: EOF_first_index_(EOF_first_index)
			, EOF_end_index_(EOF_end_index)
			, output_(output)
			, cache_size_(cache_size)
			, is_init_(false)
		{}

		void init(uint64_t EOF_first_index, uint64_t EOF_end_index);

		bool operator()(const uint64_t& first_index, const std::string& data, 
				        bool is_last_substring,
				        uint64_t& first_index_ret, std::string& data_ret);

		const int64_t& get_EOF_first() const ;
		const int64_t& get_EOF_end() const ;
		bool is_EOF() const;

	private:
		int64_t& EOF_first_index_;
		int64_t& EOF_end_index_;
		ByteStream& output_;
		const int64_t& cache_size_;
		bool is_init_;
	};


	class writer_;
	class cache_in_;

	class CacheManager
	{
	public:
		CacheManager(ByteStream& output, int64_t& waiting_index, int64_t& tail_index, int64_t& size)
			: output_(output)
			, cache_({})
			, waiting_index_(waiting_index)
			, tail_index_(tail_index)
			, size_(size)
		{}

	writer_& writer();

	cache_in_& cachin();

	protected:
		class less
		{
		public:
			bool operator()(const std::tuple<uint64_t, uint64_t, std::string>& left, const std::tuple<uint64_t, uint64_t, std::string>& right) const
			{
				return std::get<0>(left) < std::get<0>(right);
			}
		};

		ByteStream& output_;
		std::set<std::tuple<uint64_t, uint64_t, std::string>, less> cache_; 
		int64_t& waiting_index_;
		const int64_t& tail_index_;
		int64_t& size_;
	};

	// shell class for CacheManager 
	class writer_ : public CacheManager
	{
	public:
		bool write();
	};

	class cache_in_ : public CacheManager
	{
	public:
		bool cache_in(uint64_t first_index, std::string data);
	};

	// function 


	// members
	ByteStream output_; // the Reassembler writes to this ByteStream
	int64_t waiting_index_;
	int64_t tail_index_;
	int64_t EOF_first_index_;
	int64_t EOF_end_index_;
	int64_t size_;
	WindowFilter WF_;
	CacheManager CM_;
	EOFManager EOFM_;

	// v1

	//class greater
	//{
	//	bool operator()(std::pair<uint64_t, std::string> left, std::pair<uint64_t, std::string> right)
	//	{
	//		return left.first > right.first;
	//	}
	//};

	//void cut_cache_in(uint64_t first_index, std::string data);

	//ByteStream output_; // the Reassembler writes to this ByteStream
	//std::map<uint64_t, std::string> cach_;	
	//uint64_t waiting_index_;
	//uint64_t end_window_index_;
	//uint64_t size_;
	//std::pair<int64_t, int64_t> is_closed_;
};
