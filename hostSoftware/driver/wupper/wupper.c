/**
  *    ------------------------------------------------------------------------------
  *                                                              
  *            NIKHEF - National Institute for Subatomic Physics 
  *  
  *                        Electronics Department                
  *                                                              
  *  ----------------------------------------------------------------------------
  *  @class wupper driver
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
  *  @brief Original version (RobinNP driver) by Barry Green, Will Panduro (RHUL),
  *  Gordon Crone (UCL), Markus Joos (CERN)
  *  Adapted for WUPPER by Jos Vermeulen (Nikhef), Jan. 2015"); 
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

#include "../include/wupper_common.h"

/********/
/*Macros*/
/********/
#define DRIVER_DEBUG
#define DRIVER_START_STOP_DEBUG

#ifdef DRIVER_START_STOP_DEBUG
#define kdebugStartStop(x) {printk x;}
#else
#define kdebugStartStop(x)
#endif

#define DRIVER_ERROR

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

/***********/
/*Constants*/
/***********/
#define PROC_MAX_CHARS          0x10000      //The max. length of the output of /proc/wupper
#define PCI_VENDOR_ID_WUPPER     0x10ee
#define PCI_DEVICE_ID_WUPPER     0x7039
#define PCI_VENDOR_ID_WUPPER709  0x4444
#define PCI_DEVICE_ID_WUPPER709  0x4444
#define FIRSTMINOR              0

#define MAXMSI				    8   // Max. number of interrupts (MSI-X) per card

/********************/
/*driver error codes*/
/********************/
#define RD_KMALLOC 1
#define RD_CFU     3
#define RD_PROC    8


/************/
/*Prototypes*/
/************/
static int wupper_init(void);
static int wupper_Probe(struct pci_dev*, const struct pci_device_id*);
static int fill_proc_read_text(void);
static ssize_t wupper_write_procmem(struct file *file, const char *buffer, size_t count, loff_t *startOffset);
static ssize_t wupper_read_procmem(struct file *file, char *buf, size_t count, loff_t *startOffset);
static void wupper_exit(void);
static void wupper_Remove(struct pci_dev*);
int wupper_mmap(struct file*, struct vm_area_struct*);
static long wupper_ioctl(struct file *file, u_int cmd, u_long arg);
int wupper_open(struct inode*, struct file*);
int wupper_Release(struct inode*, struct file*);
int check_slot_device(u_int);
void wupper_vmclose(struct vm_area_struct*);

/************/
/*Structures*/
/************/
static struct pci_device_id WUPPER_IDs[] =
{
    { PCI_DEVICE(PCI_VENDOR_ID_WUPPER, PCI_DEVICE_ID_WUPPER) },
    { PCI_DEVICE(PCI_VENDOR_ID_WUPPER709, PCI_DEVICE_ID_WUPPER709) },
    { 0, },
};

struct file_operations fops =
{
    .owner          = THIS_MODULE,
    .mmap           = wupper_mmap,
    .unlocked_ioctl = wupper_ioctl,
    .open           = wupper_open,
    .read           = wupper_read_procmem,
    .write          = wupper_write_procmem,
    .release        = wupper_Release,
};

// needed by pci_register_driver fcall
static struct pci_driver wupper_PCI_driver =
{
        .name     = "wupper",
        .id_table = WUPPER_IDs,
        .probe    = wupper_Probe,
        .remove   = wupper_Remove,
};

// memory handler functions used by mmap
static struct vm_operations_struct wupper_vm_ops =
{
        .close =  wupper_vmclose,             // mmap-close
};

struct irqInfo_struct
{
  int interrupt;
  int card;
  int reserved;
};

typedef struct{
    uint32_t dummy[2];
} Dummy64Struct;


typedef struct {
    Dummy64Struct Dummy0;
} BAR0CommonStruct;


/*********/
/*Globals*/
/*********/
char *devName = "wupper";  //the device name as it will appear in /proc/devices
static char *proc_read_text;
static int debug = 1, errorlog = 1;
int cardsFound = 0, interruptCount = 0;
int msiblock = 8, irqFlag[MAXCARDS][MAXMSI] = {{0}}, msiStatus[MAXCARDS];
unsigned int irqCount[MAXCARDS][MAXMSI];
int irqMasked[MAXCARDS][MAXMSI];
uint64_t *memoryBase[MAXCARDS];
card_params_t cards[MAXCARDS];
struct cdev *test_cdev;
dev_t first_dev;
BAR0CommonStruct *registerBase[MAXCARDS];
uint32_t* msixBar[MAXCARDS], msixPbaOffset[MAXCARDS];
static struct irqInfo_struct irqInfo[MAXCARDS][MAXMSI];
// refclk_cfg_t cfg;
struct file* owner[MAXCARDS];

static DECLARE_WAIT_QUEUE_HEAD(waitQueue);
#ifdef MUTEX_ORG
static DECLARE_MUTEX(procMutex);
static DECLARE_MUTEX(ownerMutex);
#else
static DEFINE_MUTEX(procMutex);
static DEFINE_MUTEX(ownerMutex);
#endif

module_init(wupper_init);
module_exit(wupper_exit);

MODULE_DESCRIPTION("WUPPER driver");
MODULE_AUTHOR("Original version (RobinNP driver) Barry Green , Will Panduro (RHUL), Gordon Crone (UCL), Markus Joos (CERN), adapted for WUPPER by Jos Vermeulen (Nikhef)");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DEVICE_TABLE(pci, WUPPER_IDs);

