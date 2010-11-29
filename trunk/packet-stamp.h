/*
 * packet-stamp.h
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#ifndef PACKETSTAMP_H_
#define PACKETSTAMP_H_

/* -*- c++ -*-
   packet-stamp.h
   $Id: packet-stamp.h,v 1.3 1999/03/13 03:52:58 haoboy Exp $

   Information carried by a packet to allow a receive to decide if it
   will recieve the packet or not.

*/

#include "node.h"

class PacketStamp {
public:

  PacketStamp() : node_(0){ }

  void init(const PacketStamp *s) {
	  stamp(s->node_);
  }

  void stamp(Node *n) {
    node_= n;
  }

  inline Node * getNode() {return node_;}

protected:
  Node	*node_;
};

#endif /* PACKETSTAMP_H_ */
