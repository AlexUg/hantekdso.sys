/*
 * libusbdso.c
 *
 *  Created on: 29 янв. 2019 г.
 *      Author: ugnenko
 */

#include "libusbdso.h"


#undef   _WIN32
#undef   __CYGWIN__
#undef   _WIN32_WCE

#include <libusb-1.0/libusb.h>
#include <pthread.h>

#include <string.h>
#include <stdlib.h>

#include "errors.h"



// Device descriptor
// created for each device connected to PC
struct t_dso_device_descriptor
{

  // pointer to previous structure
  struct t_dso_device_descriptor * prev;

  // pointer to next structure
  struct t_dso_device_descriptor * next;

  // pointer to 'libusb_device' object
  // will have been defined during devices enumeration
  // (see: 'create_dso_descriptor'
  // which be called from 'libusbdso_start' or 'libusbdso_hotplug_callback')
  libusb_device * dso_device;

  // pointer to 'dso_device_handle' object
  // will have been defined when device opened
  // (see: 'libusbdso_open_device')
  libusb_device_handle * dso_device_handle;

  // pointer to 'DEVICE_OBJECT' object
  // will have been defined when windows device created
  // (see: 'dso_connect_cb' in hantekdso.c
  // which responsible for creating devices by calling 'IoCreateDevice')
  void * win_device_handle;

  // Device name part from full device name
  // full device name consists of 'device_name' and 'device_index', for example -- 'D6081-0'.
  // in this case device name will be -- 'D6081'.
  const char * dso_name;

  // Device index part from full device name
  // full device name consists of 'device_name' and 'device_index', for example -- 'D6081-0'.
  // in this case device index will be -- 0.
  uint8_t dso_index;

  // usb device out endpoint
  // will have been defined during devices enumeration
  // (see: 'create_dso_descriptor')
  uint8_t out_endpoint;

  // usb device in endpoint
  // will have been defined during devices enumeration
  // (see: 'create_dso_descriptor')
  uint8_t in_endpoint;

  // 'libusb_device_descriptor' structure
  // will have been defined during devices enumeration
  // (see: 'create_dso_descriptor')
  struct libusb_device_descriptor libusb_descriptor;
};

// holds the error code of the last failed 'libusb' function execution
static int libusb_status_code = 0;

// List of known devices
static struct t_dso_known_device * known_devices = NULL;

// Driver descriptor.
// Passed as parameter to 'int libusbdso_start (struct t_dso_driver_descriptor *'.
static struct t_dso_driver_descriptor * driver_descripor;

// Handle for 'libusb' hotplug functionality.
static libusb_hotplug_callback_handle libusb_hp_handle;

// Thread for processing 'libusb' events.
static pthread_t event_thread;
static pthread_attr_t event_thread_attr;
static char event_thread_run = 1; // Run flag for thread
static pthread_mutex_t dso_lock;

// Test for equivalence of two devices.
int
check_known_device (struct t_dso_known_device * known_device,
                    struct t_dso_known_device * new_device);

// Creates and adds 't_dso_device_descriptor' to the list of connected devices.
// Initializes fields 'dso_name' and 'dso_index' in 't_dso_device_descriptor'.
// (see: 't_dso_driver_descriptor * driver_descripor -> first_device_handle')
int
process_usb_device (libusb_device * device);

// Creates 't_dso_device_descriptor'
int
create_dso_descriptor (libusb_device *device,
                       struct t_dso_device_descriptor ** dso_descriptor);

// Adds 't_dso_device_descriptor' to the list of connected devices.
// Initializes fields 'dso_name' and 'dso_index' in 't_dso_device_descriptor'.
void
add_dso_descriptor (struct t_dso_device_descriptor * dso_desc,
                    struct t_dso_known_device * known_device);

// Removes 't_dso_device_descriptor' from the list of connected devices.
// (see: 't_dso_driver_descriptor * driver_descripor -> first_device_handle')
void
delete_dso_descriptor (struct t_dso_device_descriptor * dso_descriptor);

// Finds 't_dso_device_descriptor' in the list of connected devices.
// (see: 't_dso_driver_descriptor * driver_descripor -> first_device_handle')
struct t_dso_device_descriptor *
find_dso_descriptor (libusb_device * dev);

