/*
 * hantekdso.h
 *
 *  Created on: 14 марта 2016 г.
 *      Author: alexandr
 */

#ifndef HANTEKDSO_H_
#define HANTEKDSO_H_


#include "dsolibusb.h"

#include <stdarg.h>

#define NONAMELESSUNION

#include <debug.h>
#include <windef.h>
#include <winternl.h>
#include <ntstatus.h>
#include <ddk/wdm.h>
#include <wtypes.h>

WINE_DEFAULT_DEBUG_CHANNEL(hantekdso);


#define DSO_IOCTL_REPLAY	0x22204E
#define DSO_IOCTL_REQUEST	0x222051
#define DSO_IOCTL_CONTROL	0x222059



struct t_dso_ioctl_header {
	BYTE	direction;
	BYTE	type;
	BYTE	recipient;
	BYTE	data3;
};

struct t_dso_ioctl_data {
	struct t_dso_ioctl_header	header;
	BYTE	command;
	BYTE	data1;
	WORD	value;
	WORD	data2;

};

NTSTATUS dso_ioctl_replay(DEVICE_OBJECT *device, IRP *irp);
NTSTATUS dso_ioctl_request(DEVICE_OBJECT *device, IRP *irp);
NTSTATUS dso_ioctl_control(DEVICE_OBJECT *device, IRP *irp);


#endif /* HANTEKDSO_H_ */
