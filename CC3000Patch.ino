/*

	The Arduino CC3000 Firmware Upgrader
	
	Version 0.1 - September 19, 2013 - Chris Magagna
	
	This will upgrade a CC3000 to:
		Service Pack Version 1.24
		Release Package 1.11.1
						
	This code released into the public domain, with no usefulness implied blah
	blah blah.
	
!!!!WARNING-WARNING-WARNING-WARNING-WARNING-WARNING-WARNING-WARNING-WARNING
!	
!	WARNING! This will delete the current firmware on your CC3000. If this
!	upgrade fails you will be left with a useless piece of metal where
!	your nice WiFi module used to be.
!	
!!!!WARNING-WARNING-WARNING-WARNING-WARNING-WARNING-WARNING-WARNING-WARNING
	
	Having said that, I've tested this code now on 2 CC3000 modules I made
	myself, an AdaFruit CC3000 breakout
	(http://www.adafruit.com/products/1469), and an official TI CC3000EM
	(https://estore.ti.com/CC3000EM-CC3000-Evaluation-Module-P4257.aspx)
	and so far so good.
	
	This code has been tested with an Arduino Nano upgraded to the OptiBoot
	bootloader and a Uno R3. The code takes up nearly the entire 32K Flash
	space of an ATMega328 so it won't fit on a board with a larger
	bootloader, e.g. the Duemilanove, stock Nano, etc.
	
	This version does NOT work with the Teensy 3. I'm working on it!
	
	Many thanks to AdaFruit for the Adafruit_CC3000 library.
	
	This code is based on a combination of the TI patch programmers:
	PatchProgrammerMSP430F5529_6_11_7_14_24windows_installer.exe
	PatchProgrammerMSP430FR5739_1_11_7_14_24windows_installer.exe
	Both are available on the T.I. website.
	
	To use:
	
	1. Quit the Arduino IDE, if running
	2. Move the folder "Adafruit_CC3000_Library" out of your Library. This code
		uses a custom version of their library, and the two can't coexist. You
		won't be able to compile / upload this program if Arduino sees them both.
	3. Edit the pin assignments CC3000_IRQ, CC3000_VBAT, and CC3000_CS for your
		setup, as necessary.
	4. Wire up your CC3000 to your Arduino
	5. Upload this sketch to your Arduino
	6. Open the Serial Monitor with baud rate 115200
	7. Use option 0 to verify everything is working (see notes below)
	8. Use option 4Y to back up your CC3000's info to your Arduino's EEPROM
	9. Use option 6Y erase the CC3000's current firmware
	10. Use option 7Y to restore your CC3000's info from Arduino EEPROM
	11. Use option 8Y to update the first part of the new firmware
	12. Use option 9Y to update the second part of the new firmware
	13. Use option 0 to verify the CC3000 is now working with your new firmware
	
	For options 0 - 3 you just type in the number and hit Enter or press 'Send'
	but options 4-9 require you to also type UPPERCASE Y or D to confirm you
	want to make a (potentially) destructive change.
	
	If the CC3000 initializes but you can't read the MAC or the firmware
	version then everything else may be working but the CC3000's NVRAM may be
	corrupt. You may be able to recover from this by:
	
	1. Restart your Arduino
	2. Use option 5D to generate a new random MAC address and load the T.I.
		defaults
	3. Use option 6Y erase the CC3000's current firmware
	4. Use option 7Y to restore your CC3000's info from Arduino EEPROM
	5. Use option 8Y to update the first part of the new firmware
	6. Use option 9Y to update the second part of the new firmware
	7. Use option 0 to verify the CC3000 is now working with your new firmware
	
*/











#include <SPI.h>
#include <EEPROM.h>

#include "Adafruit_CC3000_4Patching.h"
#include "ccspi.h"
#include "nvmem.h"
#include "wlan.h"


#define COMMAND_BUFFER_SIZE	32

