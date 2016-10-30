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
		WINE_TRACE( "Open USB device\n" );
		if (pdsoDescriptor->dsoDeviceHandle)
		{
			closeDSODevice();
		}
		int errCode = libusb_open(pdsoDescriptor->dsoDevice, &pdsoDescriptor->dsoDeviceHandle);
		if (errCode)
		{
			WINE_ERR( "libusb_open error: %d->%s\n", errCode, libusb_error_name(errCode) );
		}
		else
		{
			errCode = libusb_set_auto_detach_kernel_driver(pdsoDescriptor->dsoDeviceHandle, 1);
			if (errCode)
			{
				WINE_ERR( "libusb_set_auto_detach_kernel_driver error: %d->%s\n", errCode, libusb_error_name(errCode) );
			}
			else
			{
				errCode = libusb_claim_interface(pdsoDescriptor->dsoDeviceHandle, 0);
				if (errCode)
				{
					errCode = libusb_set_configuration(pdsoDescriptor->dsoDeviceHandle, 1);
					if (errCode)
					{
						WINE_ERR( "libusb_set_configuration error: %d->%s\n", errCode, libusb_error_name(errCode) );
					}
					else
					{
						errCode = libusb_claim_interface(pdsoDescriptor->dsoDeviceHandle, 0);
						if (errCode)
						{
							WINE_ERR( "libusb_claim_interface error: %d->%s\n", errCode, libusb_error_name(errCode) );
						}
					}
				}
			}
		}
		return errCode;
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

int dsoBulkConversation(unsigned char ep, unsigned char *data, int length, int tryCount)
{
	int actualLength = 0;
	int tryCounts = 0;
	do
	{
		if (tryCounts < tryCount)
		{
			int errCode = libusb_bulk_transfer(pdsoDescriptor->dsoDeviceHandle,
												ep,
												data,
												length,
												&actualLength,
												0);
			if (errCode)
			{
				WINE_ERR( "libusb_bulk_transfer error: %d->%s\n", errCode, libusb_error_name(errCode) );
				closeDSODevice();
				usleep(500000);
				openDSODevice();
				usleep(100000);
			}
			if (!actualLength)
			{
				//usleep(100000);
				tryCounts++;
				WINE_ERR( "no data transferred, try again\n" );
			} else {
				tryCounts = 0;
				data += actualLength;
				length -= actualLength;
			}
		}
		else
		{
			WINE_ERR( "no data transferred, try exceed %d times. Exiting...\n", tryCount );
			return 1;
		}

	} while (length);
	return 0;
}

int writeDSODevice(unsigned char *data, int length)
{
	if (pdsoDescriptor && pdsoDescriptor->dsoDeviceHandle)
	{
		return dsoBulkConversation(pdsoDescriptor->outEndpoint, data, length, 10);
	}
	return 1;
}

int readDSODevice(unsigned char *data, int length)
{
	int actualLength = 0;
	if (pdsoDescriptor && pdsoDescriptor->dsoDeviceHandle)
	{
		return dsoBulkConversation(pdsoDescriptor->inEndpoint, data, length, 10);
	}
	return 1;
}

int closeDSODevice(void)
{
	if (pdsoDescriptor)
	{
		if (pdsoDescriptor->dsoDeviceHandle)
		{
			WINE_TRACE( "Close USB device\n" );
			libusb_release_interface(pdsoDescriptor->dsoDeviceHandle, 0);
			libusb_close(pdsoDescriptor->dsoDeviceHandle);
			pdsoDescriptor->dsoDeviceHandle = 0;
		}
		return 0;
	}
	return 1;
}
