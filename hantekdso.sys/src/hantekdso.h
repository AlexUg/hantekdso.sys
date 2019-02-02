/*
 * hantekdso.h
 *
 *  Created on: 30 янв. 2019 г.
 *      Author: ugnenko
 */

#ifndef HANTEKDSO_H_
#define HANTEKDSO_H_

#include <windef.h>

#define DSO_MAX_PATH_SIZE   64
#define DSO_REG_KEY_LENGTH  32

#define DSO_IOCTL_REPLAY    0x22204E
#define DSO_IOCTL_REQUEST   0x222051
#define DSO_IOCTL_CONTROL   0x222059

struct t_dso_ioctl_header
{
  BYTE direction;
  BYTE type;
  BYTE recipient;
  BYTE data3;
};

struct t_dso_ioctl_data
{
  struct t_dso_ioctl_header header;
  BYTE command;
  BYTE data1;
  WORD value;
  WORD data2;

};

#endif /* HANTEKDSO_H_ */
