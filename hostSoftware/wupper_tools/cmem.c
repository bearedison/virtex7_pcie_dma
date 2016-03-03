/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class cmem
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
  *  @brief cmem.c allocates a chunk of memory by means of the cmem driver. 
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
#include "wupper.h"
#include "../driver/include/cmem_common.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static int 
cmem_open()
{
  return open("/dev/cmem", O_RDWR);
}

static void
cmem_close(int fd)
{
  close(fd);
}

static u_long
cmem_get_phys_addr(int fd, u_int handle)
{
  cmem_t descriptor;
  descriptor.handle = handle;
  if(CMEM_SUCCESS != ioctl(fd, CMEM_GETPARAMS, &descriptor))
    {
      return 0;
    }

  return descriptor.paddr;
}

static u_long
cmem_get_virt_addr(int fd, u_int handle)
{
  cmem_t descriptor;
  descriptor.handle = handle;
  if(CMEM_SUCCESS != ioctl(fd, CMEM_GETPARAMS, &descriptor))
    {
      return 0;
    }

  return descriptor.uaddr;
}




int cmem_alloc(u_long size, cmem_buffer_t* buffer)
{
  cmem_t descriptor;
  int result = 0;
  u_int handle;
  int fd = cmem_open();

  sprintf(descriptor.name, "wuppertools");
  descriptor.size = size;
  descriptor.order = 0;
  descriptor.type = TYPE_GFPBPA;

  if(CMEM_SUCCESS != ioctl(fd, CMEM_GET, &descriptor))
    {
      result = 1;
      goto failure;
    }

  descriptor.uaddr = (u_long)mmap(0, size, PROT_READ|PROT_WRITE,
				  MAP_SHARED, fd, 
				  (long)descriptor.paddr);
  if(CMEM_SUCCESS != ioctl(fd, CMEM_SETUADDR, &descriptor))
    {
      result = 2;
      goto failure;
    }
 
  handle = descriptor.handle;
  buffer->phys_addr = cmem_get_phys_addr(fd, handle);
  buffer->virt_addr = cmem_get_virt_addr(fd, handle);
  buffer->size = size;
  buffer->handle = handle;
  
  if(buffer->phys_addr == 0 || buffer->virt_addr == 0)
    result = 3;

 failure:
  cmem_close(fd);
  return result;
}

int cmem_free(cmem_buffer_t* buffer)
{
  cmem_t descriptor;
  int result = 0;
  int fd = cmem_open();
  
  if(munmap((void*)buffer->virt_addr, buffer->size))
    {
      result = 1;
      goto failure;
    }

  if(CMEM_SUCCESS != ioctl(fd, CMEM_FREE, buffer->handle))
    {
      result = 2;
    }

 failure:
  cmem_close(fd);
  return result;
}
