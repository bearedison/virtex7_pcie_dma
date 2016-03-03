/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper_common
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
  *  @brief This is the common header file for the wupper
  *  driver, library & applications				                                     
  * 
  *  @detail
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
#ifndef _WUPPER_COMMON_H
#define _WUPPER_COMMON_H

#ifdef __KERNEL__
  #include <linux/types.h>
#else
  #include <sys/types.h>
#endif


/* ioctl "switch" flags */
#define WUPPER_MAGIC 'y'

#define GETCARDS            _IOW(WUPPER_MAGIC, 1, int)
#define SETCARD             _IOR(WUPPER_MAGIC, 2, card_params_t*)
#define GET_TLP             _IOW(WUPPER_MAGIC, 3, int)
#define WAIT_IRQ            _IOW(WUPPER_MAGIC, 4, int)
#define FAKE_IRQ            _IOWR(WUPPER_MAGIC, 6, int)
#define CANCEL_IRQ_WAIT     _IO(WUPPER_MAGIC, 8)
#define INIT_IRQ            _IO(WUPPER_MAGIC, 9)
#define RESET_IRQ_COUNTERS  _IO(WUPPER_MAGIC, 10)
#define RESET_IRQ_FLAGS     _IO(WUPPER_MAGIC, 11)
#define MASK_IRQ            _IOW(WUPPER_MAGIC, 12, int)
#define UNMASK_IRQ          _IOW(WUPPER_MAGIC, 13, int)

typedef struct
{
  u_int handle;
  u_int offs;
  u_int func;
  u_int data;
  u_int size;
} IO_PCI_CONF_t;

#define MAXCARDS				4   // Max. number of WUPPER cards

typedef struct
{
    struct pci_dev *pciDevice;
    unsigned int slot;
    unsigned int baseAddressBAR0;
    unsigned int sizeBAR0;
    unsigned int baseAddressBAR1;
    unsigned int sizeBAR1;
    unsigned int baseAddressBAR2;
    unsigned int sizeBAR2;
    unsigned int baseAddressBAR3;
    unsigned int sizeBAR3;

    /* void* writeIndexBuffer;
    uint64_t writeIndexHandle;
    int nDmaPages; //number of pre-allocated DMA buffers - can be modified when running insmod - defaults to MAXDMABUF
    int dmaPageSize;
    void* primaryDmaBuffer[MAXDMABUF];
    uint64_t primaryDmaHandle[MAXDMABUF]; */
} card_params_t;

#endif