MODULE_PARM_DESC(msiblock, " size of MSI block to enable");
module_param(msiblock,int,S_IRUGO);

MODULE_PARM_DESC(debug, "1 = enable debugging   0 = disable debugging");
module_param (debug, int, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(errorlog, "1 = enable debugging   0 = disable debugging");
module_param (errorlog, int, S_IRUGO | S_IWUSR);


#define USE_MSIX
struct msix_entry msixTable[MAXCARDS][MAXMSI];

/*************************/
static int wupper_init(void)
/*************************/
{
    int stat, cardNumber, major, interrupt;
    struct proc_dir_entry* procDir;

    for (cardNumber = 0; cardNumber < MAXCARDS; cardNumber++)
    {
        cards[cardNumber].pciDevice = NULL;
        registerBase[cardNumber] = NULL;
        memoryBase[cardNumber] = NULL;
        for (interrupt = 0; interrupt < MAXMSI; interrupt++)
        {
            irqCount[cardNumber][interrupt] = 0;
            irqMasked[cardNumber][interrupt] = 0;
        }
        owner[cardNumber]=NULL;
    }

    if (msiblock>MAXMSI)
    {
        printk(KERN_ALERT "wupper(wupper_init):msiblock > MAXMSI - setting to max (%d)\n", MAXMSI);
        msiblock=MAXMSI;
    }


    kdebugStartStop(("wupper(wupper_init): registering PCIDriver \n"));
    stat=pci_register_driver(&wupper_PCI_driver);
    if (stat < 0) {
        printk(KERN_ALERT "wupper(wupper_init): Status %d from pci_register_driver\n", stat);
        return stat;
    }

    stat=alloc_chrdev_region(&first_dev, FIRSTMINOR, MAXCARDS, devName);
    if (stat == 0)
    {
        major = MAJOR(first_dev);
        kdebugStartStop((KERN_ALERT "wupper(wupper_init): major number is %d\n", major));

        test_cdev = cdev_alloc();
        test_cdev->ops = &fops;
        test_cdev->owner = THIS_MODULE;
        stat=cdev_add(test_cdev, first_dev, 1);
        if (stat == 0)
        {
            procDir = proc_create(devName, 0644, NULL, &fops);
            if (procDir == NULL)
            {
                kerror(("wupper(wupper_init): error from call to create_proc_entry\n"));
                return(-RD_PROC);
            }
        }
        else
        {
            printk(KERN_ALERT "wupper(wupper_init): cdev_add failed, driver will not load\n");
            unregister_chrdev_region(first_dev, MAXCARDS);
            pci_unregister_driver(&wupper_PCI_driver);
            return(stat);
        }
    }
    else
    {
        kerror(("wupper_init: registering WUPPER driver failed.\n"));
        pci_unregister_driver(&wupper_PCI_driver);
        return(stat);
    }

    proc_read_text = (char *)kmalloc(PROC_MAX_CHARS, GFP_KERNEL);
    if (proc_read_text == NULL)
    {
        kerror(("wupper_init: error from kmalloc\n"));
        return(-RD_KMALLOC);
    }

    printk(KERN_INFO "wupper(wupper_init): WUPPER driver loaded, found %d device(s)\n", cardsFound);
    return 0;
}

/***************************/
static void wupper_exit(void)
/***************************/
{
    remove_proc_entry(devName, NULL /* parent dir */);
    kdebugStartStop(("wupper(wupper_exit): unregister device\n"));
    unregister_chrdev_region(first_dev,MAXCARDS);
    kdebugStartStop(("wupper(wupper_exit: unregister driver\n"));
    pci_unregister_driver(&wupper_PCI_driver);
    cdev_del(test_cdev);
    kfree(proc_read_text);
    printk(KERN_INFO "wupper(wupper__exit): driver removed\n");
}


/**********************************************/
static irqreturn_t irqHandler(int irq, void* dev
#if LINUX_VERSION_CODE==KERNEL_VERSION(2,6,18)
        , struct pt_regs* regs
#endif
)
/**********************************************/

{
    struct irqInfo_struct *info;

    info=(struct irqInfo_struct*) dev;
    irqCount[info->card][info->interrupt] += 1;
    irqFlag[info->card][info->interrupt] = 1;
    wake_up_interruptible(&waitQueue);
    return IRQ_HANDLED;
}

/**************************************************************************/
static int wupper_Probe(struct pci_dev *dev, const struct pci_device_id *id)
/**************************************************************************/
{
    int cardNumber, ret, bufferNumber;
    int interrupt, msixCapOffset, msixData, msixBarNumber, msixTableOffset, msixLength;
    uint32_t msixAddress;
    BAR0CommonStruct *registers;

    cardNumber = 0;
    // Find first available slot
    while (cards[cardNumber].pciDevice != NULL && cardNumber<MAXCARDS)
    {
        cardNumber++;
    }
    if (cardNumber<MAXCARDS) {
        kdebug(("wupper(wupper_Probe): Initialising device nr %d (counting from 0)\n", cardsFound));
        ret = pci_enable_device(dev);
        cardsFound++;
        cards[cardNumber].pciDevice = dev;
    }
    else
    {
        printk(KERN_ALERT "wupper(wupper_Probe): Too many cards present, only %d is allowed\n", MAXCARDS);
        return -1;
    }

    kdebug(("wupper(wupper_Probe): Reading configuration space for card %d :\n", cardNumber));
    cards[cardNumber].baseAddressBAR0 = pci_resource_start(dev, 0);
    cards[cardNumber].sizeBAR0        = pci_resource_len(dev, 0);
    cards[cardNumber].baseAddressBAR1 = pci_resource_start(dev, 1);
    cards[cardNumber].sizeBAR1        = pci_resource_len(dev, 1);
    cards[cardNumber].baseAddressBAR2 = pci_resource_start(dev, 2);
    cards[cardNumber].sizeBAR2        = pci_resource_len(dev, 2);

    kdebug(("wupper(wupper_Probe): ----> BAR0 start %x, end %x, size %x \n",
            cards[cardNumber].baseAddressBAR0,
            (unsigned int)pci_resource_end(dev,0),
            cards[cardNumber].sizeBAR0));
    kdebug(("wupper(wupper_Probe): ----> BAR1 start %x, end %x, size %x \n",
            cards[cardNumber].baseAddressBAR1,
            (unsigned int)pci_resource_end(dev,1),
            cards[cardNumber].sizeBAR1));
    /* kdebug(("wupper(wupper_Probe): ----> BAR2 start %x, end %x, size %x \n",
            cards[cardNumber].baseAddressBAR2,
            (unsigned int)pci_resource_end(dev,2),
            cards[cardNumber].sizeBAR2)); */

    /* remap the whole bar0 as registers */
    kdebug(("wupper(wupper_Probe): Remapping BAR0 as registers space, card %d, phys add/size: 0x%x 0x%x\n",
            cardNumber, cards[cardNumber].baseAddressBAR0, cards[cardNumber].sizeBAR0));
    registerBase[cardNumber] = ioremap_nocache(cards[cardNumber].baseAddressBAR0, cards[cardNumber].sizeBAR0);
    registers = registerBase[cardNumber];

    msixCapOffset = pci_find_capability(dev,PCI_CAP_ID_MSIX);
    if (msixCapOffset == 0)
    {
        // card may not have wupper hardware loaded
        printk(KERN_ALERT "wupper(wupper_Probe): Failed to map MSI-X BAR for card %d\n",cardNumber);
        msixBar[cardNumber] = NULL;
        return -1;
    }

    // pci_read_config_dword(dev, msixCapOffset, &msixData);
    // kdebug(("wupper(wupper_Probe): MSIX Capability structure first word %08x\n", msixData));
    pci_read_config_dword(dev, msixCapOffset + 4, &msixData);
    msixBarNumber = msixData & 0xf;
    msixTableOffset = msixData & 0xfffffff0;
    kdebug(("wupper(wupper_Probe): MSIX Vector table BAR %d, offset %08x\n", msixBarNumber, msixTableOffset));
    pci_read_config_dword(dev, msixCapOffset + 8, &msixData);
    msixBarNumber = msixData & 0xf;
    msixPbaOffset[cardNumber] = msixData & 0xfffffff0;
    kdebug(("wupper(wupper_Probe): MSIX PBA          BAR %d, offset %08x\n", msixBarNumber, msixPbaOffset[cardNumber]));
    msixAddress = pci_resource_start(dev, msixBarNumber);
    msixLength = pci_resource_len(dev, msixBarNumber);
    msixBar[cardNumber] = ioremap_nocache(msixAddress, msixLength);

    if (msixBar[cardNumber] == NULL)
    {
        printk(KERN_ALERT "wupper(wupper_Probe): Failed to map MSI-X BAR\n for card %d\n",cardNumber);
        return -1;
    }

    if (debug)
    {
        // kdebug(("wupper(wupper_Probe): msix address %08x, length %4x\n", msixAddress, msixLength));
        bufferNumber = msixTableOffset / sizeof(uint32_t);
        for (interrupt = 0; interrupt < 8 /*msiblock*/ ; interrupt++)
        {
            kdebug(("wupper(wupper_Probe): MSI-X table[%d] %08x %08x  %08x  %08x\n",
                    interrupt, msixBar[cardNumber][bufferNumber], msixBar[cardNumber][bufferNumber+1], msixBar[cardNumber][bufferNumber+2], msixBar[cardNumber][bufferNumber+3]));
            bufferNumber += 4;
        }

        if (msixPbaOffset[cardNumber]+3*sizeof(uint32_t)<msixLength)
        {
            kdebug(("wupper(wupper_Probe): MSI-X PBA      %08x %08x  %08x  %08x\n",
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t)],
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t) + 1],
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t) + 2],
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t) + 3]));
        }
        else
        {
            printk(KERN_ALERT "wupper(wupper_Probe): PBA offset %x is outside of BAR%d, length=%x \n",
                   msixPbaOffset[cardNumber], msixBarNumber, msixLength);
        }
    }

    // setup interrupts
    for (interrupt = 0; interrupt < msiblock; interrupt++)
    {
        msixTable[cardNumber][interrupt].entry = interrupt;
        kdebug(("wupper(wupper_Probe): filling interrupt table for interrupt %d, cardnumber %d\n", interrupt, cardNumber));
    }

    for (interrupt = 0; interrupt < msiblock; interrupt++)
    {
        kdebug(("wupper(wupper_Probe): entry in table %d\n", msixTable[cardNumber][interrupt].entry));
    }

