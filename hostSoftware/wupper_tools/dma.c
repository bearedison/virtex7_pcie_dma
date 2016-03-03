/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class dma
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
  *  @brief dma.c is a part of the wupper_tools. It is responisible for 
  *  DMA transfers. It creates descriptors and start/stop tranfers. 
  * 
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
#include "wupper.h"

#include <assert.h>
#include <sys/ioctl.h>

#include "../driver/include/wupper_common.h"


int
wupper_dma_max_tlp_bytes(wupper_dev_t* wupper)
{
  int tlp_bits;

  if(ioctl(wupper->fd, GET_TLP, &tlp_bits) < 0)
    return 0;

  return 128 << tlp_bits;
}

void
wupper_dma_program_write(u_int dma_id, u_long dst, size_t size, u_int tlp, u_int flags, wupper_dev_t* wupper)
{
  wupper_dma_stop(dma_id, wupper);

  wupper->dma_descriptors[dma_id].start_address = dst;
  wupper->dma_descriptors[dma_id].end_address = dst + size;
  wupper->dma_descriptors[dma_id].tlp = tlp/4;
  wupper->dma_descriptors[dma_id].read = 0;
  wupper->dma_descriptors[dma_id].wrap_around = (flags & WUPPER_DMA_WRAPAROUND) ? 1 : 0;
  wupper->dma_descriptors[dma_id].read_ptr = dst+size;

  assert(wupper->dma_descriptors[dma_id].start_address != 0);
  assert(wupper->dma_descriptors[dma_id].end_address != 0);
  
  *(wupper->dma_enable) |= 1<<dma_id;  
}

void
wupper_dma_program_read(u_int dma_id, u_long dst, size_t size, u_int tlp, u_int flags, wupper_dev_t* wupper)
{
  wupper_dma_stop(dma_id, wupper);

  wupper->dma_descriptors[dma_id].start_address = dst;
  wupper->dma_descriptors[dma_id].end_address = dst + size;
  wupper->dma_descriptors[dma_id].tlp = tlp/4;
  wupper->dma_descriptors[dma_id].read = 1;
  wupper->dma_descriptors[dma_id].wrap_around = (flags & WUPPER_DMA_WRAPAROUND) ? 1 : 0;
  wupper->dma_descriptors[dma_id].read_ptr = dst;  
  *(wupper->dma_enable) |= 1<<dma_id;  
}

void
wupper_dma_advance_read_ptr(u_int dma_id, u_long dst, size_t size, size_t bytes, wupper_dev_t* wupper)
{
  u_long* read_ptr = (u_long*)(wupper->bar0 + 0x20*dma_id + 0x18);
  *read_ptr += bytes;
  if(*read_ptr >= dst+size)
    *read_ptr -= size;
}

void
wupper_dma_stop(u_int dma_id, wupper_dev_t* wupper)
{
  *(wupper->dma_enable) &= ~(1<<dma_id);
}



void
wupper_dma_wait(u_int dma_id, wupper_dev_t* wupper)
{
  while( (*(wupper->dma_enable)) & (1<<dma_id) )
    ;
}


void
wupper_dma_fifo_flush(wupper_dev_t* wupper)
{
  *((volatile u_long*) (wupper->bar0 + 0x410)) = 1;
}

void
wupper_dma_reset(wupper_dev_t* wupper)
{
  *((volatile u_long*) (wupper->bar0 + 0x420)) = 1;
}

void
wupper_dma_soft_reset(wupper_dev_t* wupper)
{
  *((volatile u_long*) (wupper->bar0 + 0x430)) = 1;
}
