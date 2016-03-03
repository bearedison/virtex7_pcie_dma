/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper
  *  
  *  
  *  @author      Andrea Borga    (andrea.borga@nikhef.nl)<br>
  *               Frans Schreuder (frans.schreuder@nikhef.nl)<br>
  * 			  Markus Joos<br>
  * 			  Jos Vermeulen<br>
  * 			  Oussama el Kharraz Alami<br>
  *  
  *  
  *  @date        08/09/2015    created
  *  
  *  @version     1.0
  *  
  *  @brief wupper.c provides essential functions. These functions
  *  will handle the connection with the device. 
  *  Wupper.c contains three functions:
  *  - map_memory_bar:
  *  Maps the BAR addresses from register.c. 
  *  
  *  - wupper_open:
  *  Opens the device and sets the BAR addresses.
  * 
  *  - wupper_close:
  *  This function returns the status of the device. 
  *  
  *  
  *  @detail
  *  
  *  ----------------------------------------------------------------------------
  *  @TODO
  *   
  *  
  *  ------------------------------------------------------------------------------
  *  Wupper
  *  
  *  \copyright GNU LGPL License
  *  Copyright (c) Nikhef, Amsterdam, All rights reserved. <br>
  *  This library is free software; you can redistribute it and/or
  *  modify it under the terms of the GNU Lesser General Public
  *  License as published by the Free Software Foundation; either
  *  version 3.0 of the License, or (at your option) any later version.
  *  This library is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  *  Lesser General Public License for more details.<br>
  *  You should have received a copy of the GNU Lesser General Public
  *  License along with this library.
  */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "wupper.h"


int
wupper_close(wupper_dev_t* wupper)
{
  return close(wupper->fd);
}

static u_long
map_memory_bar(u_long pci_addr, size_t size, int fd)
{
    void *vaddr;
    u_long offset;

    offset = pci_addr & 0xfff;  /* mmap seems to need a 4K alignment */
    pci_addr &= 0xfffffffffffff000ll;

    vaddr = mmap(0, size, (PROT_READ|PROT_WRITE), MAP_SHARED, fd, pci_addr);
    if (vaddr == MAP_FAILED)
    {
        return 0;
    }

    return (u_long)vaddr + offset;
}


int
wupper_open(wupper_dev_t* wupper, int device_number)
{
  card_params_t card_data;

  int fd = open("/dev/wupper", O_RDWR);
  if(fd < 0) return -1;

  card_data.slot = device_number;
  int iores = ioctl(fd, SETCARD, &card_data);
  if(iores < 0) return -2;


  wupper->bar0 = map_memory_bar(card_data.baseAddressBAR0, 4096, fd);
  wupper->bar1 = map_memory_bar(card_data.baseAddressBAR1, 4096, fd);
  wupper->bar2 = map_memory_bar(card_data.baseAddressBAR2, 65536, fd);

  if(wupper->bar0==0 || wupper->bar1==0 || wupper->bar2==0)
    return -3;

  wupper->fd = fd;
  wupper->dma_descriptors = (wupper_dma_descriptor_t*) wupper->bar0;
  wupper->dma_status = (wupper_dma_status_t*) (wupper->bar0 + 0x200);
  wupper->dma_enable = (u_int*) (wupper->bar0 + 0x400);

  return 0;
}