//    if (debug)
//    {
//        kdebug(("wupper(wupper_Probe): msix address %08x, length %4x\n", msixAddress,msixLength));
//        bufferNumber = msixTableOffset / sizeof(uint32_t);
//        for (interrupt = 0; interrupt < msiblock; interrupt++) {
//            kdebug(("wupper(wupper_Probe): MSI-X table[%d] %08x %08x  %08x  %08x\n",
//                    interrupt, msixBar[cardNumber][bufferNumber], msixBar[cardNumber][bufferNumber+1], msixBar[cardNumber][bufferNumber+2], msixBar[cardNumber][bufferNumber+3]));
//            bufferNumber += 4;
//        }

//        if (msixPbaOffset[cardNumber] + 3 * sizeof(uint32_t) < msixLength) {
//            kdebug(("wupper(wupper_Probe): MSI-X PBA %08x \n",
//                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t)]));
//        }
//        else
//        {
//            printk(KERN_ALERT "wupper(wupper_Probe): PBA offset %x is outside of BAR%d, length=%x \n",
//                   msixPbaOffset[cardNumber], msixBarNumber, msixLength);
//        }
//    }

//    kdebug(("Before pci_enable_msix\n"));
    msiStatus[cardNumber] = pci_enable_msix(dev, msixTable[cardNumber], msiblock);
    kdebug(("After pci_enable_msix\n"));

    if (debug)
    {
        kdebug(("wupper(wupper_Probe): msix address %08x, length %4x\n", msixAddress,msixLength));
        bufferNumber = msixTableOffset / sizeof(uint32_t);
        for (interrupt = 0; interrupt < msiblock; interrupt++)
        {
            kdebug(("wupper(wupper_Probe): MSI-X table[%d] %08x %08x  %08x  %08x\n",
                    interrupt, msixBar[cardNumber][bufferNumber], msixBar[cardNumber][bufferNumber+1], msixBar[cardNumber][bufferNumber+2], msixBar[cardNumber][bufferNumber+3]));
            bufferNumber += 4;
        }
        if (msixPbaOffset[cardNumber] + 3 * sizeof(uint32_t) < msixLength)
        {
            kdebug(("wupper(wupper_Probe): MSI-X PBA %08x \n",
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t)]));
        }
        else
        {
            printk(KERN_ALERT "wupper(wupper_Probe): PBA offset %x is outside of BAR%d, length=%x \n",
                   msixPbaOffset[cardNumber], msixBarNumber, msixLength);
        }
    }

    if (msiStatus[cardNumber] != 0)
    {
        printk(KERN_ALERT
               "wupper(wupper_Probe): Failed to enable MSI-X interrupt block for card %d, enable returned %d\n",
               cardNumber, msiStatus[cardNumber]);
    }
    else
    {
        for (interrupt = 0; interrupt < msiblock; interrupt++)
        {
            kdebug(("wupper(wupper_Probe): Trying to register IRQ %d\n", msixTable[cardNumber][interrupt].vector));

            irqInfo[cardNumber][interrupt].interrupt = interrupt;
            irqInfo[cardNumber][interrupt].card      = cardNumber;
            irqInfo[cardNumber][interrupt].reserved    = 0;
            ret = request_irq(msixTable[cardNumber][interrupt].vector,
                              irqHandler,
                              0, devName,
                              &irqInfo[cardNumber][interrupt]);
            if (ret != 0)
            {
                printk(KERN_ALERT "wupper(wupper_Probe): Failed to register interrupt handler for MSI %d\n", interrupt);
            }
        }
    }


    // do reset

    if (debug)
    {
        kdebug(("wupper(wupper_Probe): msix address %08x, length %4x\n", msixAddress, msixLength));
        bufferNumber = msixTableOffset / sizeof(uint32_t);
        for (interrupt = 0; interrupt < msiblock; interrupt++)
        {
            kdebug(("wupper(wupper_Probe): MSI-X table[%d] %08x %08x  %08x  %08x\n",
                    interrupt, msixBar[cardNumber][bufferNumber], msixBar[cardNumber][bufferNumber+1], msixBar[cardNumber][bufferNumber+2], msixBar[cardNumber][bufferNumber+3]));
            bufferNumber += 4;
        }

        if (msixPbaOffset[cardNumber] + 3 * sizeof(uint32_t) < msixLength)
        {
            kdebug(("wupper(wupper_Probe): MSI-X PBA %08x %08x  %08x  %08x\n",
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t)],
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t) + 1],
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t) + 2],
                    msixBar[cardNumber][msixPbaOffset[cardNumber] / sizeof(uint32_t) + 3]));
        }
        else
        {
            printk(KERN_ALERT "wupper(wupper_Probe): PBA offset %x is outside of BAR%d, length=%x \n", msixPbaOffset[cardNumber], msixBarNumber, msixLength);
        }
    }

    // other initialization ...
    return 0;
}

