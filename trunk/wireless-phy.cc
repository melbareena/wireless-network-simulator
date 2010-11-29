/*
 * wireless-phy.cc
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#include <math.h>
#include <functional>
#include <map>
#include <iostream>
#include <float.h>
#include "channel.h"
#include "wireless-phy.h"
#include "mac-802_11.h"



static int InterfaceIndex = 0;

WirelessPhy::WirelessPhy(Node* node): tX_Timer(this), rX_Timer(this), preRX_Timer(this){
	assert(node!=0);
	index_ = InterfaceIndex++;
	node_ = node;
	//channel state
	state = SEARCHING;
	HeaderDuration_ = PHYHEADERDURATION;
	pkt_RX = 0;
	//power_RX = 0;
	//CSThresh_=0;
	//Pt_=1.0;
	//noise_floor_=0.0;
	//PowerMonitorThresh_=0;
	//powerMonitor = new PowerMonitor(this);
}

WirelessPhy::~WirelessPhy(){
	InterfaceIndex--;
}

void  WirelessPhy::setDownProtocol(Protocol* down){
		Channel* c=(Channel*)down;
		if(c!=0){
			c->removeInterface(this);
		}
		down_=c;
		c->setUpProtocol(this);
}
int WirelessPhy::discard(Packet *p, char* reason){
		Packet::free(p);
		return 0;
	}

void WirelessPhy::recv(Packet* p, Handler*){
	struct hdr_cmn *hdr = HDR_CMN(p);
		//struct hdr_sr *hsr = HDR_SR(p);

		/*
		 * Handle outgoing packets
		 */
		switch(hdr->direction()) {
		case hdr_cmn::DOWN :
			/*
			 * The MAC schedules its own EOT event so we just
			 * ignore the handler here.  It's only purpose
			 * it distinguishing between incoming and outgoing
			 * packets.
			 */
			sendDown(p);
			return;
		case hdr_cmn::UP :
			if (sendUp(p) == 0) {
				/*
				 * XXX - This packet, even though not detected,
				 * contributes to the Noise floor and hence
				 * may affect the reception of other packets.
				 */
				Packet::free(p);
				return;
			} else {
				up_->recv(p, (Handler*) 0);
			}
			break;
		default:
			std::cout<<"Direction for pkt-flow not specified; Sending pkt up the stack on default.\n\n";
			if (sendUp(p) == 0) {
				/*
				 * XXX - This packet, even though not detected,
				 * contributes to the Noise floor and hence
				 * may affect the reception of other packets.
				 */
				Packet::free(p);
				return;
			} else {
				up_->recv(p, (Handler*) 0);
			}
		}
}

void WirelessPhy::sendDown(Packet *p) {
	assert(initialized());

	switch (state) {
	case TXing:
		std::cout << "previous packet has not been sending out!!!!"<<std::endl;
		exit(1);
		break;
	case RXing:
		rX_Timer.cancel();
		// case 7
		discard(pkt_RX, "INT");
		pkt_RX=0;
		//power_RX=0;
		break;
	case PreRXing:
		preRX_Timer.cancel();
		// case 6
		discard(pkt_RX, "INT");
		pkt_RX=0;
		//power_RX=0;
		break;
	case SEARCHING:
		break;
	default:
		std::cout << "Only four states valid"<<std::endl;
		exit(1);
	}

	/*
	 *  Stamp the packet with the interface arguments
	 */
	p->txinfo_.stamp(node());

	// Send the packet &  set sender timer
	struct hdr_cmn * cmh = HDR_CMN(p);
	setState(TXing);
	tX_Timer.sched(cmh->txtime()+cmh->propagationDelay());
	//powerMonitor->recordPowerLevel(Pt_, cmh->txtime());
	down_->recv(p, this);
}

int WirelessPhy::sendUp(Packet *p) {
	/*
	 * Sanity Check
	 */
	assert(initialized());
	int pkt_recvd = 0;
	assert(p);
	// struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	//struct hdr_cmn * cmh = HDR_CMN(p);

	//PacketStamp s;
	//double Pr;

	//s.stamp((Node*)node(), 0);
	// pass the packet to RF model for the calculation of Pr
	//Pr = p->txinfo_.Pr;
	//powerMonitor->recordPowerLevel(Pr, cmh->txtime());

	switch (state) {
	case TXing:
		//case 12
		pkt_recvd = discard(p, "TXB");
		setState(TXing);
		break;
	case SEARCHING:
		//power_RX = Pr;
		pkt_RX=p->copy();
		setState(PreRXing);
		preRX_Timer.sched(HeaderDuration_); // preamble and PCLP header
		break;
	case PreRXing:
		HDR_CMN(pkt_RX)->error()=1;
		pkt_recvd = discard(p, "PXB");
		setState(PreRXing);
		break;
	case RXing:
		HDR_CMN(pkt_RX)->error()=1;
		pkt_recvd = discard(p, "RXB");
		setState(RXing);
		break;
	default:
		std::cout<<"packet arrive from chanel at invalid PHY state"<<std::endl;
		exit(-1);
	}
	return 0; // the incoming MAC frame will be freed by rev.
}



