/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper-irq-test
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
  *  @brief wupper-irq-test.c is an test application. It waits 
  *  for an interrupt to be fired or enable/disable the interrupt 
  *  mechanism. 
  *
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "wupper.h"

#define APPLICATION_NAME "wupper-irq-test"

void
display_help()
{
  printf("Usage: %s [OPTIONS]\n"
	 "Waits for an interrupt.\n\n"
	 "Options:\n"
	 "  -i NUMBER      Wait for the interrupt indicated by NUMBER. Default: 5.\n"
	 "  -u NUMBER      Use card indicated by NUMBER. Default: 0.\n"
	 "  -e             Enable interrupts.\n"
	 "  -d             Disable interrupts.\n"
	 "  -h             Display help.\n",
	 APPLICATION_NAME);
}


int
main(int argc, char** argv)
{
  u_int ret;
  int device_number = 0;
  int irq_id = 5;
  int opt;

  wupper_dev_t wupper;


  while ((opt = getopt(argc, argv, "huei:d:")) != -1) {
    switch (opt) {
    case 'u':
      device_number = atoi(optarg);
      break;
    case 'e':
	  wupper_irq_init(&wupper);
	  wupper_irq_enable_all(&wupper);
	  exit(0);
	case 'd':
      wupper_irq_disable_all(&wupper);
      break;  
    case 'i':
      irq_id = atoi(optarg);
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
  wupper_irq_init(&wupper);
  wupper_irq_enable_all(&wupper);
 

  printf("Waiting for interrupt %d...", irq_id);
  fflush(stdout);
  wupper_irq_wait(irq_id, &wupper);
  printf(" ok!\n");

  wupper_irq_disable_all(&wupper);

  if(wupper_close(&wupper))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not close WUPPER %d\n", device_number);
      return 1;
    }
  
  return(0);
}
