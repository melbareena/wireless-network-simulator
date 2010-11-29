/*
 * marshall.h
 *
 *  Created on: 2010-11-22
 *      Author: xuhui
 */

#ifndef MARSHALL_H_
#define MARSHALL_H_

#define STORE4BYTE(x,y)  ((*((unsigned char *)y)) = ((*x) >> 24) & 255 ,\
			  (*((unsigned char *)y+1)) = ((*x) >> 16) & 255 ,\
			  (*((unsigned char *)y+2)) = ((*x) >> 8) & 255 ,\
			  (*((unsigned char *)y+3)) = (*x) & 255 )

#define STORE2BYTE(x,y)  ((*((unsigned char *)y)) = ((*x) >> 8) & 255 ,\
			  (*((unsigned char *)y+1)) = (*x) & 255 )

#define GET2BYTE(x)      (((*(unsigned char *)(x)) << 8) |\
			  (*(((unsigned char *)(x))+1)))

#define GET4BYTE(x)      (((*(unsigned char *)(x)) << 24) |\
			  (*(((unsigned char *)(x))+1) << 16) |\
			  (*(((unsigned char *)(x))+2) << 8) |\
			  (*(((unsigned char *)(x))+3)))


#endif /* MARSHALL_H_ */
