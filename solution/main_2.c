/* *********************************************************************
 *
 * User level tester program to test Squeue driver
 *
 * Program Name:        SqTester
 * Target:              Intel Galileo Gen1
 * Architecture:		x86
 * Compiler:            i586-poky-linux-gcc
 * File version:        v1.0.0
 * Author:              Brahmesh S D Jain
 * Email Id:            Brahmesh.Jain@asu.edu
 **********************************************************************/

/* *************** INCLUDE DIRECTIVES FOR STANDARD HEADERS ************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


/*
 * Please comment this macro if you want blocking user calls(Task1)
 */
#define NON_BLOCKING
/*
 * Message choices
 */
#define MESSAGE1   "Embedded System Programming"
#define MESSAGE2   "Real time Embedded Systems"
#define MESSAGE3   "Computer Architecture"
#define MESSAGE4   "Intel Galileo Gen1"
#define MESSAGE5   "GNU is Not Unix"
/*
 * Macros required to identify requests in ioctl
 */
#define FLASHGETS   0
#define FLASHGETP   1
#define FLASHSETP   2
#define FLASHERASE  3
/*
 *Number of pages
 */
#define PAGECOUNT 512
/*
 * Error codes
 */
#define	EAGAIN		-11	/* Try again */
#define	EBUSY		-16	/* Device or resource busy */

/* ***************** PREPROCESSOR DIRECTIVES **************************/
int main()
{
	int FdInQ,res; /* File open status for InQ, Result variable */
	char MessageToBeSent[PAGECOUNT * 64];/* Reserve space for entire EEPROM space */
	unsigned int printindex = 0;
	char choice; /* to store the user's choice */
	char Message[50]; /* String that will be sent for writing*/
	char stringchoice; /* which string to write*/
	unsigned long currentptr; /* current page pointer */
	unsigned int pagecount; /* how many pages to read/write */
    /* Open the Bus In Q device */
	FdInQ = open("/dev/i2c_flash", O_RDWR);
    /* Check if device opened successfully */
	if (FdInQ < 0 )
	{
		printf("Can not open bus_in_q device file by sender .\n");
		/* return from the thread */		
		return 0;
	}
    do
    {
//		system("clear");
		printf("\n\n\n\n What do you want to do ? \n 1-Read from EEPROM \n 2-Write to EEPROM \n 3-Get EEPROM status");
		printf("\n 4-Get Page postion \n 5-Set Page postion \n 6-Erase EEPROM \n 7-Exit\n");
        do
        choice = getchar();
        while (isspace(choice));
		if ('1' == choice)
		{
			printf("Number of pages to read ?\n");
			scanf("%d",&pagecount);
#ifdef NON_BLOCKING
			do
			{
               res = read(FdInQ,MessageToBeSent,pagecount);
               if (res <0 )
               {
				   perror("\n Tester : Read Status:  ");
				   usleep(10000);
			   }
			   else
			   {
				   printf("\n Tester : Read sequence complete \n");
			   }
 		    }while(res < 0);
#else
           res = read(FdInQ,MessageToBeSent,pagecount);
#endif
           printf("\nMessage that was read \n");
           if (res >= 0)
           {
	           for(printindex = 0; printindex < (pagecount*64) ; printindex++)
               printf("%c",MessageToBeSent[printindex]);
	       }
	       else
	       {
		       printf("\n Message not copied to  used space\n");
	       }
	       getchar();
		}
		else if('2' == choice)
		{
			printf("\n Ok then , Choose a string to be written: \n 1-"MESSAGE1" \n 2-"MESSAGE2"\n 3-"MESSAGE3"\n 4-"MESSAGE4"\n 5-"MESSAGE5"\n");
            do
            stringchoice = getchar();
            while (isspace(stringchoice));
			if ('1' == stringchoice) sprintf(Message,MESSAGE1);
			else if ('2' == stringchoice) sprintf(Message,MESSAGE2);
			else if ('3' == stringchoice) sprintf(Message,MESSAGE3);
			else if ('4' == stringchoice) sprintf(Message,MESSAGE4);
			else if ('5' == stringchoice) sprintf(Message,MESSAGE5);
			else sprintf(Message,MESSAGE5);
			printf("For how many pages you want to write this string ?\n");
			scanf("%d",&pagecount);
           /* Write the message to the driver */
           for(printindex = 0; printindex < (pagecount * 64) ; printindex+=strlen(Message))
           {
		 	   memcpy(&(MessageToBeSent[printindex]),&Message[0],strlen(Message));
	       }
#ifdef NON_BLOCKING
			do
			{
				/* Get the status of EEPROM */
				res = ioctl(FdInQ,0,FLASHGETS);
				if (res < 0)
				{
					printf("\n EEPROM is busy \n");
				}
				usleep(1000000);
			}while(res < 0);
			printf("\n EEPROM is free now, Sending the write request \n");
#endif
               /* Send the write request */
               res = write(FdInQ,MessageToBeSent,pagecount);
#ifdef NON_BLOCKING
		    	do
			    {
				    /* Get the status of EEPROM */
				    res = ioctl(FdInQ,0,FLASHGETS);
				    if (res < 0)
				    {
				    	printf("\n Tester : EEPROM Write in Progress \n");
				    }
				    usleep(1000000);
			    }while(res < 0);
#endif
			printf("\n Tester : EEPROM write complete \n");
   		    memset(&Message[0],0,sizeof(Message));
	    }
	    else if('3' == choice)
	    {
			/* get status */
			if(ioctl(FdInQ,0,FLASHGETS) < 0) printf("\n Tester : EEPROM busy \n");
			else printf("\nTester : EEPROM Not busy \n");
		}
	    else if('4' == choice)
	    {
			/* get current page */
			printf("\n Current page : %d \n",(unsigned int)ioctl(FdInQ,0,FLASHGETP));
		}
	    else if('5' == choice)
	    {
			/* set current page */
			printf("\n Page number ? : \n");
			scanf("%d",&currentptr);
			if (ioctl(FdInQ,currentptr,FLASHSETP))
			{
				printf("\n page number out ofmemory \n try again\n");
		    }
		}
		else if('6' == choice)
		{
			/* Erase EEPROM */
			if(ioctl(FdInQ,0,FLASHERASE))
			{
				printf("\n EEPROM erase fail, Device Busy \n");
			}
			else
			{
#ifdef NON_BLOCKING
				printf("\n EEPROM erase initiated. Check the status for completion \n");
#else
				printf("\n EEPROM erase success\n");
#endif
			}
		}
		else
		{
			/* Do nothing */
		}
		memset(&MessageToBeSent[0],0,sizeof(MessageToBeSent));
	}while(('7' != choice));
    close(FdInQ);
    return 0;
}
