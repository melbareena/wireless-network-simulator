/*
 * mac-802_11.cc
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */
#include <stdio.h>
#include "random.h"
#include "wireless-phy.h"
#include "mac-802_11.h"
#include <iostream>

/* ======================================================================
 Mac Class Functions
 ====================================================================== */
int hdr_mac::offset_=sizeof(hdr_cmn);

static int MacIndex = 0;

Mac802_11::Mac802_11(int cahenum,double ber) :ber_(ber), bkmgr(this), csmgr(this), txc(this), rxc(this), phymib_(),macmib_() {
	index_=MacIndex ++;
	MAC_DBG=0;
	callback_=0;
	//phy_=0;
	pktRx_=0;
	pktTx_=0;

	ssrc_ = slrc_ = 0;

	sifs_ = macmib_.getSIFS();
	difs_ = macmib_.getSIFS() + 2*phymib_.getSlotTime();
	eifs_ = macmib_.getSIFS() + difs_ + txtime(phymib_.getACKlen(),phymib_.getBasicDataRate());
	cw_ = macmib_.getCWMin();
	bkmgr.setSlotTime(phymib_.getSlotTime());
	csmgr.setIFS(eifs_);
	sta_seqno_ = 1;
	cache_node_count_ = cahenum;
	cache_ = new Host[cache_node_count_ + 1];
	assert(cache_);
	memset(cache_,0, sizeof(Host) * (cache_node_count_+1));
	txConfirmCallback_=Callback_TXC;
}

Mac802_11::~Mac802_11(){
	MacIndex--;
}

void Mac802_11::log(char* event, char* additional) {
	if (MAC_DBG)
		std::cout<<"L "<<Scheduler::instance().clock()<<" "<<index_<<" "<<"MAC"<<" "<<event<<" "<<additional
				<<std::endl;
}

void Mac802_11::setDownProtocol(Protocol* down){
			WirelessPhy* phy=(WirelessPhy*)down;
			down_=phy;
			phy->setUpProtocol(this);
}

int Mac802_11::hdr_dst(char* hdr, int dst) {
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;

	if (dst > -2)
		STORE4BYTE(&dst, (dh->dh_ra));

	return ETHER_ADDR(dh->dh_ra);
}

int Mac802_11::hdr_src(char* hdr, int src) {
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
	if (src > -2)
		STORE4BYTE(&src, (dh->dh_ta));
	return ETHER_ADDR(dh->dh_ta);
}

void Mac802_11::discard(Packet *p,char* why) {
	Packet::free(p);
}

//IFSTimer
void IFSTimer::expire(Event *e) {
	csmgr->handleIFSTimer();
}

//NAVTimer
void NAVTimer::expire(Event *e) {
	csmgr->handleNAVTimer();
}

//ChannelStateMgr
ChannelStateMgr::ChannelStateMgr(Mac802_11 * m) {
	mac_ = m;
	channel_state_ = noCSnoNAV;
	ifs_value_ = -1.0;
	ifs_value_ = mac_->eifs_;
	ifsTimer_ = new IFSTimer(this);
	navTimer_ = new NAVTimer(this);
}

void ChannelStateMgr::setChannelState(ChannelState newState) {
	if (mac_->MAC_DBG) {
		char msg[1000];
		sprintf(msg, "%d -> %d", channel_state_, newState);
		mac_->log("ChState", msg);
	}
	channel_state_ = newState;
}

void ChannelStateMgr::setIFS(double tifs) {
	if (mac_->MAC_DBG) {
		char msg[1000];
		sprintf(msg, "%f", tifs);
		mac_->log("ChSetIFS", msg);
	}
	ifs_value_=tifs;
	//the following line of codes is used to solved the race conditon of event scheduling
	//IDleIndication is handled before RXEndIndication
	//therefore the IFSTimer is started before the new ifs_value_ is set in RXEndIndication
	if (ifsTimer_->status()==TIMER_PENDING) // ifsTimer has been scheduled
	{
		ifsTimer_->cancel();
		ifsTimer_->sched(ifs_value_);
	}
	//end of this change
}

void ChannelStateMgr::handlePHYBusyIndication() {
	switch (channel_state_) {
	case noCSnoNAV:
		mac_->bkmgr.handleCSBUSY();
		setChannelState(CSnoNAV);
		break;
	case noCSNAV:
		setChannelState(CSNAV);
		break;
	case WIFS:
		if (ifsTimer_->status()==TIMER_PENDING)
			ifsTimer_->cancel();
		setChannelState(CSnoNAV);
		break;
	default:
		//  the busy indication from phy will be ignored if the channel statemgr is already in CS_BUSY
		break;
	}
}

