/***********************************************************************************************************
**************I2C based Driver for 24FC256 EEEPROM and user space tester program to test it*****************
***********************************************************************************************************/
Author: Brahmesh Jain
EmailId: Brahmesh.Jain@asu.edu
Document version : 1.0.0

This zip file contains two source files.

1) i2c_flash.c is the linux kernel driver implementing the driver for the external EEPROM.
2) main_2.c is the user level program that is used for testin the program.

Other than the Assignment's requirement, the following are some of the things that Driver requires :

1) Driver can work in two modes : blocking mode or non-blocking mode.
	In blocking mode,when user thread requests read/write of a page, it gets blocked untill the completion of
	the operation. In non-blocking mode, the kernel takes the request and user thread return immediately.

2)	By default non-blocking mode is enabled.To enable blocking mode, uncomment the macro #define NON_BLOCKING 
	at the beginning of the files i2c_flash.c and main_2.c
	
3) Driver implements multiple page read/write by writing one page at time, so that kernel does not get
   blocked for longtime ensuring enough time for preemption of the driver's execution if needed.

4) In non-blocking mode, ERASE command to the driver returns immediately. User can poll for the status to
   know whether the erase is completed.

5) LED does blinks every page read/write. since the operation is so fast it is not detectable to human eye.
   To disable this blinking and enable LED for the complete operation, uncomment the macro #define LED_DYNAMIC
   in i2c_flash.c

6) At important steps in the driver execution, driver can print the messages if the macro #define DEBUG is 
   uncommented in i2c_flash.c

7) Tester(I2cFlashTester or main_2.c) for testing the writing , gives the option of 5 predefined string as 
   defined by macros MESSAGE1...MESSAGE5. user can change these string to give different string options :)
    
8) Finally steps to run the program on Intel Galielo Board :
   a) Load the SDK source of galileo y running : "source ~/SDK/environment-setup-i586-poky-linux"
   b) open terminal with root permission , run the command "make all" to compile the driver
   c) Compile the tester(user application) program, "$CC main_2.c -o I2cFlashTester"
   d) Transfer two file : i2c_flash.ko and I2cFlashTester to the galielo board using secured copy
   e) Open Galileo's terminal using putty and Install the driver by running the command "insmod i2c_flash.ko"
   f) run the user application with the command "./I2cFlashTester"
   g) to cleanup the generated files, run "make clean"

