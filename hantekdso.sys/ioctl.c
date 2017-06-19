/*
 * ioctl.c
 *
 *  Created on: 14 марта 2016 г.
 *      Author: alexandr
 */


#include "hantekdso.h"



NTSTATUS dso_ioctl_replay(DEVICE_OBJECT *device, IRP *irp)
{	// 0x22204E  =>  METHOD_OUT_DIRECT
	NTSTATUS result;
	IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
	ULONG outSize = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	PBYTE outData = (PBYTE) irp->MdlAddress->StartVa + irp->MdlAddress->ByteOffset;

	result = readDSODevice(outData, outSize);

	if (outSize)
	{
		irp->IoStatus.Information = outSize;
	}
	return result;
}

NTSTATUS dso_ioctl_request(DEVICE_OBJECT *device, IRP *irp)
{   // 0x222051  =>  METHOD_IN_DIRECT
	IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
	ULONG outSize = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	PBYTE outData = (PBYTE) irp->MdlAddress->StartVa + irp->MdlAddress->ByteOffset;

	return writeDSODevice(outData, outSize);
}

NTSTATUS dso_ioctl_control(DEVICE_OBJECT *device, IRP *irp)
{   // 0x222059  =>  METHOD_IN_DIRECT
	NTSTATUS result;
	IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
	ULONG outSize = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	PBYTE outData = (PBYTE) irp->MdlAddress->StartVa + irp->MdlAddress->ByteOffset;
	struct t_dso_ioctl_data *ioctl_data = (struct t_dso_ioctl_data *) irp->AssociatedIrp.SystemBuffer;

	if (ioctl_data->header.direction) {
		result = controlInDSODevice(ioctl_data->command, ioctl_data->value, 0, outData, outSize);
	} else {
		result = controlOutDSODevice(ioctl_data->command, ioctl_data->value, 0, outData, outSize);
	}
	if (outSize)
	{
		irp->IoStatus.Information = outSize;
	}
	return result;
}
