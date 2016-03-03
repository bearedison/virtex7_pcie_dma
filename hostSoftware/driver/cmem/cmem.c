/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class cmem
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
  *  @brief This is the cmem driver                                          
  *  Its purpose is to provide user applications with contiguous data 
  *
  * 
  * 
  * 
  *   buffers for DMA operations                                       
  * 
  *  @detail 12. Dec. 01  MAJO  created
  * 
  *  Driver name changed from cmem_rcc to cmem to avoid confusion         
  *  Modified to support kernel 3.10 changes                              
  *  Removed big phys area support                                        
  *  Dual BSD / GPL license  (OK for M.Joos)
  * 
  *  NOTES:                                                    
  *  - This driver should work on kernels from 2.6.9 onwards
  * 
  *  Background infor for the type "caddr_t":                               
  *  In /usr/src/kernels/2.6.9-78.0.5.EL.cern-i686/include/linux/types.h    
  *     typedef __kernel_caddr_t caddr_t;                                      
  *  In /usr/src/kernels/2.6.9-78.0.5.EL.cern-i686/include/asm/posix_types.h
  *     typedef char * __kernel_caddr_t;
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

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/pagemap.h>
#include <linux/page-flags.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "../include/cmem_common.h"

// Constants
#define MAX_BUFFS          1000    // Max. number of buffers for all processes
#define MAX_PROC_TEXT_SIZE 0x10000 //The output of "more /proc/cmem" must not generate more characters than that

/************************************/
/*Macros, taken from tdaq_drivers.h */
/************************************/

// #define DRIVER_START_STOP_DEBUG
#define DRIVER_DEBUG
#define DRIVER_ERROR

#ifdef DRIVER_START_STOP_DEBUG
#define kdebugStartStop(x) {printk x;}
#else
#define kdebugStartStop(x)
#endif

#ifdef DRIVER_DEBUG
#define kdebug(x) {if (debug) printk x;}
#else
#define kdebug(x)
#endif

#ifdef DRIVER_ERROR
#define kerror(x) {if (errorlog) printk x;}
#else
#define kerror(x)
#endif

// Types
typedef struct
{
    u_long paddr;
    u_long kaddr;
    u_long uaddr;
    u_long size;
    u_int locked;
    u_int order;
    u_int type;
    u_int used;
    int pid;
    char name[40];
} buffer_t;

typedef struct
{
    u_int buffer[MAX_BUFFS];
} private_stuff;

typedef struct range_struct
{
    struct range_struct *next;
    caddr_t base;			// base of allocated block
    size_t  size;			// size in bytes
} range_t;


/******************************/
/*Standard function prototypes*/
/******************************/
static int cmem_open(struct inode *inode, struct file *file);
static int cmem_release(struct inode *inode, struct file *file);
static long cmem_ioctl(struct file *file, u_int cmd, u_long arg);
static int cmem_mmap(struct file *file, struct vm_area_struct *vma);
static ssize_t cmem_proc_write(struct file *file, const char *buffer, size_t count, loff_t *startOffset);
static ssize_t cmem_proc_read(struct file *file, char *buf, size_t count, loff_t *startOffset);

/*****************************/
/*Service function prototypes*/
/*****************************/
static int membpa_init2(int priority, u_int btype);
static int gfpbpa_init(void);
static caddr_t membpa_alloc_pages(int count, int align, int priority, u_int btype);
static void membpa_free_pages(caddr_t base, u_int btype);
static void cmem_vmaClose(struct vm_area_struct *vma);
static void cmem_vmaOpen(struct vm_area_struct *vma);


#define MAX_GFPBPA_SIZE (64 * 1024)  //64 GB


// Globals
static int gfpbpainit_level, membpainit_level, debug = 0, errorlog = 0, gfpbpa_zone = 0, gfpbpa_quantum = 1;
static long  ram_top = 0, ram_size = 0, gfpbpa_size = 0;
static char *proc_read_text;
static buffer_t *buffer_table;
static u_long gfpbpa_base = 0, membpa = 0, gfpbpa = 0, gfpbpa_array[MAX_GFPBPA_SIZE], gfpbpa_num_pages;
static u_int gfpbpa_order;
static dev_t major_minor;
static struct cdev *cmem_cdev;
static range_t *membpafree_list = NULL, *gfpbpafree_list = NULL;
static range_t *membpaused_list = NULL, *gfpbpaused_list = NULL;
static u_int number_of_available_pages;


/***************************************************************/
/* Use /sbin/modinfo <module name> to extract this information */
/***************************************************************/
module_param (debug, int, S_IRUGO | S_IWUSR);  //MJ: for 2.6 p37
MODULE_PARM_DESC(debug, "1 = enable debugging   0 = disable debugging");

module_param (errorlog, int, S_IRUGO | S_IWUSR);  //MJ: for 2.6 p37
MODULE_PARM_DESC(errorlog, "1 = enable error logging   0 = disable error logging");

module_param (ram_top, long, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ram_top, "The physical size of the system RAM in MB");

module_param (ram_size, long, S_IRUGO | S_IWUSR);  //MJ: for 2.6 p37
MODULE_PARM_DESC(ram_size, "The amount of RAM in MB that will be used for the internal-BPA (mem= variant)");

module_param (gfpbpa_size, long, S_IRUGO | S_IWUSR);  //MJ: for 2.6 p37
MODULE_PARM_DESC(gfpbpa_size, "The amount of RAM in MB that will be used for the internal-BPA (get_free_pages variant)");

module_param (gfpbpa_quantum, int, S_IRUGO | S_IWUSR);  //MJ: for 2.6 p37
MODULE_PARM_DESC(gfpbpa_quantum, "The size (in MB) of a page allocated via get_free_pages for the internal-BPA (get_free_pages variant)");

module_param (gfpbpa_zone, int, S_IRUGO | S_IWUSR);  //MJ: for 2.6 p37
MODULE_PARM_DESC(gfpbpa_zone, "0: Anywhere in the RAM,  1: In the 32bit address range");

MODULE_DESCRIPTION("Allocation of contiguous memory");
MODULE_AUTHOR("Markus Joos, CERN/EP, modified by Jos Vermeulen, Nikhef");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("4.1");


// The ordinary device operations
static struct file_operations fops =
{
    .owner   = THIS_MODULE,
    .unlocked_ioctl = cmem_ioctl,
    .open    = cmem_open,
    .mmap    = cmem_mmap,
    .release = cmem_release,
    .read           = cmem_proc_read,
    .write          = cmem_proc_write,
};

// memory handler functions. MJ: Not actually required. Just for kdebug
static struct vm_operations_struct cmem_vm_ops =
{
    .close = cmem_vmaClose,
    .open  = cmem_vmaOpen,      //MJ: Note the comma at the end of the list!
};


/*****************************/
/* Standard driver functions */
/*****************************/

