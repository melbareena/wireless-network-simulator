/*
 * protocol.h
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_
//#include "packet.h"
#include "packet.h"

enum ProtocolType{
	CHANNEL,
	PHY,
	MAC,
	NETWORK,
	APP
};

class Protocol:public Handler{
public:
	Protocol():up_(0),down_(0){}
	//virtual void drop(Packet* p);
	virtual void setDownProtocol(Protocol* down){
		down_=down;
	}
	virtual void setUpProtocol(Protocol* up){
		up_=up;
	}
	virtual void recv(Packet*, Handler* callback = 0)=0;
	virtual void sendDown(Packet* p, Handler* h)
			{down_->recv(p, h); }
	virtual void sendUp(Packet* p, Handler* h)
			{ up_->recv(p, h); }
protected:
	void handle(Event* e){
		recv((Packet*)e);
	}
	Protocol* up_;
	Protocol* down_;
};

//void Protocol::drop(Packet* p){
//	Packet::free(p);
//}

#endif /* PROTOCOL_H_ */
