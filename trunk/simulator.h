/*
 * simulator.h
 *
 *  Created on: 2010-11-24
 *      Author: xuhui
 */

#ifndef SIMULATOR_H_
#define SIMULATOR_H_
#include <map>
#include <fstream>
class Scheduler;
class ParentNode;
class Node;
class Protocol;


class Simulator{
public:
	typedef std::pair<int,ParentNode* > nodepair;
	typedef std::pair<int,Protocol* > channelpair;
	typedef std::map<int,ParentNode* > Nodes;
	typedef std::map<int ,Protocol* > Channels;
	Simulator(int nodenum,double ber);
	~Simulator();
	int getNodeNum();
	inline int& interfacenum(){return interfacenum_;}
	void set(int batch,double time){
		batch_=batch;
		time_=time;
	}
	void run();
	void stopSimulation(void *);
	void addChannel(Protocol* channel);
	inline int initialized() {
			return (batch_!=-1 && time_!=-1);
	}
	void startStatistic(void *);
	void stopStatistic(void *);
private:
	int nodenum_;
	double ber_;
	int interfacenum_;
	Nodes nodes_;
	Scheduler *scheduler_;
	Channels channels_;
	int batch_;
	double time_;
	int curbatch_;
	double *throughput_;
	double delay_;
	double transmition_;
	static std::ofstream logfile_;
	friend class Node;
};


#endif /* SIMULATOR_H_ */