#define CC3000_IRQ   11  // MUST be an interrupt pin!
#define CC3000_VBAT  13
#define CC3000_CS    10



#define MAC_SIZE	6		// how many bytes in a MAC address
byte macBuffer[MAC_SIZE];	// the buffer for the MAC addr
#define MAC_ADDRESS_EEPROM_LOCATION	0x10	// where in EEPROM to write the MAC address for safekeeping


#define RM_SIZE		128		// how many bytes in the CC3000 RM file
byte rmBuffer[RM_SIZE];		// our RM buffer
#define RM_ADDRESS_EEPROM_LOCATION 0x20		// where in EEPROM to write the RM file for safekeeping



Adafruit_CC3000 cc3000 = Adafruit_CC3000(CC3000_CS, CC3000_IRQ, CC3000_VBAT, SPI_CLOCK_DIV2);





#define BIT0	B1
#define BIT1	B10
#define BIT2	B100
#define BIT3	B1000
#define BIT4	B10000
#define BIT5	B100000
#define BIT6	B1000000
#define BIT7	B10000000



/* 
	Some things are different for the Teensy 3.0, so set a flag if we're using
	that hardware.
*/

#if defined(__arm__) && defined(CORE_TEENSY) && defined(__MK20DX128__)
#define TEENSY3   1
#endif













#ifdef TEENSY3
const uint8_t wlan_drv_patch[8168] =
#else
PROGMEM const unsigned char wlan_drv_patch[0] =
#endif
{};

unsigned short drv_length = 8024;


#ifdef TEENSY3 
const uint8_t fw_patch[4616] = 
#else
PROGMEM const unsigned char fw_patch[0] =
#endif
{};

unsigned short fw_length = 5700;

#ifdef TEENSY3
const uint8_t cRMdefaultParams[128] =
#else
PROGMEM const unsigned char cRMdefaultParams[128] =
#endif
{ 0x03, 0x00, 0x01, 0x01, 0x14, 0x14, 0x00, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x23, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x23, 0x23, 0x23, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x01, 0x77, 0x80, 0x1D, 0x1F, 0x22, 0x26, 0x28, 0x29, 0x1A, 0x1F, 0x22, 0x24, 0x26, 0x28, 0x16, 0x1D, 0x1E, 0x20, 0x24, 0x25, 0x1E, 0x2D, 0x01, 0x02, 0x02, 0x02, 0x02, 0x00, 0x15, 0x15, 0x15, 0x11, 0x15, 0x15, 0x0E, 0x00};



























void PrintMACAddress(void) {
	for (int i=0; i<MAC_SIZE; i++) {
		if (i!=0) {
			Serial.print(F(":"));
			}
		Serial.print(macBuffer[i], HEX);
		}
	Serial.println();
	}
	




void StartCC3000Driverless(void) {
	Serial.println(F("    Starting CC3000 with no patches"));
	cc3000.begin(2);
	Serial.println(F("        Started"));
	}
	
	
void StopCC3000(void) {
	Serial.println(F("    Stopping CC3000...."));
	wlan_stop();
	Serial.println(F("        Stopped"));
	}



void BasicCC30000Test(void) {
	char printBuffer[20];
	
	Serial.println(F("Testing the CC3000..."));
	
	Serial.println(F("    Initializing hardware..."));
	if (!cc3000.begin()) {
		Serial.println(F("        Unable to initialize the CC3000! Check your wiring! Aborting..."));
		for (;;);
		}
		
	Serial.println(F("    CC3000 initalized. Getting info..."));		
		
	if(nvmem_get_mac_address(macBuffer)!=0)  {
		Serial.println(F("        Unable to retrieve MAC Address! Aborting..."));
		for (;;);
		}
	else {
		Serial.print(F("        MAC Address: "));
		PrintMACAddress();
		}
		
	byte versionInfo[2];
	
	if (nvmem_read_sp_version(versionInfo)!=0) {
		Serial.println(F("        Unable to retrive firmware version! Aborting..."));
		for (;;);
		}
	else {
		Serial.print(F("        Firmware version: "));
		Serial.print(versionInfo[0]);
		Serial.print(F("."));
		Serial.println(versionInfo[1]);
		}
		
	Serial.print(F("    RX buffer size: "));
	Serial.print(CC3000_RX_BUFFER_SIZE);
	Serial.println(F(" bytes"));
	
	Serial.print(F("    TX buffer size: "));
	Serial.print(CC3000_TX_BUFFER_SIZE);
	Serial.println(F(" bytes"));
	
	StopCC3000();
    
	}













