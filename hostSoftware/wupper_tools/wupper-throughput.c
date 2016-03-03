/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper-througput
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
  *  @brief wupper-througput.c is an application that measures the
  *  throughput of the DMA engine. The unity is in GigaBytes per 
  *  second.
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
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include "wupper.h"


#define DMA_ID (0)
#define BUFSIZE (1024)
#define APPLICATION_NAME "wupper-throughput"

void
display_help()
{
  printf("Usage: %s [OPTIONS]\n"
	 "Reads raw data from a WUPPER and writes them into a file.\n\n"
	 "Options:\n"
	 "  -d NUMBER      Use card indicated by NUMBER. Default: 0.\n"
	 "  -b NUM         Use a buffer of size NUM blocks. Default: 100.\n"
	 "  -w             Use circular buffer wraparound mechanism.\n"
	 "  -h             Display help.\n",
	 APPLICATION_NAME);
}

double
now()
{
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_sec + 1e-9*tp.tv_nsec;
}

int
main(int argc, char** argv)
{
  int device_number = 0;
  int nblocks = 100;
  int wraparound = 0;
  int opt;

  wupper_dev_t wupper;

  while ((opt = getopt(argc, argv, "hd:b:w")) != -1) {
    switch (opt) {
    case 'd':
      device_number = atoi(optarg);
      break;
    case 'b':
      nblocks = atoi(optarg);
      break;
    case 'w':
      wraparound = 1;
      break;
    case 'h':
      display_help();
      exit(EXIT_SUCCESS);
    default: /* '?' */
      fprintf(stderr, "Usage: %s [OPTIONS]\nTry %s -h for more information.\n",
	      APPLICATION_NAME, APPLICATION_NAME);
      exit(EXIT_FAILURE);
    }
  }


  if(wupper_open(&wupper,  device_number))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not open WUPPER %d\n", device_number);
      return 1;
    }

  wupper_dma_stop(DMA_ID, &wupper);
  *(wupper.dma_enable) = 0;
  wupper_dma_reset(&wupper);
  wupper_dma_soft_reset(&wupper);
  wupper_dma_fifo_flush(&wupper);

  wupper_cfg_set_option(&wupper, "gbt_emu_ena", 1);

  cmem_buffer_t buffer;
  cmem_alloc(BUFSIZE*nblocks, &buffer);


  int max_tlp = wupper_dma_max_tlp_bytes(&wupper);

  double timedelta = 2;
  double t0 = now();
  unsigned long long blocks_read = 0;
  
  if(wraparound)
    {
      fprintf(stderr, "TODO: wraparound support not fully implemented yet.\n");
      wupper_dma_program_write(DMA_ID, buffer.phys_addr, BUFSIZE*nblocks, max_tlp, WUPPER_DMA_WRAPAROUND, &wupper);
    }

  while(1)
    {
      if(wraparound)
	{
	  // TODO: Wait for data interrupt
	  wupper_dma_advance_read_ptr(DMA_ID, buffer.phys_addr, buffer.size,
				    BUFSIZE*nblocks, &wupper);
	  blocks_read += nblocks;
	}
      else
	{
	  wupper_dma_program_write(DMA_ID, buffer.phys_addr, BUFSIZE*nblocks, max_tlp, 0, &wupper);
	  wupper_dma_wait(DMA_ID, &wupper);
	  blocks_read += nblocks;
	}

      double t1 = now();
      if(t1-t0 > timedelta)
	{
	  printf("Blocks read:  %lld\n", blocks_read);
	  printf("Blocks rate:  %f blocks/s\n", blocks_read/(t1-t0));
	  printf("DMA Read:     %f GiB/s\n", blocks_read/(1024.*1024.)/(t1-t0));
	  printf("\n");
	  blocks_read = 0;
	  t0 = t1;
	}
    }


  cmem_free(&buffer);

  if(wupper_close(&wupper))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not close WUPPER %d\n", device_number);
      return 1;
    }
}