/****************************/
static int cmem_init(void)
/****************************/
{
    int ret, start, size, loop, ecode = 0;
    static struct proc_dir_entry *cmem_file;

    if(ram_size)
    {
        size = ram_size * 1024 * 1024;
        start = (ram_top * 1024 * 1024) - size;
        //Remap the memory that was hidden from Linux
        kdebugStartStop(("cmem(cmem_init): calling ioremap with start = 0x%08x and size = 0x%08x\n", start, size));
        membpa = (u_long)ioremap(start, size);
        if(membpa == 0)
        {
            kerror(("cmem(cmem_init): failed to ioremap hidden memory\n"));
            ecode = -ENOMEM;
            goto fail1;
        }
        kdebugStartStop(("cmem(cmem_init): hidden memory mapped to address 0x%016lx\n", membpa));

        membpainit_level = 1;
    }
    else
        kdebugStartStop(("cmem(cmem_init): No hidden memory defined\n"));

    number_of_available_pages = (gfpbpa_size * 0x100000) / PAGE_SIZE;
    kdebugStartStop(("cmem(cmem_init): attempt to allocate contiguous memory area of %d pages of %ld Byte\n", number_of_available_pages, PAGE_SIZE));

    if(gfpbpa_size)
    {
        ret = gfpbpa_init();
        if (ret == 1)
        {
            ecode = -ENOMEM;
            goto fail2;
        }
        gfpbpainit_level = 1;
    }
    else
        kdebugStartStop(("cmem(cmem_init): An internal BPA was not requested\n"));

    ecode = alloc_chrdev_region(&major_minor, 0, 1, "cmem"); //MJ: for 2.6 p45
    if (ecode)
    {
        kerror(("cmem(cmem_init): failed to obtain device numbers\n"));
        goto fail3;
    }

    proc_read_text = (char *)kmalloc(MAX_PROC_TEXT_SIZE, GFP_KERNEL);
    if (proc_read_text == NULL)
    {
        ecode = -ENOMEM;
        kerror(("cmem(cmem_init): error from kmalloc\n"));
        goto fail4;
    }

    // Install /proc entry
    cmem_file = proc_create("cmem", 0644, NULL,&fops);
    if (cmem_file == NULL)
    {
        kerror(("cmem(cmem_init): error from call to proc_create\n"));
        ecode = -EFAULT;
        goto fail5;
    }

    // Allocate memory for the buffer table
    kdebugStartStop(("cmem(cmem_init): MAX_BUFFS        = %d\n", MAX_BUFFS));
    kdebugStartStop(("cmem(cmem_init): sizeof(buffer_t) = %lu\n", (u_long)sizeof(buffer_t)));
    kdebugStartStop(("cmem(cmem_init): need %ld bytes\n", MAX_BUFFS * (u_long)sizeof(buffer_t)));
    buffer_table = (buffer_t *)kmalloc(MAX_BUFFS * sizeof(buffer_t), GFP_KERNEL);
    if (buffer_table == NULL)
    {
        kerror(("cmem(cmem_init): unable to allocate memory for buffer table\n"));
        ecode = -EFAULT;
        goto fail6;
    }

    // Clear the buffer table
    for(loop = 0; loop < MAX_BUFFS; loop++)
    {
        buffer_table[loop].paddr  = 0;
        buffer_table[loop].size   = 0;
        buffer_table[loop].used   = 0;
        buffer_table[loop].locked = 0;
        buffer_table[loop].type   = 0;
        buffer_table[loop].pid    = 0;
    }

    cmem_cdev = (struct cdev *)cdev_alloc();      //MJ: for 2.6 p55
    cmem_cdev->ops = &fops;
    ecode = cdev_add(cmem_cdev, major_minor, 1);  //MJ: for 2.6 p56
    if (ecode)
    {
        kerror(("cmem(cmem_init): error from call to cdev_add.\n"));
        goto fail7;
    }

    kdebugStartStop(("cmem(cmem_init): driver loaded; major device number = %d\n", MAJOR(major_minor)));
    return(0);

fail7:
    kfree(buffer_table);

fail6:
    remove_proc_entry("cmem", NULL);

fail5:
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    kfree(proc_read_text);
#endif

fail4:
    unregister_chrdev_region(major_minor, 1); //MJ: for 2.6 p45

fail3:
    if (gfpbpainit_level == 1)
    {
        for(loop = 0; loop < gfpbpa_num_pages; loop++)
            free_pages(gfpbpa_array[loop], gfpbpa_order);
    }

fail2:
    if(membpa)
        iounmap((void *)membpa);

fail1:
    return(ecode);
}


/********************************/
static void cmem_cleanup(void)
/********************************/
{
    int loop, loop2;
    struct page *page_ptr;

    // Release orphaned buffers
    for(loop = 0; loop < MAX_BUFFS; loop++)
    {
        if (buffer_table[loop].used)
        {
            if (buffer_table[loop].locked)
            {
                kdebugStartStop(("cmem(cmem_cleanup): releasing locked buffer: type=%d  paddr=0x%016lx  size=0x%08x  name=%s\n",
                        buffer_table[loop].type, buffer_table[loop].paddr, buffer_table[loop].size, buffer_table[loop].name));
            }
            else
            {
                kdebugStartStop(("cmem(cmem_cleanup): Releasing orphaned buffer: type=%d  paddr=0x%016lx  size=0x%08x  name=%s\n",
                        buffer_table[loop].type, buffer_table[loop].paddr, buffer_table[loop].size, buffer_table[loop].name));
            }

            if (buffer_table[loop].type == TYPE_MEMBPA)
                membpa_free_pages((caddr_t)buffer_table[loop].kaddr, buffer_table[loop].type);

            else //TYPE_GFPBPA
            {
                membpa_free_pages((caddr_t)buffer_table[loop].kaddr, buffer_table[loop].type);

                // unreserve all pages
                page_ptr = virt_to_page(buffer_table[loop].kaddr);

                for (loop2 = buffer_table[loop].order; loop2 > 0; loop2--, page_ptr++)
                    clear_bit(PG_reserved, &page_ptr->flags);
            }
        }
    }

    kdebugStartStop(("cmem(cmem_cleanup): releasing gfp_bpa pages\n"));
    for(loop = 0; loop < gfpbpa_num_pages; loop++)
    {
        kdebugStartStop(("cmem(cmem_cleanup): releasing page with kaddr = 0x%016lx\n", gfpbpa_array[loop]));
        free_pages(gfpbpa_array[loop], gfpbpa_order);
    }

    cdev_del(cmem_cdev);                     //MJ: for 2.6 p56

    // Remove /proc entry
    remove_proc_entry("cmem", NULL);
    kfree(proc_read_text);

    // Return the buffer table
    kfree(buffer_table);

    // Return the memory for the free lists(s)
    if (membpafree_list)
    {
        kdebugStartStop(("cmem(cmem_cleanup): freeing membpafree_list\n"));
        kfree(membpafree_list);
    }
    if (gfpbpafree_list)
    {
        kdebugStartStop(("cmem(cmem_cleanup): freeing gfpbpafree_list\n"));
        kfree(gfpbpafree_list);
    }

    // Unregister the device
    unregister_chrdev_region(major_minor, 1); //MJ: for 2.6 p45

    //Unmap the hidden memory
    if(membpa)
    {
        kdebugStartStop(("cmem(cmem_cleanup): Calling iounmap for address 0x%016lx\n", membpa));
        iounmap((void *)membpa);
    }
}


module_init(cmem_init);    //MJ: for 2.6 p16
module_exit(cmem_cleanup); //MJ: for 2.6 p16


/**************************************************************/
static int cmem_open(struct inode *inode, struct file *file)
/**************************************************************/
{
    int loop;
    private_stuff *pptr;

    kdebug(("cmem(cmem_open): function called for file at 0x%016lx\n", (u_long)file))
            //reserve space to store information about the memory buffers managed by this "file"
            pptr = (private_stuff *)kmalloc(sizeof(private_stuff), GFP_KERNEL);
    if (pptr == NULL)
    {
        kerror(("cmem(cmem_open): error from kmalloc\n"));
        return(-EFAULT);
    }

    //Initialize the space
    for (loop = 0; loop < MAX_BUFFS; loop++)
        pptr->buffer[loop] = 0;

    file->private_data = pptr;
    kdebug(("cmem(cmem_open): private_data = 0x%016lx\n", (u_long)file->private_data));

    return(0);
}


