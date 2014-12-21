/* *********************************************************************
 *
 * Device driver for 24FC256 EEPROM using I2C
 *
 * Program Name:        i2c_flash
 * Target:              Intel Galileo Gen1
 * Architecture:		x86
 * Compiler:            i586-poky-linux-gcc
 * File version:        v1.0.0
 * Author:              Brahmesh S D Jain
 * Email Id:            Brahmesh.Jain@asu.edu
 **********************************************************************/
 
 /* *************** INCLUDE DIRECTIVES FOR STANDARD HEADERS ************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/init.h>
#include <asm/msr.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <asm/errno.h>
/* ***************** PREPROCESSOR DIRECTIVES **************************/

/*
 * If Blocking call is required, comment this macro
 */
#define NON_BLOCKING
/*
 * LED indicator can be switched on for one complete operation by commenting this macro
 */
#define LED_DYNAMIC

/*
 * EEPROM chip address
 */
#define CHIP_ADDRESS   0x54

/*
 * EEPROM TOTAL PAGES
 */
#define PAGECOUNT 512
/*
 * Length of the device name string
 */
#define DEVICE_NAME_LENGTH   20

/*
 * Number of devices that are need to be created by the driver at the end of the
 * driver initialization using udev
 */
#define NUMBER_OF_DEVICES   1

/*
 * driver name
 */
#define DEVICE_NAME    "i2c_flash"
/*
 * Page size
 */
#define PAGESIZE   64

/*
 * Adapter minor number
 */
#define ADAPTER_MINOR_NUMBER   0

/*
 * Client flag indicating 7 bit chip address
 */
#define CHIP_ADDRESS_FLAG   0

/*
 * I2C driver class
 */
#define I2C_DRIVER_CLASS   0

/*
 * I2C Adapter class
 */
#define I2C_ADAPTER_CLASS   0
/*
 * Macros required to identify requests in ioctl
 */
#define FLASHGETS   0
#define FLASHGETP   1
#define FLASHSETP   2
#define FLASHERASE  3

/*
 * Please uncomment this when debugging, this will print the
 * important events on the console
 */
//#define DEBUG

/*
 * Macro to get page number
 */
#define PAGENO(x)  (x >> 6)

/*
 * Macro to get offset
 */
#define OFFSET(x)  (x & 0x3F)

/*
 * Marcro to reverse the bytes
 */
#define REVERSEBYTES(x)   ((x >> 8) | (x << 8))

/*
 * Macro to join Page number and offset
 */
#define JOIN(x,y)   ((x << 6) | (y))

/* uint8 and unsigned char are used interchangeably in the program */
typedef unsigned char uint8;

typedef struct I2cFlashDevTag
{
	struct cdev cdev; /* cdev structure */
	char name[DEVICE_NAME_LENGTH];   /* Driver Name */
	unsigned char Page[PAGESIZE]; /* Page to be written or read */
}I2cFlashDevType;


/*
 * States to differentiate different states of the workqueue
 */
typedef enum I2cFlashReadOrWrite_Tag
{
   I2CFLASHREAD, /* EEprom read is requested */
   I2CFLASHWRITE,   /* EEprom write is requested*/
   I2CFLASHDATAREADY, /* Set when the EEPROM read data is ready in the buffer*/
   I2CFLASHERASE,    /* Flash erase is happening */
   NONE               /* EEPROM is ready receive new requests */
}
I2cFlashReadOrWriteType;

/*
 * Global structure used for signaling between workqueue and Read/Write functions
 */
typedef struct I2cFlashWorkQueuePrivateTag
{
	I2cFlashReadOrWriteType I2cFlashReadOrWrite; /* Enum having different states */
	char* I2cFlashWorkQueueBufferPtr; /* pointer to buffer to read/write */
	unsigned long I2cFlashWorkQueuePageCount; /* number of pages requested for read/write */
}I2cFlashWorkQueuePrivateType;