// Makes a bulk transfer to device.
int
dso_bulk_conversation (libusb_device_handle * dso_device_handle,
                       unsigned char ep, unsigned char *data, int length,
                       int tryCount);

int
add_known_device (struct t_dso_known_device * known_device)
{
  struct t_dso_known_device * last_device;

  if (known_devices)
    {
      last_device = known_devices;
      if (check_known_device (last_device, known_device))
        {
          return DSO_DUBLICATE_DEVICE_DEFINITION;
        }
      while (last_device->next)
        {
          last_device = last_device->next;
          if (check_known_device (last_device, known_device))
            {
              return DSO_DUBLICATE_DEVICE_DEFINITION;
            }
        }
      last_device->next = known_device;
    }
  else
    {
      known_devices = known_device;
    }
  return DSO_NO_ERROR;
}

struct t_dso_known_device *
is_device_known (uint16_t vendor_id, uint16_t product_id)
{
  struct t_dso_known_device * last_device = known_devices;

  while (last_device)
    {
      if ((vendor_id == last_device->vendor_id)
          && (product_id == last_device->product_id))
        {
          return last_device;
        }
      last_device = last_device->next;
    }
  return NULL;
}

int
check_known_device (struct t_dso_known_device * known_device,
                    struct t_dso_known_device * new_device)
{
  if ((known_device->vendor_id == new_device->vendor_id)
      && (known_device->product_id == new_device->product_id))
    {
      return DSO_DUBLICATE_DEVICE_DEFINITION;
    }
  return DSO_NO_ERROR;
}

void *
libusbdso_event_thread_func (void *ctx)
{
  while (event_thread_run)
    {
      libusb_handle_events_completed(ctx, NULL);
    }
  return NULL;
}

int LIBUSB_CALL
libusbdso_hotplug_callback (libusb_context *ctx, libusb_device *dev,
                            libusb_hotplug_event event, void *user_data)
{
  struct t_dso_device_descriptor * dso_desc = NULL;
  struct t_dso_known_device * known_device = NULL;
  int status;

  pthread_mutex_lock(&dso_lock);

  if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event)
    {
      process_usb_device (dev);
    }
  else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event)
    {
      dso_desc = find_dso_descriptor (dev);
      if (dso_desc)
        {
          if (driver_descripor->disconnect_cb)
            {
              driver_descripor->disconnect_cb (dso_desc);
            }
          delete_dso_descriptor (dso_desc);
        }
    }
  else
    {
      // TODO report error
    }
  pthread_mutex_unlock(&dso_lock);
  return 0;
}


int
libusbdso_start (struct t_dso_driver_descriptor * driver_desc)
{
  libusb_device **devices;
  libusb_device *device;
  struct libusb_device_descriptor dev_descriptror;
  struct t_dso_device_descriptor * dso_desc = NULL;
  int dev_count, i, status;

  if (driver_desc)
    {
      driver_descripor = driver_desc;
      libusb_status_code = libusb_init (NULL);
      if (libusb_status_code)
        {
          return DSO_INIT;
        }

      pthread_attr_init (&event_thread_attr);
      pthread_create (&event_thread, &event_thread_attr,
                      libusbdso_event_thread_func,
                      NULL);

      libusb_hotplug_register_callback (
          NULL,
          LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED
              | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
          LIBUSB_HOTPLUG_ENUMERATE,
          LIBUSB_HOTPLUG_MATCH_ANY,
          LIBUSB_HOTPLUG_MATCH_ANY,
          LIBUSB_HOTPLUG_MATCH_ANY,
          libusbdso_hotplug_callback,
          NULL,
          &libusb_hp_handle);

      return DSO_NO_ERROR;
    }
  else
    {
      return DSO_NULL_DESCRIPTER;
    }
}