void ChannelStateMgr::handlePHYIdleIndication() {
	switch (channel_state_) {
	case CSnoNAV:
		ifsTimer_->sched(ifs_value_);
		setChannelState(WIFS);
		break;
	case CSNAV:
		setChannelState(noCSNAV);
		break;
	default:
		// the idle indication from phy will be ignored if the channel statemgr is already CS free
		break;
	}
}

void ChannelStateMgr::handleSetNAV(double t) {
	if (mac_->MAC_DBG) {
		char msg [1000];
		sprintf(msg, "%f", t);
		mac_->log("ChSetNAV", msg);
	}
	switch (channel_state_) {
	case noCSnoNAV:
		navTimer_->update_and_sched(t);
		mac_->bkmgr.handleCSBUSY();
		setChannelState(noCSNAV);
		break;
	case CSnoNAV:
		navTimer_->update_and_sched(t);
		setChannelState(CSNAV);
		break;
	case noCSNAV:
	case CSNAV:
		if (t > navTimer_->remaining()) {
			navTimer_->cancel();
			navTimer_->update_and_sched(t);
		}
		break;
	case WIFS:
		navTimer_->update_and_sched(t);
		if (ifsTimer_->status()==TIMER_PENDING)
			ifsTimer_->cancel();
		setChannelState(noCSNAV);
		//needs further attention
		//cout<<"logic error at ChannelStateMgr, event handleSetNAV"<<endl;
		//exit(-1);
		break;
	default:
		std::cout<<"logic error at ChannelStateMgr, event handleSetNAV"<<std::endl;
		exit(-1);
	}
}

void ChannelStateMgr::handleIFSTimer() {
	if (channel_state_ == WIFS) {
		//chi changed the order
		setChannelState(noCSnoNAV);
		mac_->bkmgr.handleCSIDLE();

	} else {
		std::cout<<"logic error at ChannelStateMgr, event handleIFSTimer"<<std::endl;
		exit(-1);
	}
}

void ChannelStateMgr::handleNAVTimer() {
	switch (channel_state_) {
	case noCSNAV:
		ifsTimer_->sched(ifs_value_);
		setChannelState(WIFS);
		break;
	case CSNAV:
		setChannelState(CSnoNAV);
		break;
	default:
		std::cout<<"logic error at ChannelStateMgr, event handleNAVTimer"<<std::endl;
		exit(-1);
	}
}

/*---------------2.0 Backoff Module----------------*/
/*--- BackoffTimer_t and BackoffMgr -----------*/
/*---------------------------------------------*/
/* BackoffTimer_t */
BackoffTimer_t::BackoffTimer_t(BackoffMgr *b) {
	bkmgr_ =b;
	tSlot = -1.0; // the real initialization is in int()
	startTime=-1.0;
	remainingSlots_=-1;
}

void BackoffTimer_t::setSlotTime(double t) {
	tSlot = t;
}

int BackoffTimer_t::init(int CW) {
	remainingSlots_ = Random::integer(CW+1); //choose a number within [0,CW]
	startTime = -1.0;
	return remainingSlots_;
}

void BackoffTimer_t::pause() {
	if (startTime ==-1.0) {
		std::cout<<"logic error at BackoffTimer_t, event pause"<<std::endl;
		exit(-1);
	}
	remainingSlots_-=(int)(floor((Scheduler::instance().clock()-startTime)/tSlot));
	if (status()==TIMER_PENDING)
		cancel();
	startTime = -1.0;
}

void BackoffTimer_t::run() {
	//chi added for testing remainging slots
	if (remainingSlots_ ==0) {
		remainingSlots_=-1;
		bkmgr_->handleBackoffTimer();
	} else {
		startTime = Scheduler::instance().clock();
		sched(remainingSlots_*tSlot);
	}
}

void BackoffTimer_t::expire(Event *e) {
	remainingSlots_=-1;
	bkmgr_->handleBackoffTimer();
	return;
}

BackoffMgr::BackoffMgr(Mac802_11 *m) {
	mac_ = m;
	bk_state_ = noBackoff; //needs further attention
	bkTimer_ = new BackoffTimer_t(this);
}

void BackoffMgr::setSlotTime(double t) {
	bkTimer_->setSlotTime(t);
}

void BackoffMgr::setBackoffMgrState(BackoffMgrState newState) {
	if (mac_->MAC_DBG) {
		char msg [1000];
		sprintf(msg, "%d -> %d, rslots: %d", bk_state_, newState,
				bkTimer_->remainingSlots_);
		mac_->log("BkState", msg);
	}
	bk_state_ = newState;
}