/*******************************************/
static void wupper_Remove(struct pci_dev *dev)
/*******************************************/
{
    int cardNumber, interrupt;

    printk(KERN_ALERT "wupper(wupper_Remove):  called\n");
    for(cardNumber = 0; cardNumber < MAXCARDS; cardNumber++ )
    {
        if (cards[cardNumber].pciDevice == dev)
        {
            printk(KERN_ALERT "wupper(wupper_Remove): for card %d\n", cardNumber);
            cards[cardNumber].pciDevice = NULL;
            owner[cardNumber]=NULL;
            cardsFound--;

            if (msiStatus[cardNumber] == 0)
            {
                for (interrupt = 0; interrupt < msiblock; interrupt++)
                {
                    kdebug(("wupper(wupper_Remove): unregestering interrupt %d, vector %d\n", interrupt, msixTable[cardNumber][interrupt].vector));
                    free_irq(msixTable[cardNumber][interrupt].vector, &irqInfo[cardNumber][interrupt]);
                }
            }
            pci_disable_msix(dev);
        }
    }
}

/**************************************************/
int wupper_open(struct inode *ino, struct file *file)
/**************************************************/
{
    card_params_t *pdata;

    kdebug(("wupper(wupper_open): called\n"));
    pdata = (card_params_t *)kmalloc(sizeof(card_params_t), GFP_KERNEL);
    if (pdata == NULL)
    {
        kerror(("wupper(wupper_open): error from kmalloc\n"))
                return(-RD_KMALLOC);
    }
    pdata->slot = 0;
    file->private_data = (char *)pdata;
    return 0;
}