/*
 * Device pointer which stores the upper layer device structure
 */
static I2cFlashDevType *I2cFlashDevMem = NULL;
/*
 * Read or write pointer for the EEPROM
 */
static unsigned short I2cFlashEepromPtr = 0;
/* Device number alloted */
static dev_t I2cFlashDevNumber;

/* Create class and device which are required for udev */
struct class *I2cFlashDevClass;
static struct device *I2cFlashDevName;

/*
 * This will point to local kmalloc structure
 */
static struct i2c_client *I2cFlashClient = NULL;

/*
 * This will store the address of the client copy sent by the core,used later for deleting the device
 */
static struct i2c_client *I2cFlashClientDeviceInit = NULL;
/*
 * Device Ids to probe
 */
static struct i2c_device_id my_device_id[] = {{"i2c_flash",0}};

/*
 * WorkQueue Data
 */
#ifdef NON_BLOCKING
struct workqueue_struct *I2cFlashWorkQueue;
#endif
struct work_struct I2cFlashWork;
static I2cFlashWorkQueuePrivateType I2cFlashWorkQueuePrivate = {NONE,NULL,0};
void I2cFlashWorkFunction(struct work_struct *work);

/* *********************************************************************
 * NAME:             I2cFlashDetect
 * CALLED BY:        i2c-core
 * DESCRIPTION:      this is called if i2c core finds the device at the
 *                   time of installation of driver. 
 * INPUT PARAMETERS: Client pointer:pointer to the client tmp created by
 *                                  the i2c-core
 *                   Board info ptr:pointer to the board info
 * RETURN VALUES:    int : status - Fail/Pass(0)
 ***********************************************************************/
int I2cFlashDetect(struct i2c_client *ReceivedClient, struct i2c_board_info *ReceivedBoardInfo)
{
#ifdef DEBUG
	printk(KERN_INFO "\n I2cFlashDetect function called with client pointer %d\n",ReceivedClient);
#endif
    /* If this functions is called twice ,then it is detected by this check */
	if (I2cFlashClient != NULL)
	{
		printk(KERN_INFO "\n I2cFlash finds two clients \n");
		return -ENOMEM;
	}
	/* Client structure given by i2c-core is a temporary structure, so safely copying the entire structure */
	I2cFlashClient = kzalloc(sizeof(struct i2c_client),GFP_KERNEL);
	if (NULL != I2cFlashClient)
	{
		/* copying the entire structure to a i2c_flash copy of the client */
	   memcpy(I2cFlashClient,ReceivedClient,sizeof(struct i2c_client));
#ifdef DEBUG
	   printk(KERN_INFO "\n client found by I2cFlashDetect : \n chip adddress = %d \n client.name = %s \n Board info addr = %d \n Board Name = %s\n",
	          I2cFlashClient->addr,I2cFlashClient->name,ReceivedBoardInfo->addr,ReceivedBoardInfo->type);
#endif
	   return 0;
    }
    else
    {
		printk(KERN_INFO "\nI2cFlash client not allocated\n");
		return -ENOMEM;
	}
}

/* *********************************************************************
 * NAME:             I2cFlashProbe
 * CALLED BY:        i2c-core
 * DESCRIPTION:      this is called if i2c core finds the device at the
 *                   when device is added by this module. 
 * INPUT PARAMETERS: Client pointer:pointer to the client tmp created by
 *                                  the i2c-core
 *                   device id :received device id info
 * RETURN VALUES:    int : status - Fail/Pass(0)
 ***********************************************************************/
