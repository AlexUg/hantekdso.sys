/*
 * dsolibusb.c
 *
 *  Created on: 10 апр. 2016 г.
 *      Author: alexandr
 */


#include <libusb-1.0/libusb.h>

#include <debug.h>

#include "dsolibusb.h"

WINE_DEFAULT_DEBUG_CHANNEL(hantekdso);

struct t_dso_device_descriptor {
	struct libusb_device_descriptor libusbDescriptor;
	libusb_device *dsoDevice;
	libusb_device_handle *dsoDeviceHandle;
	unsigned char outEndpoint;
	unsigned char inEndpoint;
};

static struct t_dso_device_descriptor *pdsoDescriptor = 0, dsoDescriptor;

int initDSODevice(void)
{
	ssize_t devCount = 0;
	ssize_t i;
	libusb_device **devices;
	libusb_device *device;
	struct libusb_device_descriptor devDescriptror;
	
	int status = libusb_init(NULL);

	if (status)
	{
		WINE_ERR( "libusb_init() failed. Error: %d\n", status );
		return 1;
	} else
	{
		devCount = libusb_get_device_list(NULL, &devices);
		if (devCount < 0)
		{
			WINE_ERR( "libusb_get_device_list() failed.\n" );
			return 2;
		}

		for (i = 0; i < devCount; i++)
		{
			device = devices[i];
			if (libusb_get_device_descriptor(device, &devDescriptror) < 0)
			{
				continue;
			}
			WINE_TRACE( "check device: 0x%04x:0x%04x\n", devDescriptror.idVendor, devDescriptror.idProduct );
			if ((devDescriptror.idVendor == 0x04B5)
					&& (devDescriptror.idProduct == 0x6081))
			{
				dsoDescriptor.libusbDescriptor = devDescriptror;
				dsoDescriptor.dsoDevice = device;
				dsoDescriptor.outEndpoint = 0x02;
				dsoDescriptor.inEndpoint = 0x86;
				pdsoDescriptor = &dsoDescriptor;
				return 0;
			}
		}
	}
	WINE_TRACE( "Hantek DSO devices aren't found\n" );
	return 3;
}

int openDSODevice(void)
{
	if (pdsoDescriptor)
	{
		return libusb_open(pdsoDescriptor->dsoDevice, &pdsoDescriptor->dsoDeviceHandle);
	}
	return 1;
}

int controlInDSODevice(uint8_t bRequest,
						uint16_t wValue,
						uint16_t wIndex,
						unsigned char *data,
						uint16_t wLength)
{
	int actualLength = 0;
	if (pdsoDescriptor && pdsoDescriptor->dsoDeviceHandle)
	{
		actualLength = libusb_control_transfer(pdsoDescriptor->dsoDeviceHandle,
												0x80 | LIBUSB_REQUEST_TYPE_VENDOR,
												bRequest,
												wValue,
												wIndex,
												data,
												wLength,
												0);
		return actualLength != wLength;
	}
	return 1;
}

int controlOutDSODevice(uint8_t bRequest,
						uint16_t wValue,
						uint16_t wIndex,
						unsigned char *data,
						uint16_t wLength)
{
	int actualLength = 0;
	if (pdsoDescriptor && pdsoDescriptor->dsoDeviceHandle)
	{
		actualLength = libusb_control_transfer(pdsoDescriptor->dsoDeviceHandle,
												LIBUSB_REQUEST_TYPE_VENDOR,
												bRequest,
												wValue,
												wIndex,
												data,
												wLength,
												0);
		return actualLength != wLength;
	}
	return 1;
}

int writeDSODevice(unsigned char *data, int length)
{
	int actualLength = 0;
	if (pdsoDescriptor && pdsoDescriptor->dsoDeviceHandle)
	{
		do
		{
			libusb_bulk_transfer(pdsoDescriptor->dsoDeviceHandle,
											pdsoDescriptor->outEndpoint,
											data,
											length,
											&actualLength,
											0);
			data += actualLength;
			length -= actualLength;
		} while (length);
		return 0;
	}
	return 1;
}

int readDSODevice(unsigned char *data, int length)
{
	int actualLength = 0;
	if (pdsoDescriptor && pdsoDescriptor->dsoDeviceHandle)
	{
		do
		{
			libusb_bulk_transfer(pdsoDescriptor->dsoDeviceHandle,
											pdsoDescriptor->inEndpoint,
											data,
											length,
											&actualLength,
											0);
			data += actualLength;
			length -= actualLength;
		} while (length);
		return 0;
	}
	return 1;
}

int closeDSODevice(void)
{
	if (pdsoDescriptor)
	{
		libusb_close(pdsoDescriptor->dsoDeviceHandle);
		pdsoDescriptor->dsoDeviceHandle = 0;
		return 0;
	}
	return 1;
}