/******************************************************/
int wupper_Release(struct inode *ino, struct file *file)
/******************************************************/
{
    card_params_t *pdata;
    int card;

    kdebug(("wupper(wupper_release): called\n"));
    for (card=0; card<MAXCARDS; card++)
    {
        if (owner[card]==file)
        {
            owner[card]=0;
        }
    }
    pdata = (card_params_t *)file->private_data;
    kdebug(("wupper(wupper_release): Releasing orphaned resources for slot %d\n", pdata->slot))
    kfree(file->private_data);
    return 0;
}

/*******************************/
int check_slot_device(u_int slot)
/*******************************/
{
    if (slot >= MAXCARDS)
    {
        printk(KERN_ALERT "wupper(check_slot_device): Invalid (%d) slot number\n", slot);
        return -1;
    }
    if (cards[slot].pciDevice == NULL)
    {
        printk(KERN_ALERT "wupper(check_slot_device): No card at this (%d) slot!\n", slot);
        return -1;
    }
    return 0;
}

/************************************************************************************************/
static ssize_t wupper_read_procmem(struct file *file, char *buf, size_t count, loff_t *startOffset)
/************************************************************************************************/
{
    int nchars = 0, loop1;
    static int len = 0;
    long offset = *startOffset;

    kdebug(("wupper(wupper_read_proc): Called with buf    = 0x%016lx\n", (u_long)buf));
    kdebug(("wupper(wupper_read_proc): Called with offset = %ld\n", offset));
    kdebug(("wupper(wupper_read_proc): Called with count  = %d\n", (int) count));

#ifdef MUTEX_ORG
    if (down_interruptible(&procMutex))
#else
    if (mutex_lock_interruptible(&procMutex))
#endif
    {
        return 0;
    }

    if (offset == 0) len = fill_proc_read_text();
    kdebug(("wupper(wupper_read_proc): len = %d\n", len));

    if (count < (len - offset))
    {
        nchars = count;
    }
    else
    {
        nchars = len - offset;
    }
    kdebug(("wupper(wupper_read_proc): min nchars         = %d\n", nchars));
    kdebug(("wupper(wupper_read_proc): position           = %ld\n", (offset & (PAGE_SIZE - 1))));

    if (nchars > 0)
    {
        for (loop1 = 0; loop1 < nchars; loop1++)
        {
            buf[loop1 + (offset & (PAGE_SIZE - 1))] = proc_read_text[offset + loop1];
        }
        *startOffset = len + (offset & (PAGE_SIZE - 1));
    }
    else
    {
        nchars = 0;
    }
    kdebug(("wupper(wupper_read_proc): returning *start   = 0x%016lx\n", (u_long)*startOffset));
    kdebug(("wupper(wupper_read_proc): returning nchars   = %d\n", nchars));
#ifdef MUTEX_ORG
    up(&procMutex);
#else
    mutex_unlock(&procMutex);
#endif

    return(nchars);
}

/**********************************************************************************************************/
static ssize_t wupper_write_procmem(struct file *file, const char *buffer, size_t count, loff_t *startOffset)
/**********************************************************************************************************/
{
  int len;
  char textReceived[100];

  kdebug(("wupper(wupper_write_proc): robin_write_procmem called\n"));

  if (count > 99) len = 99;
  else len = count;

  if (copy_from_user(textReceived, buffer, len))
  {
    kerror(("wupper(wupper_write_proc): error from copy_from_user\n"));
    return(-RD_CFU);
  }
  kdebug(("wupper(wupper_write_proc): len = %d\n", len));
  textReceived[len - 1] = '\0';
  kdebug(("wupper(wupper_write_proc): text passed = %s\n", textReceived));

  if (!strcmp(textReceived, "debug"))
  {
    debug = 1;
    kdebug(("wupper(wupper_write_proc): debugging enabled\n"));
  }
  if (!strcmp(textReceived, "nodebug"))
  {
    kdebug(("wupper(wupper_write_proc): debugging disabled\n"));
    debug = 0;
  }
  if (!strcmp(textReceived, "elog"))
  {
    kdebug(("wupper(wupper_write_proc): Error logging enabled\n"))
    errorlog = 1;
  }
  if (!strcmp(textReceived, "noelog"))
  {
    kdebug(("wupper(wupper_write_proc): Error logging disabled\n"))
    errorlog = 0;
  }
  return(len);
}