int I2cFlashProbe(struct i2c_client *ReceivedClient, const struct i2c_device_id *ReceivedDeviceIdInfo)
{
#ifdef DEBUG
    printk(KERN_INFO "\n I2cFlashProbe function called  %d\n",ReceivedClient);
#endif
    /* If this functions is called twice ,then it is detected by this check */
	if (I2cFlashClient != NULL)
	{
		printk(KERN_INFO "\n I2cFlash probe not succeed, may be I2cFlash has laready detected the driver \n");
		return -ENOMEM;
	}
	/* Client structure given by i2c-core is a temporary structure, so safely copying the entire structure */
	I2cFlashClient = kzalloc(sizeof(struct i2c_client),GFP_KERNEL);
	if (NULL != I2cFlashClient)
	{
	   /* copying the entire structure to a i2c_flash copy of the client */
	   memcpy(I2cFlashClient,ReceivedClient,sizeof(struct i2c_client));
#ifdef DEBUG
	   printk(KERN_INFO "\n client found by I2cFlashProbe: \n chip adddress = %d \n client.name = %s \n Device id name = %s\n",
	          I2cFlashClient->addr,I2cFlashClient->name,ReceivedDeviceIdInfo->name);
#endif
	   return 0;
    }
    else
    {
		printk(KERN_INFO "\nI2cFlash client not allocated\n");
		return -ENOMEM;
	}
}

/* *********************************************************************
 * NAME:             I2cFlashRemove
 * CALLED BY:        i2c-core
 * DESCRIPTION:      this is called if i2c core finds a device is 
 *                   is unregistered.
 * INPUT PARAMETERS: Client pointer:pointer to the client tmp created by
 *                                  the i2c-core
 * RETURN VALUES:    int : status - Fail/Pass(0)
 ***********************************************************************/
int I2cFlashRemove(struct i2c_client *ReceivedClient)
{
#ifdef DEBUG
	printk(KERN_INFO "\n I2cFlash client is being deleted \n");
#endif
    /* free the client memory allocated by our driver for this device */
    kfree(I2cFlashClient);
    I2cFlashClient = NULL;
    return 0;
}

/* This is the driver that will be inserted */
static struct i2c_driver I2cFlashDriver = {
	.id_table   = my_device_id,
	.driver     = {
	                .owner = THIS_MODULE,
	                .name		= "i2c_flash"
	             },
	.probe = &I2cFlashProbe,
	.remove = &I2cFlashRemove,
	.detect = &I2cFlashDetect
};
/* *********************************************************************
 * NAME:             I2cFlashDriverOpen
 * CALLED BY:        User App through kernel
 * DESCRIPTION:      copies the device structure pointer to the private  
 *                   data of the file pointer. 
 * INPUT PARAMETERS: inode pointer:pointer to the inode of the caller
 *                   filept:file pointer used by this inode
 * RETURN VALUES:    int : status - Fail/Pass(0)
 ***********************************************************************/
int I2cFlashDriverOpen(struct inode *inode, struct file *filept)
{
	I2cFlashDevType *dev; /* dev pointer for the present device */
	/* to get the device specific structure from cdev pointer */
	dev = container_of(inode->i_cdev, I2cFlashDevType, cdev);
	/* stored to private data so that next time filept can be directly used */
	filept->private_data = dev;
#ifdef DEBUG
	/* Print that device has opened succesfully */
	printk("Device %s opened succesfully ! \n",(char *)&(dev->name));
#endif
    /* Request the GPIO for LED indication */
    gpio_request_one(26,GPIOF_OUT_INIT_LOW,"I2cFlashLed");
    gpio_set_value_cansleep(26,0);
    return 0;
}

/* *********************************************************************
 * NAME:             I2cFlashDriverRelease
 * CALLED BY:        User App through kernel
 * DESCRIPTION:      Releases the file structure
 * INPUT PARAMETERS: inode pointer:pointer to the inode of the caller
 *                   filept:file pointer used by this inode
 * RETURN VALUES:    int : status - Fail/Pass(0)
 ***********************************************************************/
int I2cFlashDriverRelease(struct inode *inode, struct file *filept)
{
	I2cFlashDevType *dev = (I2cFlashDevType*)(filept->private_data);
	printk("\n%s is closing\n", dev->name);
	return 0;
}