void PrintRMRecord(void) {
	for (int i=0; i<8; i++) {
		Serial.print(F("        "));
		for (int j=0; j<16; j++) {
			if (rmBuffer[i*16+j]<16) {
				Serial.print(F(" "));
				}
			Serial.print(rmBuffer[i*16+j], HEX);
			Serial.print(F(" "));
			}
		Serial.println();
		}
	}




void ShowEEPROM(void) {

	Serial.println(F("Show MAC and radio file info from Arduino EEPROM"));

	Serial.println(F("    Reading MAC address data from EEPROM"));
	for (int i=0; i<MAC_SIZE; i++) {
		macBuffer[i] = EEPROM.read(MAC_ADDRESS_EEPROM_LOCATION+i);
		}
		
	Serial.println(F("    Reading RM record data from EEPROM"));
	for (int i=0; i<RM_SIZE; i++) {
		rmBuffer[i] = EEPROM.read(RM_ADDRESS_EEPROM_LOCATION+i);
		}
		
	Serial.print(F("    MAC address: "));
	PrintMACAddress();
		
	Serial.println(F("    Radio data:"));
	PrintRMRecord();
	}





void GetCC3000Info(bool copyToEEPROM) {

	if (copyToEEPROM) {
		Serial.println(F("Copy CC3000 MAC and radio file to EEPROM"));
		}
	else {
		Serial.println(F("Show CC3000 MAC and radio file"));
		}

	StartCC3000Driverless();
		
	Serial.println(F("    Reading MAC address"));
	byte mac_status = nvmem_get_mac_address(macBuffer);
	if (mac_status==0) {
		Serial.print(F("        MAC address read successful. MAC is: "));
		PrintMACAddress();
		
		if (copyToEEPROM) {
			Serial.println(F("        Writing MAC address to Arduino EEPROM..."));
			for (int i=0; i<MAC_SIZE; i++) {
				EEPROM.write(MAC_ADDRESS_EEPROM_LOCATION+i, macBuffer[i]);
				}
			Serial.println(F("        Verifying..."));
			for (int i=0; i<MAC_SIZE; i++) {
				if (EEPROM.read(MAC_ADDRESS_EEPROM_LOCATION+i)  != macBuffer[i]) {
					Serial.println(F("            Verify failed. Aborting."));
					for (;;);
					}
				}
			Serial.println(F("        Verify OK"));
			}
		}
	else {
		Serial.print(F("         Unable to read MAC address. "));
		if (copyToEEPROM) {
			Serial.print(F("Will not save to EEPROM"));
			}
		Serial.println();
		}
		
		
	Serial.println(F("    Reading radio record"));
	
	// The TI PatchProgrammer reads this 128 byte record as 16 chunks of 8 bytes, so that's what we'll do
	for (int i=0; i<16; i++) {
		byte rval = nvmem_read(NVMEM_RM_FILEID, 8, 8*i, (unsigned char *)&rmBuffer[i*8]);
		if (rval!=0) {
			Serial.print(F("    Reading chunk "));
			Serial.print(i);
			Serial.print(F(" failed. Aborting."));
			for (;;);
			}
		}
	Serial.println(F("        Read OK. Radio data:"));
	PrintRMRecord();
		
	if (copyToEEPROM) {
		Serial.println(F("        Writing radio record to Arduino EEPROM..."));
		for (int i=0; i<RM_SIZE; i++) {
			EEPROM.write(RM_ADDRESS_EEPROM_LOCATION+i, rmBuffer[i]);
			}
		Serial.println(F("        Verifying..."));
		for (int i=0; i<RM_SIZE; i++) {
			if (EEPROM.read(RM_ADDRESS_EEPROM_LOCATION+i)  != rmBuffer[i]) {
				Serial.println(F("            Verify failed. Aborting."));
				for (;;);
				}
			}
		Serial.println(F("        Verify OK"));
		}

	StopCC3000();
	
	}














