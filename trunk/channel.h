/*
 * channel.h
 *
 *  Created on: 2010-11-23
 *      Author: xuhui
 */

#ifndef CHANNEL_H_
#define CHANNEL_H_
#include <string.h>
#include <list>
#include "protocol.h"

class Packet;
class WirelessPhy;
class Node;
/*=================================================================
Channel:  a shared medium that supports contention and collision
        This class is used to represent the physical media to which
	network interfaces are attached.  As such, the sendUp()
	function simply schedules packet reception at the interfaces.
	The recv() function should never be called.
=================================================================*/

class Channel : public Protocol {
public:
	typedef std::list<WirelessPhy* > if_list;
	Channel(double delay=0);
	~Channel();
	virtual void recv(Packet* p, Handler*);
	double maxdelay() { return delay_; };
  	int index() {return index_;}

  	void setUpProtocol(const Protocol* up){
  			ifs.push_back((WirelessPhy*)up);
  	}

  	void removeInterface(WirelessPhy* phy){
  		ifs.remove(phy);
  	}

private:
	void sendUp(Packet* p, WirelessPhy *tifp);
	void dump(void);

protected:
	virtual double get_pdelay(Node* tnode, Node* rnode);
	int index_;        // multichannel support
	double delay_;     // channel delay, for collision interval
	if_list ifs;
};

#endif /* CHANNEL_H_ */
