/*
 ============================================================================
 Name        : hantekdso.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 ============================================================================
 */

//


#include <ntstatus.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <winerror.h>

#define NONAMELESSUNION

#include <wine/debug.h>
#include <wine/windows/windows.h>
#include <wine/windows/winnt.h>
#include <wine/windows/ntsecapi.h>
#include <wine/windows/winternl.h>
#include <wine/windows/winreg.h>
#include <wine/windows/ddk/wdm.h>

#include "hantekdso.h"
#include "libusbdso.h"
#include "errors.h"

WINE_DEFAULT_DEBUG_CHANNEL(hantekdso);
WINE_DECLARE_DEBUG_CHANNEL(hantekdsodev);

#define REG_VAL_VENDOR_IDX	0
#define REG_VAL_PRODUCT_IDX	1
#define REG_VAL_COUNT		2
static const char const * const reg_values[2] =
  { "VendorID", "ProductID" };

static const char * const dso_device_path = "\\Device\\";
static const char * const dso_device_dos_path = "\\DosDevices\\";

DRIVER_OBJECT * dso_driver = NULL;
struct t_dso_driver_descriptor dso_driver_desc;

NTSTATUS
dso_init_device_path (void * dso_device_handle, WCHAR * device_path,
                      const char * prefix);
NTSTATUS
dso_ioctl_replay (void * dso_device_handle, IRP * irp);
NTSTATUS
dso_ioctl_request (void * dso_device_handle, IRP * irp);
NTSTATUS
dso_ioctl_control (void * dso_device_handle, IRP * irp);


// This call-back will have been invoked for each
// known device present or attached to PC
int
dso_connect_cb (void * dso_device_handle)
{
  WCHAR device_pathW[DSO_MAX_PATH_SIZE];
  WCHAR device_dos_pathW[DSO_MAX_PATH_SIZE];
  UNICODE_STRING pathW, dos_pathW;
  DEVICE_OBJECT * device;
  NTSTATUS status;
  size_t count;

  status = dso_init_device_path (dso_device_handle, device_pathW,
                                 dso_device_path);
  if (status)
    {
      WINE_ERR ("Init device path failed. Resulting path name exceeds max path size: %d. Error code: 0x%x\n", DSO_MAX_PATH_SIZE - 1, status);
    }
  else
    {
      status = dso_init_device_path (dso_device_handle, device_dos_pathW,
                                     dso_device_dos_path);
      if (status)
        {
          WINE_ERR ("Init device path failed. Resulting path name exceeds max path size: %d. Error code: 0x%x\n", DSO_MAX_PATH_SIZE - 1, status);
        }
      else
        {
          RtlInitUnicodeString (&dos_pathW, device_dos_pathW);
          RtlInitUnicodeString (&pathW, device_pathW);
          status = IoCreateDevice (dso_driver, sizeof(void *), &pathW, 0, 0, FALSE, &device);
          if (status)
            {
              WINE_ERR ("create device failed: error 0x%x\n", status);
            }
          else
            {
              device->DeviceExtension = dso_device_handle;
              libusbdso_set_win_device_handle (dso_device_handle, device);
              status = IoCreateSymbolicLink (&dos_pathW, &pathW);
              if (status)
                {
                  WINE_ERR ("create symbolic link to device failed. Error code: 0x%x\n", status);
                }
              else
                {
                  WINE_TRACE("created Hantek DSO device: '%s-%d' (handle: 0x%lx)\n",
                             libusbdso_get_device_name(dso_device_handle),
                             libusbdso_get_device_index(dso_device_handle),
                             (long) libusbdso_get_win_device_handle(dso_device_handle));
                }
            }
        }
    }
  return status;
}

