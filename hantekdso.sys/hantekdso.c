/*
 * hantekdso.c
 *
 *  Created on: 14 марта 2016 г.
 *      Author: alexandr
 */


#include "hantekdso.h"



static NTSTATUS WINAPI dso_create( DEVICE_OBJECT *device, IRP *irp ) {
	WINE_TRACE( "Hantek DSO open device\n" );
	irp->IoStatus.u.Status = openDSODevice();
	IoCompleteRequest( irp, IO_NO_INCREMENT );
	return STATUS_SUCCESS;
}

static NTSTATUS WINAPI dso_read( DEVICE_OBJECT *device, IRP *irp ) {
	WINE_TRACE( "Hantek DSO read device\n" );
	irp->IoStatus.u.Status = STATUS_SUCCESS;
	IoCompleteRequest( irp, IO_NO_INCREMENT );
	return STATUS_SUCCESS;
}

static NTSTATUS WINAPI dso_write( DEVICE_OBJECT *device, IRP *irp ) {
	WINE_TRACE( "Hantek DSO write device\n" );
	irp->IoStatus.u.Status = STATUS_SUCCESS;
	IoCompleteRequest( irp, IO_NO_INCREMENT );
	return STATUS_SUCCESS;
}


static NTSTATUS WINAPI dso_ioctl( DEVICE_OBJECT *device, IRP *irp )
{
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
    switch(irpsp->Parameters.DeviceIoControl.IoControlCode)
    {
    case DSO_IOCTL_REPLAY:	// Get replay     METHOD_OUT_DIRECT
    	irp->IoStatus.u.Status = dso_ioctl_replay(device, irp);
        break;
    case DSO_IOCTL_REQUEST:	// Send command   METHOD_IN_DIRECT
		irp->IoStatus.u.Status = dso_ioctl_request(device, irp);
		break;
    case DSO_IOCTL_CONTROL:	// Control        METHOD_IN_DIRECT
		irp->IoStatus.u.Status = dso_ioctl_control(device, irp);
		break;
    default:
    	WINE_FIXME( "ioctl %x not supported\n", irpsp->Parameters.DeviceIoControl.IoControlCode );
        irp->IoStatus.u.Status = STATUS_NOT_SUPPORTED;
        break;
    }
    IoCompleteRequest( irp, IO_NO_INCREMENT );
    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI dso_close( DEVICE_OBJECT *device, IRP *irp ) {
	WINE_TRACE( "Hantek DSO close device\n" );
	irp->IoStatus.u.Status = closeDSODevice();
	IoCompleteRequest( irp, IO_NO_INCREMENT );
	return STATUS_SUCCESS;
}

NTSTATUS WINAPI DriverEntry( DRIVER_OBJECT *driver, UNICODE_STRING *path )
{
    static const WCHAR device_dsoW[] = {'\\','D','e','v','i','c','e','\\','H','a','n','t','e','k','D','S','O',0};
    static const WCHAR link_dsoW[] = {'\\','D','o','s','D','e','v','i','c','e','s','\\','D','6','0','8','1','-','0',0};

    UNICODE_STRING nameW, linkW;
    NTSTATUS status;
    DEVICE_OBJECT *device;

    WINE_TRACE( "Starting Hantek DSO Wine Driver\n" );

    if (initDSODevice() == 0)
    {

    	WINE_TRACE( "Found Hantek DSO Device 0x04B5:0x6081\n" );

		driver->MajorFunction[IRP_MJ_CREATE] = dso_create;
		driver->MajorFunction[IRP_MJ_READ] = dso_read;
		driver->MajorFunction[IRP_MJ_WRITE] = dso_write;
		driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dso_ioctl;
		driver->MajorFunction[IRP_MJ_CLOSE] = dso_close;

		RtlInitUnicodeString(&nameW, device_dsoW);
		RtlInitUnicodeString(&linkW, link_dsoW);
		if (!(status = IoCreateDevice( driver, 0, &nameW, 0, 0, FALSE, &device )))
			status = IoCreateSymbolicLink( &linkW, &nameW );
		if (status)
		{
			WINE_FIXME( "failed to create device error %x\n", status );
			return status;
		}
    } else {
    	WINE_ERR( "Hantek DSO Device initialization failed\n" );
    }

    WINE_TRACE( "Hantek DSO Wine Driver was started\n" );

    return status;
}