/*****************************************************************/
static int cmem_release(struct inode *inode, struct file *file)
/*****************************************************************/
{
    int loop, loop2;
    struct page *page_ptr;
    private_stuff *pptr;

    kdebug(("cmem(cmem_release): function called from process %d for file at 0x%016lx\n", current->pid, (u_long)file));
    pptr = (private_stuff *) file->private_data;

    // Release orphaned buffers of the current process
    //MJ-SMP: protect this fragment (preferrably with a spinlock)
    for(loop = 0; loop < MAX_BUFFS; loop++)
    {
        if ((pptr->buffer[loop] == 1) && (!buffer_table[loop].locked == 1))
        {
            if (buffer_table[loop].type == TYPE_MEMBPA)
                membpa_free_pages((caddr_t)buffer_table[loop].kaddr, buffer_table[loop].type);

            else //TYPE_GFPBPA
            {
                membpa_free_pages((caddr_t)buffer_table[loop].kaddr, buffer_table[loop].type);

                // unreserve all pages
                page_ptr = virt_to_page(buffer_table[loop].kaddr);

                for (loop2 = buffer_table[loop].order; loop2 > 0; loop2--, page_ptr++)
                    clear_bit(PG_reserved, &page_ptr->flags);
            }

            kdebug(("cmem(cmem_release): Releasing orphaned buffer of process %d: type=%d  paddr=0x%016lx  size=0x%08lx  name=%s\n",
                    buffer_table[loop].pid, buffer_table[loop].type, buffer_table[loop].paddr, buffer_table[loop].size, buffer_table[loop].name));

            // clear the entry in the buffer table
            buffer_table[loop].paddr = 0;
            buffer_table[loop].kaddr = 0;
            buffer_table[loop].size  = 0;
            buffer_table[loop].type  = 0;
            buffer_table[loop].pid   = 0;
            buffer_table[loop].order = 0;
            buffer_table[loop].used  = 0;
            pptr->buffer[loop] = 0;
        }
    }
    //MJ-SMP: end of protected zone

    kfree(pptr);
    return(0);
}