// This call-back will have been invoked for each
// known device disconnected from PC
int
dso_disconnect_cb (void * dso_device_handle)
{
  WCHAR device_dos_pathW[DSO_MAX_PATH_SIZE];
  UNICODE_STRING pathW, dos_pathW;
  DEVICE_OBJECT * device;
  NTSTATUS status;

  device = libusbdso_get_win_device_handle (dso_device_handle);
  if (device)
    {
      status = dso_init_device_path (dso_device_handle, device_dos_pathW,
                                     dso_device_dos_path);
      if (status)
        {
          WINE_ERR ("Init device path failed. Resulting path name exceeds max path size: %d. Error code: 0x%x\n", DSO_MAX_PATH_SIZE - 1, status);
        }
      else
        {
          RtlInitUnicodeString (&dos_pathW, device_dos_pathW);
          status = IoDeleteSymbolicLink (&dos_pathW);
          if (status)
            {
              WINE_ERR ("delete symbolic link to device failed. Error code: 0x%x\n", status);
            }

          WINE_TRACE("delete Hantek DSO device: '%s-%d' (handle: 0x%lx)\n",
                      libusbdso_get_device_name(dso_device_handle),
                      libusbdso_get_device_index(dso_device_handle),
                      (long) libusbdso_get_win_device_handle(dso_device_handle));
          IoDeleteDevice (device);
        }
    }
  return status;
}

static NTSTATUS WINAPI
dso_create (DEVICE_OBJECT *device, IRP *irp)
{
  int libusbSatus;
  PVOID dso_device_handle = device->DeviceExtension;

  if (dso_device_handle)
    {
      WINE_TRACE_(hantekdsodev)("Hantek DSO open device: '%s-%d' (handle: 0x%lx)\n",
                                libusbdso_get_device_name(dso_device_handle),
                                libusbdso_get_device_index(dso_device_handle),
                                (long) libusbdso_get_win_device_handle(dso_device_handle));
      libusbSatus = libusbdso_open_device (dso_device_handle);
      irp->IoStatus.u.Status = libusbSatus;
      IoCompleteRequest (irp, IO_NO_INCREMENT);
      if (libusbSatus)
        {
          WINE_ERR_(hantekdsodev)("Hantek DSO (handle: 0x%lx) open device failed: %s, libusb last error: %s\n",
                                  (long) libusbdso_get_win_device_handle(dso_device_handle),
                                  dso_error_name(libusbSatus),
                                  libusbdso_last_error());
        }
      else
        {
          WINE_TRACE_(hantekdsodev)("Hantek DSO open device done (handle 0x%lx)\n",
                                    (long) libusbdso_get_win_device_handle(dso_device_handle));
        }
      return STATUS_SUCCESS;
    }
  else
    {
      WINE_ERR_(hantekdsodev)("Hantek DSO open device failed. No DSO device handler provided\n");
      return STATUS_DRIVER_INTERNAL_ERROR;
    }
}

static NTSTATUS WINAPI
dso_read (DEVICE_OBJECT *device, IRP *irp)
{
  PVOID dso_device_handle = device->DeviceExtension;

  if (dso_device_handle)
    {
      WINE_TRACE_(hantekdsodev)("Hantek DSO read device: '%s-%d' (handle: 0x%lx)\n",
                                libusbdso_get_device_name(dso_device_handle),
                                libusbdso_get_device_index(dso_device_handle),
                                (long) libusbdso_get_win_device_handle(dso_device_handle));
      irp->IoStatus.u.Status = STATUS_SUCCESS;
      IoCompleteRequest (irp, IO_NO_INCREMENT);
      return STATUS_SUCCESS;
    }
  else
    {
      WINE_ERR_(hantekdsodev)("Hantek DSO read failed. No DSO device handler provided\n");
      return STATUS_DRIVER_INTERNAL_ERROR;
    }
}

static NTSTATUS WINAPI
dso_write (DEVICE_OBJECT *device, IRP *irp)
{
  PVOID dso_device_handle = device->DeviceExtension;

  if (dso_device_handle)
    {
      WINE_TRACE_(hantekdsodev)("Hantek DSO write device: '%s-%d' (handle: 0x%lx)\n",
                                libusbdso_get_device_name(dso_device_handle),
                                libusbdso_get_device_index(dso_device_handle),
                                (long) libusbdso_get_win_device_handle(dso_device_handle));
      irp->IoStatus.u.Status = STATUS_SUCCESS;
      IoCompleteRequest (irp, IO_NO_INCREMENT);
      return STATUS_SUCCESS;
    }
  else
    {
      WINE_ERR_(hantekdsodev)("Hantek DSO write failed. No DSO device handler provided\n");
      return STATUS_DRIVER_INTERNAL_ERROR;
    }
}

