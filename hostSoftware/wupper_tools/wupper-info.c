/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper-info
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
  *  @brief wupper-info.c is a part of the wupper tools. This tool
  *  displays the information from the device. This tool accept 
  *  one command to specify which information shows.
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
#include <string.h>

#include "wupper.h"

#define APPLICATION_NAME "wupper-info"

/**
 * @brief Different option selections.
 *
 * Each field identifies a option from the command line.
 * Only one can be used at the same time.
 */

enum info_mode {
  INFO_UNKNOWN,
  INFO_ALL,
};

/**
 * @brief Displays information about how to use wupper-info.
 *
 * Also is shown when a wrong command is introduced.
 */

static void
display_help()
{
  printf("Usage: %s [OPTIONS]\n"
	 "Displays information about a WUPPER device.\n\n"
	 "Options:\n"
	 "  -d NUMBER                       Use card indicated by NUMBER. Default: 0.\n"
	 "  -h                              Display help.\n"
	 APPLICATION_NAME);
  printf("\n");
}

/**
 * @brief Displays general board parameters.
 * @param Pointer to wupper device.
 *
 * Parameters displayed:
 *
 * -Board ID.
 * -Card ID
 */


static
void display_board_id(wupper_dev_t* wupper)
{
  u_long card_id=0;
  int card_control=0;
  u_long board_id = *((u_long*)(wupper->bar2));
  printf("Board ID:        %x\n", board_id);

  wupper_cfg_get_option(wupper,"CARD_TYPE",&card_id);
  if(card_id==0x2C6){
    printf("Card ID:         HTG-710\n");
    card_control=1;
  }
  if(card_id==0x2C5){
    printf("Card ID:         VC-709\n");
    card_control=1;
  }
  if(card_control==0){
    printf("Card ID:         UNKNOWN\n");
  }
}

/**
 * @brief Displays general FW parameters.
 * @param Pointer to wupper device.
 *
 * Parameters displayed:
 *
 * -FW version date.
 */

static
void display_FW_date(wupper_dev_t* wupper)
{
  unsigned long date=0;

  unsigned int version_day=0;
  unsigned int version_month=0;
  unsigned int version_year=0;
  unsigned int version_hour=0;
  unsigned int version_minute=0;
  unsigned int version_second=0;

  wupper_cfg_get_option(wupper,"BOARD_ID_TIMESTAMP",&date);
  //Not very elegant
  version_year=(date >> 32);
  version_month=(0x00FF)&(date >> 24);
  version_day=(0x0000FF)&(date >> 16);
  version_hour=(0x000000FF)&(date >> 8);
  version_minute=(0x00000000FF)&date;

  printf("FW version date: %llx/%llx/%llx %llx:%llx\n",version_day,version_month,version_year,version_hour,version_minute );
}

/**
 * @brief Displays commit SVN version.
 * @param Pointer to wupper device.
 *
 */
 
static
void display_SVN_version(wupper_dev_t* wupper)
{
  unsigned long value=0;
  wupper_cfg_get_option(wupper,"BOARD_ID_SVN",&value);
  printf("SVN version:     %d\n", value);
}


/**
 * @brief Displays general board parameters.
 * @param Pointer to wupper device.
 *
 * Parameters displayed:
 *
 * -Board ID.
 * -Card ID
 */

static
void display_interrupts_descriptors(wupper_dev_t* wupper)
{
  unsigned long value=0;
  unsigned int descriptors=0;
  unsigned int interrupts=0;
  wupper_cfg_get_option(wupper,"GENERIC_CONSTANTS",&value);
  descriptors=value&(0x00FF);
  interrupts=(value >> 8);
  printf("Number of interrupts:  %d\n",interrupts);
  printf("Number of descriptors: %d\n", descriptors);
}

int
main(int argc, char** argv)
{
  u_int ret;
  int device_number = 0;
  u_int handle = 0;
  int opt;
  int verbose = 0;
  int common_info=0;

  int arguments=0;
  u_long card_model=0;

  int mode = INFO_UNKNOWN;

  wupper_dev_t wupper;

  while ((opt = getopt(argc, argv, "hvd:")) != -1) {
    switch (opt) {
    case 'd':
      device_number = atoi(optarg);
      arguments=arguments+2;
      break;
    case 'v':
      verbose++;
      arguments++;
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

  if(optind != argc)
  {
    if(0 == strcasecmp(argv[optind], "ALL"))  mode = INFO_ALL;

    if(mode == INFO_UNKNOWN)
      {
        fprintf(stderr, "Unrecognized command '%s'\n"
          "Usage: "APPLICATION_NAME" COMMAND [OPTIONS]\nTry "APPLICATION_NAME" -h for more information.\n",
          argv[1]);
        exit(EXIT_FAILURE);
      }
  }

  switch(mode)
    {
    case INFO_ALL:
      common_info=1;
      break;
    default: /* '?' */
      //fprintf(stderr, "Usage: %s COMMAND [OPTIONS]\nTry %s -h for more information.\n",
        //APPLICATION_NAME, APPLICATION_NAME);
      common_info=1;
    }
    

  if(wupper_open(&wupper,  device_number))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not open WUPPER %d\n", device_number);
      return 1;
    }

  if(common_info){
    //Information display.
    printf("\nGeneral information\n");
    printf("-------------------\n");
    display_board_id(&wupper);
    display_FW_date(&wupper);
    display_SVN_version(&wupper);
    printf("\nInterrupts & descriptors \n");
    printf("----------------------------------\n");
    display_interrupts_descriptors(&wupper);
    printf("\n");

    u_long value;

    wupper_cfg_get_option(&wupper, "pll_lock", &value);
    if(value & 1)
      printf("Internal PLL Lock : Yes\n");
    else
      printf("Internal PLL Lock : NO !!\n");
  }

  if(wupper_close(&wupper))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not close WUPPER %d\n", device_number);
      return 1;
    }
  
  return(0);
}