/******************************************************************/
static long cmem_ioctl(struct file *file, u_int cmd, u_long arg)
/******************************************************************/
{
    private_stuff *pptr;

    kdebug(("cmem(unlocked_ioctl): cmd = %u (0x%08x)\n", cmd, cmd));
    pptr = (private_stuff *) file->private_data;
    //return(0);

    switch (cmd)
    {
    case CMEM_GET:
    {
        u_int loop, tnum, ok, pagecount;
        cmem_t uio_desc;
        struct page *page_ptr;

        if (copy_from_user(&uio_desc, (void *)arg, sizeof(cmem_t)) !=0)
        {
            kerror(("cmem(ioctl,cmem_GET): error in from copy_from_user\n"));
            return(-CMEM_CFU);
        }
        kdebug(("cmem(ioctl,cmem_GET): uio_desc.order = 0x%08x\n", uio_desc.order));
        kdebug(("cmem(ioctl,cmem_GET): uio_desc.size = 0x%08lx\n", uio_desc.size));
        //Note: depending on the type of buffer either "order" or "size" is required. The other parameter is dummy
        kdebug(("cmem(ioctl,cmem_GET): uio_desc.type = 0x%08x\n", uio_desc.type));

        //MJ-SMP: protect this fragment (preferrably with a spinlock)
        // Look for a free slot in the buffer table
        ok = 0;
        for(tnum = 0; tnum < MAX_BUFFS; tnum++)
        {
            if (buffer_table[tnum].used == 0)
            {
                buffer_table[tnum].used = 1;  //This is to reserve the entry
                pptr->buffer[tnum] = 1;       //Remember which file this buffer will belong to
                uio_desc.handle = tnum;
                ok = 1;
                kdebug(("cmem(ioctl,cmem_GET): tnum = %d\n", tnum));
                break;
            }
        }
        //MJ-SMP: end of protected zone

        if (!ok)
        {
            kerror(("cmem(ioctl,cmem_GET): all buffers are in use\n"));
            return(-CMEM_OVERFLOW);
        }

        if(uio_desc.type == TYPE_GFP)
        {
             kerror(("cmem(ioctl,cmem_GET): TYPE_GFP not supported\n"));
             return(-CMEM_BPA);
        }
        if(uio_desc.type == TYPE_OLDBPA)
        {
            kerror(("cmem(ioctl,cmem_GET): Pool: BPA is not supported\n"));
            return(-CMEM_BPA);
        }

        pagecount = (int)((uio_desc.size - 1) / PAGE_SIZE + 1); // pages
        // check if enough memory is available
        if (number_of_available_pages < pagecount)
        {
            kdebug(("cmem(ioctl,cmem_GET): requested number of BPA pages = %d\n", pagecount));
            kdebug(("number of available pages = %d -> memory not allocated\n", number_of_available_pages));
            return(-CMEM_NOSIZE);
        }
        number_of_available_pages = number_of_available_pages - pagecount;

        buffer_table[tnum].order  = pagecount;  //MJ note: for the BPA variant we abuse "order". It is not the "order" but the "number of pages"
        kdebug(("cmem(ioctl,cmem_GET): requested number of BPA pages = %d, number of pages left will be %d\n", pagecount, number_of_available_pages));

        if(uio_desc.type == TYPE_GFPBPA)
        {
            kdebug(("cmem(ioctl,cmem_GET): Pool: GFPBPA\n"));
            uio_desc.kaddr = (u_long)membpa_alloc_pages(pagecount, 0, GFP_KERNEL, uio_desc.type);
            uio_desc.paddr = virt_to_bus((void *) uio_desc.kaddr);
            kdebug(("cmem(ioctl,cmem_GET,GFPBPA): uio_desc.kaddr = 0x%016lx\n", uio_desc.kaddr));
            kdebug(("cmem(ioctl,cmem_GET,GFPBPA): uio_desc.paddr = 0x%016lx\n", uio_desc.paddr));

            // Reserve all pages to make them remapable
            page_ptr = virt_to_page(uio_desc.kaddr);
            kdebug(("cmem(ioctl,cmem_GET): reserving %d pages\n", pagecount));

            for (loop = pagecount; loop > 0; loop--, page_ptr++)
                set_bit(PG_reserved, &page_ptr->flags);            //MJ: have a look at the kernel book
        }
        else
        {
            kdebug(("cmem(ioctl,cmem_GET): Pool: MEMBPA\n"));
            uio_desc.kaddr = (u_long)membpa_alloc_pages(pagecount, 0, GFP_KERNEL, uio_desc.type);
            uio_desc.paddr = ((ram_top - ram_size) * 1024 * 1024) + uio_desc.kaddr - membpa;
            kdebug(("cmem(ioctl,cmem_GET): uio_desc.kaddr = 0x%016lx\n", uio_desc.kaddr));
            kdebug(("cmem(ioctl,cmem_GET): uio_desc.paddr = 0x%016lx\n", uio_desc.paddr));
        }

        uio_desc.size = PAGE_SIZE * pagecount;
        if (uio_desc.kaddr == 0)
        {
            kerror(("cmem(ioctl,cmem_GET): error on buffer allocation\n"));
            //MJ-SMP: protect this fragment (preferrably with a spinlock)
            buffer_table[tnum].used = 0;  //Not required any more
            pptr->buffer[tnum] = 0;
            //MJ-SMP: end of protected zone
            return(-CMEM_BPA);
        }

        // Complete the entry in the buffer table
        buffer_table[tnum].size   = uio_desc.size;
        buffer_table[tnum].paddr  = uio_desc.paddr;
        buffer_table[tnum].kaddr  = uio_desc.kaddr;
        buffer_table[tnum].pid    = current->pid;
        buffer_table[tnum].type   = uio_desc.type;
        buffer_table[tnum].locked = 0;
        strcpy(buffer_table[tnum].name, uio_desc.name);

        kdebug(("cmem(ioctl,cmem_GET): PAGE_SIZE       = 0x%08x\n", (u_int)PAGE_SIZE));
        kdebug(("cmem(ioctl,cmem_GET): buffer_table[%d].kaddr = 0x%016lx\n", tnum, (u_long)buffer_table[tnum].kaddr));
        kdebug(("cmem(ioctl,cmem_GET): buffer_table[%d].paddr = 0x%016lx\n", tnum, (u_long)buffer_table[tnum].paddr));
        kdebug(("cmem(ioctl,cmem_GET): buffer_table[%d].size  = 0x%08x\n", tnum, (u_int)buffer_table[tnum].size));
        kdebug(("cmem(ioctl,cmem_GET): buffer_table[%d].name  = %s\n", tnum, buffer_table[tnum].name));
        kdebug(("cmem(ioctl,cmem_GET): buffer_table[%d].order = 0x%08x\n", tnum, (u_int)buffer_table[tnum].order));
        kdebug(("cmem(ioctl,cmem_GET): buffer_table[%d].type  = 0x%08x\n", tnum, (u_int)buffer_table[tnum].type));
        kdebug(("cmem(ioctl,cmem_GET): uio_desc.handle = 0x%08x\n", (u_int)uio_desc.handle));

        if (copy_to_user((void *)arg, &uio_desc, sizeof(cmem_t)) != 0)
        {
            kerror(("cmem(ioctl,cmem_GET): error in from copy_to_user\n"));
            return(-CMEM_CTU);
        }

        kdebug(("cmem(ioctl,cmem_GET): done\n"));
        break;
    }

    case CMEM_FREE:
    {
        u_int handle, loop;
        struct page *page_ptr;

        if (copy_from_user(&handle, (void *)arg, sizeof(int)) !=0)
        {
            kerror(("cmem(ioctl,cmem_FREE): error in from copy_from_user\n"));
            return(-CMEM_CFU);
        }

        kdebug(("cmem(ioctl,cmem_FREE): handle = 0x%08x\n", handle));
        kdebug(("cmem(ioctl,cmem_FREE): Number of available pages before freeing = %d\n", number_of_available_pages));

        //MJ-SMP: protect this fragment (preferrably with a spinlock)
        // Check if the handle makes sense
        if (buffer_table[handle].used == 0)
        {
            kerror(("cmem(ioctl,cmem_FREE): Invalid handle\n"));
            return(-CMEM_ILLHAND);
        }
        buffer_table[handle].used = 0;
        //MJ-SMP: end of protected zone

        if (buffer_table[handle].type == TYPE_MEMBPA)
        {
            membpa_free_pages((caddr_t)buffer_table[handle].kaddr, buffer_table[handle].type);
            kdebug(("cmem(ioctl,cmem_FREE): MEMBPA memory freed @ address 0x%016lx\n", buffer_table[handle].kaddr));
        }
        else //TYPE_GFPBA
        {
            membpa_free_pages((caddr_t)buffer_table[handle].kaddr, buffer_table[handle].type);
            kdebug(("cmem(ioctl,cmem_FREE): MEMBPA memory freed @ address 0x%016lx\n", buffer_table[handle].kaddr));

            // unreserve all pages
            kdebug(("cmem(ioctl,cmem_FREE): unreserving GFPBPA pages\n"));
            page_ptr = virt_to_page(buffer_table[handle].kaddr);

            for (loop = buffer_table[handle].order; loop > 0; loop--, page_ptr++)
                clear_bit(PG_reserved, &page_ptr->flags);
            kdebug(("cmem(ioctl,cmem_FREE): pages unreserved\n"));
        }
        number_of_available_pages = number_of_available_pages + ((buffer_table[handle].size - 1) / PAGE_SIZE + 1);
        kdebug(("cmem(ioctl,cmem_FREE): Number of available pages after freeing = %d\n", number_of_available_pages));

        // Delete the entry in the buffer table
        buffer_table[handle].paddr  = 0;
        buffer_table[handle].locked = 0;
        buffer_table[handle].pid    = 0;
        buffer_table[handle].kaddr  = 0;
        buffer_table[handle].type   = 0;
        buffer_table[handle].order  = 0;
        buffer_table[handle].size   = 0;  //This enables the entry to be re-used
        pptr->buffer[handle] = 0;

        kdebug(("cmem(ioctl,cmem_FREE): done\n"));
        break;
    }

    case CMEM_LOCK:
    {
        u_int handle;

        if (copy_from_user(&handle, (void *)arg, sizeof(int)) !=0)
        {
            kerror(("cmem(ioctl,cmem_LOCK): error in from copy_from_user\n"));
            return(-CMEM_CFU);
        }

        kdebug(("cmem(ioctl,cmem_LOCK): handle = 0x%08x\n", handle));

        // Check if the handle makes sense
        if (buffer_table[handle].used == 0)
        {
            kerror(("cmem(ioctl,cmem_LOCK): Invalid handle\n"));
            return(-CMEM_ILLHAND);
        }

        buffer_table[handle].locked = 1;

        kdebug(("cmem(ioctl,cmem_LOCK): done\n"));
        break;
    }

    case CMEM_UNLOCK:
    {
        u_int handle;

        if (copy_from_user(&handle, (void *)arg, sizeof(int)) !=0)
        {
            kerror(("cmem(ioctl,cmem_UNLOCK): error in from copy_from_user\n"));
            return(-CMEM_CFU);
        }

        kdebug(("cmem(ioctl,cmem_UNLOCK): handle = 0x%08x\n", handle));

        // Check if the handle makes sense
        if (buffer_table[handle].used == 0)
        {
            kerror(("cmem(ioctl,cmem_UNLOCK): Invalid handle\n"));
            return(-CMEM_ILLHAND);
        }

        buffer_table[handle].locked = 0;

        kdebug(("cmem(ioctl,cmem_UNLOCK): done\n"));
        break;
    }

    case CMEM_GETPARAMS:
    {
        cmem_t uio_desc;

        if (copy_from_user(&uio_desc, (void *)arg, sizeof(cmem_t)) !=0)
        {
            kerror(("cmem(ioctl,cmem_GETPARAMS): error in from copy_from_user\n"));
            return(-CMEM_CFU);
        }

        // Check if the handle makes sense
        if (buffer_table[uio_desc.handle].used == 0)
        {
            kerror(("cmem(ioctl,cmem_GETPARAMS): Invalid handle\n"));
            return(-CMEM_ILLHAND);
        }
        kdebug(("cmem(ioctl,cmem_GETPARAMS): called for handle %d\n", uio_desc.handle));
        uio_desc.paddr  = buffer_table[uio_desc.handle].paddr;
        uio_desc.uaddr  = buffer_table[uio_desc.handle].uaddr;
        uio_desc.kaddr  = buffer_table[uio_desc.handle].kaddr;
        uio_desc.size   = buffer_table[uio_desc.handle].size;
        uio_desc.order  = buffer_table[uio_desc.handle].order;
        uio_desc.locked = buffer_table[uio_desc.handle].locked;
        uio_desc.type   = buffer_table[uio_desc.handle].type;
        strcpy(uio_desc.name, buffer_table[uio_desc.handle].name);

        if (copy_to_user((void *)arg, &uio_desc, sizeof(cmem_t)) != 0)
        {
            kerror(("cmem(ioctl,cmem_GETPARAMS): error in from copy_to_user\n"));
            return(-CMEM_CTU);
        }
        kdebug(("cmem(ioctl,cmem_GETPARAMS): done\n"));
        break;
    }

    case CMEM_SETUADDR:
    {
        cmem_t uio_desc;

        if (copy_from_user(&uio_desc, (void *)arg, sizeof(cmem_t)) !=0)
        {
            kerror(("cmem(ioctl,cmem_SETUADDR): error in from copy_from_user\n"));
            return(-CMEM_CFU);
        }

        // Check if the handle makes sense
        if (buffer_table[uio_desc.handle].used == 0)
        {
            kerror(("cmem(ioctl,cmem_SETUADDR): Invalid handle\n"));
            return(-CMEM_ILLHAND);
        }
        kdebug(("cmem(ioctl,cmem_SETUADDR): called for handle %d\n", uio_desc.handle));
        kdebug(("cmem(ioctl,cmem_SETUADDR): uaddr = 0x%016lx\n", uio_desc.uaddr));
        buffer_table[uio_desc.handle].uaddr = uio_desc.uaddr;

        kdebug(("cmem(ioctl,cmem_SETUADDR): done\n"));
        break;
    }

    case CMEM_DUMP:
    {
        char *buf;
        int len, loop;

        kdebug(("cmem(ioctl,cmem_DUMP): called\n"));

        buf = (char *)kmalloc(TEXT_SIZE, GFP_KERNEL);
        if (buf == NULL)
        {
            kerror(("cmem(ioctl,cmem_DUMP): error from kmalloc\n"));
            return(-CMEM_KMALLOC);
        }

        len = 0;
        //MJ-SMP: protect this fragment (preferrably with a spinlock)
        if(membpa)
        {
            len += sprintf(buf + len, "Memory allocated by MEMBPA\n");
            len += sprintf(buf + len, "  PID |         Phys. address |          Size | Locked | Name\n");
            for(loop = 0; loop < MAX_BUFFS; loop++)
            {
                if (buffer_table[loop].used && buffer_table[loop].type == TYPE_MEMBPA)
                {
                    len += sprintf(buf + len, "%5d |", buffer_table[loop].pid);
                    len += sprintf(buf + len, "    0x%016lx |", buffer_table[loop].paddr);
                    len += sprintf(buf + len, "    0x%08lx |", buffer_table[loop].size);
                    len += sprintf(buf + len, "    %s |", buffer_table[loop].locked ? "yes" : " no");
                    len += sprintf(buf + len, " %s\n", buffer_table[loop].name);
                }
            }
        }

        if(gfpbpa)
        {
            len += sprintf(buf + len, "Memory allocated by GFPBPA\n");
            len += sprintf(buf + len, "  PID |         Phys. address |          Size | Locked | Name\n");
            for(loop = 0; loop < MAX_BUFFS; loop++)
            {
                if (buffer_table[loop].used && buffer_table[loop].type == TYPE_GFPBPA)
                {
                    len += sprintf(buf + len, "%5d |", buffer_table[loop].pid);
                    len += sprintf(buf + len, "    0x%016lx |", buffer_table[loop].paddr);
                    len += sprintf(buf + len, "    0x%08lx |", buffer_table[loop].size);
                    len += sprintf(buf + len, "    %s |", buffer_table[loop].locked ? "yes" : " no");
                    len += sprintf(buf + len, " %s\n", buffer_table[loop].name);
                }
            }
        }
        //MJ-SMP: end of protected zone

        if (copy_to_user((void *)arg, buf, TEXT_SIZE * sizeof(char)) != 0)
        {
            kerror(("cmem(ioctl,cmem_DUMP): error from copy_to_user\n"));
            return(-CMEM_CTU);
        }

        kfree(buf);
        break;
    }
    }
    return(0);
}