static NTSTATUS WINAPI
dso_ioctl (DEVICE_OBJECT *device, IRP *irp)
{
  int libusbSatus = STATUS_NOT_SUPPORTED;
  PVOID dso_device_handle = device->DeviceExtension;

  if (dso_device_handle)
    {
      IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation(irp);
      switch (irpsp->Parameters.DeviceIoControl.IoControlCode)
        {
        case DSO_IOCTL_REPLAY:	// Get replay     METHOD_OUT_DIRECT
          WINE_TRACE_(hantekdsodev)("Hantek DSO ioctl (DSO_IOCTL_REPLAY) device: '%s-%d' (handle: 0x%lx)\n",
                                    libusbdso_get_device_name(dso_device_handle),
                                    libusbdso_get_device_index(dso_device_handle),
                                    (long) libusbdso_get_win_device_handle(dso_device_handle));
          libusbSatus = dso_ioctl_replay (dso_device_handle, irp);
          break;
        case DSO_IOCTL_REQUEST:	// Send command   METHOD_IN_DIRECT
          WINE_TRACE_(hantekdsodev)("Hantek DSO ioctl (DSO_IOCTL_REQUEST) device: '%s-%d' (handle: 0x%lx)\n",
                                    libusbdso_get_device_name(dso_device_handle),
                                    libusbdso_get_device_index(dso_device_handle),
                                    (long) libusbdso_get_win_device_handle(dso_device_handle));
          libusbSatus = dso_ioctl_request (dso_device_handle, irp);
          break;
        case DSO_IOCTL_CONTROL:	// Control        METHOD_IN_DIRECT
          WINE_TRACE_(hantekdsodev)("Hantek DSO ioctl (DSO_IOCTL_CONTROL) device: '%s-%d' (handle: 0x%lx)\n",
                                    libusbdso_get_device_name(dso_device_handle),
                                    libusbdso_get_device_index(dso_device_handle),
                                    (long) libusbdso_get_win_device_handle(dso_device_handle));
          libusbSatus = dso_ioctl_control (dso_device_handle, irp);
          break;
        default:
          WINE_FIXME( "Hantek DSO (handle: 0x%lx) ioctl %x not supported\n",
                      (long) libusbdso_get_win_device_handle(dso_device_handle),
                      irpsp->Parameters.DeviceIoControl.IoControlCode );
          irp->IoStatus.u.Status = STATUS_NOT_SUPPORTED;
          break;
        }
      irp->IoStatus.u.Status = libusbSatus;
      IoCompleteRequest (irp, IO_NO_INCREMENT);
      if (libusbSatus)
        {
          WINE_ERR_(hantekdsodev)("Hantek DSO (handle: 0x%lx) ioctl device failed: %s, libusb last error: %s (%d)\n",
                                  (long) libusbdso_get_win_device_handle(dso_device_handle),
                                  dso_error_name(libusbSatus),
                                  libusbdso_last_error(),
                                  libusbdso_last_error_code());
        }
      else
        {
          WINE_TRACE_(hantekdsodev)("Hantek DSO ioctl device done (handle 0x%lx)\n",
                                    (long) libusbdso_get_win_device_handle(dso_device_handle));
        }
      return STATUS_SUCCESS;
    }
  else
    {
      WINE_ERR_(hantekdsodev)("Hantek DSO ioctl failed. No DSO device handler provided\n");
      return STATUS_DRIVER_INTERNAL_ERROR;
    }
}

static NTSTATUS WINAPI
dso_close (DEVICE_OBJECT *device, IRP *irp)
{
  int libusbSatus;
  PVOID dso_device_handle = device->DeviceExtension;

  if (dso_device_handle)
    {
      WINE_TRACE_(hantekdsodev)("Hantek DSO close device: '%s-%d'\n",
                                libusbdso_get_device_name(dso_device_handle),
                                libusbdso_get_device_index(dso_device_handle));
      libusbSatus = libusbdso_close_device (dso_device_handle);
      irp->IoStatus.u.Status = libusbSatus;
      IoCompleteRequest (irp, IO_NO_INCREMENT);
      if (libusbSatus)
        {
          WINE_ERR_(hantekdsodev)("Hantek DSO (handle: 0x%lx) close device failed: %s, libusb last error: %s\n",
                                  (long) libusbdso_get_win_device_handle(dso_device_handle),
                                  dso_error_name(libusbSatus),
                                  libusbdso_last_error());
        }
      else
        {
          WINE_TRACE_(hantekdsodev)("Hantek DSO close device done (handle 0x%lx)\n",
                                    (long) libusbdso_get_win_device_handle(dso_device_handle));
        }
      return STATUS_SUCCESS;
    }
  else
    {
      WINE_ERR_(hantekdsodev)("Hantek DSO close failed. No DSO device handler provided\n");
      return STATUS_DRIVER_INTERNAL_ERROR;
    }
}

