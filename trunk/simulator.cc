/*
 * simulator.cc
 *
 *  Created on: 2010-11-24
 *      Author: xuhui
 */
#include <math.h>
#include <iostream>
#include "simulator.h"
#include "channel.h"
#include "traffic.h"

enum Level{
	sixty,
	eighty ,
	ninety,
	ninetyfive,
	ninetynine
};

double Z[]={0.842,1.282,1.645,1.960,2.576};

//Sample Standard Deviation
double ssd(double *sample,int size,double average){
	double temp=0;
	for(int i=0;i<size;i++){
		temp+=pow(sample[i]-average,2.0);
	}
	return sqrt(temp/(size-1));
}

double confidence(double *sample,int size,double s,double average,Level level){
	return Z[level]*(s/sqrt((double)size));
	//up=average+Z[level]*(s/sqrt((double)size));
}


static void startStatistic(void *param){
	Simulator* s=(Simulator*)param;
	s->startStatistic(0);
}

static void stopStatistic(void *param){
	Simulator* s=(Simulator*)param;
	s->stopStatistic(0);
}

static void stopSimulation(void *param){
	Simulator* s=(Simulator*)param;
	s->stopSimulation(0);
}

std::ofstream Simulator::logfile_(".\\ELog.txt");

Simulator::Simulator(int nodenum,double ber):nodenum_(nodenum), ber_(ber),interfacenum_(0),batch_(-1),time_(-1){
	scheduler_=new CalendarScheduler;
	scheduler_->reset();
	addChannel(new Channel());
	for(int i=0;i<nodenum;i++){
		Node* node=new Node(this);
		nodes_.insert(nodepair(node->index(),node));
	}
}

Simulator::~Simulator(){
	delete scheduler_;
	for(Nodes::iterator iter=nodes_.begin();iter!=nodes_.end();++iter){
		delete iter->second;
	}
	for(Channels::iterator iter=channels_.begin();iter!=channels_.end();++iter){
			delete iter->second;
	}
	delete[] throughput_;
}

int Simulator::getNodeNum(){
	return nodenum_;
}

void Simulator::addChannel(Protocol* channel){
	Channel* c=(Channel*)channel;
	channels_.insert(channelpair(c->index(),c));
}

void Simulator::run(){
	assert(initialized());
	double duration=0;
	for(int i=1;i<=batch_;i++){
		duration+=30;
		AtEvent* start=new AtEvent(::startStatistic,this);
		scheduler_->schedule(&at_handler,start,duration);
		duration+=time_;
		AtEvent* stop=new AtEvent(::stopStatistic,this);
		scheduler_->schedule(&at_handler,stop,duration);
	}
	duration+=10;
	AtEvent* stop=new AtEvent(::stopSimulation,this);
	scheduler_->schedule(&at_handler,stop,duration);
	throughput_=new double[batch_];
	curbatch_=0;
	memset(throughput_,0,sizeof(double)*batch_);
	delay_=0;
	transmition_=0;
	Nodes::iterator iter;	
	for(iter=nodes_.begin();iter!=nodes_.end();++iter){
		Node* node=(Node*)(iter->second);
		node->run();
	}
	scheduler_->run();
	
}

void Simulator::stopSimulation(void *){
	for(Nodes::iterator iter=nodes_.begin();iter!=nodes_.end();++iter){
		Node* node=(Node*) iter->second;
		node->stop();
	}
	double average=0;
	//double low=0;
	//double up=0;
	for(int i=0;i<batch_;i++){
		average+=throughput_[i];
	}
	average/=batch_;
	double s=ssd(throughput_,batch_,average);
	delay_/=batch_;
	transmition_/=batch_;
	logfile_<<"############################"<<std::endl;
	logfile_<<"#n="<<nodenum_<<std::endl;
	logfile_<<"#ber="<<ber_<<std::endl;
	logfile_<<"############################"<<std::endl;
	logfile_<<"throughput\t"<<average<<std::endl;
	double interval=confidence(throughput_,batch_,s,average,sixty);
	logfile_<<"60% confidence\t"<<"["<<average-interval<<","<<average+interval<<"]\tinterval\t"<<2*interval<<std::endl;
	interval=confidence(throughput_,batch_,s,average,eighty);
	logfile_<<"80% confidence\t"<<"["<<average-interval<<","<<average+interval<<"]\tinterval\t"<<2*interval<<std::endl;
	interval=confidence(throughput_,batch_,s,average,ninety);
	logfile_<<"90% confidence\t"<<"["<<average-interval<<","<<average+interval<<"]\tinterval\t"<<2*interval<<std::endl;
	interval=confidence(throughput_,batch_,s,average,ninetyfive);
	logfile_<<"95% confidence\t"<<"["<<average-interval<<","<<average+interval<<"]\tinterval\t"<<2*interval<<std::endl;
	interval=confidence(throughput_,batch_,s,average,ninetynine);
	logfile_<<"99% confidence\t"<<"["<<average-interval<<","<<average+interval<<"]\tinterval\t"<<2*interval<<std::endl;
	logfile_<<"delay\t"<<delay_<<std::endl;
	logfile_<<"transmition\t"<<transmition_<<std::endl;
	logfile_<<std::endl;
}

void Simulator::startStatistic(void *){
	Nodes::iterator iter;
	for(iter=nodes_.begin();iter!=nodes_.end();++iter){
		Node* node=(Node*)(iter->second);
		Node::traffics::iterator i;
		for(i=node->traffics_.begin();i!=node->traffics_.end();++i){
			Traffic* traffic=(Traffic*)(i->second);
			traffic->reset();
		}
	}
}

void Simulator::stopStatistic(void *){
	Nodes::iterator iter;
	double totalthroughput=0;
	double totaldelay=0;
	int totalpacket=0;
	int totaltransmition=0;
	for(iter=nodes_.begin();iter!=nodes_.end();++iter){
		Node* node=(Node*)(iter->second);
		Node::traffics::iterator iter;
		for(iter=node->traffics_.begin();iter!=node->traffics_.end();++iter){
			Traffic* traffic=(Traffic*)(iter->second);
			totalthroughput+=traffic->totalrev_;
			totaldelay+=traffic->totaldelay_;
			totalpacket+=traffic->totalpacket_;
			totaltransmition+=traffic->totaltransmition_;
		}
	}
	throughput_[curbatch_]+=totalthroughput/time_;
	curbatch_++;
	delay_+=totaldelay/totalpacket;
	transmition_+=(double)totaltransmition/totalpacket;

}
