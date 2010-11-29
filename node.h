/*
 * node.h
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#ifndef NODE_H_
#define NODE_H_
#include <list>
#include <map>
//#include <iostream>
//#include "protocol.h"

class Simulator;
class Protocol;

class ParentNode {
public:
	ParentNode() : nodeid_(-1) {}
	virtual ~ParentNode(){}
	virtual inline int index() { return nodeid_;}
protected:
  int nodeid_;
};

class Node : public ParentNode {
public:
	//typedef std::list<WirelessPhy* > if_list
	typedef std::map<int ,Protocol* > traffics;
	typedef std::map<int ,Protocol* > macs;
	typedef std::map<int ,Protocol* > interfaces;
	typedef std::pair<int,Protocol* > pair;
	Node(Simulator* simulator);
	~Node();
	inline int index() { return nodeid_;}
	void addInterface(int channel=0);
	void run();
	void stop();
	//virtual void namlog(const char *fmt, ...);

protected:
	traffics traffics_;
	macs macs_;
	interfaces interfaces_;
	Simulator* simulator_;

	friend class Simulator;
};


#endif /* NODE_H_ */
