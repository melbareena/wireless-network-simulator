/*
 * timer-handler.h
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#ifndef TIMERHANDLER_H_
#define TIMERHANDLER_H_

#include "scheduler.h"

#define TIMER_HANDLED -1.0	// xxx: should be const double in class?

class TimerHandler : public Handler {
public:
	TimerHandler() : status_(TIMER_IDLE) { }

	void sched(double delay);	// cannot be pending
	void resched(double delay);	// may or may not be pending
					// if you don't know the pending status
					// call resched()
	void cancel();			// must be pending
	inline void force_cancel() {	// cancel!
		if (status_ == TIMER_PENDING) {
			_cancel();
			status_ = TIMER_IDLE;
		}
	}
	enum TimerStatus { TIMER_IDLE, TIMER_PENDING, TIMER_HANDLING };
	int status() { return status_; };

protected:
	virtual void expire(Event *) = 0;  // must be filled in by client
	// Should call resched() if it wants to reschedule the interface.

	virtual void handle(Event *);
	int status_;
	Event event_;

private:
	inline void _sched(double delay) {
		(void)Scheduler::instance().schedule(this, &event_, delay);
	}
	inline void _cancel() {
		(void)Scheduler::instance().cancel(&event_);
		// no need to free event_ since it's statically allocated
	}
};

#endif /* TIMERHANDLER_H_ */
