/*
 * main.cc
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#include "simulator.h"

int n[]={5,10,15,20,30,40,50};
double ber[]={1.0e-6,1.0e-5,5.0e-5,0.0001};

int main(){

	Simulator simulator(20,0.00005);//20 站点数， 0.00005 BER
	simulator.set(2,300);//2 采样数，300每次采样时间
	simulator.run();
	//for(int i=0;i<sizeof(n)/sizeof(int);i++){
	//	for(int j=0;j<sizeof(ber)/sizeof(double);j++){
	//	Simulator simulator(n[i],ber[j]);
	//		simulator.set(40,300);
	//		simulator.run();
	//	}
	//}
	
	return 0;
}