/******************************************************/
static void cmem_vmaOpen(struct vm_area_struct *vma)
/******************************************************/
{
    kdebug(("cmem_vmaOpen: Called\n"));
}


/*******************************************************/
static void cmem_vmaClose(struct vm_area_struct *vma)
/*******************************************************/
{
    kdebug(("cmem(cmem_vmaClose): Virtual address  = 0x%016lx\n", (u_long)vma->vm_start));
    kdebug(("cmem(cmem_vmaClose): mmap released\n"));
}


/*********************************************************************/
static int cmem_mmap(struct file *file, struct vm_area_struct *vma)
/*********************************************************************/
{
    u_long offset, size;

    kdebug(("cmem(cmem_mmap): cmem_mmap called\n"));
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
    vma->vm_flags |= VM_RESERVED;
#else
    vma->vm_flags |= VM_DONTEXPAND;
    vma->vm_flags |= VM_DONTDUMP;
#endif
    vma->vm_flags |= VM_LOCKED;
    kdebug(("cmem(cmem_mmap): vma->vm_end    = 0x%016lx\n", (u_long)vma->vm_end));
    kdebug(("cmem(cmem_mmap): vma->vm_start  = 0x%016lx\n", (u_long)vma->vm_start));
    kdebug(("cmem(cmem_mmap): vma->vm_offset = 0x%016lx\n", (u_long)vma->vm_pgoff << PAGE_SHIFT));
    kdebug(("cmem(cmem_mmap): vma->vm_flags  = 0x%08x\n", (u_int)vma->vm_flags));

    size = vma->vm_end - vma->vm_start;
    offset = vma->vm_pgoff << PAGE_SHIFT;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
    if (remap_page_range(vma, vma->vm_start, offset, size, vma->vm_page_prot))
#else
    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot))
#endif
    {
        kerror(("cmem(cmem_mmap): function remap_page_range failed \n"));
        return(-CMEM_MMAP);
    }
    kdebug(("cmem(cmem_mmap): vma->vm_start(2) = 0x%016lx\n", (u_long)vma->vm_start));

    vma->vm_ops = &cmem_vm_ops;
    kdebug(("cmem(cmem_mmap): cmem_mmap done\n"));
    return(0);
}


/******************************************************************************************************/
static ssize_t cmem_proc_write(struct file *file, const char *buffer, size_t count, loff_t *startOffset)
/******************************************************************************************************/
{
    int len, loop, loop2;
    struct page *page_ptr;
    char value[100];

    kdebug(("cmem(cmem_proc_write): cmem_proc_write called\n"));

    if(count > 99)
        len = 99;
    else
        len = count;

    if (copy_from_user(value, buffer, len))
    {
        kerror(("cmem(cmem_proc_write): error from copy_from_user\n"));
        return(-EFAULT);
    }

    kdebug(("cmem(cmem_proc_write): len = %d\n", len));
    value[len - 1] = '\0';
    kdebug(("cmem(cmem_proc_write): text passed = %s\n", value));

    if (!strcmp(value, "debug"))
    {
        debug = 1;
        kdebug(("cmem(cmem_proc_write): debugging enabled\n"));
    }

    if (!strcmp(value, "nodebug"))
    {
        kdebug(("cmem(cmem_proc_write): debugging disabled\n"));
        debug = 0;
    }

    if (!strcmp(value, "elog"))
    {
        kdebug(("cmem(cmem_proc_write): error logging enabled\n"))
                errorlog = 1;
    }

    if (!strcmp(value, "noelog"))
    {
        kdebug(("cmem(cmem_proc_write): error logging disabled\n"))
                errorlog = 0;
    }

    if (!strcmp(value, "freelock"))
    {
        kdebug(("cmem(cmem_proc_write): releasing all locked segments\n"));
        //MJ-SMP: protect this fragment (preferrably with a spinlock)
        for(loop = 0; loop < MAX_BUFFS; loop++)
        {
            if (buffer_table[loop].used && buffer_table[loop].locked)
            {
                kdebug(("cmem(cmem_proc_write): releasing locked buffer: type=%d  paddr=0x%016lx  size=0x%08lx  name=%s\n", buffer_table[loop].type, buffer_table[loop].paddr, buffer_table[loop].size, buffer_table[loop].name));
                if (buffer_table[loop].type == TYPE_MEMBPA)
                    membpa_free_pages((caddr_t)buffer_table[loop].kaddr, buffer_table[loop].type);

                else  //TYPE_GFPBPA
                {
                    membpa_free_pages((caddr_t)buffer_table[loop].kaddr, buffer_table[loop].type);
                    //unreserve pages
                    page_ptr = virt_to_page(buffer_table[loop].kaddr);
                    for (loop2 = buffer_table[loop].order; loop2 > 0; loop2--, page_ptr++)
                        clear_bit(PG_reserved, &page_ptr->flags);
                }

                // clear the entry in the buffer table
                buffer_table[loop].paddr = 0;
                buffer_table[loop].kaddr = 0;
                buffer_table[loop].size  = 0;
                buffer_table[loop].type  = 0;
                buffer_table[loop].pid   = 0;
                buffer_table[loop].order = 0;
                buffer_table[loop].used  = 0;
            }
        }
        //MJ-SMP: end of protected zone
    }

    return len;
}


