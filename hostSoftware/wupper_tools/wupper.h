/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper
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
  *  @brief wupper.c provides essential functions. These functions
  *  will handle the connection with the device. 
  *  Wupper.c contains three functions:
  *  - map_memory_bar:
  *  Maps the BAR addresses from register.c. 
  *  
  *  - wupper_open:
  *  Opens the device and sets the BAR addresses.
  * 
  *  - wupper_close:
  *  This function returns the status of the device. 
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
#ifndef WUPPER_H
#define WUPPER_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../driver/include/wupper_common.h"

///////////DEFINED VALUES//////////////

//General information
#define WUPPER_VENDOR_ID (0x10ee)
#define WUPPER_DEVICE_ID (0x7039)
//DMA
#define WUPPER_DMA_WRAPAROUND (1)
//Registers
#define WUPPER_REG_READ               (1)
#define WUPPER_REG_WRITE              (2)
#define WUPPER_CFG_ERROR_NOT_READABLE (1)
#define WUPPER_CFG_ERROR_NOT_WRITABLE (2)
#define WUPPER_CFG_ERROR_NO_EXIST     (3)
//Board models
#define HTG_710       0x2C6
#define VC_709        0x2C5

//////////////////////////////////////
/////////////STRUCTURES///////////////

typedef struct wupper_dma_descriptor
{
  volatile u_long start_address;
  volatile u_long end_address;
  volatile u_long tlp : 11;
  volatile u_long read:1;
  volatile u_long wrap_around:1;
  volatile u_long _reserved:51;
  volatile u_long read_ptr;
} wupper_dma_descriptor_t;

/*****************************/
typedef struct wupper_dma_status
{
  volatile u_long current_address;
  volatile u_long descriptor_done:1;
  volatile u_long even_addr_dma:1;
  volatile u_long even_addr_pc:1;
} wupper_dma_status_t;

/*****************************/
typedef struct wupper_dev
{
  u_long bar0;
  u_long bar1;
  u_long bar2;

  int fd;
  card_params_t card_params;

  volatile wupper_dma_descriptor_t* dma_descriptors;
  volatile wupper_dma_status_t* dma_status;
  volatile u_int* dma_enable;
} wupper_dev_t;

/*****************************/
typedef struct wupper_register {
  const char* name;
  const char* description;
  u_long address;
  u_int flags;
} wupper_register_t;

extern wupper_register_t wupper_registers[];
/*****************************/

typedef struct cmem_buffer
{
  u_long phys_addr;
  u_long virt_addr;
  u_long size;
  u_int handle;
} cmem_buffer_t;
/*****************************/

typedef struct version_delay
{
  const char* version;
  const char* delay;
} version_delay_t;
/*****************************/


//////////////////////////////////////////////
//////////////////FUNCTIONS///////////////////

/*************Generals***************/
int  wupper_close(wupper_dev_t* wupper);
int  wupper_open(wupper_dev_t* wupper, int deviceNumber);
/************DMA Access***************/
int  wupper_dma_max_tlp_bytes(wupper_dev_t* wupper);
void wupper_dma_program_write(u_int dma_id, u_long dst, size_t size, u_int tlp, u_int flags, wupper_dev_t* wupper);
void wupper_dma_program_read(u_int dma_id, u_long dst, size_t size, u_int tlp, u_int flags, wupper_dev_t* wupper);
void wupper_dma_wait(u_int dma_id, wupper_dev_t* wupper);
void wupper_dma_stop(u_int dma_id, wupper_dev_t* wupper);
void wupper_dma_advance_read_ptr(u_int dma_id, u_long dst, size_t size, size_t bytes, wupper_dev_t* wupper);
void wupper_dma_fifo_flush(wupper_dev_t* wupper);
void wupper_dma_reset(wupper_dev_t* wupper);
void wupper_dma_soft_reset(wupper_dev_t* wupper);
/*************Descriptor Access***************/
int cmem_alloc(u_long size, cmem_buffer_t* buffer);
int cmem_free(cmem_buffer_t* buffer);
/*******************IRQ*******************/
int  wupper_irq_init(wupper_dev_t* wupper);
void wupper_irq_enable(wupper_dev_t* wupper, u_long interrupt);
void wupper_irq_enable_all(wupper_dev_t* wupper);
void wupper_irq_disable(wupper_dev_t* wupper, u_long interrupt);
void wupper_irq_disable_all(wupper_dev_t* wupper);
int  wupper_irq_wait(int n, wupper_dev_t* wupper);
int  wupper_irq_cancel(wupper_dev_t* wupper);
/******************Register access*******************/
int wupper_cfg_get_option(wupper_dev_t* wupper, const char* key, u_long* value);
int wupper_cfg_set_option(wupper_dev_t* wupper, const char* key, u_long value);


#ifdef __cplusplus
}
#endif


#endif
