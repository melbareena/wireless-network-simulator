/*
 * scheduler.cc
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#include "scheduler.h"

Scheduler* Scheduler::instance_;
//scheduler_uid_t Scheduler::uid_ = 1;

Scheduler::Scheduler() : clock_(SCHED_START), halted_(0),uid_(1)
{
}

Scheduler::~Scheduler(){
	instance_ = NULL ;
}

/*
 * Schedule an event delay time units into the future.
 * The event will be dispatched to the specified handler.
 * We use a relative time to avoid the problem of scheduling
 * something in the past.
 *
 * Scheduler::schedule does a fair amount of error checking
 * because debugging problems when events are triggered
 * is much harder (because we've lost all context about who did
 * the scheduling).
 */
void
Scheduler::schedule(Handler* h, Event* e, double delay)
{
	// handler should ALWAYS be set... if it's not, it's a bug in the caller
	if (!h) {
		fprintf(stderr,
			"Scheduler: attempt to schedule an event with a NULL handler."
			"  Don't DO that at time %f\n", clock_);
		abort();
	};

	if (e->uid_ > 0) {
		printf("Scheduler: Event UID not valid!\n\n");
		abort();
	}

	if (delay < 0) {
		// You probably don't want to do this
		// (it probably represents a bug in your simulation).
		fprintf(stderr,
			"warning: ns Scheduler::schedule: scheduling event\n\t"
			"with negative delay (%f) at time %f.\n", delay, clock_);
	}

	if (uid_ < 0) {
		fprintf(stderr, "Scheduler: UID space exhausted!\n");
		abort();
	}
	e->uid_ = uid_++;
	e->handler_ = h;
	double t = clock_ + delay;

	e->time_ = t;		//the time it is when the event done
	insert(e);
}

void
Scheduler::run()
{
	//instance_ = this;
	Event *p;
	/*
	 * The order is significant: if halted_ is checked later,
	 * event p could be lost when the simulator resumes.
	 * Patch by Thomas Kaemer <Thomas.Kaemer@eas.iis.fhg.de>.
	 */
	while (!halted_ && (p = deque())) {
		dispatch(p, p->time_);
	}
}

/*
 * dispatch a single simulator event by setting the system
 * virtul clock to the event's timestamp and calling its handler.
 * Note that the event may have side effects of placing other items
 * in the scheduling queue
 */

void
Scheduler::dispatch(Event* p, double t)
{
	if (t < clock_) {
		fprintf(stderr, "ns: scheduler going backwards in time from %f to %f.\n", clock_, t);
		abort();
	}

	clock_ = t;
	p->uid_ = -p->uid_;	// being dispatched
	p->handler_->handle(p);	// dispatch
}

void
Scheduler::dispatch(Event* p)
{
	dispatch(p, p->time_);
}

void
Scheduler::reset()
{
	clock_ = SCHED_START;
}

void
Scheduler::dumpq()
{
	Event *p;

	printf("Contents of scheduler queue (events) [cur time: %f]---\n",
		clock());
	while ((p = deque()) != NULL) {
		printf("t:%f uid: ", p->time_);
		//printf(UID_PRINTF_FORMAT, p->uid_);
		printf(" handler: %p\n", reinterpret_cast<void *>(p->handler_) );
	}
}



/*
 * Calendar queue scheduler contributed by
 * David Wetherall <djw@juniper.lcs.mit.edu>
 * March 18, 1997.
 *
 * A calendar queue basically hashes events into buckets based on
 * arrival time.
 *
 * See R.Brown. "Calendar queues: A fast O(1) priority queue implementation
 *  for the simulation event set problem."
 *  Comm. of ACM, 31(10):1220-1227, Oct 1988
 */

#define CALENDAR_HASH(t) ((int)fmod((t)/width_, nbuckets_))

CalendarScheduler::CalendarScheduler() : cal_clock_(clock_) {
	instance_ = this;
	adjust_new_width_interval_=10;
	min_bin_width_=1e-18;
	if (adjust_new_width_interval_) {
		avg_gap_ = -2;
		last_time_ = -2;
		gap_num_ = 0;
		head_search_ = 0;
		insert_search_ = 0;
		round_num_ = 0;
		time_to_newwidth = adjust_new_width_interval_;
	}
	reinit(4, 1.0, cal_clock_);
	//halted_=0;
}