/* *********************************************************************
 * NAME:             I2cFlashWorkFunction
 * CALLED BY:        Kernel work queue
 * DESCRIPTION:      Function is used for non blocking implementation
 * INPUT PARAMETERS: work ptr: Pointer to the work structure
 * RETURN VALUES:    None
 ***********************************************************************/
void I2cFlashWorkFunction(struct work_struct *work)
{
    unsigned short loopindex = 0; /* For loop */
    int Status = 0; /* For storing read and write status */
    unsigned short Address = REVERSEBYTES(JOIN(PAGENO(I2cFlashEepromPtr),0x00));/*this is bcoz MSB should be sent first*/
    unsigned char TempMessage[PAGESIZE + 2] = {0};/* to store the address temporarily */
    /* Check if READ was requested that resulted the work queue */
    if (I2CFLASHREAD == I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite)
    {
        /* Set the read ptr on the eeprom */
        i2c_master_send(I2cFlashClient,(const char *)&Address,sizeof(Address));
       /* Though 32K of data can be read in one shot, here page read is implemented so that kernel is not blocked for such a long time */
#ifndef LED_DYNAMIC
       gpio_set_value_cansleep(26,1);
#endif
       for (loopindex = 0; loopindex < I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount; loopindex++)
       {
	    	do
		   {
#ifdef DEBUG
		      printk("\n Inside do while");
#endif
#ifdef LED_DYNAMIC
              /* switch on led */
              gpio_set_value_cansleep(26,1);
#endif
              /* Receive one page of data */
		      Status = i2c_master_recv(I2cFlashClient,((I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr) + (loopindex * PAGESIZE)),PAGESIZE);
		      /* Switch off led */
#ifdef LED_DYNAMIC
		      gpio_set_value_cansleep(26,0);
#endif
#ifdef DEBUG
		      printk("\nRead status = %i",Status);
#endif
	       }while(PAGESIZE != Status);
	   }
#ifndef LED_DYNAMIC
       gpio_set_value_cansleep(26,0);
#endif
	   /* incrment the pointer by 64 bytes, and wrap around if required */
	   I2cFlashEepromPtr = (((PAGENO(I2cFlashEepromPtr)) + (I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount)) <= (PAGECOUNT-1)) ? (JOIN(((PAGENO(I2cFlashEepromPtr)) + (I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount)),(0x00))) : 
	                                                (JOIN(((I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount)- ((PAGECOUNT-1) - (PAGENO(I2cFlashEepromPtr))) - (1)),0x00));
	   /* Change the state to READ DATA READY state */
	   I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite = I2CFLASHDATAREADY;
	}
	else if(I2CFLASHWRITE == I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite)
	{
#ifndef LED_DYNAMIC
       gpio_set_value_cansleep(26,1);
#endif
       /* Join the address to the message */
       for (loopindex = 0; loopindex < (I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount); loopindex++)
       {
	    	/* Prepare the message */
            /* put the Address */
            memcpy(&TempMessage[0],&Address,sizeof(Address));
            memcpy(&TempMessage[2],(I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr + (loopindex * PAGESIZE)), PAGESIZE);
		    do
		    {
#ifdef DEBUG
		       printk("\n Inside do while");
#endif
#ifdef LED_DYNAMIC
               gpio_set_value_cansleep(26,1);
#endif
               /* Send one page along with the adress pointer */
		       Status = i2c_master_send(I2cFlashClient,(const char *)&TempMessage,sizeof(TempMessage));
#ifdef LED_DYNAMIC
		       gpio_set_value_cansleep(26,0);
#endif
#ifdef DEBUG
		       printk("\nWrite status = %i",Status);
#endif
	        }while((PAGESIZE +2) != Status);
	        /* incrment the pointer by 64 bytes, and wrap around if required */
	        I2cFlashEepromPtr = (((PAGENO(I2cFlashEepromPtr)) + (1)) <= (PAGECOUNT-1)) ? (JOIN(((PAGENO(I2cFlashEepromPtr)) + (1)),(0x00))) : (JOIN(0x00,0x00));
	        /* reverse the address bytes which is required for sending the next page write */
	        Address = REVERSEBYTES(JOIN(PAGENO(I2cFlashEepromPtr),0x00));
	    }
#ifndef LED_DYNAMIC
       gpio_set_value_cansleep(26,0);
#endif
	    /* Free up the memory which was allocated in the .write function */
	    kfree(I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr);
	    I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount = 0;
	    I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr = NULL;
	    /* Be the last lastment */
	    I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite = NONE;
	}
	else if(I2CFLASHERASE == I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite)
	{
       /* bring up the write pointer to 0 */
       I2cFlashEepromPtr = 0;
       /* Below part of the code is similar to that of write procedure */
       I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr = (unsigned char*)kmalloc((PAGESIZE + 2),GFP_KERNEL);
       memset(I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr,0xFF,(PAGESIZE + 2));
#ifndef LED_DYNAMIC
       gpio_set_value_cansleep(26,1);
#endif
       /* Join the address to the message */
       for (loopindex = 0; loopindex < PAGECOUNT; loopindex++)
       {
		   /* prepare the address */
	       Address = REVERSEBYTES(JOIN(loopindex,0x00));
	       /* Prepare the message */
           /* put the Address */
           memcpy(I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr,&Address,sizeof(Address));
		   do
		   {
#ifdef DEBUG
		      printk("\n Inside do while");
#endif
#ifdef LED_DYNAMIC
               gpio_set_value_cansleep(26,1);
#endif
		      Status = i2c_master_send(I2cFlashClient,(const char *)I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr,(PAGESIZE + 2));
#ifdef LED_DYNAMIC
		       gpio_set_value_cansleep(26,0);
#endif
#ifdef DEBUG
		      printk("\nWrite status = %i",Status);
#endif
	       }while((PAGESIZE +2) != Status);
	   }
#ifndef LED_DYNAMIC
       gpio_set_value_cansleep(26,0);
#endif
	   /* freeup the memory just allocated for erase purpose */
	   kfree(I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr);
       I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite = NONE;
	}
	else
	{
		/* Work function need not to do anything in I2CFLASHDATAREADY or NONE */
	}
}
/* *********************************************************************
 * NAME:             I2cFlashDriverWrite
 * CALLED BY:        User App through kernel
 * DESCRIPTION:      writes chunk of data to the message buffer
 * INPUT PARAMETERS: filept:file pointer used by this inode
 *                   buf : pointer to the user data
 *                   count : no of bytes to be copied to the msg buffer
 *                   offp: offset from which the string to be written
 *                         (not used)
 * RETURN VALUES:    ssize_t : remaining number of bytes that are to be
 *                             written. EBUSY if EEPROM is busy
 ***********************************************************************/
