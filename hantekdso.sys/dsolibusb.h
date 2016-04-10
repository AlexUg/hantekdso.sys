/*
 * dsolibusb.h
 *
 *  Created on: 10 апр. 2016 г.
 *      Author: alexandr
 */

#ifndef DSOLIBUSB_H_
#define DSOLIBUSB_H_


#include <stdint.h>


int initDSODevice(void);
int openDSODevice(void);
int controlInDSODevice(uint8_t bRequest,
						uint16_t wValue,
						uint16_t wIndex,
						unsigned char *data,
						uint16_t wLength);
int controlOutDSODevice(uint8_t bRequest,
						uint16_t wValue,
						uint16_t wIndex,
						unsigned char *data,
						uint16_t wLength);
int writeDSODevice(unsigned char *data, int length);
int readDSODevice(unsigned char *data, int length);
int closeDSODevice(void);


#endif /* DSOLIBUSB_H_ */