CalendarScheduler::~CalendarScheduler() {
	// XXX free events?
	delete [] buckets_;
	qsize_ = 0;
	stat_qsize_ = 0;
	instance_=0;
}

void
CalendarScheduler::insert(Event* e)
{
	int i;
	double newtime = e->time_;
	if (cal_clock_ > newtime) {
		// may happen in RT scheduler
		cal_clock_ = newtime;
		i = lastbucket_ = CALENDAR_HASH(cal_clock_);
	} else
		i = CALENDAR_HASH(newtime);

	Bucket* current=(&buckets_[i]);
	Event *head = current->list_;
	Event *after=0;

	if (!head) {
		current->list_ = e;
		e->next_ = e->prev_ = e;
		++stat_qsize_;
		++(current->count_);
	} else {
		insert_search_++;
		if (newtime < head->time_) {
			//  e-> head -> ...
			e->next_ = head;
			e->prev_ = head->prev_;
			e->prev_->next_ = e;
			head->prev_ = e;
			current->list_ = e;
                        ++stat_qsize_;
                        ++(current->count_);
		} else {
                        for (after = head->prev_; newtime < after->time_; after = after->prev_) { insert_search_++; };
			//...-> after -> e -> ...
			e->next_ = after->next_;
			e->prev_ = after;
			e->next_->prev_ = e;
			after->next_ = e;
			if (after->time_ < newtime) {
				//unique timing
				++stat_qsize_;
				++(current->count_);
			}
		}
	}
	++qsize_;
	//assert(e == buckets_[i].list_ ||  e->prev_->time_ <= e->time_);
	//assert(e == buckets_[i].list_->prev_ || e->next_->time_ >= e->time_);

  	if (stat_qsize_ > top_threshold_) {
  		resize(nbuckets_ << 1, cal_clock_);
		return;
	}
}

void
CalendarScheduler::insert2(Event* e)
{
	// Same as insert, but for inserts e *before* any same-time-events, if
	//   there should be any.  Since it is used only by CalendarScheduler::newwidth(),
	//   some important checks present in insert() need not be performed.

	int i = CALENDAR_HASH(e->time_);
	Event *head = buckets_[i].list_;
	Event *before=0;
	if (!head) {
		buckets_[i].list_ = e;
		e->next_ = e->prev_ = e;
		++stat_qsize_;
		++buckets_[i].count_;
	} else {
		bool newhead;
		if (e->time_ > head->prev_->time_) { //strict LIFO, so > and not >=
			// insert at the tail
			before = head;
			newhead = false;
		} else {
			// insert event in time sorted order, LIFO for sim-time events
			for (before = head; e->time_ > before->time_; before = before->next_)
				;
			newhead = (before == head);
		}

		e->next_ = before;
		e->prev_ = before->prev_;
		before->prev_ = e;
		e->prev_->next_ = e;
		if (newhead) {
			buckets_[i].list_ = e;
			//assert(e->time_ <= e->next_->time_);
		}

		if (e != e->next_ && e->next_->time_ != e->time_) {
			// unique timing
			++stat_qsize_;
			++buckets_[i].count_;
		}
	}
	//assert(e == buckets_[i].list_ ||  e->prev_->time_ <= e->time_);
	//assert(e == buckets_[i].list_->prev_ || e->next_->time_ >= e->time_);

	++qsize_;
	// no need to check resizing
}