/**********************************/
static int fill_proc_read_text(void)
/**********************************/
{
    //MJ-SMP: protect this function (preferrably with a spinlock)
    int interrupt, cardIndex[MAXCARDS], index;
    u_int len, card;

    index = 0;
    for (card = 0; card < cardsFound; card++)
    {
        while (cards[index].pciDevice == NULL) index++;

        if (index<MAXCARDS)
        {
            cardIndex[card]=index;
            index++;
            kdebug(("wupper(fill_proc_read_text): card %d has index %d\n", card,index));
        }
        else
        {
            kerror(("wupper(fill_proc_read_text): Card indexing error\n"));
            return 0;
        }
    }

    kdebug(("wupper(fill_proc_read_text): Creating text....\n"));
    len = 0;
    len += sprintf(proc_read_text + len, "WUPPER driver (debug version 1)\n");
    len += sprintf(proc_read_text + len, "\nDebug                       = %d\n", debug);
    len += sprintf(proc_read_text + len, "Number of cards detected    = %d\n", cardsFound);

    for (card = 0; card < cardsFound; card++)
    {
        // Addresses depend on firmware version
        uint32_t *pp = ioremap_nocache(cards[card].baseAddressBAR2, cards[card].sizeBAR2);

        unsigned int buildDate = *pp;
        unsigned int buildYear = *(pp+1);
        unsigned int buildRevision = *(pp+2);
        unsigned int genConstants = *(pp + 8);


        int buildMonth = (buildDate >> 24) & 0xff;
        int buildDay = (buildDate >> 16) & 0xff;
        int buildHour = (buildDate >> 8) & 0xff;
        int buildMinute = buildDate & 0xff;
        len += sprintf(proc_read_text + len, "\nCard %d: ", cardIndex[card]);
        len += sprintf(proc_read_text + len, "build revision: %x, ", buildRevision);
        len += sprintf(proc_read_text + len, "date: %2x-%x-20%x, time: %xh%x\n",
               buildDay, buildMonth, buildYear, buildHour, buildMinute);
        len += sprintf(proc_read_text + len, "Number of descriptors: %d, ", (genConstants & 0xff) );
        len += sprintf(proc_read_text + len, "number of interrupts : %d\n", ( (genConstants >> 8) & 0xff) ) ;


        if (msixBar[cardIndex[card]] != NULL)
        {
            len += sprintf(proc_read_text + len, "Interrupt count |");
            for (interrupt = 0; interrupt < msiblock; interrupt++)
                len += sprintf(proc_read_text + len, " %6d |", irqCount[cardIndex[card]][interrupt]);

            len += sprintf(proc_read_text + len, "\nInterrupt flag  |");
            for (interrupt = 0; interrupt < msiblock; interrupt++)
                len += sprintf(proc_read_text + len, " %6d |", irqFlag[cardIndex[card]][interrupt]);

            len += sprintf(proc_read_text + len, "\nInterrupt mask  |");
            for (interrupt = 0; interrupt < msiblock; interrupt++)
                len += sprintf(proc_read_text + len, " %6d |", irqMasked[cardIndex[card]][interrupt]);

            len += sprintf(proc_read_text + len, "\nMSIX PBA        %08x\n",
                    msixBar[cardIndex[card]][msixPbaOffset[cardIndex[card]] / sizeof(uint32_t)]);
            len += sprintf(proc_read_text + len, "\n");
        }
        else
        {
            len += sprintf(proc_read_text + len, "No MSIX interrupts for card %d\n\n", card);
        }
    }

    len += sprintf(proc_read_text + len, " \n");
    len += sprintf(proc_read_text + len, "The command 'echo <action> > /proc/wupper', executed as root,\n");
    len += sprintf(proc_read_text + len, "allows you to interact with the driver. Possible actions are:\n");
    len += sprintf(proc_read_text + len, "debug   -> enable debugging\n");
    len += sprintf(proc_read_text + len, "nodebug -> disable debugging\n");
    len += sprintf(proc_read_text + len, "elog    -> Log errors to /var/log/message\n");
    len += sprintf(proc_read_text + len, "noelog  -> Do not log errors to /var/log/message\n");
    kdebug(("wupper(fill_proc_read_text): Number of characters created = %d\n", len));
    return(len);
}

/***********************************************************/
int wupper_mmap(struct file *file, struct vm_area_struct *vma)
/***********************************************************/
{
    u32 moff,msize;

    /* it should be "shared" memory */
    if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
    {
        printk(KERN_ALERT "wupper(wupper_mmap): writeable mappings must be shared, rejecting\n");
        return -1;
    }

    msize = vma->vm_end - vma->vm_start;
    // moff = (vma->vm_pgoff << PAGE_SHIFT);
    moff = vma->vm_pgoff;
    kdebug(("wupper(wupper_mmap): offset: 0x%x, size: 0x%x\n", moff, msize));
    moff = moff << PAGE_SHIFT;
    if (moff & ~PAGE_MASK)
    {
        printk(KERN_ALERT "wupper(wupper_mmap): offset not aligned: %u\n", moff);
        return(-1);
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
    vma->vm_flags |= VM_RESERVED;
#else
    vma->vm_flags |= VM_DONTEXPAND;
    vma->vm_flags |= VM_DONTDUMP;
#endif
    // we do not want to have this area swapped out, lock it
    vma->vm_flags |= VM_LOCKED;
    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,msize,vma->vm_page_prot) != 0)
    {
        printk(KERN_ALERT "wupper(wupper_mmap): remap page range failed\n");
        return -EAGAIN;
    }

    vma->vm_ops = &wupper_vm_ops;
    return 0;
}