static NTSTATUS WINAPI
dso_shutdown (DEVICE_OBJECT *device, IRP *irp)
{
  WINE_TRACE("Shutdown Hantek DSO Driver\n");
  libusbdso_stop();
  return STATUS_SUCCESS;
}

NTSTATUS
dso_ioctl_replay (void * dso_device_handle, IRP * irp)
{  // 0x22204E  =>  METHOD_OUT_DIRECT
  NTSTATUS result;

  IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation(irp);
  ULONG out_size = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
  PBYTE out_data = (PBYTE) irp->MdlAddress->StartVa + irp->MdlAddress->ByteOffset;

  result = libusbdso_read_device (dso_device_handle, out_data, out_size);

  if (out_size)
    {
      irp->IoStatus.Information = out_size;
    }
  return result;
}

NTSTATUS
dso_ioctl_request (void * dso_device_handle, IRP * irp)
{  // 0x222051  =>  METHOD_IN_DIRECT

  IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation(irp);
  ULONG out_size = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
  PBYTE out_data = (PBYTE) irp->MdlAddress->StartVa + irp->MdlAddress->ByteOffset;

  return libusbdso_write_device (dso_device_handle, out_data, out_size);
}

NTSTATUS
dso_ioctl_control (void * dso_device_handle, IRP * irp)
{  // 0x222059  =>  METHOD_IN_DIRECT
  NTSTATUS result;

  IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation(irp);
  ULONG out_size = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
  PBYTE out_data = (PBYTE) irp->MdlAddress->StartVa + irp->MdlAddress->ByteOffset;
  struct t_dso_ioctl_data *ioctl_data = (struct t_dso_ioctl_data *) irp->AssociatedIrp.SystemBuffer;

  if (ioctl_data->header.direction)
    {
      result = libusbdso_control_in_device (dso_device_handle,
                                            ioctl_data->command,
                                            ioctl_data->value, 0, out_data,
                                            out_size);
    }
  else
    {
      result = libusbdso_control_out_device (dso_device_handle,
                                             ioctl_data->command,
                                             ioctl_data->value, 0, out_data,
                                             out_size);
    }
  if (out_size)
    {
      irp->IoStatus.Information = out_size;
    }
  return result;
}

int
fill_wstring (WCHAR * wstring, const char * string)
{
  char ch;
  int i;

  for (i = 0; string[i]; i++)
    {
      wstring[i] = (WCHAR) string[i];
    }
  wstring[i] = 0;
  return i;
}

