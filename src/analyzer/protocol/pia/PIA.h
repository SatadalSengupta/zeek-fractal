// An analyzer for application-layer protocol-detection.

#pragma once

#include "analyzer/Analyzer.h"
#include "analyzer/protocol/tcp/TCP.h"
#include "RuleMatcher.h"

ZEEK_FORWARD_DECLARE_NAMESPACED(RuleEndpointState, zeek::detail);

namespace zeek::analyzer::pia {

// Abstract PIA class providing common functionality for both TCP and UDP.
// Accepts only packet input.
//
// Note that the PIA provides our main interface to the signature engine and
// also keeps the matching state.  This is because (i) it needs to match
// itself, and (ii) in case of tunnel-decapsulation we may have multiple
// PIAs and then each needs its own matching-state.
class PIA : public zeek::detail::RuleMatcherState {
public:
	explicit PIA(zeek::analyzer::Analyzer* as_analyzer);
	virtual ~PIA();

	// Called when PIA wants to put an Analyzer in charge.  rule is the
	// signature that triggered the activitation, if any.
	virtual void ActivateAnalyzer(zeek::analyzer::Tag tag,
	                              const zeek::detail::Rule* rule = nullptr) = 0;

	// Called when PIA wants to remove an Analyzer.
	virtual void DeactivateAnalyzer(zeek::analyzer::Tag tag) = 0;

	void Match(zeek::detail::Rule::PatternType type, const u_char* data, int len,
	           bool is_orig, bool bol, bool eol, bool clear_state);

	void ReplayPacketBuffer(zeek::analyzer::Analyzer* analyzer);

	// Children are also derived from Analyzer. Return this object
	// as pointer to an Analyzer.
	zeek::analyzer::Analyzer* AsAnalyzer()	{ return as_analyzer; }

protected:
	void PIA_Done();
	void PIA_DeliverPacket(int len, const u_char* data, bool is_orig,
				uint64_t seq, const zeek::IP_Hdr* ip, int caplen, bool clear_state);

	enum State { INIT, BUFFERING, MATCHING_ONLY, SKIPPING } state;

	// Buffers one chunk of data.  Used both for packet payload (incl.
	// sequence numbers for TCP) and chunks of a reassembled stream.
	struct DataBlock {
		zeek::IP_Hdr* ip;
		const u_char* data;
		bool is_orig;
		int len;
		uint64_t seq;
		DataBlock* next;
	};

	struct Buffer {
		Buffer() { head = tail = nullptr; size = 0; state = INIT; }

		DataBlock* head;
		DataBlock* tail;
		int size;
		State state;
	};

	void AddToBuffer(Buffer* buffer, uint64_t seq, int len,
	                 const u_char* data, bool is_orig, const zeek::IP_Hdr* ip = nullptr);
	void AddToBuffer(Buffer* buffer, int len,
	                 const u_char* data, bool is_orig, const zeek::IP_Hdr* ip = nullptr);
	void ClearBuffer(Buffer* buffer);

	DataBlock* CurrentPacket()	{ return &current_packet; }

	void DoMatch(const u_char* data, int len, bool is_orig, bool bol,
	             bool eol, bool clear_state, const zeek::IP_Hdr* ip = nullptr);

	void SetConn(zeek::Connection* c)	{ conn = c; }

	Buffer pkt_buffer;

private:
	zeek::analyzer::Analyzer* as_analyzer;
	zeek::Connection* conn;
	DataBlock current_packet;
};

// PIA for UDP.
class PIA_UDP : public PIA, public zeek::analyzer::Analyzer {
public:
	explicit PIA_UDP(zeek::Connection* conn)
	: PIA(this), Analyzer("PIA_UDP", conn)
		{ SetConn(conn); }
	~PIA_UDP() override { }

	static zeek::analyzer::Analyzer* Instantiate(zeek::Connection* conn)
		{ return new PIA_UDP(conn); }

protected:
	void Done() override
		{
		Analyzer::Done();
		PIA_Done();
		}

	void DeliverPacket(int len, const u_char* data, bool is_orig,
	                   uint64_t seq, const zeek::IP_Hdr* ip, int caplen) override
		{
		Analyzer::DeliverPacket(len, data, is_orig, seq, ip, caplen);
		PIA_DeliverPacket(len, data, is_orig, seq, ip, caplen, true);
		}

	void ActivateAnalyzer(zeek::analyzer::Tag tag, const zeek::detail::Rule* rule) override;
	void DeactivateAnalyzer(zeek::analyzer::Tag tag) override;
};

// PIA for TCP.  Accepts both packet and stream input (and reassembles
// packets before passing payload on to children).
class PIA_TCP : public PIA, public zeek::analyzer::tcp::TCP_ApplicationAnalyzer {
public:
	explicit PIA_TCP(zeek::Connection* conn)
		: PIA(this), zeek::analyzer::tcp::TCP_ApplicationAnalyzer("PIA_TCP", conn)
		{ stream_mode = false; SetConn(conn); }

	~PIA_TCP() override;

	void Init() override;

	// The first packet for each direction of a connection is passed
	// in here.
	//
	// (This is a bit crude as it doesn't really fit nicely into the
	// analyzer interface.  Yet we need it for initializing the packet
	// matcher in the case that we already get reassembled input,
	// and making it part of the general analyzer interface seems
	// to be unnecessary overhead.)
	void FirstPacket(bool is_orig, const zeek::IP_Hdr* ip);

	void ReplayStreamBuffer(zeek::analyzer::Analyzer* analyzer);

	static zeek::analyzer::Analyzer* Instantiate(zeek::Connection* conn)
		{ return new PIA_TCP(conn); }

protected:
	void Done() override
		{
		Analyzer::Done();
		PIA_Done();
		}

	void DeliverPacket(int len, const u_char* data, bool is_orig,
	                   uint64_t seq, const zeek::IP_Hdr* ip, int caplen) override
		{
		Analyzer::DeliverPacket(len, data, is_orig, seq, ip, caplen);
		PIA_DeliverPacket(len, data, is_orig, seq, ip, caplen, false);
		}

	void DeliverStream(int len, const u_char* data, bool is_orig) override;
	void Undelivered(uint64_t seq, int len, bool is_orig) override;

	void ActivateAnalyzer(zeek::analyzer::Tag tag,
	                      const zeek::detail::Rule* rule = nullptr) override;
	void DeactivateAnalyzer(zeek::analyzer::Tag tag) override;

private:
	// FIXME: Not sure yet whether we need both pkt_buffer and stream_buffer.
	// In any case, it's easier this way...
	Buffer stream_buffer;

	bool stream_mode;
};

} // namespace zeek::analyzer::pia

namespace analzyer::pia {
	using PIA [[deprecated("Remove in v4.1. Use zeek::analyzer::pia::PIA.")]] = zeek::analyzer::pia::PIA;
	using PIA_TCP [[deprecated("Remove in v4.1. Use zeek::analyzer::pia::PIA_TCP.")]] = zeek::analyzer::pia::PIA_TCP;
	using PIA_UDP [[deprecated("Remove in v4.1. Use zeek::analyzer::pia::PIA_UDP.")]] = zeek::analyzer::pia::PIA_UDP;
}
