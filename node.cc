/*
 * node.cc
 *
 *  Created on: 2010-11-24
 *      Author: xuhui
 */

#include "channel.h"
#include "wireless-phy.h"
#include "mac-802_11.h"
#include "traffic.h"
#include "simulator.h"
#include "node.h"

static int Nodeindex=0;

Node::Node(Simulator* simulator):simulator_(simulator){
		nodeid_=Nodeindex++;
		addInterface();
}

Node::~Node(){
	for(traffics::iterator iter=traffics_.begin();iter!=traffics_.end();++iter){
			delete iter->second;
	}
	for(macs::iterator iter=macs_.begin();iter!=macs_.end();++iter){
			delete iter->second;
	}
	for(interfaces::iterator iter=interfaces_.begin();iter!=interfaces_.end();++iter){
		delete iter->second;
	}
	Nodeindex--;
}

void Node::addInterface(int channel){
		assert(simulator_->channels_.size()>0);
		std::map<int ,Protocol* >::iterator iter;
		//Channel* c=0;
		if(channel==0)
			iter=simulator_->channels_.find(1);
		else
			iter=simulator_->channels_.find(channel);
		if(iter==simulator_->channels_.end()) return;
		WirelessPhy* phy=new WirelessPhy(this);
		interfaces_.insert(pair(phy->index(),phy));
		phy->setDownProtocol(iter->second);
		Mac802_11* mac=new Mac802_11(simulator_->getNodeNum(),simulator_->ber_);
		macs_.insert(pair(mac->index(),mac));
		mac->setDownProtocol(phy);
		Traffic* traffic=new Traffic(simulator_,mac->index());
		traffics_.insert(pair(traffic->index(),traffic));
		traffic->setDownProtocol((Protocol*)mac);
		simulator_->interfacenum()++;
}

void Node::run(){
	traffics::iterator iter;
	for(iter= traffics_.begin();iter!= traffics_.end();++iter){
		Traffic* traffic=(Traffic*)(iter->second);
		traffic->start();
	}
}

void Node::stop(){
	traffics::iterator iter;
	for(iter= traffics_.begin();iter!= traffics_.end();++iter){
		Traffic* traffic=(Traffic*)(iter->second);
		traffic->stop();
	}

}