NTSTATUS
dso_init_device_path (void * dso_device_handle, WCHAR * device_path,
                      const char * prefix)
{
  const char * dev_name;
  uint8_t dev_index;
  int index;

  if (dso_device_handle)
    {
      dev_name = libusbdso_get_device_name (dso_device_handle);
      dev_index = libusbdso_get_device_index (dso_device_handle);

      if (strlen (dev_name)
          < (DSO_MAX_PATH_SIZE - strlen (dso_device_dos_path) - 3))
        {
          index = fill_wstring (device_path, prefix);
          index += fill_wstring (device_path + index, dev_name);
          device_path[index++] = '-';
          if (dev_index > 100)
            {
              device_path[index++] = dev_index / 100;
              dev_index = dev_index % 100;
            }
          if (dev_index > 10)
            {
              device_path[index++] = dev_index / 10;
              dev_index = dev_index % 10;
            }
          device_path[index++] = dev_index + 0x30;
          device_path[index] = 0;
        }
      else
        {
          return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
      return STATUS_SUCCESS;
    }
  else
    {
      return STATUS_DRIVER_INTERNAL_ERROR;
    }
}

NTSTATUS
dso_init_device_db (void)
{
  HKEY hKey;
  NTSTATUS status;
  DWORD index, cchName, dwType, val_index;
  CHAR keyName[DSO_REG_KEY_LENGTH], *end;
  DWORD valueData;
  struct t_dso_known_device * known_device;

  hKey = NULL;
  status = RegOpenKeyExA (HKEY_LOCAL_MACHINE, "SOFTWARE\\Hantek\\Devices", 0,
                          KEY_READ, &hKey);
  if (status)
    {
      WINE_ERR ("open registry key 'SOFTWARE\\Hantek\\Devices' failed. Error code: %d.\n", status);
    }
  else
    {
      cchName = DSO_REG_KEY_LENGTH;
      for (index = 0;
          (status = RegEnumKeyExA (hKey, index, keyName, &cchName, 0, NULL,
                                   NULL, NULL)) == 0; index++)
        {
          known_device = malloc (sizeof(struct t_dso_known_device));
          if (known_device)
            {
              known_device->name = malloc (cchName + 1);
              if (known_device->name)
                {
                  strncpy (known_device->name, keyName, cchName + 1);
                  for (val_index = 0; val_index < REG_VAL_COUNT; val_index++)
                    {
                      cchName = DSO_REG_KEY_LENGTH;
                      status = RegGetValueA (hKey, known_device->name,
                                             reg_values[val_index],
                                             RRF_RT_DWORD, &dwType, &valueData,
                                             &cchName);
                      if (status)
                        {
                          WINE_ERR ("read registry value '%s' in key 'SOFTWARE\\Hantek\\Devices\\%s' failed. Error code: %d.\n",
                              reg_values[val_index],
                              known_device->name,
                              status);
                          free (known_device->name);
                          free (known_device);
                          known_device = NULL;
                        }
                      else
                        {
                          switch (val_index)
                            {
                            case REG_VAL_VENDOR_IDX:
                              known_device->vendor_id = valueData;
                              break;
                            case REG_VAL_PRODUCT_IDX:
                              known_device->product_id = valueData;
                              break;
                            default:
                              ;
                            }
                        }
                    }
                  if (known_device)
                    {
                      add_known_device (known_device);
                      WINE_TRACE ("Hantek device '%04X:%04X - %s' added as known device\n",
                          known_device->vendor_id,
                          known_device->product_id,
                          known_device->name);
                    }
                }
              else
                {
                  free (known_device);
                  return STATUS_MEMORY_NOT_ALLOCATED;
                }
            }
          else
            {
              status = STATUS_MEMORY_NOT_ALLOCATED;
              break;
            }
          cchName = DSO_REG_KEY_LENGTH;
        }
      if (status && (status != ERROR_NO_MORE_ITEMS))
        {
          WINE_ERR ("enumerating keys in registry key 'SOFTWARE\\Hantek\\Devices' failed. Error code: %d.\n", status);
        }
      else
        {
          status = STATUS_SUCCESS;
        }
      RegCloseKey(hKey);
    }
  return status;
}

NTSTATUS WINAPI
DriverEntry (DRIVER_OBJECT * driver, UNICODE_STRING * path)
{
  NTSTATUS status;

  WINE_TRACE ("Starting Hantek DSO Wine Driver\n");

  dso_driver_desc.first_device_handle = NULL;
  dso_driver_desc.connect_cb = dso_connect_cb;
  dso_driver_desc.disconnect_cb = dso_disconnect_cb;

  dso_driver = driver;
  dso_driver->MajorFunction[IRP_MJ_CREATE] = dso_create;
  dso_driver->MajorFunction[IRP_MJ_READ] = dso_read;
  dso_driver->MajorFunction[IRP_MJ_WRITE] = dso_write;
  dso_driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dso_ioctl;
  dso_driver->MajorFunction[IRP_MJ_CLOSE] = dso_close;
  dso_driver->MajorFunction[IRP_MJ_SHUTDOWN] = dso_shutdown; // !!!! wine does not use this IRP

  status = dso_init_device_db ();
  if (status)
    {
      WINE_ERR ("Hantek DSO Device initialization failed (stage: init devices db). Error: %d\n", status);
      return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

  status = libusbdso_start (&dso_driver_desc);
  if (status)
    {
      WINE_ERR ("Hantek DSO Device initialization failed (stage: libusb). Error: %s\n", dso_error_name (status));
      WINE_ERR ("libusb last error: %s\n", libusbdso_last_error ());
      return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

  WINE_TRACE ("Hantek DSO Wine Driver was started\n");
  return status;
}

