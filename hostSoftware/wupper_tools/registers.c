/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class register
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
  *  @brief registers.c consist the BAR2 registers. This BAR is 
  *  reserverd for user application. 
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "wupper.h"


static
void str_upper(char *str) 
{
  do 
    {
      *str = toupper((unsigned char) *str);
    } while (*str++);
}


int
wupper_cfg_set_option(wupper_dev_t* wupper, const char* key, u_long value)
{
  int result = 0;

  char* upper = strdup(key);
  str_upper(upper);
   
  wupper_register_t* reg;
  int found = 0;

  for(reg=wupper_registers; reg->name != NULL; reg++)
    {
      if(0 == strcmp(upper, reg->name))
	{
	  if(!(reg->flags & WUPPER_REG_WRITE))
	    {
	      result = WUPPER_CFG_ERROR_NOT_WRITABLE;
        printf("Register not writable!\n");
	    }

	  u_long* v = (u_long*)(wupper->bar2 + reg->address);
	  *v = value;
	  found = 1;
	  break;
	}
    }

  if(!found)
    {
      result = WUPPER_CFG_ERROR_NO_EXIST;
      printf("Register does not exits!\n");
    }

  free(upper);
  return result;
}

int
wupper_cfg_get_option(wupper_dev_t* wupper, const char* key, u_long* value)
{
  int result = 0;

  char* upper = strdup(key);
  str_upper(upper);
   
  wupper_register_t* reg;
  int found = 0;

  for(reg=wupper_registers; reg->name != NULL; reg++)
    {
      if(0 == strcmp(upper, reg->name))
	{
	  if(!(reg->flags & WUPPER_REG_READ))
	    {
	      result = WUPPER_CFG_ERROR_NOT_READABLE;
        printf("Register not readable!\n");
	      break;
	    }

	  u_long* v = (u_long*)(wupper->bar2 + reg->address);
	  *value = *v;
	  found = 1;
	  break;
	}
    }

  if(!found)
    {
      result = WUPPER_CFG_ERROR_NO_EXIST;
      printf("Register not exist: %s!\n", upper);
    }

  free(upper);
  return result;
}


wupper_register_t wupper_registers[] = 
  {
    { "BOARD_ID_TIMESTAMP",
      "Board ID Timestamp",
      0x0000,
      WUPPER_REG_READ
    },
    { "BOARD_ID_SVN",
      "Board ID SVN Revision",
      0x0008,
      WUPPER_REG_READ
    },
    { "STATUS_LEDS",
      "Board GPIO LEDs",
      0x0010,
      WUPPER_REG_READ|WUPPER_REG_WRITE
    },
    { "GENERIC_CONSTANTS",
      "Number of descriptors / number of interrupts",
      0x0020,
      WUPPER_REG_READ
    },    
    { "PLL_LOCK",
      "PLL locked status",
      0x0300,
      WUPPER_REG_READ,
    },
    { "CARD_TYPE",
      "ID of the card model",
      0x0040,
      WUPPER_REG_READ
    },   
    { "INT_TEST_4",
      "Fire a test MSIx interrupt #4",
      0x1060,
      WUPPER_REG_WRITE
    },
    { "INT_TEST_5",
      "Fire a test MSIx interrupt #5",
      0x1070,
      WUPPER_REG_WRITE
    },
    { NULL, NULL, 0, 0 }
  };