const Event*
CalendarScheduler::head()
{
	if (qsize_ == 0)
		return NULL;

	int l = -1, i = lastbucket_;
	int lastbucket_dec = (lastbucket_) ? lastbucket_ - 1 : nbuckets_ - 1;
	double diff;
	Event *e, *min_e = NULL;
#define CAL_DEQUEUE(x) 						\
do { 								\
	head_search_++;						\
	if ((e = buckets_[i].list_) != NULL) {			\
		diff = e->time_ - cal_clock_;			\
		if (diff < diff##x##_)	{			\
			l = i;					\
			goto found_l;				\
		}						\
		if (min_e == NULL || min_e->time_ > e->time_) {	\
			min_e = e;				\
			l = i;					\
		}						\
	}							\
	if (++i == nbuckets_) i = 0;				\
} while (0)

	// CAL_DEQUEUE applied successively will find the event to
	// dequeue (within one year) and keep track of the
	// minimum-priority event seen so far; the argument controls
	// the criteria used to decide whether the event should be
	// considered `within one year'.  Importantly, the number of
	// buckets should not be less than 4.
	CAL_DEQUEUE(0);
	CAL_DEQUEUE(1);
	for (; i != lastbucket_dec; ) {
		CAL_DEQUEUE(2);
	}
	// one last bucket is left unchecked - take the minimum
	// [could have done CAL_DEQUEUE(3) with diff3_ = bwidth*(nbuck*3/2-1)]
	e = buckets_[i].list_;
	if (min_e != NULL &&
	    (e == NULL || min_e->time_ < e->time_))
		e = min_e;
	else {
		//assert(e);
		l = i;
	}
 found_l:
	//assert(buckets_[l].count_ >= 0);
	//assert(buckets_[l].list_ == e);

	/* l is the index of the bucket to dequeue, e is the event */
	/*
	 * still want to advance lastbucket_ and cal_clock_ to save
	 * time when deque() follows.
	 */
	assert (l != -1);
	lastbucket_ = l;
 	cal_clock_  = e->time_;

	return e;
}

Event*
CalendarScheduler::deque()
{
	Event *e = const_cast<Event *>(head());

	if (!e)
		return 0;

	if (adjust_new_width_interval_) {
		if (last_time_< 0) last_time_ = e->time_;
		else
		{
			gap_num_ ++;
			if (gap_num_ >= qsize_ ) {
	                	double tt_gap_ = e->time_ - last_time_;
				avg_gap_ = tt_gap_ / gap_num_;
        	                gap_num_ = 0;
                	        last_time_ = e->time_;
				round_num_ ++;
				if ((round_num_ > 20) &&
					   (( head_search_> (insert_search_<<1))
					  ||( insert_search_> (head_search_<<1)) ))
				{
					resize(nbuckets_, cal_clock_);
					round_num_ = 0;
				} else {
                        	        if (round_num_ > 100) {
                                	        round_num_ = 0;
                                        	head_search_ = 0;
	                                        insert_search_ = 0;
        	                        }
				}
			}
		}
	};

	int l = lastbucket_;

	// update stats and unlink
	if (e->next_ == e || e->next_->time_ != e->time_) {
		--stat_qsize_;
		//assert(stat_qsize_ >= 0);
		--buckets_[l].count_;
		//assert(buckets_[l].count_ >= 0);

	}
	--qsize_;

	if (e->next_ == e)
		buckets_[l].list_ = 0;
	else {
		e->next_->prev_ = e->prev_;
		e->prev_->next_ = e->next_;
		buckets_[l].list_ = e->next_;
	}

	e->next_ = e->prev_ = NULL;


	//if (buckets_[l].count_ == 0)
	//	assert(buckets_[l].list_ == 0);

 	if (stat_qsize_ < bot_threshold_) {
		resize(nbuckets_ >> 1, cal_clock_);
	}

	return e;
}

void
CalendarScheduler::reinit(int nbuck, double bwidth, double start)
{
	buckets_ = new Bucket[nbuck];

	memset(buckets_, 0, sizeof(Bucket)*nbuck); //faster than ctor

	width_ = bwidth;
	nbuckets_ = nbuck;
	qsize_ = 0;
	stat_qsize_ = 0;

	lastbucket_ =  CALENDAR_HASH(start);

	diff0_ = bwidth*nbuck/2;
	diff1_ = diff0_ + bwidth;
	diff2_ = bwidth*nbuck;
	//diff3_ = bwidth*(nbuck*3/2-1);

	bot_threshold_ = (nbuck >> 1) - 2;
	top_threshold_ = (nbuck << 1);
}

