#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <cstdint>
#include "wrapping_integers.hh"

class TCPReceiver
{
public:
  // Construct with given Reassembler
  explicit TCPReceiver( Reassembler&& reassembler ) 
	  : reassembler_( std::move( reassembler ) ) 
  	  , zero_point(100)
	  , checkpoint(0)
  	  , SYN(0)
	  , SYN_before(0)
	  , FIN(0)
	  , need_FIN(0)
	{}

  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  void receive( TCPSenderMessage message );

  // The TCPReceiver sends TCPReceiverMessages to the peer's TCPSender.
  TCPReceiverMessage send() const;

  // Access the output (only Reader is accessible non-const)
  const Reassembler& reassembler() const { return reassembler_; }
  Reader& reader() { return reassembler_.reader(); }
  const Reader& reader() const { return reassembler_.reader(); }
  const Writer& writer() const { return reassembler_.writer(); }

private:
  	Reassembler reassembler_;

	Wrap32 zero_point; // ISN	
	uint64_t checkpoint; // stream index, When actually used, it should be used after the self-decrement operation
	uint32_t SYN;
	uint32_t SYN_before;
	uint32_t FIN;
	uint32_t need_FIN;
};