void UseDefaults(void) {
	Serial.println(F("Make random MAC and copy default Radio File to Arduino EEPROM"));
	
	Serial.println(F("    Generating random MAC..."));
	for (int i=0; i<MAC_SIZE; i++) {
		EEPROM.write(MAC_ADDRESS_EEPROM_LOCATION+i, random(256));
		}
		
	Serial.println(F("    Copying default RM file data..."));
	for (int i=0; i<RM_SIZE; i++) {
		#ifdef TEENSY3
		byte zaz = cRMdefaultParams[i];
		#else
		byte zaz = pgm_read_byte(cRMdefaultParams + i);
		#endif
		EEPROM.write(RM_ADDRESS_EEPROM_LOCATION+i, zaz);
		}
	Serial.println(F("        Verifying..."));
	for (int i=0; i<RM_SIZE; i++) {
		#ifdef TEENSY3
		byte zaz = cRMdefaultParams[i];
		#else
		byte zaz = pgm_read_byte(cRMdefaultParams + i);
		#endif
		if (EEPROM.read(RM_ADDRESS_EEPROM_LOCATION+i)  != zaz) {
			Serial.println(F("            Verify failed. Aborting."));
			for (;;);
			}
		}
	Serial.println(F("        Verify OK"));
		
	Serial.println(F("    Done!"));
	}




















//*****************************************************************************
//
//! fat_read_content
//!
//! \param[out] is_allocated  array of is_allocated in FAT table:\n
//!						 an allocated entry implies the address and length of the file are valid.
//!  		    			 0: not allocated; 1: allocated.
//! \param[out] is_valid  array of is_valid in FAT table:\n
//!						 a valid entry implies the content of the file is relevant.
//!  		    			 0: not valid; 1: valid.
//! \param[out] write_protected  array of write_protected in FAT table:\n
//!						 a write protected entry implies it is not possible to write into this entry.
//!  		    			 0: not protected; 1: protected.
//! \param[out] file_address  array of file address in FAT table:\n
//!						 this is the absolute address of the file in the EEPROM.
//! \param[out] file_length  array of file length in FAT table:\n
//!						 this is the upper limit of the file size in the EEPROM.
//!
//! \return on succes 0, error otherwise
//!
//! \brief  parse the FAT table from eeprom 
//
//*****************************************************************************
unsigned char fat_read_content(unsigned char *is_allocated,
                               unsigned char *is_valid,
                               unsigned char *write_protected,
                               unsigned short *file_address,
                               unsigned short *file_length)
{
	unsigned short  index;
	unsigned char   ucStatus;
	unsigned char   fatTable[48];
	unsigned char*  fatTablePtr = fatTable;
	
	// read in 6 parts to work with tiny driver
	for (index = 0; index < 6; index++)
	{
		ucStatus = nvmem_read(16, 8, 4 + 8*index, fatTablePtr); 
		fatTablePtr += 8;
	}
	
	fatTablePtr = fatTable;
	
	for (index = 0; index <= NVMEM_RM_FILEID; index++)
	{
		*is_allocated++ = (*fatTablePtr) & BIT0;
		*is_valid++ = ((*fatTablePtr) & BIT1) >> 1;
		*write_protected++ = ((*fatTablePtr) & BIT2) >> 2;
		*file_address++ = (*(fatTablePtr+1)<<8) | (*fatTablePtr) & (BIT4|BIT5|BIT6|BIT7);
		*file_length++ = (*(fatTablePtr+3)<<8) | (*(fatTablePtr+2)) & (BIT4|BIT5|BIT6|BIT7);
		
		fatTablePtr += 4;  // move to next file ID
	}
	
	return ucStatus;
}