void
CalendarScheduler::resize(int newsize, double start)
{
	double bwidth;
	if (newsize == nbuckets_) {
		/* we resize for bwidth*/
		if (head_search_) bwidth = head_search_; else bwidth = 1;
		if (insert_search_) bwidth = bwidth / insert_search_;
		bwidth = sqrt (bwidth) * width_;
 		if (bwidth < min_bin_width_) {
 			if (time_to_newwidth>0) {
 				time_to_newwidth --;
 			        head_search_ = 0;
 			        insert_search_ = 0;
 				round_num_ = 0;
 				return; //failed to adjust bwidth
 			} else {
				// We have many (adjust_new_width_interval_) times failure in adjusting bwidth.
				// should do a reshuffle with newwidth
 				bwidth = newwidth(newsize);
 			}
 		};
		//snoopy queue calculation
	} else {
		/* we resize for size */
		bwidth = newwidth(newsize);
		if (newsize < 4)
			newsize = 4;
	}

	Bucket *oldb = buckets_;
	int oldn = nbuckets_;

	reinit(newsize, bwidth, start);

	// copy events to new buckets
	int i;

	for (i = 0; i < oldn; ++i) {
		// we can do inserts faster, if we use insert2, but to
		// preserve FIFO, we have to start from the end of
		// each bucket and use insert2
		if  (oldb[i].list_) {
			Event *tail = oldb[i].list_->prev_;
			Event *e = tail;
			do {
				Event* ep = e->prev_;
				e->next_ = e->prev_ = 0;
				insert2(e);
				e = ep;
			} while (e != tail);
		}
	}
        head_search_ = 0;
        insert_search_ = 0;
	round_num_ = 0;
        delete [] oldb;
}

// take samples from the most populated bucket.
double
CalendarScheduler::newwidth(int newsize)
{
	if (adjust_new_width_interval_) {
		time_to_newwidth = adjust_new_width_interval_;
		if (avg_gap_ > 0) return avg_gap_*4.0;
	}
	int i;
	int max_bucket = 0; // index of the fullest bucket
	for (i = 1; i < nbuckets_; ++i) {
		if (buckets_[i].count_ > buckets_[max_bucket].count_)
			max_bucket = i;
	}
	int nsamples = buckets_[max_bucket].count_;

	if (nsamples <= 4) return width_;

	double nw = buckets_[max_bucket].list_->prev_->time_
		- buckets_[max_bucket].list_->time_;
	assert(nw > 0);

	nw /= ((newsize < nsamples) ? newsize : nsamples); // min (newsize, nsamples)
	nw *= 4.0;

	return nw;
}

/*
 * Cancel an event.  It is an error to call this routine
 * when the event is not actually in the queue.  The caller
 * must free the event if necessary; this routine only removes
 * it from the scheduler queue.
 *
 */
void
CalendarScheduler::cancel(Event* e)
{
	if (e->uid_ <= 0)	// event not in queue
		return;

	int i = CALENDAR_HASH(e->time_);

	assert(e->prev_->next_ == e);
	assert(e->next_->prev_ == e);

	if (e->next_ == e ||
	    (e->next_->time_ != e->time_ &&
	    (e->prev_->time_ != e->time_))) {
		--stat_qsize_;
		assert(stat_qsize_ >= 0);
		--buckets_[i].count_;
		assert(buckets_[i].count_ >= 0);
	}

	if (e->next_ == e) {
		assert(buckets_[i].list_ == e);
		buckets_[i].list_ = 0;
	} else {
		e->next_->prev_ = e->prev_;
		e->prev_->next_ = e->next_;
		if (buckets_[i].list_ == e)
			buckets_[i].list_ = e->next_;
	}

	if (buckets_[i].count_ == 0)
		assert(buckets_[i].list_ == 0);

	e->uid_ = -e->uid_;
	e->next_ = e->prev_ = NULL;

	--qsize_;

	return;
}

Event *
CalendarScheduler::lookup(scheduler_uid_t uid)
{
	for (int i = 0; i < nbuckets_; i++) {
		Event* head =  buckets_[i].list_;
		Event* p = head;
		if (p) {
			do {
				if (p->uid_== uid)
					return p;
				p = p->next_;
			} while (p != head);
		}
	}
	return NULL;
}

AtHandler at_handler;

void
AtHandler::handle(Event* e)
{
	AtEvent* at = (AtEvent*)e;
	at->callback_(at->param_);
	delete at;
}

