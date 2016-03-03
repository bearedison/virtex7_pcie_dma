/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper-dma-test
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
  *  @brief wupper-dma-test.cpp is an application that runs
  *  a 1024 Byte DMA transfer every second and dumps it to 
  *  the screen.
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "wupper.h"

#define APPLICATION_NAME "wupper-dma-test"
#define BUFSIZE (1024)
#define DMA_ID (0)

void
display_help()
{
  printf("Usage: %s [OPTIONS]\n"
	 "Initializes DMA transfers and the DMA memory on the screen in 1s intervals.\n\n"
	 "Options:\n"
	 "  -d NUMBER      Use card indicated by NUMBER. Default: 0.\n"
	 "  -h             Display help.\n",
	 APPLICATION_NAME);
}

void
dump_buffer(u_long virt_addr)
{
  unsigned char* buf = (unsigned char*)virt_addr;
  int i;
  for(i=0; i<BUFSIZE; i++)
    {
      if(i%32==0) printf("\n0x  ");
      printf("%02x ", *buf++);
    }
  printf("\n");
}

int
main(int argc, char** argv)
{
  u_int ret;
  int device_number = 0;
  u_int handle = 0;
  int opt;

  wupper_dev_t wupper;

  while ((opt = getopt(argc, argv, "hd:")) != -1) {
    switch (opt) {
    case 'd':
      device_number = atoi(optarg);
      break;
    case 'h':
      display_help();
      exit(EXIT_SUCCESS);
    default: /* '?' */
      fprintf(stderr, "Usage: %s COMMAND [OPTIONS]\nTry %s -h for more information.\n",
	      APPLICATION_NAME, APPLICATION_NAME);
      exit(EXIT_FAILURE);
    }
  }

  if(wupper_open(&wupper,  device_number))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not open WUPPER %d\n", device_number);
      return 1;
    }

  wupper_dma_reset(&wupper);
  wupper_dma_soft_reset(&wupper);
  wupper_dma_fifo_flush(&wupper);


  u_long board_id = *((u_long*)(wupper.bar2));
  printf("Board ID: %x\n", board_id);

  cmem_buffer_t buffer;
  if(cmem_alloc(BUFSIZE, &buffer))
    {
      fprintf(stderr, "Could not allocate memory\n");
      exit(EXIT_FAILURE);
    }
  printf("Allocated Memory Segment\n  Phys. Addr: 0x%016llx\n  Virt. Addr: 0x016%llx\n",
	 buffer.phys_addr, buffer.virt_addr);

  printf("\nBuffer before DMA write:\n");
  dump_buffer(buffer.virt_addr);

  int max_tlp = wupper_dma_max_tlp_bytes(&wupper);

  wupper_dma_program_write(DMA_ID, buffer.phys_addr, BUFSIZE, max_tlp, WUPPER_DMA_WRAPAROUND, &wupper);
  wupper_dma_wait(DMA_ID, &wupper);


  printf("Start Ptr:  0x%016llx\n", 	 
	 *((u_long*)(wupper.bar0 + 0x0)));
  printf("End Ptr: 0x%016llx\n", 	 
	 *((u_long*)(wupper.bar0 + 0x8)));

  printf("Enable: 0x%016llx\n", 	 
	 *((u_long*)(wupper.bar0 + 0x400)));


  printf("Read Ptr:  0x%016llx\n", 	 
	 *((u_long*)(wupper.bar0 + 0x18)));
  printf("Write Ptr: 0x%016llx\n", 	 
	 *((u_long*)(wupper.bar0 + 0x200)));

  printf("Status DMA0: 0x%llx\n", 	 
	 *((u_long*)(wupper.bar0 + 0x208)));
  printf("Status DMA1: 0x%llx\n", 	 
	 *((u_long*)(wupper.bar0 + 0x218)));

  wupper_dma_descriptor* dma0 = (wupper_dma_descriptor*)wupper.bar0;
  printf("Start Addr: %016llx\nEnd Addr:  %016llx\nRead Ptr: %016llx\n",
	 dma0[0].start_address, dma0[0].end_address, dma0[0].read_ptr);
  
  //wupper_dma_fifo_flush(&wupper);


  printf("\nBuffer after DMA write:\n");
  dump_buffer(buffer.virt_addr);

  int i;
  for(i=0;;i++)
    {
      printf("\n--------------------\n  %d:\n", i);
      //wupper_dma_program_write(DMA_ID, buffer.phys_addr, BUFSIZE, max_tlp, 0, &wupper);
      wupper_dma_advance_read_ptr(DMA_ID, buffer.phys_addr, BUFSIZE, 512, &wupper);
      wupper_dma_wait(DMA_ID, &wupper);

      printf("Read Ptr:  0x%016llx\n", 	 
	     *((u_long*)(wupper.bar0 + 0x18)));
      printf("Write Ptr: 0x%016llx\n", 	 
	     *((u_long*)(wupper.bar0 + 0x200)));    
      printf("Status DMA0: 0x%llx\n", 	 
	     *((u_long*)(wupper.bar0 + 0x208)));

      dump_buffer(buffer.virt_addr);
      sleep(1);
    }


  cmem_free(&buffer);

  if(wupper_close(&wupper))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not close WUPPER %d\n", device_number);
      return 1;
    }
  
  return(0);
}