#define NUM_FAT_RECORDS	(NVMEM_RM_FILEID + 1)

void ShowFAT(void) {

	unsigned char isAllocated[NUM_FAT_RECORDS],
		isValid[NUM_FAT_RECORDS],
		writeProtected[NUM_FAT_RECORDS];
	unsigned short fileAddress[NUM_FAT_RECORDS], fileLength[NUM_FAT_RECORDS];
	
	Serial.println(F("CC3000 File Table"));	

	StartCC3000Driverless();	
			
	Serial.println(F("    Reading FAT"));
	byte status = fat_read_content(isAllocated, isValid, writeProtected, fileAddress, fileLength);
	if (status==0) {
		Serial.println(F("        FAT read successful"));
		Serial.println(F("              #   A   V   W   Addr    Len"));
		Serial.println(F("             ----------------------------"));
		for (int i=0; i<NUM_FAT_RECORDS; i++) {
		
			Serial.print(F("             "));
			
			if (i<10) {
				Serial.print(F(" "));
				}
			Serial.print(i);
			Serial.print(F("   "));
			
			if (isAllocated[i]==1) {
				Serial.print(F("Y"));
				}
			else if (isAllocated[i]==0) {
				Serial.print(F("N"));
				}
			else {
				Serial.print(F("?"));
				}
			Serial.print(F("   "));
			
			if (isValid[i]==1) {
				Serial.print(F("Y"));
				}
			else if (isValid[i]==0) {
				Serial.print(F("N"));
				}
			else {
				Serial.print(F("?"));
				}
			Serial.print(F("   "));
			
			if (writeProtected[i]==1) {
				Serial.print(F("Y"));
				}
			else if (writeProtected[i]==0) {
				Serial.print(F("N"));
				}
			else {
				Serial.print(F("?"));
				}
			Serial.print(F("   "));
			
			if (fileAddress[i]<4096) { Serial.print(F(" ")); }
			if (fileAddress[i]<256) { Serial.print(F(" ")); }
			if (fileAddress[i]<16) { Serial.print(F(" ")); }
			Serial.print(fileAddress[i], HEX);
			Serial.print(F("   "));
			
			if (fileLength[i]<4096) { Serial.print(F(" ")); }
			if (fileLength[i]<256) { Serial.print(F(" ")); }
			if (fileLength[i]<16) { Serial.print(F(" ")); }
			Serial.print(fileLength[i], HEX);
			
			Serial.println();
			}
		Serial.println(F("             ----------------------------"));
		Serial.println(F("             A=Active  V=Valid  W=Write Protected  Addr=Start address  Len=Length"));
		Serial.println(F("             ----------------------------"));
		}
	else {
		Serial.println(F("        Unable to read FAT."));
		}
		
	StopCC3000();
	}




















// 2 dim array to store address and length of new FAT
const unsigned short aFATEntries[2][NVMEM_RM_FILEID + 1] = 
/*  address 	*/  {{0x50, 	0x1f0, 	0x390, 	0x1390, 	0x2390, 	0x4390, 	0x6390, 	0x63a0, 	0x63b0, 	0x63f0, 	0x6430, 	0x6830},
/*  length	*/	{0x1a0, 	0x1a0, 	0x1000, 	0x1000, 	0x2000, 	0x2000, 	0x10, 	0x10, 	0x40, 	0x40, 	0x400, 	0x200}};
/* 0. NVS */
/* 1. NVS Shadow */
/* 2. Wireless Conf */
/* 3. Wireless Conf Shadow */
/* 4. BT (WLAN driver) Patches */
/* 5. WiLink (Firmware) Patches */
/* 6. MAC addr */
/* 7. Frontend Vars */
/* 8. IP config */
/* 9. IP config Shadow */
/* 10. Bootloader Patches */
/* 11. Radio Module params */
/* 12. AES128 for smart config */
/* 13. user file */
/* 14. user file */
/* 15. user file */