/********************************************************************************************/
static ssize_t cmem_proc_read(struct file *file, char *buf, size_t count, loff_t *startOffset)
/********************************************************************************************/
{
    int loop, nchars = 0;
    static int len = 0;
    int offset = *startOffset;

    kdebug(("cmem(cmem_proc_read): Called with buf    = 0x%016lx\n", (u_long)buf));
    kdebug(("cmem(cmem_proc_read): Called with offset = %d\n", offset));
    kdebug(("cmem(cmem_proc_read): Called with count  = %d\n", (int) count));

    if (offset == 0)
    {
        kdebug(("cmem(cmem_proc_read): Creating text....\n"));
        len = 0;

        len += sprintf(proc_read_text, "\n");
        len += sprintf(proc_read_text + len, "CMEM driver\n");

        if(membpa)
        {
            len += sprintf(proc_read_text + len, "\nMEMBPA (size = %ld MB)\n", ram_size);
            len += sprintf(proc_read_text + len, "  PID | Handle |         Phys. address |          Size | Locked | Name\n");
            for(loop = 0; loop < MAX_BUFFS; loop++)
            {
                if (buffer_table[loop].used && buffer_table[loop].type == TYPE_MEMBPA)
                {
                    len += sprintf(proc_read_text + len, "%5d |", buffer_table[loop].pid);
                    len += sprintf(proc_read_text + len, "%7d |", loop);
                    len += sprintf(proc_read_text + len, "    0x%016lx |", buffer_table[loop].paddr);
                    len += sprintf(proc_read_text + len, "    0x%08lx |", buffer_table[loop].size);
                    len += sprintf(proc_read_text + len, "    %s |", buffer_table[loop].locked ? "yes" : " no");
                    len += sprintf(proc_read_text + len, " %s\n", buffer_table[loop].name);
                }
            }
        }
        if (gfpbpa)
        {
            len += sprintf(proc_read_text + len, "\nGFPBPA (size = %ld MB, base = 0x%016lx)\n", gfpbpa_size, gfpbpa_base);
            len += sprintf(proc_read_text + len, "  PID | Handle |         Phys. address |          Size | Locked | Name\n");
            for(loop = 0; loop < MAX_BUFFS; loop++)
            {
                if (buffer_table[loop].used && buffer_table[loop].type == TYPE_GFPBPA)
                {
                    len += sprintf(proc_read_text + len, "%5d |", buffer_table[loop].pid);
                    len += sprintf(proc_read_text + len, "%7d |", loop);
                    len += sprintf(proc_read_text + len, "    0x%016lx |", buffer_table[loop].paddr);
                    len += sprintf(proc_read_text + len, "    0x%08lx |", buffer_table[loop].size);
                    len += sprintf(proc_read_text + len, "    %s |", buffer_table[loop].locked ? "yes" : " no");
                    len += sprintf(proc_read_text + len, " %s\n", buffer_table[loop].name);
                }
            }
        }

        len += sprintf(proc_read_text + len, " \n");
        len += sprintf(proc_read_text + len, "The command 'echo <action> > /proc/cmem', executed as root,\n");
        len += sprintf(proc_read_text + len, "allows you to interact with the driver. Possible actions are:\n");
        len += sprintf(proc_read_text + len, "debug    -> enable debugging\n");
        len += sprintf(proc_read_text + len, "nodebug  -> disable debugging\n");
        len += sprintf(proc_read_text + len, "elog     -> Log errors to /var/log/messages\n");
        len += sprintf(proc_read_text + len, "noelog   -> Do not log errors to /var/log/messages\n");
        len += sprintf(proc_read_text + len, "freelock -> release all locked segments\n");
    }
    kdebug(("cmem(cmem_proc_read): number of characters in text buffer = %d\n", len));

    if (count < (len - offset))
        nchars = count;
    else
        nchars = len - offset;
    kdebug(("cmem(cmem_proc_read): min nchars         = %d\n", nchars));

    if (nchars > 0)
    {
        for (loop = 0; loop < nchars; loop++)
        {
            buf[loop + (offset & (PAGE_SIZE - 1))] = proc_read_text[offset + loop];
        }
        *startOffset = len + (offset & (PAGE_SIZE - 1));
    }
    else
    {
        nchars = 0;
    }

    kdebug(("cmem(cmem_proc_read): returning *start   = 0x%016lx\n", (u_long)*startOffset));
    kdebug(("cmem(cmem_proc_read): returning nchars   = %d\n", nchars));
    return(nchars);
}


/****************************************/
/* Service function (insourcing of BPA) */
/****************************************/


/************************************************/
static int membpa_init2(int priority, u_int btype)
/************************************************/
{
    range_t *free_list;

    if (btype != TYPE_GFPBPA && btype != TYPE_MEMBPA)
    {
        kerror(("cmem(membpa_init2): ERROR: btype = %d\n", btype));
        return(1);
    }

    kdebug(("cmem(membpa_init2): called with priority = %d and btype = %d\n", priority, btype));
    if ((btype == TYPE_MEMBPA && membpainit_level == 1) || (btype == TYPE_GFPBPA && gfpbpainit_level == 1))
    {
        free_list = (range_t *)kmalloc(sizeof(range_t), priority);
        if (free_list != NULL)
        {
            free_list->next = NULL;

            if (btype == TYPE_MEMBPA)
            {
                free_list->base = (caddr_t)membpa;
                free_list->size = ram_size * 1024 * 1024;
                membpainit_level = 2;
            }
            else
            {
                free_list->base = (caddr_t)gfpbpa;
                free_list->size = gfpbpa_size * 1024 * 1024;
                gfpbpainit_level = 2;
            }

            if(btype == TYPE_MEMBPA)
            {
                kdebug(("cmem(membpa_init2): Initializing membpafree_list\n"));
                membpafree_list = free_list;
            }
            else
            {
                kdebug(("cmem(membpa_init2): Initializing gfpbpafree_list\n"));
                gfpbpafree_list = free_list;
            }

            kdebug(("cmem(membpa_init2): OK\n"));
            return 0;
        }
        kerror(("cmem(membpa_init2): ERROR: free_list is NULL\n"));
    }
    kerror(("cmem(membpa_init2): ERROR: init_level != 1\n"));
    return 1;
}


