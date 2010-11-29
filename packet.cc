/*
 * packet.cc
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */
#include "packet.h"
#include "mac-802_11.h"



int hdr_cmn::offset_=0;			// static offset of common header

int Packet::hdrlen_ =sizeof(hdr_cmn) +sizeof(hdr_mac);		// size of a packet's header
Packet* Packet::free_;			// free list

void Packet::init(Packet* p)
{
	memset(p->bits_,0, hdrlen_);
}

Packet* Packet::alloc()
{
	Packet* p = free_;
	if (p != 0) {
		assert(p->fflag_ == false);
		free_ = p->next_;
		assert(p->data_ == 0);
		p->uid_ = 0;
		p->time_ = 0;
	} else {
		p = new Packet;
		p->bits_ = new unsigned char[hdrlen_];
		if (p == 0 || p->bits_ == 0)
			abort();
	}
	init(p); // Initialize bits_[]
	p->fflag_ = true;
	(HDR_CMN(p))->direction() = hdr_cmn::DOWN;
	/* setting all direction of pkts to be downward as default;
	   until channel changes it to +1 (upward) */
	p->next_ = 0;
	return (p);
}

/*
 * Allocate an n byte data buffer to an existing packet
 *
 */
void Packet::allocdata(int n)
{
	assert(data_ == 0);
	data_ = new unsigned char[n];
	if (data_ == 0)
		abort();
}

/* allocate a packet with an n byte data buffer */
Packet* Packet::alloc(int n)
{
	Packet* p = alloc();
	if (n > 0)
		p->allocdata(n);
	return (p);
}

void Packet::free(Packet* p)
{
	if (p->fflag_) {
		if (p->ref_count_ == 0) {
			/*
			 * A packet's uid may be < 0 (out of a event queue), or
			 * == 0 (newed but never gets into the event queue.
			 */
			assert(p->uid_ <= 0);
			// Delete user data because we won't need it any more.
			if (p->data_ != 0) {
				delete p->data_;
				p->data_ = 0;
			}
			init(p);
			p->next_ = free_;
			free_ = p;
			p->fflag_ = false;
		} else {
			--p->ref_count_;
		}
	}
}

Packet* Packet::copy() const
{

	Packet* p = alloc();
	memcpy(p->bits(), bits_, hdrlen_);
	if (data_){
		p->data_=new unsigned char[this->datalen()];
		memcpy(p->data_,data_,this->datalen());
	}
	p->txinfo_.init(&txinfo_);

	return (p);
}