//*****************************************************************************
//
//! fat_write_content
//!
//! \param[in] file_address  array of file address in FAT table:\n
//!						 this is the absolute address of the file in the EEPROM.
//! \param[in] file_length  array of file length in FAT table:\n
//!						 this is the upper limit of the file size in the EEPROM.
//!
//! \return on succes 0, error otherwise
//!
//! \brief  parse the FAT table from eeprom 
//
//*****************************************************************************
unsigned char fat_write_content(
																unsigned short const *file_address,
																unsigned short const *file_length)
{
	unsigned short  index = 0;
	unsigned char   ucStatus;
	unsigned char   fatTable[48];
	unsigned char*  fatTablePtr = fatTable;
	
	// first, write the magic number
	ucStatus = nvmem_write(16, 2, 0, (unsigned char *)"LS");
	
	for (; index <= NVMEM_RM_FILEID; index++)
	{
		// write address low char and mark as allocated
		*fatTablePtr++ = (unsigned char)(file_address[index] & 0xff) | BIT0;
		
		// write address high char
		*fatTablePtr++ = (unsigned char)((file_address[index]>>8) & 0xff);
		
		// write length low char
		*fatTablePtr++ = (unsigned char)(file_length[index] & 0xff);
		
		// write length high char
		*fatTablePtr++ = (unsigned char)((file_length[index]>>8) & 0xff);		
	}
	
	// second, write the FAT
	// write in two parts to work with tiny driver
	ucStatus = nvmem_write(16, 24, 4, fatTable); 
	ucStatus = nvmem_write(16, 24, 24+4, &fatTable[24]); 
	
	// third, we want to erase any user files
	memset(fatTable, 0, sizeof(fatTable));
	ucStatus = nvmem_write(16, 16, 52, fatTable); 
	
	return ucStatus;
}





void WriteFAT(void) {

	Serial.println(F("Writing File Table to CC3000"));

	StartCC3000Driverless();
	
	Serial.println(F("    Writing FAT"));

	byte rval = fat_write_content(aFATEntries[0], aFATEntries[1]);
	if (rval==0) {
		Serial.println(F("        Successful!"));
		}
	else {
		Serial.print(F("        Problem writing FAT. Aborting on error "));
		Serial.println(rval);
		for(;;);
		}
	
	StopCC3000();
	}

















void CopyInfoToNVRAM(void) {

	Serial.println(F("Writing Mac and RM record to CC3000"));

	StartCC3000Driverless();
		
	Serial.println(F("    Reading MAC address data from EEPROM"));
	for (int i=0; i<MAC_SIZE; i++) {
		macBuffer[i] = EEPROM.read(MAC_ADDRESS_EEPROM_LOCATION+i);
		}
	Serial.println(F("    Writing MAC to CC3000..."));
	byte rval = nvmem_set_mac_address(macBuffer);
	if (rval==0) {
		Serial.println(F("        Successful."));
		}
	else {
		Serial.println(F("        Problem writing MAC address. Aborting."));
		for (;;);
		}
		
	Serial.println(F("    Reading RM record data from EEPROM"));
	for (int i=0; i<RM_SIZE; i++) {
		rmBuffer[i] = EEPROM.read(RM_ADDRESS_EEPROM_LOCATION+i);
		}
	Serial.println(F("    Writing RM record to CC3000..."));
	// TI writes the RM record back as 4 chunks of 32 bytes, so that's what we'll do
	for (int i=0; i<4; i++) {
		rval = nvmem_write(NVMEM_RM_FILEID, 32, 32*i, (unsigned char *)&rmBuffer[i*32]);
		if (rval!=0) {
			Serial.print(F("        Problem writing RM record, chunk "));
			Serial.print(i);
			Serial.print(F(". Aborting."));
			for(;;);
			}
		}
	Serial.println(F("        Successful."));

	
	StopCC3000();
	}