void BackoffMgr::handleCSIDLE() {
	switch (bk_state_) {
	case BackoffPause:
		setBackoffMgrState(BackoffRunning);
		bkTimer_->run();
		break;
	case noBackoff:
		//ignore this signal
		break;
	default:
		std::cout<<"logic error at BackoffMgr, event handleCSIDLE"<<std::endl;
		exit(-1);
	}
}

void BackoffMgr::handleCSBUSY() {
	switch (bk_state_) {
	case noBackoff:
		//ignore this signal
		break;
	case BackoffPause:
		//ignore this signal
		break;
	case BackoffRunning:

		bkTimer_->pause();
		setBackoffMgrState(BackoffPause);
		break;
	default:
		std::cout<<"logic error at BackoffMgr, event handleCSBUSY"<<std::endl;
		exit(-1);
	}
}

void BackoffMgr::handleBKStart(int CW) {
	switch (bk_state_) {
	case noBackoff:
		//Chi changed

		if (mac_->csmgr.getChannelState()==noCSnoNAV) {
			if (bkTimer_->init(CW) == 0) {
				setBackoffMgrState(noBackoff);
				BKDone();

			} else {
				setBackoffMgrState(BackoffRunning);
				bkTimer_->run();

			}
		} else {

			bkTimer_->init(CW);
			setBackoffMgrState(BackoffPause);
		}
		break;
	case BackoffRunning:
		//ignore this signal
		break;
	case BackoffPause:
		//ignore this signal
		break;
	default:
		std::cout<<"logic error at BackoffMgr, event handleBKStart"<<std::endl;
		exit(-1);
	}
}

void BackoffMgr::handleBackoffTimer() {
	switch (bk_state_) {
	case BackoffRunning:

		setBackoffMgrState(noBackoff);
		BKDone();
		break;
	default:
		std::cout<<"logic error at BackoffMgr, event handleBackoffTimer"<<std::endl;
		exit(-1);
	}
}

void BackoffMgr::BKDone() {
	mac_->handleBKDone();
}

/*----------------------------------------------------*/
/*-------------3.0 reception module-------------------*/
/*---------------PHYBusyIndication PHYIdleIndication--*/
/*---------------RXStartIndication RXEndIndication----*/

void Mac802_11::handlePHYBusyIndication() {
	if (MAC_DBG)
		log("PHYBusyInd", "");
	csmgr.handlePHYBusyIndication();
}

void Mac802_11::handlePHYIdleIndication() {
	if (MAC_DBG)
		log("PHYIdleInd", "");
	csmgr.handlePHYIdleIndication();
}

void Mac802_11::handleRXStartIndication() {
	if (MAC_DBG)
		log("RxStartInd", "");
}

void Mac802_11::handleRXEndIndication(Packet * pktRx_) {
	if (!pktRx_) {
		//collision happen
		if (MAC_DBG)
			log("RxEndInd", "Collided");
		csmgr.setIFS(eifs_);
		return;
	}

	 struct hdr_cmn *ch = HDR_CMN(pktRx_);
	hdr_mac802_11 *mh = HDR_MAC802_11(pktRx_);
	u_int32_t dst= ETHER_ADDR(mh->dh_ra);
	//u_int8_t type = mh->dh_fc.fc_type;
	u_int8_t subtype = mh->dh_fc.fc_subtype;
	int frame_len=ch->size();
	double frame_error_rate = 1 - pow ( 1-ber_, frame_len<<3 );
	csmgr.setIFS(difs_);

	if(MAC_Subtype_Data==subtype && ch->frame_error_rate_<=frame_error_rate){
		discard(pktRx_, "frame error");
		if (MAC_DBG)
			log("RxEndInd", "FRAME ERROR");
		csmgr.setIFS(eifs_);
		return;
	}

	if (MAC_DBG)
		log("RxEndInd", "SUCC");

	/*
	 * Address Filtering
	 */
	if (dst == (u_int32_t)index_ || dst == MAC_BROADCAST) {
		rxc.handleMsgFromBelow(pktRx_);
	} else {
		/*
		 *  We don't want to log this event, so we just free
		 *  the packet instead of calling the drop routine.
		 */
		if (MAC_DBG)
			log("RxEndInd", "NAV");
		csmgr.handleSetNAV(sec(mh->dh_duration));
		discard(pktRx_, "---");
		return;
	}
}

//-------------------4.0 transmission module----------------//
//---------------------TxTimer and MAC handlers-------------//

// this function is used by transmission module

void Mac802_11::transmit(Packet *p, TXConfirmCallback callback) {
	txConfirmCallback_=callback;
	// check memory leak?
	hdr_cmn* ch = HDR_CMN(p);
	ch->transmition_times_++;
	if (MAC_DBG)
		log("Tx", "Start");
	down_->recv(p->copy(), this);
}

