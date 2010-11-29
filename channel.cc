/*
 * channel.cc
 *
 *  Created on: 2010-11-23
 *      Author: xuhui
 */

#include <float.h>
#include "random.h"
#include "wireless-phy.h"
#include "channel.h"
//#include "wireless-phy.h"
//#include "node.h"

static int ChannelIndex = 1;

Channel::Channel(double delay):delay_(delay),ifs(){
	index_ = ChannelIndex++;
}

Channel::~Channel(){
	 ChannelIndex--;
}

void Channel::recv(Packet* p, Handler* h)
{
	sendUp(p, (WirelessPhy *)h);
}

void
Channel::sendUp(Packet* p, WirelessPhy *tifp)
{
	 Scheduler &s = Scheduler::instance();
	 Node *tnode = tifp->node();
	 Node *rnode = 0;
	 Packet *newp;
	 double propdelay = 0.0;
	 struct hdr_cmn *hdr = HDR_CMN(p);

	 hdr->frame_error_rate_=Random::uniform();
	 hdr->direction() = hdr_cmn::UP;
	 if_list::iterator iter;
	 for(iter=ifs.begin();iter!=ifs.end();++iter){
		 if(tifp==*iter)
			 continue;
		 rnode=(*iter)->node();
		 newp = p->copy();
		 propdelay = get_pdelay(tnode, rnode); //the time that wave go through the media cost
		 s.schedule(*iter, newp, propdelay+hdr->propagationDelay());
	 }
	 Packet::free(p);
}

double Channel::get_pdelay(Node* tnode, Node* rnode){
	return delay_;
}