/********************************************************************************/
static caddr_t membpa_alloc_pages(int count, int align, int priority, u_int btype)
/********************************************************************************/
{
    range_t *range, **range_ptr, *new_range, *align_range;
    caddr_t aligned_base = 0;

    kdebug(("cmem(membpa_alloc_pages): called with count = 0x%08x, align = 0x%08x, priority = 0x%08x,\n", count, align, priority));

    if ((btype == TYPE_MEMBPA && membpainit_level < 2) || (btype == TYPE_GFPBPA && gfpbpainit_level < 2))
    {
        if (membpa_init2(priority, btype))
        {
            kerror(("cmem(membpa_alloc_pages): error in membpa_init2\n"));
            return(0);
        }
    }

    if (btype == TYPE_GFPBPA)
    {
        kdebug(("cmem(membpa_alloc_pages): Memory will be allocated from the GFP pool at 0x%016lx\n", (u_long)&gfpbpafree_list));
        range_ptr = &gfpbpafree_list;
    }
    else if (btype == TYPE_MEMBPA)
    {
        kdebug(("cmem(membpa_alloc_pages): Memory will be allocated from the MEM pool\n"));
        range_ptr = &membpafree_list;
    }
    else
    {
        kerror(("cmem(membpa_alloc_pages): ERROR: btype = %d\n", btype));
        return(0);
    }

    kdebug(("cmem(membpa_alloc_pages): range_ptr is at 0x%016lx\n", (u_long)range_ptr));

    new_range   = NULL;
    align_range = NULL;

    if (align == 0)
        align = PAGE_SIZE;
    else
        align = align * PAGE_SIZE;

    kdebug(("cmem(membpa_alloc_pages): align = %d\n", align));

    // Search a free block which is large enough, even with alignment.
    while (*range_ptr != NULL)
    {
        range = *range_ptr;
        aligned_base = (caddr_t)((((u_long)range->base + align - 1) / align) * align);
        if (aligned_base + count * PAGE_SIZE <= range->base + range->size)
            break;
        range_ptr = &range->next;
    }

    if (*range_ptr == NULL)
    {
        kerror(("cmem(membpa_alloc_pages): ERROR: *range_ptr is NULL\n"));
        return(0);
    }

    range = *range_ptr;
    // When we have to align, the pages needed for alignment can
    // be put back to the free pool.
    // We check here if we need a second range data structure later
    // and allocate it now, so that we don't have to check for a
    // failed kmalloc later.

    if (aligned_base - range->base + count * PAGE_SIZE < range->size)
    {
        new_range = (range_t *)kmalloc(sizeof(range_t), priority);
        if (new_range == NULL)
        {
            kerror(("cmem(membpa_alloc_pages): ERROR: new_range is NULL\n"));
            return(0);
        }
    }

    if (aligned_base != range->base)
    {
        align_range = (range_t *)kmalloc(sizeof(range_t), priority);
        if (align_range == NULL)
        {
            if (new_range != NULL)
                kfree(new_range);
            {
                kerror(("cmem(membpa_alloc_pages): ERROR: align_range is NULL\n"));
                return(0);
            }
        }

        align_range->base = range->base;
        align_range->size = aligned_base - range->base;
        range->base = aligned_base;
        range->size -= align_range->size;
        align_range->next = range;
        *range_ptr = align_range;
        range_ptr = &align_range->next;
    }

    if (new_range != NULL)
    {
        // Range is larger than needed, create a new list element for
        // the used list and shrink the element in the free list.
        new_range->base = range->base;
        new_range->size = count * PAGE_SIZE;
        range->base = new_range->base + new_range->size;
        range->size = range->size - new_range->size;
    }
    else
    {
        // Range fits perfectly, remove it from free list.
        *range_ptr = range->next;
        new_range = range;
    }
    // Insert block into used list

    if (btype == TYPE_GFPBPA)
    {
        new_range->next = gfpbpaused_list;
        gfpbpaused_list = new_range;
    }
    else
    {
        new_range->next = membpaused_list;
        membpaused_list = new_range;
    }

    return new_range->base;
}


/******************************************************/
static void membpa_free_pages(caddr_t base, u_int btype)
/******************************************************/
{
    range_t *prev, *next, *range, **range_ptr;

    kdebug(("cmem(membpa_free_pages): called with base = 0x%016lx and btype = %d\n", (u_long)base, btype));

    if((btype != TYPE_GFPBPA) && (btype != TYPE_MEMBPA))
    {
        kerror(("cmem(membpa_free_pages): ERROR: btype is %d\n", btype));
        return;
    }

    // Search the block in the used list.
    if (btype == TYPE_GFPBPA)
        range_ptr = &gfpbpaused_list;
    else
        range_ptr = &membpaused_list;
    for (; *range_ptr != NULL; range_ptr = &(*range_ptr)->next)
        if ((*range_ptr)->base == base)
            break;

    if (*range_ptr == NULL)
    {
        kerror(("cmem(membpa_free_pages): membpa_free_pages(0x%016lx), not allocated!\n", (u_long)base));
        return;
    }
    range = *range_ptr;

    // Remove range from the used list:
    *range_ptr = (*range_ptr)->next;

    // The free-list is sorted by address, search insertion point and insert block in free list.
    if (btype == TYPE_GFPBPA)
        range_ptr = &gfpbpafree_list;
    else
        range_ptr = &membpafree_list;
    for (prev = NULL; *range_ptr != NULL; prev = *range_ptr, range_ptr = &(*range_ptr)->next)
        if ((*range_ptr)->base >= base)
            break;

    range->next = *range_ptr;
    *range_ptr  = range;

    // Concatenate free range with neighbors, if possible.
    // Try for upper neighbor (next in list) first, then for lower neighbor (predecessor in list).
    if (range->next != NULL && range->base + range->size == range->next->base)
    {
        next = range->next;
        range->size += range->next->size;
        range->next = next->next;
        kfree(next);
    }

    if (prev != NULL && prev->base + prev->size == range->base)
    {
        prev->size += prev->next->size;
        prev->next = range->next;
        kfree(range);
    }
}