ssize_t I2cFlashDriverWrite(struct file *filept, const char *buf,size_t count, loff_t *offp)
{
	ssize_t RetValue =  0; /* Error code sent when the buffer is full */
    /* If no read-write operation is going on , invoke new write operation */
    if (NONE == I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite)
    {
		/* allocate the memory and copy the entire data sent by the user */
		I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr = (unsigned char*)kzalloc((PAGESIZE * count),GFP_KERNEL);
	    if (copy_from_user(I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr,buf,(PAGESIZE * count)))
	    {
		    printk(" \nError copying from user space");
		    return 0;
	    }
	    else
	    {
#ifdef DEBUG
	        printk(" Driver received data from main \n started to write to EEPROM !! \n ");
#endif
        }
		/* No Read/Write operation is going on */
        I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite = I2CFLASHWRITE;
        I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount = count;
#ifdef NON_BLOCKING
        /* Submit to work queue */
        queue_work(I2cFlashWorkQueue,&I2cFlashWork);
#else
         /* Call the function directly in the user's context itself */
        I2cFlashWorkFunction(&I2cFlashWork);
#endif
	    RetValue = 0;
	}
	else
	{
		/* The previous read/write operation is still going on so return -1 with EBUSY */
		RetValue = -EBUSY;
	}
    return RetValue;
}