void Mac802_11::handleTXEndIndication() {
	if (MAC_DBG)
		log("Tx", "Fin");
	switch (txConfirmCallback_) {
	case Callback_TXC:
		txc.handleTXConfirm();
		break;
	case Callback_RXC:
		rxc.handleTXConfirm();
		break;
	default:
		std::cout<<"logic error at handleTXTimer"<<std::endl;
	}
}

/*-------------------5.0 protocol coodination module----------
 ---------------------5.1 tx coordination----------------------
 ---------------------TxTimeout and MAC handlers-------------*/

// this function is used by tx coordination to prepare the MAC frame in detail

void Mac802_11::handleBKDone() {
	txc.handleBKDone();
}

// this function is used to handle the packets from the upper layer
void Mac802_11::recv(Packet *p, Handler *h) {
	struct hdr_cmn *hdr = HDR_CMN(p);
	/*
	 * Sanity Check
	 */
	assert(initialized());

	if (hdr->direction() == hdr_cmn::DOWN) {
		if (MAC_DBG)
			log("FROM_IFQ", "");
		//			send(p, h); replaced by txc
		callback_=h;
		txc.handleMsgFromUp(p);
		return;
	} else {
		//the reception of a packet is moved to handleRXStart/EndIndication
		std::cout<<"logic error at recv, event sendUP"<<std::endl;
	}
}

//-------------------5.2 rx coordination----------------------------------//
//------------------------------------------------------------------------//
void Mac802_11::recvDATA(Packet *p) {
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);
	u_int32_t dst, src, size;

	struct hdr_cmn *ch = HDR_CMN(p);
	dst = ETHER_ADDR(dh->dh_ra);
	src = ETHER_ADDR(dh->dh_ta);
	size = ch->size();
	ch->size() -= phymib_.getHdrLen11();

	if (dst != MAC_BROADCAST) {
		if (src < (u_int32_t) cache_node_count_) {
			Host *h = &cache_[src];

			if (h->seqno && h->seqno == dh->dh_scontrol) {
				if (MAC_DBG)
					log("DUPLICATE", "");
				discard(p);
				return;
			}
			h->seqno = dh->dh_scontrol;
		} else {
			if (MAC_DBG) {
				static int count = 0;
				if (++count <= 10) {
					log("DUPLICATE", "Accessing MAC CacheArray out of range");
					if (count == 10)
						log("DUPLICATE", "Suppressing additional MAC Cache warnings");
				}
			}
		}
	}

	if (MAC_DBG)
		log("SEND_UP", "");
	up_->recv(p, (Handler*) 0);
}

//-------------------6.0 txtime----------------------------------//
//------------------------------------------------------------------------//

/*
 * txtime()	- pluck the precomputed tx time from the packet header
 */
double Mac802_11::txtime(Packet *p) {
	struct hdr_cmn *ch = HDR_CMN(p);
	double t = ch->txtime();
	if (t < 0.0) {
		discard(p);
		exit(1);
	}
	return t;
}

/*
 * txtime()	- calculate tx time for packet of size "psz" bytes
 *		  at rate "drt" bps
 */
double Mac802_11::txtime(double psz, double drt) {
	int datalen = (int) psz << 3;
	double t = phymib_.getHeaderDuration() + (double)datalen/drt;
	return (t);
}

/*new code ends here*/

//-------tx coordination function-------//
void TXC_CTSTimer::expire(Event *e) {
	txc_->handleTCTStimeout();
}

void TXC_SIFSTimer::expire(Event *e) {
	txc_->handleSIFStimeout();
}

void TXC_ACKTimer::expire(Event *e) {
	txc_->handleTACKtimeout();
}

TXC::TXC(Mac802_11 *m) :
	txcCTSTimer(this), txcSIFSTimer(this), txcACKTimer(this) {
	mac_=m;
	pRTS=0;
	pDATA=0;
	txc_state_=TXC_Idle;
	shortretrycounter = 0;
	longretrycounter = 0;
	//	rtsretrycounter=0;
	//	dataretrycounter=0;
}

