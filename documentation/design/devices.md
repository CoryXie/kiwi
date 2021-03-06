Kiwi Device Architecture
========================

The purpose of this document is to describe the Kiwi device architecture. There
are three main parts to the device system:

 * Device Tree
 * Device Classes
 * Drivers

These parts will be explained below. Note that this document is currently not
complete. It was started mainly because I wanted to document how custom device
events/requests should be numbered.

Device Tree
-----------

The device tree is a tree of all devices in the system, including the buses
they are connected to. A device can be placed in the tree in multiple places
using aliases to the main entry. An example device tree is shown below.

    bus
      pci
        0
          00.0
          01.0
          01.1
            ata0
              0
                part0
                part1
            ata1
          01.3
          02.0
          03.0
    console
      0
        master
        slave
      manager
    disk
      0                 -> /bus/pci/0/01.1/ata0/0
        part0
        part1
    display
      0
    input
      0

In this example, an ATA controller with one device attached is on the PCI bus.
The device class manager (explained below) for the disk device has placed an
alias for the device under its class directory, and found partitions on the
device.

Devices in the tree can also have various attributes that can be used to
expose information about them to userspace and other parts of the kernel.

Devices in the tree can be referred to by a path, seperated by '/' characters,
that gives their location in the tree, for example `/bus/pci/0/01.1/ata0/0`.

Device Classes
--------------

There are many different types of device - audio devices, display devices,
input devices, disk devices, and many more. A device class allows all devices
of a certain class to be accessed via a uniform interface - however the API is
flexible enough to allow devices to implement their own specific APIs as well
as the class API (this is described below).

Each device class has a 'device class manager' module. These modules provide
functions to add devices of the class they manage, and convert requests on the
devices they manage to calls into the driver, so that each driver does not have
to handle requests directly.

Drivers
-------

Drivers are what manage actual devices. A driver is usually built on top of a
device class, however it does not have to be if it is for a very specific
device that does not fit into a device class.

A driver just needs to implement the functions required by its device class,
and publish all devices it manages in the device tree using the functions for
the class. However, it may be necessary for a driver to provide its own request
types, or events to wait for. A driver can do this by providing the
request/wait/unwait functions in its device operation structures. Custom
requests should be numbered from `DEVICE_CUSTOM_REQUEST_START`, defined in
`io/device.h`, and custom event types should be numbered from
`DEVICE_CUSTOM_EVENT_START`, also defined in `io/device.h`.