/* *********************************************************************
 * NAME:             I2cFlashDriverRead
 * CALLED BY:        User App through kernel
 * DESCRIPTION:      reads chunk of data from the message buffer
 * INPUT PARAMETERS: filept:file pointer used by this inode
 *                   buf : pointer to the user data
 *                   count : no of bytes to be copied to the user buffer
 *                   offp: offset from which the string to be read
 *                         (not used)
 * RETURN VALUES:    ssize_t : number of bytes written to the user space
 *                  -EAGAIN, if the request is submitted to the workqueue
 *                  -EBUSY, if the EEPROM is busy with read or write oprtn 
 ***********************************************************************/
ssize_t I2cFlashDriverRead(struct file *filept, char *buf,size_t count, loff_t *offp)
{
	ssize_t RetValue = -1;

    if (I2CFLASHDATAREADY == I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite)
    {
		/* The data for previous Read request is ready so copy to the user space */
        /* Copy to the user space*/
        if(copy_to_user(buf, (I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr),(PAGESIZE * I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount)))
        {
#ifdef DEBUGMODE
            printk("\n Buffer writing failed ");
#endif
	    }
	    else
	    {
	     	RetValue = count;
	    }
	    /* No the read buffer can be freed and set other information */
	    kfree(I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr);
	    I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr = NULL;
	    I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount = 0;
	    RetValue = 0;
	    /* be the last statement */
	    I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite = NONE;
	}
	else if(NONE == I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite)
	{
		/* No Read/Write operation is going on */
        I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr = (unsigned char*)kzalloc((PAGESIZE * count),GFP_KERNEL);
        I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite = I2CFLASHREAD;
        I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount = count;
#ifdef NON_BLOCKING
        /* submit the new read request to the work queue */
        queue_work(I2cFlashWorkQueue,&I2cFlashWork);
	    RetValue = -EAGAIN;
#else
         /* Call the function in the user's context itself */
        I2cFlashWorkFunction(&I2cFlashWork);
        /* Since this is a blocking call , the data will be ready immediately */
        /* Copy to the user space*/
        if(copy_to_user(buf, (I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr),(PAGESIZE * I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount)))
        {
            printk("\n Buffer writing failed ");
	    }
	    kfree(I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr);
	    I2cFlashWorkQueuePrivate.I2cFlashWorkQueueBufferPtr = NULL;
	    I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite = NONE;
	    I2cFlashWorkQueuePrivate.I2cFlashWorkQueuePageCount = 0;
	    RetValue = 0;
#endif
	}
    else
	{
		/* The previous read/write operation is still going on so return -1 with EBUSY */
		RetValue = -EBUSY;
	}
    return RetValue;
}

/* *********************************************************************
 * NAME:             I2cFlashDriverIoctl
 * CALLED BY:        User App through kernel
 * DESCRIPTION:      Does Iocntrl like setting the pointer,status,erase
 * INPUT PARAMETERS: filept:file pointer used by this inode
 *                   pagepostion : used in case the command is FLASHSETP
 *                   Request : request/command by user
 * RETURN VALUES:    long : error codes / return success
 ***********************************************************************/
