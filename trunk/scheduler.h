/*
 * scheduler.h
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

typedef int scheduler_uid_t;

class Handler;

class Event {
public:
	Event* next_;		/* event list */
	Event* prev_;
	Handler* handler_;	/* handler to call when event ready */
	double time_;		/* time at which event is ready */
	scheduler_uid_t uid_;	/* unique ID */
	Event() : time_(0), uid_(0) {}
};

/*
 * The base class for all event handlers.  When an event's scheduled
 * time arrives, it is passed to handle which must consume it.
 * i.e., if it needs to be freed it, it must be freed by the handler.
 */
class Handler {
 public:
	virtual ~Handler () {}
	virtual void handle(Event* event) = 0;
};

#define	SCHED_START	0.0	/* start time (secs) */

class Scheduler {
public:
	virtual ~Scheduler();
	static Scheduler& instance() {
		return (*instance_);		// general access to scheduler
	}
	void schedule(Handler*, Event*, double delay);	// sched later event
	virtual void run();			// execute the simulator
	virtual int& halted(){return halted_;}
	virtual void cancel(Event*) = 0;	// cancel event
	virtual void insert(Event*) = 0;	// schedule event
	virtual Event* lookup(scheduler_uid_t uid) = 0;	// look for event
	virtual Event* deque() = 0;		// next event (removes from q)
	virtual const Event* head() = 0;	// next event (not removed from q)
	double clock() const {			// simulator virtual time
		return (clock_);
	}
	virtual void sync() {};
	virtual double start() {		// start time
		return SCHED_START;
	}
	virtual void reset();
protected:
	void dumpq();	// for debug: remove + print remaining events
	void dispatch(Event*);	// execute an event
	void dispatch(Event*, double);	// exec event, set clock_
	Scheduler();
	double clock_;
	int halted_;
	static Scheduler* instance_;
	scheduler_uid_t uid_;
};

class CalendarScheduler : public Scheduler {
public:
	CalendarScheduler();
	~CalendarScheduler();
	void cancel(Event*);
	void insert(Event*);
	Event* lookup(scheduler_uid_t uid);
	Event* deque();
	const Event* head();

protected:
	double min_bin_width_;		// minimum bin width for Calendar Queue
	unsigned int adjust_new_width_interval_; // The interval (in unit of resize time) for adjustment of bin width. A zero value disables automatic bin width adjustment
	unsigned time_to_newwidth;	// how many time we failed to adjust the width based on snoopy-queue
	long unsigned head_search_;
	long unsigned insert_search_;
	int round_num_;
	long int gap_num_;		//the number of gap samples in this window (in process of calculation)
	double last_time_;		//the departure time of first event in this window
	double avg_gap_;		//the average gap in last window (finished calculation)

	double width_;
	double diff0_, diff1_, diff2_; /* wrap-around checks */

	int stat_qsize_;		/* # of distinct priorities in queue*/
	int nbuckets_;
	int lastbucket_;
	int top_threshold_;
	int bot_threshold_;

	struct Bucket {
		Event *list_;
		int    count_;
	} *buckets_;

	int qsize_;

	virtual void reinit(int nbuck, double bwidth, double start);
	virtual void resize(int newsize, double start);
	virtual double newwidth(int newsize);

private:
	virtual void insert2(Event*);
	double cal_clock_;  // same as clock in sims, may be different in RT-scheduling.

};

typedef void (*AtEventCallBack)(void *);

class AtEvent : public Event {
public:
	AtEvent(AtEventCallBack callback,void *param) : callback_(callback),param_(param) {
	}
	AtEventCallBack callback_;
	void *param_;
};

class AtHandler : public Handler {
public:
	void handle(Event* event);
} ;

extern AtHandler at_handler;



#endif /* SCHEDULER_H_ */