void TXC::handleMsgFromUp(Packet *p) {
	if (mac_->MAC_DBG)
		mac_->log("TXC", "msgFromUp");
	pDATA=p;
	prepareMPDU(pDATA);
	hdr_mac802_11* mh = HDR_MAC802_11(pDATA);
	struct hdr_cmn *ch = HDR_CMN(pDATA);

	if ((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST&& (unsigned int)(ch->size())
			>mac_->macmib_.getRTSThreshold()) //should get the right size //check type
	{
		if (mac_->MAC_DBG)
			mac_->log("TXC", "msgFromUp, RTS/CTS");
		generateRTSFrame(pDATA);

	}
	if (mac_->bkmgr.getBackoffMgrState()==noBackoff) {
		if (mac_->csmgr.getChannelState()==noCSnoNAV) {
			if (mac_->MAC_DBG)
				mac_->log("TXC", "msgFromUp, noBackoff, noCSnoNAV");
			if (pRTS!=0) {
				mac_->transmit(pRTS, Callback_TXC);
				setTXCState(TXC_wait_RTSsent);
			} else {
				mac_->transmit(pDATA, Callback_TXC);
				setTXCState(TXC_wait_PDUsent);
			}
		} else {
			if (mac_->MAC_DBG)
				mac_->log("TXC", "msgFromUp, Backoff started");
			if (pRTS!=0) {
				setTXCState(TXC_RTS_pending);
			} else {
				setTXCState(TXC_DATA_pending);
			}
			mac_->bkmgr.handleBKStart(mac_->cw_);
		}
	} else {
		if (mac_->MAC_DBG)
			mac_->log("TXC", "msgFromUp, Backoff pending");
		if (pRTS!=0) {
			setTXCState(TXC_RTS_pending);
		} else {
			setTXCState(TXC_DATA_pending);
		}
	}
}

void TXC::prepareMPDU(Packet *p) {
	hdr_cmn* ch = HDR_CMN(p);
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	ch->size() += mac_->phymib_.getHdrLen11();

	dh->dh_fc.fc_protocol_version = MAC_ProtocolVersion;
	dh->dh_fc.fc_type = MAC_Type_Data;
	dh->dh_fc.fc_subtype = MAC_Subtype_Data;
	dh->dh_fc.fc_to_ds = 0;
	dh->dh_fc.fc_from_ds = 0;
	dh->dh_fc.fc_more_frag = 0;
	dh->dh_fc.fc_retry = 0;
	dh->dh_fc.fc_pwr_mgt = 0;
	dh->dh_fc.fc_more_data = 0;
	dh->dh_fc.fc_wep = 0;
	dh->dh_fc.fc_order = 0;
	dh->dh_scontrol = mac_->sta_seqno_++;

	ch->txtime() = mac_->txtime(ch->size(), ch->rate());

	if ((u_int32_t)ETHER_ADDR(dh->dh_ra) != MAC_BROADCAST) {
		dh->dh_duration = mac_->usec(mac_->txtime(mac_->phymib_.getACKlen(),
				mac_->phymib_.getBasicDataRate())
				+ mac_->macmib_.getSIFS());
	} else {
		dh->dh_duration = 0;
	}
}

void TXC::handleBKDone() {
	if (mac_->MAC_DBG)
		mac_->log("TXC", "BK Done");
	switch (txc_state_) {
	case TXC_RTS_pending:
		mac_->transmit(pRTS, Callback_TXC);
		setTXCState(TXC_wait_RTSsent);
		break;
	case TXC_DATA_pending:
		mac_->transmit(pDATA, Callback_TXC);
		setTXCState(TXC_wait_PDUsent);
		break;
	case TXC_Idle: //check : not in the SDL
	case TXC_wait_CTS:
	case TXC_wait_ACK:
		break;
	default:
		std::cout<<txc_state_<<std::endl;
		std::cout<<"logic error at TXC handling BKDone"<<std::endl;
	}
}

void TXC::handleTXConfirm() {
	if (mac_->MAC_DBG)
		mac_->log("TXC", "TX Confirm");
	hdr_mac802_11* mh = HDR_MAC802_11(pDATA);
	switch (txc_state_) {
	case TXC_wait_PDUsent:
		if ((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST) {
			txcACKTimer.sched(mac_->macmib_.getSIFS()+ mac_->txtime(
					mac_->phymib_.getACKlen(),
					mac_->phymib_.getBasicDataRate())
					+ DSSS_MaxPropagationDelay);
			setTXCState(TXC_wait_ACK);
		} else {
			Packet::free(pDATA);
			pDATA = 0;
			mac_->bkmgr.handleBKStart(mac_->cw_);
			checkQueue();
		}
		break;
	case TXC_wait_RTSsent:
		txcCTSTimer.sched(mac_->macmib_.getSIFS()+ mac_->txtime(
				mac_->phymib_.getCTSlen(),
				mac_->phymib_.getBasicDataRate())
				+ DSSS_MaxPropagationDelay);
		setTXCState(TXC_wait_CTS);
		break;
	default:
		std::cout<<"logic error at TXC handling txconfirm"<<std::endl;

	}
}

void TXC::checkQueue() {
	setTXCState(TXC_Idle);
	if (mac_->callback_) {
		if (mac_->MAC_DBG)
			mac_->log("TXC", "checkQueue, empty");
		Handler *h = mac_->callback_;
		mac_->callback_=0;
		//???????????
		h->handle((Event*) 0);
	} else {
		if (mac_->MAC_DBG)
			mac_->log("TXC", "checkQueue, pending");
	}
}

void TXC::handleCTSIndication() {
	if (mac_->MAC_DBG)
		mac_->log("TXC", "CTS indication");
	if (txc_state_==TXC_wait_CTS) {
		Packet::free(pRTS);
		pRTS = 0;
		txcCTSTimer.cancel();
		shortretrycounter = 0;
		setTXCState(TXC_wait_SIFS);
		txcSIFSTimer.sched(mac_->macmib_.getSIFS());
	} else {
		std::cout<<"txc_state_"<<txc_state_<<std::endl;
		std::cout<<"logic error at TXC handling CTS indication"<<std::endl;
	}
}

void TXC::handleTCTStimeout() {
	if (txc_state_==TXC_wait_CTS) {
		shortretrycounter++;
		mac_->inc_cw();
		if (shortretrycounter >= mac_->macmib_.getShortRetryLimit()) {
			if (mac_->MAC_DBG)
				mac_->log("TXC", "CTS timeout, limit reached");
			mac_->discard(pRTS);
			pRTS = 0;

			// higher layers feedback support
			struct hdr_cmn *ch = HDR_CMN(pDATA);
			if (ch->xmit_failure_) {
				ch->xmit_reason_ = XMIT_REASON_RTS;
			    ch->xmit_failure_(pDATA->copy(), ch->xmit_failure_data_);
			}
			mac_->discard(pDATA);
			pDATA = 0;

			shortretrycounter = 0;
			longretrycounter = 0;
			mac_->cw_=mac_->macmib_.getCWMin();
			mac_->bkmgr.handleBKStart(mac_->cw_);
			checkQueue();
		} else {
			if (mac_->MAC_DBG)
				mac_->log("TXC", "CTS timeout, retry");
			setTXCState(TXC_RTS_pending);
			mac_->bkmgr.handleBKStart(mac_->cw_);
		}

	} else
		std::cout<<"logic error at TXC handleTCTS timeout"<<std::endl;
}

void TXC::handleSIFStimeout() {
	if (mac_->MAC_DBG)
		mac_->log("TXC", "SIFS finished");
	if (txc_state_==TXC_wait_SIFS) {
		mac_->transmit(pDATA, Callback_TXC);
		setTXCState(TXC_wait_PDUsent);
	} else
		std::cout<<"logic error at TXC handling SIFS timeout"<<std::endl;

}

void TXC::handleTACKtimeout() {
	if (txc_state_==TXC_wait_ACK) {
		hdr_mac802_11* mh = HDR_MAC802_11(pDATA);
		struct hdr_cmn *ch = HDR_CMN(pDATA);
		mh->dh_fc.fc_retry=1;

		unsigned int limit = 0;
		unsigned int * counter;
		if ((unsigned int)ch->size() <= mac_->macmib_.getRTSThreshold()) {
			limit = mac_->macmib_.getShortRetryLimit();
			counter = &shortretrycounter;
		} else {
			limit = mac_->macmib_.getLongRetryLimit();
			counter = &longretrycounter;
		}
		(*counter)++;

		mac_->inc_cw();

		if (*counter >= limit) {
			if (mac_->MAC_DBG)
				mac_->log("TXC", "ACK timeout, limit reached");

			// higher layers feedback support
			struct hdr_cmn *ch = HDR_CMN(pDATA);
			if (ch->xmit_failure_) {
				ch->xmit_reason_ = XMIT_REASON_LIMIT;
			    ch->xmit_failure_(pDATA->copy(), ch->xmit_failure_data_);
			}

			mac_->discard(pDATA);
			pDATA = 0;

			shortretrycounter = 0;
			longretrycounter = 0;
			mac_->cw_=mac_->macmib_.getCWMin();
			mac_->bkmgr.handleBKStart(mac_->cw_);
			checkQueue();
		} else {
			if (mac_->MAC_DBG)
				mac_->log("TXC", "ACK timeout, retry");

			if ((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST&&(unsigned int)(ch->size())
					> mac_->macmib_.getRTSThreshold()) {
				generateRTSFrame(pDATA);
				setTXCState(TXC_RTS_pending);
			} else {
				setTXCState(TXC_DATA_pending);
			}

			mac_->bkmgr.handleBKStart(mac_->cw_);

		}
	} else
		std::cout<<"logic error at TXC handling TACK timeout"<<std::endl;
}

void TXC::generateRTSFrame(Packet * p) {
	Packet *rts = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(rts);
	struct rts_frame *rf = (struct rts_frame*)rts->access(hdr_mac::offset_);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = mac_->phymib_.getRTSlen();
	ch->iface() = -2;
	ch->error() = 0;
	ch->rate_= mac_->phymib_.getBasicDataRate();
	memset(rf,0, sizeof(rts_frame));

	rf->rf_fc.fc_protocol_version = MAC_ProtocolVersion;
	rf->rf_fc.fc_type = MAC_Type_Control;
	rf->rf_fc.fc_subtype = MAC_Subtype_RTS;
	rf->rf_fc.fc_to_ds = 0;
	rf->rf_fc.fc_from_ds = 0;
	rf->rf_fc.fc_more_frag = 0;
	rf->rf_fc.fc_retry = 0;
	rf->rf_fc.fc_pwr_mgt = 0;
	rf->rf_fc.fc_more_data = 0;
	rf->rf_fc.fc_wep = 0;
	rf->rf_fc.fc_order = 0;

	struct hdr_mac802_11 *mh = HDR_MAC802_11(p);
	u_int32_t dst= ETHER_ADDR(mh->dh_ra);
	STORE4BYTE(&dst, (rf->rf_ra));
	STORE4BYTE(&(mac_->index_), (rf->rf_ta));

	ch->txtime() = mac_->txtime(ch->size(), ch->rate());
	rf->rf_duration = mac_->usec(mac_->macmib_.getSIFS()
			+ mac_->txtime(mac_->phymib_.getCTSlen(),
					mac_->phymib_.getBasicDataRate())
			+ mac_->macmib_.getSIFS()+ mac_->txtime(p)+ mac_->macmib_.getSIFS()
			+ mac_->txtime(mac_->phymib_.getACKlen(),
					mac_->phymib_.getBasicDataRate()));
	pRTS = rts;
}

void TXC::handleACKIndication() {
	if (mac_->MAC_DBG)
		mac_->log("TXC", "ACK indication");
	if (txc_state_==TXC_wait_ACK ) {
		txcACKTimer.cancel();
		shortretrycounter = 0;
		longretrycounter = 0;
		Packet::free(pDATA);
		pDATA=0;
		mac_->cw_=mac_->macmib_.getCWMin();
		mac_->bkmgr.handleBKStart(mac_->cw_);
		checkQueue();
	} else {
		std::cout<<"logic error at TXC handling ACKIndication"<<std::endl;
		std::cout<<txc_state_<<std::endl;
	}

}

void TXC::setTXCState(TXCState newstate) {
	if (mac_->MAC_DBG) {
		char msg [1000];
		sprintf(msg, "%d -> %d", txc_state_, newstate);
		if (mac_->MAC_DBG)
			mac_->log("TXCState", msg);
	}
	txc_state_=newstate;

}

//-------tx coordination-------//

//-------rx coordination-------//

void RXC_SIFSTimer::expire(Event *e) {
	rxc_->handleSIFStimeout();
}

RXC::RXC(Mac802_11 *m) :
	rxcSIFSTimer_(this) {
	mac_=m;
	pCTRL=0;
	rxc_state_=RXC_Idle;
}

void RXC::handleMsgFromBelow(Packet *p) {
	hdr_mac802_11* mh = HDR_MAC802_11(p);
	switch (mh->dh_fc.fc_type) {

	case MAC_Type_Control:
		switch (mh->dh_fc.fc_subtype) {
		case MAC_Subtype_ACK:
			if (mac_->MAC_DBG)
				mac_->log("RXC", "msgFromBelow: ACK");
			mac_->txc.handleACKIndication();
			mac_->mac_log(p);
			setRXCState(RXC_Idle);
			break;

		case MAC_Subtype_RTS:
			if (mac_->MAC_DBG)
				mac_->log("RXC", "msgFromBelow: RTS");
			if (mac_->csmgr.getChannelState()==CSNAV
					|| mac_->csmgr.getChannelState()==noCSNAV) {
				setRXCState(RXC_Idle);
				mac_->mac_log(p);
			} else {
				generateCTSFrame(p);
				mac_->mac_log(p);
				rxcSIFSTimer_.sched(mac_->macmib_.getSIFS());
				setRXCState(RXC_wait_SIFS);
			}
			break;
		case MAC_Subtype_CTS:
			if (mac_->MAC_DBG)
				mac_->log("RXC", "msgFromBelow: CTS");
			mac_->txc.handleCTSIndication();
			mac_->mac_log(p);
			setRXCState(RXC_Idle);
			break;
		default:
			//
			;
		}
		break;

	case MAC_Type_Data:
		if (mac_->MAC_DBG)
			mac_->log("RXC", "msgFromBelow: DATA");
		switch (mh->dh_fc.fc_subtype) {
		case MAC_Subtype_Data:
			if ((u_int32_t)ETHER_ADDR(mh->dh_ra) == (u_int32_t)(mac_->index_)) {
				generateACKFrame(p);
				rxcSIFSTimer_.sched(mac_->macmib_.getSIFS());
				mac_->recvDATA(p);
				setRXCState(RXC_wait_SIFS);
			} else {
				mac_->recvDATA(p);
				setRXCState(RXC_Idle);
			}
			break;
		default:
			std::cout<<"unkown mac data type"<<std::endl;
		}
		break;
	default:
		std::cout<<"logic error at handleincomingframe"<<std::endl;
	}
}

void RXC::handleSIFStimeout() {
	if (mac_->MAC_DBG)
		mac_->log("RXC", "SIFS finished");
	if (pCTRL != 0) {
		mac_->transmit(pCTRL, Callback_RXC);
	}
	setRXCState(RXC_wait_sent);
}

void RXC::handleTXConfirm() {
	Packet::free(pCTRL);
	pCTRL = 0;
	setRXCState(RXC_Idle);
}

void RXC::generateACKFrame(Packet *p) {
	Packet *ack = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(ack);
	struct ack_frame *af = (struct ack_frame*)ack->access(hdr_mac::offset_);
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = mac_->phymib_.getACKlen();
	ch->iface() = -2;
	ch->error() = 0;
	ch->rate_= mac_->phymib_.getBasicDataRate();
	memset(af,0, sizeof(ack_frame));

	af->af_fc.fc_protocol_version = MAC_ProtocolVersion;
	af->af_fc.fc_type = MAC_Type_Control;
	af->af_fc.fc_subtype = MAC_Subtype_ACK;
	af->af_fc.fc_to_ds = 0;
	af->af_fc.fc_from_ds = 0;
	af->af_fc.fc_more_frag = 0;
	af->af_fc.fc_retry = 0;
	af->af_fc.fc_pwr_mgt = 0;
	af->af_fc.fc_more_data = 0;
	af->af_fc.fc_wep = 0;
	af->af_fc.fc_order = 0;

	u_int32_t dst = ETHER_ADDR(dh->dh_ta);
	STORE4BYTE(&dst, (af->af_ra));
	ch->txtime() = mac_->txtime(ch->size(),
			mac_->phymib_.getBasicDataRate());
	af->af_duration = 0;
	pCTRL = ack;
}

void RXC::generateCTSFrame(Packet *p) {
	Packet *cts = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(cts);
	struct rts_frame *rf = (struct rts_frame*)p->access(hdr_mac::offset_);
	struct cts_frame *cf = (struct cts_frame*)cts->access(hdr_mac::offset_);
	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = mac_->phymib_.getCTSlen();
	ch->iface() = -2;
	ch->error() = 0;
	ch->rate_= mac_->phymib_.getBasicDataRate();
	memset(cf,0, sizeof(cts_frame));

	cf->cf_fc.fc_protocol_version = MAC_ProtocolVersion;
	cf->cf_fc.fc_type = MAC_Type_Control;
	cf->cf_fc.fc_subtype = MAC_Subtype_CTS;
	cf->cf_fc.fc_to_ds = 0;
	cf->cf_fc.fc_from_ds = 0;
	cf->cf_fc.fc_more_frag = 0;
	cf->cf_fc.fc_retry = 0;
	cf->cf_fc.fc_pwr_mgt = 0;
	cf->cf_fc.fc_more_data = 0;
	cf->cf_fc.fc_wep = 0;
	cf->cf_fc.fc_order = 0;

	u_int32_t dst = ETHER_ADDR(rf->rf_ta);
	STORE4BYTE(&dst, (cf->cf_ra));
	ch->txtime() = mac_->txtime(ch->size(),
			mac_->phymib_.getBasicDataRate());
	cf->cf_duration = mac_->usec(mac_->sec(rf->rf_duration)
			- mac_->macmib_.getSIFS()- mac_->txtime(mac_->phymib_.getCTSlen(),
			mac_->phymib_.getBasicDataRate()));
	pCTRL = cts;
}

void RXC::setRXCState(RXCState newstate) {
	if (mac_->MAC_DBG) {
		char msg [1000];
		sprintf(msg, "%d -> %d", rxc_state_, newstate);
		mac_->log("RXCState", msg);
	}
	rxc_state_=newstate;
}

//-------rx coordination-------//