long I2cFlashDriverIoctl(struct file *filept,unsigned int pageposition, unsigned long Request)
{
	int RetValue =  -1; /* Error code by default */
	/* is the request for get status */
	if (FLASHGETS == Request)
	{
		if (NONE == I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite)
		{
			RetValue = 0;
		}
		else
		{
			RetValue = -EBUSY;
		}
	}
	else if (FLASHGETP == Request)
	{
		/* is the request for getting page pointer */
		RetValue = PAGENO(I2cFlashEepromPtr);
	}
	else if (FLASHSETP == Request)
	{
		/* is the request for setting the page pointer */
		if (pageposition < PAGECOUNT)
		{
			I2cFlashEepromPtr = JOIN(pageposition,0x00);
			RetValue = 0;
		}
		else
		{
			RetValue = -1;
		}
	}
	else if (FLASHERASE == Request)
	{
		/* is the request for erase */
		/* check if the EEPROM is free */
        if (NONE == I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite)
        {
			/* change the state to ERASE */
			I2cFlashWorkQueuePrivate.I2cFlashReadOrWrite = I2CFLASHERASE;
#ifdef NON_BLOCKING
            /* submit it to the work queue */
            queue_work(I2cFlashWorkQueue,&I2cFlashWork);
#else
            /* Call the function in the user's context itself */
            I2cFlashWorkFunction(&I2cFlashWork);
#endif
            RetValue = 0;
		}
		else
		{
			RetValue = -EBUSY;
		}
	}
	else
	{
		/* do nothing */
		RetValue = 0;
	}
	return RetValue;
}

/* Assigning operations to file operation structure */
static struct file_operations I2cFlashFops = {
    .owner = THIS_MODULE, /* Owner */
    .open = I2cFlashDriverOpen, /* Open method */
    .release = I2cFlashDriverRelease, /* Release method */
    .write = I2cFlashDriverWrite, /* Write method */
    .read = I2cFlashDriverRead, /* Read method */
    .unlocked_ioctl = I2cFlashDriverIoctl,
};

/* *********************************************************************
 * NAME:             I2cFlashDriverInit
 * CALLED BY:        By system when the driver is installed
 * DESCRIPTION:      Initializes the driver and creates the 4 devices
 * INPUT PARAMETERS: None
 * RETURN VALUES:    int : initialization status
 ***********************************************************************/