int
libusbdso_open_device (void * dso_device_handle)
{
  struct t_dso_device_descriptor * dso_desc;
  int status;

  if (dso_device_handle)
    {
      pthread_mutex_lock(&dso_lock);

      dso_desc = dso_device_handle;
      if (dso_desc->dso_device_handle)
        {
//          libusbdso_close_device (dso_desc);
          status = DSO_NO_ERROR;
        }
      else
        {
        libusb_status_code = libusb_open (dso_desc->dso_device,
                                          &dso_desc->dso_device_handle);
        if (libusb_status_code)
          {
            status = DSO_OPEN_DEIVCE;
          }
        else
          {
            status = DSO_NO_ERROR;
            libusb_status_code = libusb_set_auto_detach_kernel_driver (
                dso_desc->dso_device_handle, 1);
            if (libusb_status_code)
              {
                status = DSO_DETACH_KRN_DRV;
              }
            else
              {
                libusb_status_code = libusb_claim_interface (
                    dso_desc->dso_device_handle, 0);
                if (libusb_status_code)
                  {
                    libusb_status_code = libusb_set_configuration (
                        dso_desc->dso_device_handle, 1);
                    if (libusb_status_code)
                      {
                        status = DSO_SET_CONF;
                      }
                    else
                      {
                        libusb_status_code = libusb_claim_interface (
                            dso_desc->dso_device_handle, 0);
                        if (libusb_status_code)
                          {
                            status = DSO_CLAIM_IFACE;
                          }
                      }
                  }
              }
            if (status)
              {
                libusb_close (dso_desc->dso_device_handle);
                dso_desc->dso_device_handle = 0;
              }
          }
        }

      pthread_mutex_unlock(&dso_lock);

    }
  else
    {
      status = DSO_NULL_DESCRIPTER;
    }
  return status;
}

int
libusbdso_write_device (void * dso_device_handle, unsigned char *data,
                        int length)
{
  struct t_dso_device_descriptor * dso_desc;
  int status;

  if (dso_device_handle)
    {
      pthread_mutex_lock(&dso_lock);
      status = DSO_NO_ERROR;

      dso_desc = dso_device_handle;
      if (dso_desc->dso_device_handle)
        {
          if (dso_bulk_conversation (dso_desc->dso_device_handle,
                                     dso_desc->out_endpoint, data, length,
                                     USB_TRY_COUNTS))
            {
              status = DSO_WRITE_DEVICE;
            }
        }
      else
        {
          status = DSO_NULL_HANDLE;
        }

      pthread_mutex_unlock(&dso_lock);
    }
  else
    {
      status = DSO_NULL_DESCRIPTER;
    }
  return status;
}

int
libusbdso_read_device (void * dso_device_handle, unsigned char *data,
                       int length)
{
  struct t_dso_device_descriptor * dso_desc;
  int status;

  if (dso_device_handle)
    {
      pthread_mutex_lock(&dso_lock);
      status = DSO_NO_ERROR;

      dso_desc = dso_device_handle;
      if (dso_desc->dso_device_handle)
        {
          if (dso_bulk_conversation (dso_desc->dso_device_handle,
                                     dso_desc->in_endpoint, data, length,
                                     USB_TRY_COUNTS))
            {
              status = DSO_READ_DEVICE;
            }
        }
      else
        {
          status = DSO_NULL_HANDLE;
        }

      pthread_mutex_unlock(&dso_lock);
    }
  else
    {
      status = DSO_NULL_DESCRIPTER;
    }
  return status;
}

int
libusbdso_control_in_device (void * dso_device_handle, uint8_t request,
                             uint16_t value, uint16_t index,
                             unsigned char * data, uint16_t length)
{
  struct t_dso_device_descriptor * dso_desc;
  int status;

  if (dso_device_handle)
    {
      pthread_mutex_lock(&dso_lock);
      status = DSO_NO_ERROR;

      dso_desc = dso_device_handle;
      if (dso_desc->dso_device_handle)
        {
          libusb_status_code = libusb_control_transfer (
              dso_desc->dso_device_handle,
              LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR, request, value,
              index, data, length, 0);
//          if ((libusb_status_code < 0) || (libusb_status_code != length))
//            {
//              status = DSO_CTRL_IN_DEVICE;
//            }
          if (libusb_status_code < 0)
            {
              status = DSO_CTRL_IN_DEVICE;
            }
        }
      else
        {
          status = DSO_NULL_HANDLE;
        }

      pthread_mutex_unlock(&dso_lock);
    }
  else
    {
      status = DSO_NULL_DESCRIPTER;
    }
  return status;
}

