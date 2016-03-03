/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper-reset
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
  *  @brief wupper-reset.c reset different parts of the card.
  *  Available command are:
  *  -flush         ->Flushes the main output FIFO from wupper device 0
  *  -flush -d 1    ->Flushes the main output FIFO from wupper device 1
  *  -all           ->Does everything for device 0.
  *  -reset         ->Displays an error.
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
#include <stdlib.h>
#include <unistd.h>

#include "wupper.h"

#define APPLICATION_NAME "wupper-reset"

enum cmd_mode {
  UNKNOWN,
  FLUSH,
  RESET,
  SOFT_RESET,
  ALL,
};


void
display_help()
{
  printf("Usage: %s [OPTIONS]\n"
	 "Tool to reset different aspects from the card.\n"
	 "\nCommands:\n"
	 "Options:\n"
	 "  flush         Flushes (resets) the main output FIFO toward Wupper  .\n"
	 "  reset         Resets the whole Wupper_core .\n"
	 "  soft-reset    Global application soft reset .\n"
   "  all           Do everything .\n"
	 "Options:\n"
	 "  -d NUMBER      Use card indicated by NUMBER. Default: 0.\n"
	 "  -h             Display help.\n",
	 APPLICATION_NAME);
}


int
main(int argc, char** argv)
{
  int opt;
  int device_number = 0;
  int mode = UNKNOWN;
  wupper_dev_t wupper;
  int do_flush=0;
  int do_reset=0;
  int do_soft_reset=0;

  if(argc < 2)
    {
      display_help();
      exit(EXIT_FAILURE);
    }

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

  if(optind == argc)
    {
      fprintf(stderr, "No command given\n"
	      "Usage: "APPLICATION_NAME" COMMAND [OPTIONS]\nTry "APPLICATION_NAME" -h for more information.\n");
      exit(EXIT_FAILURE);

    }

  if(0 == strcasecmp(argv[optind], "flush"))  mode = FLUSH;
  if(0 == strcasecmp(argv[optind], "reset"))  mode = RESET;
  if(0 == strcasecmp(argv[optind], "soft-reset"))  mode = SOFT_RESET;
  if(0 == strcasecmp(argv[optind], "all"))  mode = ALL;

  if(mode == UNKNOWN)
    {
      fprintf(stderr, "Unrecognized command '%s'\n"
	      "Usage: "APPLICATION_NAME" COMMAND [OPTIONS]\nTry "APPLICATION_NAME" -h for more information.\n",
	      argv[1]);
      exit(EXIT_FAILURE);
    }


  if(wupper_open(&wupper,  device_number))
    {
      fprintf(stderr, "Could not open device %d\n", device_number);
      exit(EXIT_FAILURE);
    }

  switch(mode)
    {
    case FLUSH:
      do_flush=1;
      break;
    case RESET:
      do_reset=1;
      break;
    case SOFT_RESET:
      do_soft_reset=1;
      break;
    case ALL:
      do_soft_reset=1;
      do_reset=1;
      do_flush=1;
      break;
    }

  if(do_reset){
    wupper_dma_reset(&wupper);
  }
  if(do_flush){
    wupper_dma_fifo_flush(&wupper);
  }
  if(do_soft_reset){
    wupper_dma_soft_reset(&wupper);
  }

  if(wupper_close(&wupper))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not close WUPPER %d\n", device_number);
      return 1;
    }
}