int __init I2cFlashDriverInit(void)
{
	int Ret = -1; /* return variable */
    struct i2c_adapter *I2cFlashAdapterPtr;
    struct i2c_board_info I2cFlashBoardInfo[] = {{I2C_BOARD_INFO("i2c_flash", CHIP_ADDRESS)}};

	/* Allocate device major number dynamically */
	if (alloc_chrdev_region(&I2cFlashDevNumber, 0, NUMBER_OF_DEVICES, DEVICE_NAME) < 0)
	{
         printk("Device could not acquire a major number ! \n");
         return -1;
	}
	
	/* Populate sysfs entries */
	I2cFlashDevClass = class_create(THIS_MODULE, DEVICE_NAME);
   
    /* Allocate memory for all the devices */
    I2cFlashDevMem = (I2cFlashDevType*)kmalloc(((sizeof(I2cFlashDevType)) * NUMBER_OF_DEVICES), GFP_KERNEL);
    
    /* Check if memory was allocated properly */
   	if (NULL == I2cFlashDevMem)
	{
       printk("Kmalloc Fail \n");

	   /* Remove the device class that was created earlier */
	   class_destroy(I2cFlashDevClass);
       /* Unregister devices */
	   unregister_chrdev_region(MKDEV(MAJOR(I2cFlashDevNumber), 0), NUMBER_OF_DEVICES);
       return -ENOMEM;
	} 

    /* Device Creation */ 
    /* Copy the respective device name */
    sprintf(I2cFlashDevMem->name,DEVICE_NAME);
    /* clear the memory */
    memset(I2cFlashDevMem->Page,0,PAGESIZE);
    /* Connect the file operations with the cdev */
    cdev_init(&I2cFlashDevMem->cdev,&I2cFlashFops);
    I2cFlashDevMem->cdev.owner = THIS_MODULE;
    /* Connect the major/minor number to the cdev */
    Ret = cdev_add(&I2cFlashDevMem->cdev,I2cFlashDevNumber,1);
	if (Ret)
	{
	    printk("Bad cdev\n");
	    return Ret;
	}

	I2cFlashDevName = device_create(I2cFlashDevClass,NULL,I2cFlashDevNumber,NULL,DEVICE_NAME);

	/* Enable scl and sda */
    gpio_request_one(29,GPIOF_OUT_INIT_LOW,"I2cEnable");
    gpio_set_value_cansleep(29,0);

    Ret = i2c_add_driver(&I2cFlashDriver);
#ifdef DEBUG
    printk("\n Added Driver");
#endif
	if (Ret)
	{
		printk(KERN_ERR "i2c_flash.ko: Driver registration failed, module not inserted.\n");
       /* Destroy the devices first */
	   device_destroy(I2cFlashDevClass,I2cFlashDevNumber);

	   /* Delete each of the cdevs */
	   cdev_del(&(I2cFlashDevMem->cdev));

	   /* Free up the allocated memory for all of the device */
	   kfree(I2cFlashDevMem);

	   /* Remove the device class that was created earlier */
	   class_destroy(I2cFlashDevClass);
	
	   /* Unregister devices */
	   unregister_chrdev_region(I2cFlashDevNumber, NUMBER_OF_DEVICES);

	   return Ret;
	}
    /* Get the adapter pointer */
    I2cFlashAdapterPtr = i2c_get_adapter(ADAPTER_MINOR_NUMBER);
    /* create a new device */
	I2cFlashClientDeviceInit = i2c_new_device(I2cFlashAdapterPtr,I2cFlashBoardInfo);
#ifdef DEBUG
	printk("\n Added Device");
	if (NULL != I2cFlashClientDeviceInit)
	{
	   printk(KERN_INFO "\n Init client found : \n chip adddress = %d \n client.name = %s \n ",
	          I2cFlashClientDeviceInit->addr,I2cFlashClientDeviceInit->name);
    }
    else
    {
		printk(KERN_INFO "\nI2cFlash client init not allocated\n");
		return -ENOMEM;
	}
#endif

#ifdef DEBUG
	printk("\n client reference ptr = %d ",I2cFlashClient);
	printk("\n Init  client reference ptr = %d ",I2cFlashClientDeviceInit);
#endif
#ifdef NON_BLOCKING
    /* create workqueue for non-blocking implementation */
    I2cFlashWorkQueue = create_singlethread_workqueue("i2c_flash");
    INIT_WORK(&I2cFlashWork,I2cFlashWorkFunction);
#endif
	printk("\n I2C_flash Driver is initialized \n");
	
	return Ret;
}
/*
 * Driver Deinitialization
 */
void __exit I2cFlashDriverExit(void)
{
    /* Destroy the devices first */
	device_destroy(I2cFlashDevClass,I2cFlashDevNumber);

	/* Delete each of the cdevs */
	cdev_del(&(I2cFlashDevMem->cdev));

	/* Free up the allocated memory for all of the device */
	 kfree(I2cFlashDevMem);

	/* Remove the device class that was created earlier */
	class_destroy(I2cFlashDevClass);
	
	/* Unregister char devices */
	unregister_chrdev_region(I2cFlashDevNumber, NUMBER_OF_DEVICES);

    /* unregister the i2c device*/
    i2c_unregister_device(I2cFlashClientDeviceInit);

	/* Delete the driver */
	i2c_del_driver(&I2cFlashDriver);

#ifdef NON_BLOCKING
    /* check if any if the work in queue is pending */
    if (!cancel_work_sync(&I2cFlashWork )) flush_workqueue(I2cFlashWorkQueue);
    /* destroy the workqueue */
    destroy_workqueue(I2cFlashWorkQueue);
#endif
	printk("\n I2C-Flash device and driver are removed ! \n ");
}

module_init(I2cFlashDriverInit);
module_exit(I2cFlashDriverExit);
MODULE_LICENSE("GPL v2");

