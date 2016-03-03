/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper-config
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
  *  @brief wupper-config.c is a part of the wupper tools. This tool
  *  allows to look at the PCIe configuration registers, set 
  *  configuration parameters, store and load configuration.
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
#include <string.h>
#include <ctype.h>

#include "wupper.h"

#define APPLICATION_NAME "wupper-config"

enum cmd_mode {
  CMD_UNKNOWN,
  CMD_LIST,
  CMD_SET,
  CMD_LOAD,
  CMD_STORE
};

void
display_help()
{
  printf("Usage: %s COMMAND [OPTIONS]\n"
	 "\nCommands:\n"
	 "  list            List card configuration.\n"
	 "  set KEY=VALUE   Set option KEY to VALUE. Multiple settings may be given. If no\n"
	 "                  value is given, 0 is assumed.\n"
	 "  store FILENAME  Store current configuration in the given file.\n"
	 "  load FILENAME   Load the configuration in the given file. The file format\n"
	 "                  used is the one produced by the 'store' command.\n" 
	 "\nOptions:\n"
	 "  -d NUMBER      Use card indicated by NUMBER. Default: 0.\n"
	 "  -h             Display help.\n",
	 APPLICATION_NAME);
}

static
long long split_key_value(char* str)
{
  char* pos = strchr(str, '=');
  if(pos==NULL)
    return 0;
  *pos = '\0';
  return strtoll(pos+1, NULL, 0);
}

static
void cmd_list(wupper_dev_t* wupper)
{
  wupper_register_t* reg;
  int i=0;

  printf("Offset   RW   Name                     Value"
	 "                    Description\n"
	 "============================================"
	 "============================================"
	 "========================\n");

  for(reg=wupper_registers; reg->name != NULL; reg++)
    {
			u_long* value = (u_long*)(wupper->bar2 + reg->address);
            if(reg->flags & WUPPER_REG_READ)
      	{
      	  printf("0x%04llx  [%c%c]  %-24s 0x%016llx \t%s\n", 
      		 reg->address, 
      		 reg->flags & WUPPER_REG_READ ? 'R' : ' ',
      		 reg->flags & WUPPER_REG_WRITE ? 'W' : ' ',
      		 reg->name, 
      		 *value,
      		 reg->description);
      	}
            else
      	{
      	  printf("0x%04llx  [%c%c]  %-24s                  - \t%s\n", 
      		 reg->address, 
      		 reg->flags & WUPPER_REG_READ ? 'R' : ' ',
      		 reg->flags & WUPPER_REG_WRITE ? 'W' : ' ',
      		 reg->name, 
      		 reg->description);
      	}

      /*if(++i % 5 == 0) printf("\n");*/
      }
    }


static
void cmd_set(wupper_dev_t* wupper, char** config, int n)
{
  int i;
  for(i=0; i<n; i++)
    {
      long long value = split_key_value(config[i]);
      wupper_cfg_set_option(wupper, config[i], value);
    }
}


static
void cmd_load(wupper_dev_t* wupper, char* filename)
{
  FILE* fp;
  char* line = NULL;
  size_t len = 0;
  ssize_t read;
  
  fp = fopen(filename, "r");
  if (fp == NULL)
    {
      fprintf(stderr, "Could not open '%s'\n", filename);
      exit(EXIT_FAILURE);
    }

  while (getline(&line, &len, fp) != -1) 
    {
      long long value = split_key_value(line);
      wupper_cfg_set_option(wupper, line, value);
    }
  
  if (line) free(line);
  fclose(fp);
}

static
void cmd_store(wupper_dev_t* wupper, char* filename)
{
  FILE* fp;
  wupper_register_t* reg;
  
  fp = fopen(filename, "w");
  if (fp == NULL)
    {
      fprintf(stderr, "Could not open '%s'\n", filename);
      exit(EXIT_FAILURE);
    }

  for(reg=wupper_registers; reg->name != NULL; reg++)
    {
      if(reg->flags & WUPPER_REG_WRITE && reg->flags & WUPPER_REG_READ)
	{
	  u_long* value = (u_long*)(wupper->bar2 + reg->address);
	  fprintf(fp, "%s=0x%llx\n", reg->name, *value);
	}
    } 

  fclose(fp);
}

int
main(int argc, char** argv)
{
  int opt;
  int device_number = 0;
  int mode = CMD_UNKNOWN;
  wupper_dev_t wupper;

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

  if(0 == strcasecmp(argv[optind], "list"))  mode = CMD_LIST;
  if(0 == strcasecmp(argv[optind], "set"))  mode = CMD_SET;
  if(0 == strcasecmp(argv[optind], "load"))  mode = CMD_LOAD;
  if(0 == strcasecmp(argv[optind], "store"))  mode = CMD_STORE;

  if(mode == CMD_UNKNOWN)
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
    case CMD_LIST:
      cmd_list(&wupper);
      break;
    case CMD_SET:
      cmd_set(&wupper, argv+optind+1, argc-1-optind);
      break;
    case CMD_LOAD:
      if(argc < 3 || argv[2][0]=='-')
	{
	  fprintf(stderr, "No filename given\n");
	  exit(EXIT_FAILURE);
	}
      cmd_load(&wupper, *(argv+optind+1));
      break;
    case CMD_STORE:
      if(argc < 3 || argv[2][0]=='-')
	{
	  fprintf(stderr, "No filename given\n");
	  exit(EXIT_FAILURE);
	}
      cmd_store(&wupper, *(argv+optind+1));
      break;
    }

  if(wupper_close(&wupper))
    {
      fprintf(stderr, APPLICATION_NAME": error: could not close WUPPER %d\n", device_number);
      return 1;
    }
}
  
