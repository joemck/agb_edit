#ifndef __GBACIA_H__
#define __GBACIA_H__

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <malloc.h>	//XXX: Windows only. Linux uses alloca.h.

//define signed/unsigned data types by size if we don't already have them
#ifndef u8
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
#endif

//Save type enum
enum saveType {
	EEPROM_8K_SMALLROM       = 0x0,
	EEPROM_8K_256MROM        = 0x1,
	EEPROM_64K_SMALLROM      = 0x2,
	EEPROM_64K_256MROM       = 0x3,
	FLASH_512K_ATMEL_RTC     = 0x4,
	FLASH_512K_ATMEL         = 0x5,
	FLASH_512K_SST_RTC       = 0x6,
	FLASH_512K_SST           = 0x7,
	FLASH_512K_PANASONIC_RTC = 0x8,
	FLASH_512K_PANASONIC     = 0x9,
	FLASM_1M_MACRONIX_RTC    = 0xa,
	FLASM_1M_MACRONIX        = 0xb,
	FLASM_1M_SANYO_RTC       = 0xc,
	FLASM_1M_SANYO           = 0xd,
	SRAM_256K                = 0xe,
	NO_SAVE                  = 0xf
};

//Button enum -- OR these together to make a button mask
enum buttonBits {
	BTN_A      = 1<<0,
	BTN_B      = 1<<1,
	BTN_SELECT = 1<<2,
	BTN_START  = 1<<3,
	BTN_RIGHT  = 1<<4,
	BTN_LEFT   = 1<<5,
	BTN_UP     = 1<<6,
	BTN_DOWN   = 1<<7,
	BTN_R      = 1<<8,
	BTN_L      = 1<<9,
	BTN_X      = 1<<10,	//X&Y are (3)DS only and not used with AGB_FIRM
	BTN_Y      = 1<<11	//they're only included in this enum for completeness
};

//save config bytes -- these are the registers the 0x10 byte "safe config" field go into
//seems to configure how many CPU cycles the fake save chip takes to do various operations
//possibly interesting to put lower values for faster saving in games with slowish saving like Pokemon
struct saveConfig {	//0x10 bytes
	u32 flashChipEraseCycles;
	u32 flashSectorEraseCycles;
	u32 flashProgramCycles;
	u32 eepromWriteCycles;
} __attribute__((aligned(1)));

//AGB_FIRM config block
struct config {	//0x324 bytes
	u32 padding0;
	u32 romSize;
	u32 saveType;	//use enum saveType
	u16 padding1;
	u16 sleepButtons;	//use enum buttonBits values OR'd together
	struct saveConfig saveConfig;	//0x10 bytes
	u32 lcdGhosting;	//01-FF, lower value=more ghosting
	u8 videoLUT[3 * 256];	//0x300 bytes, 256 RGB triplets
} __attribute__((aligned(1)));

//AGB_FIRM section descriptor
struct sectionDescriptor {	//0x10
	u32 type;	//0=ROM (must be at offset 0); 1=config
	u32 offset;	//offset to section in file
	u32 size;	//size of section, should be 0x324 for the config, romSize for the ROM
	u32 padding;
} __attribute__((aligned(1)));

//AGB_FIRM footer
struct footer {	//0x10 bytes
	u32 magic;	//'.CAA'
	u32 active;	//must be 1
	u32 offset;	//offset to array of section descriptors
	u32 nDesc;	//number of section descriptors << 4
} __attribute__((aligned(1)));



//values that we'll prompt for and set in the cia (defined in gbacia.c)
extern int onlyInfo, dumpRom, extractAll, setSleepButtons, setLcdGhosting, setVideoLUT;
extern u16 sleepButtons;
extern u32 lcdGhosting;
extern u8 videoLUT[3 * 256];
extern char tmpName[4096];	//where we construct the name of the temp dir for dumping

//function declarations
const char* saveTypeToString(u32 saveType);
const char* sectionTypeToString(u32 sectionType);
const char* decodeButtons(u16 mask);
u16 encodeButtons(const char *buttons);
const char* processCodeBin(const char *codeBin, const char *ciaName);
const char* process(char *fname);
void cleanup(void);

#endif /* __GBACIA_H__ */