/*********************************************/
void wupper_vmclose(struct vm_area_struct *vma)
/*********************************************/
{
    kdebug(("wupper(wupper_mmap): closing mmap memory\n"));
}

/***************************************************************/
static long wupper_ioctl(struct file *file, u_int cmd, u_long arg)
/***************************************************************/
{
    card_params_t *cardParams;
    static struct vm_area_struct *vmas, uvmas;
    unsigned int interrupt, card;
    char capabilityId;
    char capabilityIdOffset;
    u_int address;
    u_short deviceControlRegister;
    int tlp;
    int count;

    kdebug(("wupper(wupper_ioctl): entered\n"));
    vmas = &uvmas;

    switch(cmd)
    {
    case GETCARDS:
        kdebug(("wupper(wupper_ioctl) GETCARDS\n"));
        if (copy_to_user(((int*)arg), &cardsFound, sizeof(int)) !=0)
        {
            printk(KERN_ALERT "wupper(wupper_ioctl,GETCARDS): Copy cardsFound to user space failed!\n");
            return(-1);
        }
        break;
    case GET_TLP:
        kdebug(("wupper(wupper_ioctl) GET_TLP\n"));
        cardParams = (card_params_t*)file->private_data;
        card = cardParams->slot;
        address = 0x34;
        pci_read_config_byte(cards[card].pciDevice, address, &capabilityIdOffset);
        kdebug(("wupper(wupper_ioctl) GET_TLP first capabilityIdOffset %x\n", (u_char) capabilityIdOffset));
        // count protects against loop not terminating
        count = 0;
        while (count < 10)
        {
            pci_read_config_byte(cards[card].pciDevice, (u_int) capabilityIdOffset, &capabilityId);
            kdebug(("wupper(wupper_ioctl) GET_TLP capabilityIdOffset %x capabilityId %x\n", (u_char) capabilityIdOffset, capabilityId));
            if (capabilityId == 0x10)
            {
                break;
            }
            address = (u_int) (capabilityIdOffset +1);
            pci_read_config_byte(cards[card].pciDevice, address, &capabilityIdOffset);
            kdebug(("wupper(wupper_ioctl) GET_TLP new capabilityIdOffset %x\n", (u_char) capabilityIdOffset));
            count ++;
        }
        if (count == 10)
        {
            printk(KERN_ALERT "wupper(wupper_ioctl,GET_TLP): Did not find capability with TLP id\n");
            return(-1);
         }
        address = capabilityIdOffset + 8;
        pci_read_config_word(cards[card].pciDevice, address, &deviceControlRegister);
        kdebug(("wupper(wupper_ioctl) GET_TLP new deviceControlRegister %x\n", deviceControlRegister));
        tlp = (deviceControlRegister >> 5) & 0x7;
        if (copy_to_user(((int*)arg), &tlp, sizeof(int)) !=0)
        {
            printk(KERN_ALERT "wupper(wupper_ioctl,GET_TLP): Copy value of TLP to user space failed!\n");
            return(-1);
        }
        break;
    case WAIT_IRQ:    //WAIT_DMA
        kdebug(("wupper(wupper_ioctl) WAIT_IRQ\n"));
        cardParams = (card_params_t*)file->private_data;
        card = cardParams->slot;
        if (copy_from_user(&interrupt, (void *)arg, sizeof(unsigned int)) !=0)
        {
            kerror(("wupper(wupper_ioctl WAIT_IRQ): error from copy_from_user\n"));
            return(-RD_CFU);
        }
        if (interrupt >= msiblock)
        {
            kerror(("wupper(wupper_ioctl) WAIT_IRQ): invalid interrupt specified %d\n", interrupt));
            return(-1);
        }
        wait_event_interruptible(waitQueue, irqFlag[card][interrupt] == 1);
        irqFlag[card][interrupt] = 0;
        kdebug(("wupper(wupper_ioctl) WAIT_IRQ): finished waiting for IRQ %d\n", interrupt));
        break;

    case CANCEL_IRQ_WAIT:
        kdebug(("wupper(wupper_ioctl) CANCEL_IRQ_WAIT  looping over %d interrupts\n", msiblock));
        cardParams = (card_params_t*)file->private_data;
        card = cardParams->slot;
        // set all flags to 1, wake_up_interruptible will only result in waking up a process if the flag specified
        // in wait_event_interruptible is set to 1
        for (interrupt = 0; interrupt < msiblock; interrupt++)
        {
            irqFlag[card][interrupt] = 1;
        }
        /* Wake up everybody who was waiting for an interrupt */
        wake_up_interruptible(&waitQueue);
        break;
    case RESET_IRQ_COUNTERS:
        kdebug(("wupper(wupper_ioctl) RESET_IRQ_COUNTERS: looping over %d interrupts\n", msiblock));
        cardParams = (card_params_t*)file->private_data;
        card = cardParams->slot;
        for (interrupt = 0; interrupt < msiblock; interrupt++)
        {
            irqCount[card][interrupt] = 0;
        }
        break;
    case RESET_IRQ_FLAGS:
        kdebug(("wupper(wupper_ioctl) RESET_IRQ_FLAGS: looping over %d interrupts\n", msiblock));
        cardParams = (card_params_t*)file->private_data;
        card = cardParams->slot;
        for (interrupt = 0; interrupt < msiblock; interrupt++)
        {
            irqFlag[card][interrupt] = 0;
        }
        break;
    case MASK_IRQ:
        cardParams = (card_params_t*)file->private_data;
        card = cardParams->slot;
        if (copy_from_user(&interrupt, (void *)arg, sizeof(unsigned int)) !=0)
        {
            kerror(("wupper(wupper_ioctl MASK_IRQ): error from copy_from_user\n"));
            return(-RD_CFU);
        }
        if (interrupt >= msiblock)
        {
            kerror(("wupper(wupper_ioctl) MASK_IRQ): invalid interrupt specified %d\n", interrupt));
            return(-1);
        }
        // check that interrupt was not already masked
        if (irqMasked[card][interrupt] == 0)
        {
            disable_irq(msixTable[card][interrupt].vector);
            irqMasked[card][interrupt] = 1;
            kdebug(("wupper(wupper_ioctl) MASK_IRQ: masked interrupt %d\n", interrupt));
        }
        else
        {
            kdebug(("wupper(wupper_ioctl) MASK_IRQ: interrupt %d already masked -> no action\n", interrupt));
        }
        break;
    case UNMASK_IRQ:
        cardParams = (card_params_t*)file->private_data;
        card = cardParams->slot;
        if (copy_from_user(&interrupt, (void *)arg, sizeof(unsigned int)) !=0)
        {
            kerror(("wupper(wupper_ioctl UNMASK_IRQ): error from copy_from_user\n"));
            return(-RD_CFU);
        }
        if (interrupt >= msiblock)
        {
            kerror(("wupper(wupper_ioctl) UNMASK_IRQ): invalid interrupt specified %d\n", interrupt));
            return(-1);
        }
        kdebug(("wupper(wupper_ioctl) UNMASK_IRQ: unmasked interrupt %d\n", interrupt));
        // check that interrupt was not already unmasked
        if (irqMasked[card][interrupt] == 1)
        {
            enable_irq(msixTable[card][interrupt].vector);
            irqMasked[card][interrupt] = 0;
            kdebug(("wupper(wupper_ioctl) UNMASK_IRQ: unmasked interrupt %d\n", interrupt));
        }
        else
        {
            kdebug(("wupper(wupper_ioctl) UNMASK_IRQ: interrupt %d already unmasked -> no action\n", interrupt));
        }
        break;
    case SETCARD:
        kdebug(("wupper ioctl SETCARD\n"));
        cardParams = (card_params_t*)arg;
        card = cardParams->slot;
        kdebug(("card = %d\n", card));
        if (check_slot_device(card) != 0)
        {
            printk(KERN_ALERT "wupper(wupper_ioctl ,SETCARD): Wrong slot !\n");
            return -6;
        }


#ifdef MUTEX_ORG
    if (down_interruptible(&ownerMutex))
#else
    if (mutex_lock_interruptible(&ownerMutex))
#endif
        {
            return -4;
        }
        owner[card]=file;
#ifdef MUTEX_ORG
    up(&ownerMutex);
#else
    mutex_unlock(&ownerMutex);
#endif

        cardParams = (card_params_t*)file->private_data;
        /* if (cardParams->slot != -1)
        {
            printk(KERN_ALERT "wupper(wupper_ioctl ,SETCARD): setting card to %d, card already set to %d!\n", card, cardParams->slot);
        } */
        cardParams->slot = card;
        cardParams->baseAddressBAR0 = cards[card].baseAddressBAR0;
        cardParams->sizeBAR0 = cards[card].sizeBAR0;
        cardParams->baseAddressBAR1 = cards[card].baseAddressBAR1;
        cardParams->sizeBAR1 = cards[card].sizeBAR1;
        cardParams->baseAddressBAR2 = cards[card].baseAddressBAR2;
        cardParams->sizeBAR2 = cards[card].sizeBAR2;
        // kdebug(("wupper ioctl SETCARD BAR0 %08x BAR1 %08x BAR2 %08x\n", cards[card].baseAddressBAR0, cards[card].baseAddressBAR1,  cards[card].baseAddressBAR2));
        // OK, we have a valid slot, copy configuration back to user
        if (copy_to_user(((card_params_t*)arg), &cards[card], sizeof(card_params_t)) !=0)
        {
            printk(KERN_ALERT "wupper(wupper_ioctl ,SETCARD): Copy card_params_t to user space failed!\n");
            return(-1);
        }
        kdebug(("wupper(wupper_ioctl ,SETCARD): end of ioctl SETCARD\n"));
        break;
    default:
        printk(KERN_ALERT "wupper(wupper_ioctl) default: Unknown ioctl %x\n", cmd);
        return(-1);
    }

    return 0;
}

