/*
 * packet.h
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#ifndef PACKET_H_
#define PACKET_H_

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "scheduler.h"
#include "packet-stamp.h"


#define HDR_CMN(p)      (hdr_cmn::access(p))
#define HDR_MAC802_11(p) ((hdr_mac802_11 *)hdr_mac::access(p))

typedef unsigned int packet_t;

static const packet_t PT_MAC = 0;

// insert new packet types here
static packet_t       PT_NTYPE = 1; // This MUST be the LAST one



class Packet : public Event {
private:
	unsigned char* bits_;	// header bits
    unsigned char* data_;	// variable size buffer for 'data'
    unsigned int datalen_;	// length of variable size buffer
	static void init(Packet*);     // initialize pkt hdr
	bool fflag_;
protected:
	static Packet* free_;	// packet free list
	int	ref_count_;	// free the pkt until count to 0
public:
	Packet* next_;		// for queues and the free list
	static int hdrlen_;

	Packet() : bits_(0), data_(0), ref_count_(0), next_(0) { }
	unsigned char* const bits() { return (bits_); }
	Packet* copy() const;
	Packet* refcopy() { ++ref_count_; return this; }
	int& ref_count() { return (ref_count_); }
	static Packet* alloc();
	static Packet* alloc(int);
	void allocdata(int);
	// dirty hack for diffusion data
	void initdata() { data_  = 0;}
	static void free(Packet*);
	unsigned char* access(int off) const {
		if (off < 0)
			abort();
		return (&bits_[off]);
	}
	// This is used for backward compatibility, i.e., assuming user data
	// is PacketData and return its pointer.
	unsigned char* accessdata() const {
		return data_;
	}

	int datalen() const { return data_ ? datalen_ : 0; }

	// the pkt stamp carries all info about how/where the pkt
        // was sent needed for a receiver to determine if it correctly
        // receives the pkt
	PacketStamp	txinfo_;

};

typedef void (*FailureCallback)(Packet *,void *);





struct hdr_cmn {
	enum dir_t { DOWN= -1, NONE= 0, UP= 1 };
	packet_t ptype_;	// packet type (see above)
	int	size_;		// simulated packet size
	int	uid_;		// unique id
	int	error_;		// error flag
	int     errbitcnt_;     // # of corrupted bits jahn
	double	ts_;		// timestamp: for q-delay measurement
	double    propagationDelay_;
	double frame_error_rate_;
	int transmition_times_;
	//xuhui
	double  sendtime_;
	//
	int	iface_;		// receiving interface (label)
	dir_t	direction_;	// direction: 0=none, 1=up, -1=down

        // called if pkt can't obtain media or isn't ack'd. not called if
        // droped by a queue
        FailureCallback xmit_failure_;
        void *xmit_failure_data_;

        /*
         * MONARCH wants to know if the MAC layer is passing this back because
         * it could not get the RTS through or because it did not receive
         * an ACK.
         */
        int     xmit_reason_;
#define XMIT_REASON_RTS 0x01
#define XMIT_REASON_ACK 0x02
#define XMIT_REASON_LIMIT 0x03

	// tx time for this packet in sec
	double txtime_;
	double& txtime() { return(txtime_); }

	static int offset_;	// offset for this header
	static int& offset() { return offset_; }
	static hdr_cmn* access(const Packet* p) {
		return (hdr_cmn*) p->access(offset_);
	}

        /* per-field member functions */
	packet_t& ptype() { return (ptype_); }
	int& size() { return (size_); }
	int& uid() { return (uid_); }
	int& error() { return error_; }
	int& errbitcnt() {return errbitcnt_; }
	double& timestamp() { return (ts_); }
	int& iface() { return (iface_); }
	dir_t& direction() { return (direction_); }
	double rate_;
	double& rate(){return (rate_);}
	double& propagationDelay(){return (propagationDelay_);}
};




#endif /* PACKET_H_ */
