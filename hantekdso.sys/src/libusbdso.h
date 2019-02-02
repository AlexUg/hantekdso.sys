/*
 * libusbdso.h
 *
 *  Created on: 29 янв. 2019 г.
 *      Author: Aleksandr Ugnenko <ugnenko@mail.ru>
 */

#ifndef LIBUSBDSO_H_
#define LIBUSBDSO_H_


// Order of using this library:
// 1. initialize list of known devices by invoking 'int add_known_device(struct t_dso_known_device *)'
//           for each needed device;
// 2. start library using 'int libusbdso_start (struct t_dso_driver_descriptor *)':
//     - libusb will be initialized;
//     - present devices will be enumerated and for devices defined in list of known devices
//           will be called call-back 't_dso_driver_descriptor -> connect_cb(dso_device_handle),
//           call-back will create WinDevice for each device;
//     - libusb hotplug callback functionality will be enabled for tracking connection and disconnection of devices;
// 3. open device using 'dso_device_handle'
//           which had passed to call-back 't_dso_driver_descriptor -> connect_cb(dso_device_handle)
// 4. work with device using 'dso_device_handle' -- 'libusbdso_write_device', 'libusbdso_read_device'
//                                                  'libusbdso_control_in_device', 'libusbdso_control_out_device'.
// 5. close device using 'int libusbdso_close_device(dso_device_handle)';
// 6. use steps 2 - 5 for working with devices;
// 7. stop library using 'int libusbdso_stop()'



#include <stdint.h>


// Defines count of libusb failed transfers
// would be trying to be recovered
#define USB_TRY_COUNTS	10

// Defines parameters of known devices.
// List of known devices is used for filtering
// devices when WinDevices are created.
struct t_dso_known_device
{
  // VendorID of device
  uint16_t vendor_id;

  // ProductID of device
  uint16_t product_id;

  // Name of device
  // will be used as part of WinDevice path (in IoCreateDevice)
  char * name;

  struct t_dso_known_device * next;
};

// Driver descriptor
// created when driver initialized when wine starts
struct t_dso_driver_descriptor
{
  // Pointer to the device descriptor 'struct t_dso_device_descriptor'
  // of first recognized usb device connected to PC.
  // After initialization 'first_device_handle' must be NULL.
  // This field will have been initialized during devices enumeration
  // (see: 'libusbdso_start' and 'libusbdso_hotplug_callback').
  void * first_device_handle;

  // Call-back function that will have been called
  // when usb device will have been connected to PC.
  // May be NULL, but this is strongly not recommended.
  int (*connect_cb) (void * dso_device_handle);

  // Call-back function that will have been called
  // when usb device will have been disconnected from PC.
  // May be NULL, but this is strongly not recommended.
  int (*disconnect_cb) (void * dso_device_handle);
};

// Adds new known device
int
add_known_device(struct t_dso_known_device * known_device);

// Checks if device is known
struct t_dso_known_device *
is_device_known(uint16_t vendor_id, uint16_t product_id);

// Initializes 'libusb' and this library.
// This function must have been started before any the others below in this library.
// driver_desc -- pointer to 'struct t_dso_driver_descriptor' object
// which must have been created and initialized in 'DriverEntry' of wine driver
int
libusbdso_start (struct t_dso_driver_descriptor * driver_desc);

// Opens the libusb device and claims intarfece.
// dso_device_handle -- pointer to the device descriptor 'struct t_dso_device_descriptor'
int
libusbdso_open_device (void * dso_device_handle);

// Write data to usb device.
// Device must be opened by 'libusbdso_open_device' previously.
// dso_device_handle -- pointer to the device descriptor 'struct t_dso_device_descriptor'
// data -- pointer to data
// length -- size of data
int
libusbdso_write_device (void * dso_device_handle, unsigned char *data,
			int length);

// Read data from usb device.
// Device must be opened by 'libusbdso_open_device' previously.
// dso_device_handle -- pointer to the device descriptor 'struct t_dso_device_descriptor'
// data -- pointer to data
// length -- size of data
int
libusbdso_read_device (void * dso_device_handle, unsigned char *data,
		       int length);

// Make input control request to usb device.
// Device must be opened by 'libusbdso_open_device' previously.
// dso_device_handle -- pointer to the device descriptor 'struct t_dso_device_descriptor'
// request -- request type
// value -- request value
// index -- request index
// data -- pointer to data of request
// length -- size of data
int
libusbdso_control_in_device (void * dso_device_handle, uint8_t request,
			     uint16_t value, uint16_t index,
			     unsigned char * data, uint16_t length);

// Make output control request to usb device.
// Device must be opened by 'libusbdso_open_device' previously.
// dso_device_handle -- pointer to the device descriptor 'struct t_dso_device_descriptor'
// request -- request type
// value -- request value
// index -- request index
// data -- pointer to data of request
// length -- size of data
int
libusbdso_control_out_device (void * dso_device_handle, uint8_t request,
			      uint16_t value, uint16_t index,
			      unsigned char * data, uint16_t length);

// Closes the libusb device and free intarfece.
// dso_device_handle -- pointer to the device descriptor 'struct t_dso_device_descriptor'
int
libusbdso_close_device (void * dso_device_handle);

// Closes 'libusb' and stops this library.
// After calling this function no other functions must not are called.
int
libusbdso_stop ();

// Returns device name.
// Full device name consists of 'device_name' and 'device_index', for example -- 'D6081-0'.
// In this case device name will be -- 'D6081'.
// Parameter 'dso_device_handle' is the pointer to 'struct t_dso_device_descriptor' object.
const char *
libusbdso_get_device_name(void * dso_device_handle);

// Returns device index.
// Full device name consists of 'device_name' and 'device_index', for example -- 'D6081-0'.
// In this case device index will be -- '0'.
// Parameter 'dso_device_handle' is the pointer to 'struct t_dso_device_descriptor' object.
uint8_t
libusbdso_get_device_index(void * dso_device_handle);

// Return pointer to DEVICE_OBJECT.
// DEVICE_OBJECT are initialized by 'IoCreateDevice' function.
// Parameter 'dso_device_handle' is the pointer to 'struct t_dso_device_descriptor' object.
void *
libusbdso_get_win_device_handle(void * dso_device_handle);

void
libusbdso_set_win_device_handle (void * dso_device_handle, void * handle);

// Returns VENDOR_ID.
// Parameter 'dso_device_handle' is the pointer to 'struct t_dso_device_descriptor' object.
uint16_t
libusbdso_get_vendor_id(void * dso_device_handle);

// Returns VENDOR_ID.
// Parameter 'dso_device_handle' is the pointer to 'struct t_dso_device_descriptor' object.
uint16_t
libusbdso_get_product_id(void * dso_device_handle);

// Returns last remembered error occured when libusb function execution had failed.
const char *
libusbdso_last_error ();

#endif /* LIBUSBDSO_H_ */
