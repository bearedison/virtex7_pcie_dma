/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class irq
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
  *  @brief irq.c configures the interrupts on the card. This can be used
  *  for testing/debugging. The interupt register can be found in
  *  register.c. It is possible to (dis)enable a part or all interrupts.
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
#include "wupper.h"
#include <asm/ioctl.h>


#define INTERRUPT_ENABLE_OFFSET (0x100)
#define NUM_INTERRUPTS (8)

int
wupper_irq_init(wupper_dev_t* wupper)
{
  return ioctl(wupper->fd, INIT_IRQ);
}

void
wupper_irq_enable(wupper_dev_t* wupper, u_long interrupt)
{    
    u_long *virtAddress = (u_long *) (wupper->bar1 + INTERRUPT_ENABLE_OFFSET);
    *virtAddress |= (1<<interrupt);
}    

void
wupper_irq_enable_all(wupper_dev_t* wupper)
 {

  unsigned i;
  for(i=0; i<NUM_INTERRUPTS; i++)
  {
    wupper_irq_enable(wupper, i);
  }
}

void
wupper_irq_disable(wupper_dev_t* wupper, u_long interrupt)
{
  u_long *virtAddress = (u_long *) (wupper->bar1 + INTERRUPT_ENABLE_OFFSET);
  *virtAddress &= ~(1<<interrupt);
}

void
wupper_irq_disable_all(wupper_dev_t* wupper)
{
  unsigned i;
  for(i=0; i<NUM_INTERRUPTS; i++)
  {
    wupper_irq_disable(wupper, i);
  }
}

int
wupper_irq_wait(int n, wupper_dev_t* wupper)
{
  return ioctl(wupper->fd, WAIT_IRQ, &n);
}

int
wupper_irq_cancel(wupper_dev_t* wupper)
{
  return ioctl(wupper->fd, CANCEL_IRQ_WAIT);
}