void WirelessPhy::handle_TXtimeout() {
	assert(state==TXing);
	Mac802_11 * mac_ = (Mac802_11*)(up_);
	mac_->handleTXEndIndication();
	setState(SEARCHING);
}

void WirelessPhy::handle_RXtimeout() {
	assert(state==RXing);
	setState(SEARCHING);
	struct hdr_cmn * cmh = HDR_CMN(pkt_RX);
	Mac802_11 * mac_ = (Mac802_11*)(up_);
	if (cmh->error()) {
		//collision happen
		//case 5, 9
		discard(pkt_RX, "RXB");
		pkt_RX=0;
		//power_RX=0;
		mac_->handleRXEndIndication(0);
	} else {
		mac_->handleRXEndIndication(pkt_RX);
		pkt_RX=0;
		//power_RX=0;
	}
}

void WirelessPhy::handle_PreRXtimeout() {
	assert(state==PreRXing);

	assert(pkt_RX);
	struct hdr_cmn * cmh = HDR_CMN(pkt_RX);
	setState(RXing);
	rX_Timer.sched(cmh->txtime()-HeaderDuration_); //receiving rest of the MAC frame
	Mac802_11 * mac = (Mac802_11*)(up_);
	mac->handleRXStartIndication();
}

void WirelessPhy::sendCSBusyIndication() {
	Mac802_11 * mac = (Mac802_11*)(up_);
	mac->handlePHYBusyIndication();
}

void WirelessPhy::sendCSIdleIndication() {
	Mac802_11 * mac = (Mac802_11*)(up_);
	mac->handlePHYIdleIndication();
}

void WirelessPhy::log(char * event, char* additional) {
}

void WirelessPhy::setState(int newstate) {

	if ( state == SEARCHING && newstate != SEARCHING ) {
		sendCSBusyIndication();
    } else if ( state != SEARCHING && newstate == SEARCHING) {
    	sendCSIdleIndication();
	}
	state = newstate;
}

int WirelessPhy::getState() {
	return state;
}

void TX_Timer::expire(Event *e) {
	wirelessPhy->handle_TXtimeout();
	return;
}

void RX_Timer::expire(Event *e) {
	wirelessPhy->handle_RXtimeout();
	return;
}

void PreRX_Timer::expire(Event *e) {
	wirelessPhy->handle_PreRXtimeout();
	return;
}



/*
PowerMonitor::PowerMonitor(WirelessPhy * phy) {
	// initialize, the NOISE is the environmental noise
	wirelessPhy = phy;
	CS_Thresh = wirelessPhy->CSThresh_; //  monitor_Thresh = CS_Thresh;
	monitor_Thresh = wirelessPhy->PowerMonitorThresh_;
	powerLevel = wirelessPhy->noise_floor_; // noise floor is 0
}

void PowerMonitor::recordPowerLevel(double signalPower, double duration) {
	// to reduce the number of entries recorded in the interfList
	if (signalPower < monitor_Thresh )
		return;

	interf timerEntry;
    timerEntry.Pt  = signalPower;
    timerEntry.end = Scheduler::instance().clock() + duration;

    list<interf>:: iterator i;
    for (i=interfList_.begin();  i != interfList_.end() && i->end <= timerEntry.end; i++) { }
    interfList_.insert(i, timerEntry);

	resched((interfList_.begin())->end - Scheduler::instance().clock());

    powerLevel += signalPower; // update the powerLevel

    if (wirelessPhy->getState() == SEARCHING && powerLevel >= CS_Thresh) {
		wirelessPhy->sendCSBusyIndication();
    }
}

double PowerMonitor::getPowerLevel() {
	if (powerLevel > wirelessPhy->noise_floor_)
		return powerLevel;
	else
		return wirelessPhy->noise_floor_;
}

void PowerMonitor::setPowerLevel(double power) {
	powerLevel = power;
}

//need fix
double PowerMonitor::SINR(double Pr) {
	if(getPowerLevel()-Pr>0) return 0.0;
	return 1.0;
}

void PowerMonitor::expire(Event *) {
	double pre_power = powerLevel;
	double time = Scheduler::instance().clock();

   	std::list<interf>:: iterator i;
   	i=interfList_.begin();
   	while(i != interfList_.end() && i->end <= time) {
       	powerLevel -= i->Pt;
       	interfList_.erase(i++);
   	}
	if (!interfList_.empty())
		resched((interfList_.begin())->end - Scheduler::instance().clock());

    	char msg[1000];
	sprintf(msg, "Power: %f -> %f", pre_power*1e9, powerLevel*1e9);
	wirelessPhy->log("PMX", msg);

	// check if the channel becomes idle ( busy -> idle )
	if (wirelessPhy->getState() == SEARCHING && powerLevel < CS_Thresh) {
		wirelessPhy->sendCSIdleIndication();
	}
}
*/