/**************************/
static int gfpbpa_init(void)
/**************************/
{
    u_char *page_pool;
    int min_index, max_index;
    u_int limit_32, pcnt = 0, block_found = 0, block_start = 0, max_chunk_sum, chunk_sum, chunks_required, pool_index, loop, loop2, chunk_shift, chunk_mask;
    u_long paddr, kaddr;

    page_pool = (u_char *)vmalloc(MAX_GFPBPA_SIZE); //support at most 16 GB of RAM
    if (page_pool == NULL)
    {
        kerror(("cmem(gfpbpa_init): page_pool is NULL\n"));
        return(1);
    }
    for (loop = 0; loop < MAX_GFPBPA_SIZE; loop++)
        page_pool[loop] = 0;                     //Set all pool entries to "no memory allocated"

    if (gfpbpa_quantum == 1) {chunk_shift = 20; chunk_mask = 0x0fffff;}
    if (gfpbpa_quantum == 2) {chunk_shift = 21; chunk_mask = 0x1fffff;}
    if (gfpbpa_quantum == 4) {chunk_shift = 22; chunk_mask = 0x3fffff;}
    if (gfpbpa_quantum == 8) {chunk_shift = 23; chunk_mask = 0x7fffff;}
    kdebug(("cmem(gfpbpa_init): gfpbpa_quantum = %d\n", gfpbpa_quantum));
    kdebug(("cmem(gfpbpa_init): chunk_shift    = %d\n", chunk_shift));
    chunks_required = gfpbpa_size / gfpbpa_quantum;
    kdebug(("cmem(gfpbpa_init): chunks_required = %d\n", chunks_required));

    limit_32 = (4096 / gfpbpa_quantum) - 1;
    kdebug(("cmem(gfpbpa_init): limit_32 = %d\n", limit_32));

    if (gfpbpa_quantum != 1 && gfpbpa_quantum != 2 && gfpbpa_quantum != 4 && gfpbpa_quantum != 8)
    {
        kerror(("cmem(gfpbpa_init): gfpbpa_quantum is not 1, 2, 4 or 8\n"));
        vfree((void *)page_pool);
        return(1);
    }

    if (gfpbpa_size % gfpbpa_quantum)
    {
        kerror(("cmem(gfpbpa_init): gfpbpa_size is not a multiple of gfpbpa_quantum\n"));
        vfree((void *)page_pool);
        return(1);
    }

    kdebug(("cmem(gfpbpa_init): Trying to allocate a contiguous buffer of %ld MB in pieces of %d MB\n", gfpbpa_size, gfpbpa_quantum));
    gfpbpa_order = get_order(gfpbpa_quantum * 1024 * 1024);

    //Allocate pages until we have foud a large enough contiguous buffer
    while(pcnt < MAX_GFPBPA_SIZE)
    {
        kdebug(("cmem(gfpbpa_init): Calling __get_free_pages with order = %d\n", gfpbpa_order));
        kaddr = __get_free_pages(GFP_KERNEL, gfpbpa_order);
        if (!kaddr)
        {
            kdebug(("cmem(gfpbpa_init): Failed to allocate a buffer\n"));
            break;
        }

        paddr = virt_to_bus((void *)kaddr);
        kdebug(("cmem(gfpbpa_init): Got buffer @ physical address 0x%016lx (pcnt = %d, kaddr = 0x%016lx)\n", paddr, pcnt, kaddr));

        if (paddr & chunk_mask)
        {
            kerror(("cmem(gfpbpa_init): paddr (0x%016lx) is not properly aligned to chunk_mask = 0x%08x\n", paddr, chunk_mask));
            vfree((void *)page_pool);
            return(1);
        }

        kdebug(("cmem(gfpbpa_init): paddr = 0x%016lx\n", paddr));
        pool_index = paddr >> chunk_shift;
        kdebug(("cmem(gfpbpa_init): pool_index = 0x%08x\n", pool_index));

        if (pool_index > (MAX_GFPBPA_SIZE - 1))
        {
            kerror(("cmem(gfpbpa_init): pool_index (0x%08x) out of range. You seem to have more than 16 GB of RAM\n", pool_index));

            for(loop = 0; loop < MAX_GFPBPA_SIZE; loop++)   //return memory allocated so far
            {
                if (page_pool[loop])
                {
                    paddr = (u_long)loop << chunk_shift;
                    kaddr = (u_long)bus_to_virt(paddr);
                    kdebug(("cmem(gfpbpa_init): Returning index %d, paddr = 0x%016lx (kaddr = 0x%016lx)\n", loop, paddr, kaddr));
                    free_pages(kaddr, gfpbpa_order);
                }
            }

            vfree((void *)page_pool);
            return(1);
        }
        page_pool[pool_index] = 1;

        min_index = pool_index - chunks_required + 1;
        if (min_index < 0)
            min_index = 0;

        max_index = pool_index + chunks_required - 1;
        if (max_index > MAX_GFPBPA_SIZE)
            max_index = MAX_GFPBPA_SIZE;

        if (gfpbpa_zone) //we want memory in the 32bit range
        {
            if (max_index > limit_32)
                max_index = limit_32;
        }

        kdebug(("cmem(gfpbpa_init): max_index = %d, min_index = %d\n", max_index, min_index));
        if (max_index > min_index)
        {
            max_chunk_sum = 0;
            for (loop = min_index; loop < max_index; loop++)
            {
                chunk_sum = 0;
                for (loop2 = 0; loop2 < chunks_required; loop2++)
                    chunk_sum += page_pool[loop + loop2];

                //kdebug(("cmem(gfpbpa_init): chunk_sum = %d\n", chunk_sum));
                if (chunk_sum > max_chunk_sum);
                max_chunk_sum = chunk_sum;

                if (chunk_sum == chunks_required)
                {
                    kdebug(("cmem(gfpbpa_init): BLOCK FOUND\n"));
                    block_found = 1;
                    block_start = loop;
                    kdebug(("cmem(gfpbpa_init): Range = %d to %d\n", block_start, block_start + chunks_required - 1));
                    for (loop2 = 0; loop2 < MAX_GFPBPA_SIZE; loop2++)
                        if (page_pool[loop2] == 1)
                            kdebug(("cmem(gfpbpa_init): index %d is set\n", loop2));

                    break;
                }
            }
            kdebug(("cmem(gfpbpa_init): max_chunk_sum = %d\n", max_chunk_sum));
        }

        if (block_found)
        {
            kdebug(("cmem(gfpbpa_init): BLOCK FOUND 2\n"));
            break;
        }

        pcnt++;
    }

    //Return the pages that we don't use
    if (block_found == 0)
    {
        kdebug(("cmem(gfpbpa_init): No buffer found. Returning all pages\n"));
        for(loop = 0; loop < MAX_GFPBPA_SIZE; loop++)   //return memory allocated so far
        {
            if (page_pool[loop])
            {
                paddr = (u_long)loop << chunk_shift;
                kaddr = (u_long)bus_to_virt(paddr);
                kdebug(("cmem(gfpbpa_init): Returning index %d, paddr = 0x%016lx, kaddr = 0x%016lx)\n", loop, paddr, kaddr));
                free_pages(kaddr, gfpbpa_order);
            }
        }
    }
    else
    {
        kdebug(("cmem(gfpbpa_init): Buffer found. Returning unused pages\n"));
        for(loop = 0; loop < MAX_GFPBPA_SIZE; loop++)   //return memory allocated so far
        {
            if (page_pool[loop] && (loop < block_start || loop > (block_start + chunks_required - 1)))
            {
                paddr = (u_long)loop << chunk_shift;
                kaddr = (u_long)bus_to_virt(paddr);
                kdebug(("cmem(gfpbpa_init): Returning index %d, paddr = 0x%016lx, kaddr = 0x%016lx)\n", loop, paddr, kaddr));
                free_pages(kaddr, gfpbpa_order);
            }
        }
    }

    if(block_found)
    {
        kdebug(("cmem(gfpbpa_init): Job done. Now copying used pages\n"));
        gfpbpa_num_pages = chunks_required;
        //Copy the pages we want to keep to the global array
        for(loop = 0; loop < chunks_required ; loop++)
        {
            paddr = (u_long)(loop + block_start) << chunk_shift;
            kaddr = (u_long)bus_to_virt(paddr);
            kdebug(("cmem(gfpbpa_init): Keep page %d with kaddr = 0x%016lx, paddr = 0x%016lx\n", loop + block_start, kaddr, paddr));
            gfpbpa_array[loop] = kaddr;

            if (loop == 0)
                gfpbpa_base = paddr;
        }

        gfpbpa = gfpbpa_array[0];
        kdebug(("cmem(gfpbpa_init): End of function\n"));
        vfree((void *)page_pool);
        return(0);
    }
    else
    {
        kerror(("cmem(gfpbpa_init): No suitable buffer found\n"));
        gfpbpa_num_pages = 0;
        kdebug(("cmem(gfpbpa_init): End of function\n"));
        vfree((void *)page_pool);
        return(1);
    }
}


