int
libusbdso_control_out_device (void * dso_device_handle, uint8_t request,
                              uint16_t value, uint16_t index,
                              unsigned char * data, uint16_t length)
{
  struct t_dso_device_descriptor * dso_desc;
  int status;

  if (dso_device_handle)
    {
      pthread_mutex_lock(&dso_lock);
      status = DSO_NO_ERROR;

      dso_desc = dso_device_handle;
      if (dso_desc->dso_device_handle)
        {
          libusb_status_code = libusb_control_transfer (
              dso_desc->dso_device_handle,
              LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR, request, value,
              index, data, length, 0);
//          if ((libusb_status_code < 0) || (libusb_status_code != length))
//            {
//              status = DSO_CTRL_IN_DEVICE;
//            }
          if (libusb_status_code < 0)
            {
              status = DSO_CTRL_IN_DEVICE;
            }
        }
      else
        {
          status = DSO_NULL_HANDLE;
        }

      pthread_mutex_unlock(&dso_lock);
    }
  else
    {
      status = DSO_NULL_DESCRIPTER;
    }
  return status;
}

int
libusbdso_close_device (void * dso_device_handle)
{
  struct t_dso_device_descriptor * dso_desc;
  int status;

  if (dso_device_handle)
    {
      pthread_mutex_lock(&dso_lock);
      status = DSO_NO_ERROR;

      dso_desc = dso_device_handle;
      if (dso_desc->dso_device_handle)
        {
          libusb_status_code = libusb_release_interface (
              dso_desc->dso_device_handle, 0);
          if (libusb_status_code)
            {
              status = DSO_RELEASE_IFACE;
            }
          libusb_close (dso_desc->dso_device_handle);
          dso_desc->dso_device_handle = 0;
        }

      pthread_mutex_unlock(&dso_lock);
    }
  else
    {
      status = DSO_NULL_DESCRIPTER;
    }
  return status;
}

int
libusbdso_stop ()
{
  struct t_dso_known_device * known_device;

  event_thread_run = 0;
  libusb_hotplug_deregister_callback (NULL, libusb_hp_handle);
  pthread_join (event_thread, NULL);
  libusb_exit (NULL);

  while (driver_descripor->first_device_handle)
    {
      delete_dso_descriptor (driver_descripor->first_device_handle);
    }
  known_device = known_devices;
  while (known_device)
    {
      known_devices = known_devices->next;
      free(known_device->name);
      free(known_device);
      known_device = known_devices;
    }

  return 0;
}

const char *
libusbdso_get_device_name (void * dso_device_handle)
{
  if (dso_device_handle)
    {
      return ((struct t_dso_device_descriptor *) dso_device_handle)->dso_name;
    }
  else
    {
      return 0;
    }
}

uint8_t
libusbdso_get_device_index (void * dso_device_handle)
{
  if (dso_device_handle)
    {
      return ((struct t_dso_device_descriptor *) dso_device_handle)->dso_index;
    }
  else
    {
      return 0;
    }
}

void *
libusbdso_get_win_device_handle (void * dso_device_handle)
{
  if (dso_device_handle)
    {
      return ((struct t_dso_device_descriptor *) dso_device_handle)->win_device_handle;
    }
  else
    {
      return 0;
    }
}

void
libusbdso_set_win_device_handle (void * dso_device_handle, void * handle)
{
  if (dso_device_handle)
    {
      ((struct t_dso_device_descriptor *) dso_device_handle)->win_device_handle = handle;
    }
}

uint16_t
libusbdso_get_vendor_id (void * dso_device_handle)
{
  if (dso_device_handle)
    {
      return ((struct t_dso_device_descriptor *) dso_device_handle)->libusb_descriptor.idVendor;
    }
  else
    {
      return 0;
    }
}

uint16_t
libusbdso_get_product_id (void * dso_device_handle)
{
  if (dso_device_handle)
    {
      return ((struct t_dso_device_descriptor *) dso_device_handle)->libusb_descriptor.idProduct;
    }
  else
    {
      return 0;
    }
}

const char *
libusbdso_last_error ()
{
  return libusb_error_name (libusb_status_code);
}

int
libusbdso_last_error_code ()
{
  return libusb_status_code;
}

