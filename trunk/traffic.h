/*
 * traffic.h
 *
 *  Created on: 2010-11-24
 *      Author: xuhui
 */

#ifndef TRAFFIC_H_
#define TRAFFIC_H_
#include "protocol.h"
//class Protocol;
class Packet;
class Simulator;
class Node;

class Traffic:public Protocol{
public:
	Traffic(Simulator* simulator,int host);
	~Traffic();
	int index(){return index_;}
	void recv(Packet *p, Handler *h);
	void setDownProtocol(Protocol* down);
	Packet* genPacket(int size,double rate);
	void callBack(Packet *p);
	inline int initialized() {
			return (simulator_  && down_);
	}
	void reset(){
		totalrev_=0;
		totalpacket_=0;
		totaldelay_=0;
		totaltransmition_=0;
	}
	double getTotalRev(){return totalrev_; };
	void start();
	void stop(){active_=false;};
protected:
	Simulator* simulator_;
	int host_;
	double totalrev_; //in bit
	int totalpacket_;
	double totaldelay_;
	int totaltransmition_;
	int index_;
	bool active_;
	//int size_;
	friend class Simulator;
};

#endif /* TRAFFIC_H_ */