void WritePatch(bool writeDriver) {
	if (writeDriver) {
		Serial.println(F("Write new driver to CC3000"));
		}
	else {
		Serial.println(F("Write new firmware to CC3000"));
		}
		
	StartCC3000Driverless();
	
	byte rval;
	
	Serial.println(F("    Writing data..."));
	if (writeDriver) {
		rval = nvmem_write_patch(NVMEM_WLAN_DRIVER_SP_FILEID, drv_length, wlan_drv_patch);
		}
	else {
		rval = nvmem_write_patch(NVMEM_WLAN_FW_SP_FILEID, fw_length, fw_patch);
		}
		
	if (rval==0) {
		Serial.println(F("        Successful!"));
		}
	else {
		Serial.println(F("        Problem writing patch. Aborting."));
		for(;;);
		}
	
	StopCC3000();
	}














void DisplayMenu(void) {
	Serial.println(F("\n\nCommand options:"));
	
	Serial.println(F("    0  - Test the CC3000"));
	Serial.println(F("    1  - Show MAC address and radio file on CC3000"));
	Serial.println(F("    2  - Show MAC address and radio file in Arduino EEPROM"));
	Serial.println(F("    3  - Show current CC3000 file table"));
	Serial.println(F("    4Y - Copy MAC address and radio file from CC3000 to Arduino EEPROM"));
	Serial.println(F("    5D - Copy default MAC and radio file to Arduino EEPROM"));
	Serial.println(F("    6Y - Write a new file table to the CC3000"));
	Serial.println(F("    7Y - Copy MAC address and radio file from Arduino EEPROM to CC3000"));
	Serial.println(F("    8Y - Write new driver to CC3000"));
	Serial.println(F("    9Y - Write new firmware to CC3000"));

	Serial.println();
	Serial.println();
	}
	
	
	
void CommandProcessor(char *buffer) {
	if (buffer[0]=='0') {
		BasicCC30000Test();
		}
	else if (buffer[0]=='1') {
		GetCC3000Info(false);
		}
	else if (buffer[0]=='2') {
		ShowEEPROM();
		}
	else if (buffer[0]=='3') {
		ShowFAT();
		}
	else if ((buffer[0]=='4') && (buffer[1]=='Y')) {
		GetCC3000Info(true);
		}
	else if ((buffer[0]=='5') && (buffer[1]=='D')) {
		UseDefaults();
		}
	else if ((buffer[0]=='6') && (buffer[1]=='Y')) {
		WriteFAT();
		}
	else if ((buffer[0]=='7') && (buffer[1]=='Y')) {
		CopyInfoToNVRAM();
		}
	else if ((buffer[0]=='8') && (buffer[1]=='Y')) {
		WritePatch(true);
		}
	else if ((buffer[0]=='9') && (buffer[1]=='Y')) {
		WritePatch(false);
		}
	else {
		Serial.print(F("Unrecognized command \""));
		Serial.print(buffer);
		Serial.println(F("\""));
		}
	}







void setup(void) {
	Serial.begin(115200);

	Serial.println(F("The Arduino CC3000 Firmware 1.24 Patcher"));
	Serial.println(F("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-"));
	}




	
	
void loop(void) {
	char buffer[COMMAND_BUFFER_SIZE+1];	// 1 extra byte to allow room for the terminating null
	
	memset(buffer, 0, COMMAND_BUFFER_SIZE+1);
	int offset=0;
	
	DisplayMenu();
	
	for (;;) {
		if (Serial.available()) {
			char ch = Serial.read();
			if (ch == '\n') {
				break;
				}
			buffer[offset++] = ch;
			if (offset >= COMMAND_BUFFER_SIZE) {
				offset = COMMAND_BUFFER_SIZE-1;
				}
			}
		}
		
	CommandProcessor(buffer);		
	}
	







