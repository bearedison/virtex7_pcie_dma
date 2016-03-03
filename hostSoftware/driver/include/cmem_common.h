/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class cmem_common
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
  *  @brief This is the common header file for the CMEM 
  *  driver, library & applications				                                     
  * 
  *  @detail 12. Dec. 01  MAJO  created
  *                                      
  *  ------------------------------------------------------------------------------
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

#ifndef _CMEM_COMMON_H
#define _CMEM_COMMON_H

#define P_ID_CMEM 7

#define CMEM_MAX_NAME    40
#define TYPE_GFP         1
#define TYPE_OLDBPA      2
#define TYPE_MEMBPA      3
#define TYPE_GFPBPA      4
#define TYPE_BPA         4       // For compatibility with old S/W
#define TEXT_SIZE        3000    // For ioctl(CMEM_DUMP)

#ifdef __KERNEL__
  #include <linux/types.h>
  #include <linux/ioctl.h>
  #define P_ID_CMEM 7        // Needs to be re-defined here since we do not want to include rcc_error.h at this level
#else
  #include <sys/types.h>
  #include <sys/ioctl.h>
/* #include "rcc_error/rcc_error.h" */
#endif

/*************/
/*ioctl codes*/
/*************/

#define CMEM_MAGIC 'x'

#define CMEM_GET        _IOR(CMEM_MAGIC, 1, cmem_t)
#define CMEM_FREE       _IOW(CMEM_MAGIC, 2, int)
#define CMEM_LOCK       _IOW(CMEM_MAGIC, 3, int)
#define CMEM_UNLOCK     _IOW(CMEM_MAGIC, 4, int)
#define CMEM_GETPARAMS  _IOR(CMEM_MAGIC, 5, cmem_t)
#define CMEM_SETUADDR   _IOW(CMEM_MAGIC, 6, cmem_t)
#define CMEM_DUMP       _IO(CMEM_MAGIC, 7)

/*
enum
{
  CMEM_GET = 1,
  CMEM_FREE,
  CMEM_LOCK,
  CMEM_UNLOCK,
  CMEM_GETPARAMS,
  CMEM_SETUADDR,
  CMEM_DUMP
};
*/

/*************/
/*error codes*/
/*************/
enum
{
  CMEM_SUCCESS = 0,
  CMEM_ERROR_FAIL = (P_ID_CMEM << 8) + 1,
  CMEM_FILE,
  CMEM_NOTOPEN,
  CMEM_IOCTL,
  CMEM_MMAP,
  CMEM_MUNMAP,
  CMEM_OVERFLOW,
  CMEM_TOOBIG,
  CMEM_ILLHAND,
  CMEM_NOSIZE,
  CMEM_GETP,
  CMEM_CFU,
  CMEM_GFP,
  CMEM_BPA,
  CMEM_CTU,
  CMEM_KMALLOC,
  CMEM_LL,
  CMEM_NOSUP,
  CMEM_ABOVE4G,
  CMEM_NO_CODE
};

typedef struct
{
  u_long paddr;
  u_long uaddr;
  u_long kaddr;
  u_long size;
  u_int order;
  u_int locked;
  u_int type;
  u_int handle;
  char name[CMEM_MAX_NAME];
} cmem_t;

typedef u_int CMEM_Error_code_t;

#endif
