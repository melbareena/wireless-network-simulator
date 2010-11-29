/*
 * wireless-phy.h
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#ifndef WIRELESSPHY_H_
#define WIRELESSPHY_H_

#include "timer-handler.h"
#include "protocol.h"


enum PhyState {SEARCHING = 0, PreRXing = 1, RXing = 2, TXing = 3};

class Protocal;
class Node;
class Packet;
class WirelessPhy;
//class PowerMonitor;

class TX_Timer : public TimerHandler {
public:
	TX_Timer(WirelessPhy* w) :
		TimerHandler() {
		wirelessPhy = w;
	}
protected:
	void expire(Event *e);
private:
	WirelessPhy * wirelessPhy;
};

class RX_Timer : public TimerHandler {
public:
	RX_Timer(WirelessPhy * w) :
		TimerHandler() {
		wirelessPhy = w;
	}
	void expire(Event *e);
private:
	WirelessPhy * wirelessPhy;
};

class PreRX_Timer : public TimerHandler {
public:
	PreRX_Timer(WirelessPhy * w) :
		TimerHandler() {
		wirelessPhy = w;
	}
	void expire(Event *e);
private:
	WirelessPhy * wirelessPhy;
};

//**************************************************************/
//                   1.0 WirelessPhyExt
//**************************************************************/

/*
 The PHY is in the Searching state when it is neither in transmission nor reception of a frame. Within this state, the PHY evaluates each transmission event notification from the Wireless Channel object it is attached to for potentially receivable frames. If a frame arrives with sufficient received signal strength for preamble detection (i.e. SINR > BPSK threshold), the PHY moves into the PreRXing state.
 The PHY stays in the PreRXing state for the duration of preamble and Signal portion of the PLCP header.  If the SINR of this frame stays above the BPSK and 1/2 coding rate reception threshold throughout this period, the PHY moves into the RXing state to stay for the frame duration. If a later arriving frame from the channel has sufficient received signal strength to prevent proper preamble and PLCP header reception for the current frame, the PHY moves back to the Searching state. However, if this later frame has sufficiently higher signal strength for its own preamble to be heard above others, it will trigger preamble capture, which means the PHY stays in the PreRXing state with a reset timer for the new frame.
 Within the RXing state, the PHY handles the reception of the body of the current frame. It monitors the SINR throughout the frame body duration. If the SINR drops below the threshold required by the modulation and coding rate used for the frame body at any time while in this state, the PHY marks the frame with an error flag. After RXing timeout, the PHY moves back to the Searching state. It also passes the frame to the MAC, where the error flag is directly used for the CRC check decision.
 If the frame body capture feature is enabled, then it is possible for a later arriving frame to trigger the PHY to move back to the PreRXing state for the new frame in the manner described in section 3.3.2. Otherwise, the later arriving frame has no chance of being received and is only tracked by the power monitor as an interference source.
 A transmit command from the MAC will move the PHY into the TXing state for the duration of a frame transmission regardless what the PHY is doing at the moment. The expiration of the transmission timer ends the TXing state. If a frame comes in from the channel when the PHY is in the TXing state, it is ignored and only tracked by the power monitor as interference.
 Usually the MAC will not issue a transmit command while the PHY is in the PreRXing or RXing state because of the carrier sense mechanism. However, the IEEE 802.11 standard mandates the receiver of a unicast data frame addressed to itself to turn around after SIFS and transmit an ACK frame regardless of the channel condition. Similarly, the receiver of a RTS frame, if it has an empty NAV (Network Allocation Vector), will wait for SIFS and then transmit a CTS frame regardless of the channel condition. Such scenarios are represented by the two dashed lines shown in the state machine. The PHY is designed to drop and clean up the frame it is attempting to receive and move into TXing state when this happens.
 The MAC, however, should never issue a transmit command when the PHY is still in the TXing state. The new frame has little chance of being received within its intended audience because others in general have no means to tell that a new frame is suddenly started. Therefore, the PHY is designed to issue an error and halt the simulator when this event happens because it means the MAC above has a critical error in design or implementation.

 */

class WirelessPhy:public Protocol{
public:
	WirelessPhy(Node* node);
	~WirelessPhy();
	inline Node* node(void) const {
		return node_;
	}

	int index(){
		return index_;
	}
	void setDownProtocol(Protocol* down);
	//virtual Channel* channel(void) const {return (Channel *)down_;}
	//void setchnl (Channel *chnl) { (Channel *)down_ = chnl; }
	void setState(int newstate);
	int getState();

	//timer handlers
	void handle_TXtimeout();
	void handle_RXtimeout();
	void handle_PreRXtimeout();

	//signalling to MAC layer
	void sendCSBusyIndication();
	void sendCSIdleIndication();

	void recv(Packet* p, Handler*);
	void sendDown(Packet *p);
	int sendUp(Packet *p);

	int discard(Packet *p, char* reason);

protected:
	int             index_;
	Node* node_;

private:
	// variables to be bound
	//double CSThresh_; // carrier sense threshold (W) fixed by chipset
	//double CPThresh_; // capture threshold
	//double RXThresh_; // capture threshold
	//double Pt_; // transmitted signal power (W)
	//double freq_; // frequency
	//double L_; // system loss factor
	double HeaderDuration_; // preamble+SIGNAL

	//double noise_floor_;
	//double PowerMonitorThresh_;
	//PowerMonitor *powerMonitor;

	TX_Timer tX_Timer;
	RX_Timer rX_Timer;
	PreRX_Timer preRX_Timer;
	int state;

	Packet *pkt_RX;
	//double power_RX;

	//Channel         *channel_;

	void log(char * event, char* additional); // print out state informration
	inline int initialized() {
		return (node_ && up_ && down_ );
	}

	friend class TX_Timer;
	friend class RX_Timer;
	friend class PreRX_Timer;
	//friend class PowerMonitor;
};

//**************************************************************/
//                  2.0 PowerMonitor
//**************************************************************/
/*
 The power monitor module corresponds to the PMD (Physical Media Dependent) sub-layer within the PHY. PMD is the only sub-layer that directly interacts with the analog RF signals. Therefore, all information on received signals is processed and managed in this module.
 The power monitor module keeps track of all the noise and interferences experienced by the node individually for their respective durations. Whenever the cumulative interference and noise level rises crosses the carrier sense threshold, it signals the MAC on physical carrier sense status changes. It should be noted that a node's own transmission is treated as carrier sense busy through this signaling interface as well.
 */
//**************************************************************/
/*
enum PowerMonitorState {IDLE = 0, BUSY = 1};

struct interf {
      double Pt;
      double end;
};

class PowerMonitor : public TimerHandler {
public:
	PowerMonitor(WirelessPhy *);
	void recordPowerLevel(double power, double duration);
	double getPowerLevel();
	void setPowerLevel(double);
	double SINR(double Pr);
	void expire(Event *); //virtual function, which must be implemented

private:
	double CS_Thresh;
	double monitor_Thresh;//packet with power > monitor_thresh will be recorded in the monitor
	double powerLevel;
	WirelessPhy * wirelessPhy;
    list<interf> interfList_;
};
*/



#endif /* WIRELESSPHY_H_ */
