/*
 * traffic.cc
 *
 *  Created on: 2010-11-24
 *      Author: xuhui
 */
#include "random.h"
#include "traffic.h"
#include "mac-802_11.h"
#include "simulator.h"


#define PAYLOAD 1500
#define DATARATE 1.0e6

static void failed_callback(Packet *p, void *arg) {
	((Traffic*) arg)->callBack(p);
}

static int TrafficIndex = 0;

Traffic::Traffic(Simulator* simulator,int host):simulator_(simulator),host_(host),totalrev_(0),totalpacket_(0),totaldelay_(0),totaltransmition_(0){
	index_=TrafficIndex++;
}

Traffic::~Traffic(){
	TrafficIndex--;
}

void Traffic::recv(Packet *p, Handler *h){
	if(p==0){
		if(!active_) return;
		Packet* p=Traffic::genPacket(PAYLOAD,DATARATE);
		Mac802_11* mac=(Mac802_11*)down_;
		mac->recv(p,this);
	}
	else{
		hdr_cmn* ch = HDR_CMN(p);
		totalrev_+=ch->size()<<3;
		totalpacket_++;
		totaldelay_+=Scheduler::instance().clock()-ch->sendtime_;
		totaltransmition_+=ch->transmition_times_;
		Packet::free(p);
	}
}

void Traffic::start(){
	active_=true;
	Packet* p=Traffic::genPacket(PAYLOAD,DATARATE);
	Mac802_11* mac=(Mac802_11*)down_;
	mac->recv(p,this);
}

void Traffic::setDownProtocol(Protocol* down){
	Mac802_11* mac=(Mac802_11*)down;
	down_=mac;
	mac->setUpProtocol(this);
}

Packet* Traffic::genPacket(int size,double rate){
	assert(initialized());
	Mac802_11* mac=(Mac802_11*)down_;
	int dst=0;
	do{
		 dst=Random::integer(simulator_->interfacenum());
	}while(dst==host_);
	Packet *p = Packet::alloc();
	hdr_mac802_11*  hdr_mac=HDR_MAC802_11(p);
	memset((char*)hdr_mac,0,sizeof(hdr_mac802_11));
	mac->hdr_dst((char*)hdr_mac,dst);
	mac->hdr_src((char*)hdr_mac,host_);
	hdr_cmn* ch = HDR_CMN(p);
	memset((char*)ch,0,sizeof(hdr_cmn));
	ch->direction()=hdr_cmn::DOWN;
	ch->size()=size;
	ch->rate()=rate;
	ch->xmit_failure_=0;
	ch->sendtime_=Scheduler::instance().clock();
	ch->transmition_times_=0;
	ch->propagationDelay_=DSSS_MaxPropagationDelay ;
	return p;
}

void Traffic::callBack(Packet *p){
	Packet::free(p);
}