int
process_usb_device (libusb_device * device)
{
  struct t_dso_device_descriptor * dso_desc = NULL;
  struct t_dso_known_device * known_device = NULL;
  int status;

  dso_desc = find_dso_descriptor(device);
  if (!dso_desc)
    {
      status = create_dso_descriptor (device, &dso_desc);
      if (status)
        {
          return status;
        }
      else
        {
          known_device = is_device_known (dso_desc->libusb_descriptor.idVendor,
                                          dso_desc->libusb_descriptor.idProduct);
          if (known_device)
            {
              add_dso_descriptor (dso_desc, known_device);
              if (driver_descripor->connect_cb)
                {
                  driver_descripor->connect_cb (dso_desc);
                }
            }
          else
            {
              free (dso_desc);
            }
        }
    }
  return 0;
}

int
create_dso_descriptor (libusb_device * device,
                       struct t_dso_device_descriptor ** dso_descriptor)
{
  *dso_descriptor = malloc (sizeof(struct t_dso_device_descriptor));
  if (*dso_descriptor)
    {
      memset (*dso_descriptor, 0, sizeof(struct t_dso_device_descriptor));
      (*dso_descriptor)->dso_device = device;
      libusb_get_device_descriptor (device,
                                    &((*dso_descriptor)->libusb_descriptor));

      // TODO find and/or check endpoints
      (*dso_descriptor)->out_endpoint = 0x02;
      (*dso_descriptor)->in_endpoint = 0x86;

      return 0;
    }
  else
    {
      return DSO_NO_MEMORY;
    }
}

void
add_dso_descriptor (struct t_dso_device_descriptor * dso_desc,
                    struct t_dso_known_device * known_device)
{
  struct t_dso_device_descriptor * prev_dso_desc, *next_dso_desc;
  int16_t last_index = -1;

  prev_dso_desc = NULL;
  next_dso_desc = driver_descripor->first_device_handle;
  if (next_dso_desc)
    {
      while (next_dso_desc)
        {
          if ((next_dso_desc->libusb_descriptor.idVendor
              == known_device->vendor_id)
              && (next_dso_desc->libusb_descriptor.idProduct
                  == known_device->product_id))
            {
              if ((last_index + 1) < next_dso_desc->dso_index)
                {
                  break;
                }
              last_index = next_dso_desc->dso_index;
            }
          prev_dso_desc = next_dso_desc;
          next_dso_desc = next_dso_desc->next;
        }
      if (next_dso_desc)
        {
          if (prev_dso_desc)
            {
              prev_dso_desc->next = dso_desc;
            }
          else
            {
              driver_descripor->first_device_handle = dso_desc;
            }
          dso_desc->prev = prev_dso_desc;
          dso_desc->next = next_dso_desc;
          next_dso_desc->prev = dso_desc;
        }
      else
        {
          prev_dso_desc->next = dso_desc;
          dso_desc->prev = prev_dso_desc;
        }
    }
  else
    {
      driver_descripor->first_device_handle = dso_desc;
    }
  dso_desc->dso_name = known_device->name;
  dso_desc->dso_index = last_index + 1;
}

void
delete_dso_descriptor (struct t_dso_device_descriptor * dso_descriptor)
{
  if (dso_descriptor == driver_descripor->first_device_handle)
    {
      if (dso_descriptor->next)
        {
          dso_descriptor->next->prev = NULL;
        }
      driver_descripor->first_device_handle = dso_descriptor->next;
    }
  else
    {
      if (dso_descriptor->prev)
        {
          dso_descriptor->prev->next = dso_descriptor->next;
        }
      if (dso_descriptor->next)
        {
          dso_descriptor->next->prev = dso_descriptor->prev;
        }
    }
  free (dso_descriptor);
}

struct t_dso_device_descriptor *
find_dso_descriptor (libusb_device * dev)
{
  struct t_dso_device_descriptor * result =
      driver_descripor->first_device_handle;
  while (result)
    {
      if (result->dso_device == dev)
        {
          break;
        }
      else
        {
          result = result->next;
        }
    }
  return result;
}

int
dso_bulk_conversation (libusb_device_handle * dso_device_handle,
                       unsigned char ep, unsigned char * data, int length,
                       int try_counts)
{
  int actual_length = 0;
  int try_counts_done = 0;

  while (length && (try_counts_done < try_counts))
    {
      libusb_status_code = libusb_bulk_transfer (dso_device_handle, ep, data,
                                                 length, &actual_length, 0);
      if (actual_length)
        {
          try_counts_done = 0;
          data += actual_length;
          length -= actual_length;
        }
      else
        {
          try_counts_done++;
        }
    }
  return length;
}

