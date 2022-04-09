/* agb_edit GBA VC cia handling functions */

#include "gbacia.h"
#include "console_ui.h"

//values that we'll prompt for and set in the cia
int onlyInfo = 0, dumpRom = 0, extractAll = 0, setSleepButtons = 0, setLcdGhosting = 0, setVideoLUT = 0;
u16 sleepButtons = 0;
u32 lcdGhosting = 0;
u8 videoLUT[3 * 256] = {0};

char tmpName[4096];	//where we construct the name of the temp dir for dumping

//button names for button encoding and decoding functions
//                                     0    1    2         3        4        5       6     7       8    9    10   11
static const char *buttonNames[12] = {"A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y"};
//reorder the button names to look prettier when displayed
static const int buttonOrder[12] = {/*A*/0, /*B*/1, /*X*/10, /*Y*/11, /*L*/9, /*R*/8, /*Up*/6, /*Down*/7, /*Left*/5, /*Right*/4, /*Start*/3, /*Select*/2};

//lookup save type names
const char* saveTypeToString(u32 saveType) {
	switch(saveType) {
		case EEPROM_8K_SMALLROM:	//0
			return "EEPROM 8k with ROM < 256 Mbit";
		case EEPROM_8K_256MROM:	//1
			return "EEPROM 8k with 256 Mbit ROM";
		case EEPROM_64K_SMALLROM:	//2
			return "EEPROM 64k with ROM < 256 Mbit";
		case EEPROM_64K_256MROM:	//3
			return "EEPROM 64k with 256 Mbit ROM";
		case FLASH_512K_ATMEL_RTC:	//4
			return "Flash 512k (Atmel) with RTC";
		case FLASH_512K_ATMEL:	//5
			return "Flash 512k (Atmel)";
		case FLASH_512K_SST_RTC:	//6
			return "Flash 512k (SST) with RTC";
		case FLASH_512K_SST:	//7
			return "Flash 512k (SST)";
		case FLASH_512K_PANASONIC_RTC:	//8
			return "Flash 512k (Panasonic) with RTC";
		case FLASH_512K_PANASONIC:	//9
			return "Flash 512k (Panasonic)";
		case FLASM_1M_MACRONIX_RTC:	//a
			return "Flash 1M (Macronix) with RTC";
		case FLASM_1M_MACRONIX:	//b
			return "Flash 1M (Macronix)";
		case FLASM_1M_SANYO_RTC:	//c
			return "Flash 1M (Sanyo) with RTC";
		case FLASM_1M_SANYO:	//d
			return "Flash 1M (Sanyo)";
		case SRAM_256K:	//e
			return "SRAM 256k";
		default:
			return "No save";
	}
}

//lookup section type name
const char* sectionTypeToString(u32 sectionType) {
	switch(sectionType) {
		case 0: return "ROM";
		case 1: return "Config";
		default: return "Unknown";
	}
}

//decode sleep button list into a string like "L R Select"
const char* decodeButtons(u16 mask) {
	static char result[512];

	result[0] = '\0';	//clear any string that was there
	for(int i=0; i<12; i++) {
		int btnIdx = buttonOrder[i];	//display buttons in a more intuitive order
		if(mask & (1<<btnIdx)) {
			if(result[0] != '\0')
				strcat(result, " ");
			strcat(result, buttonNames[btnIdx]);
		}
	}
	if(result[0] == '\0')
		return "(None)";
	else
		return result;
}

//encode buttons list to a string
//Normal format is like "L R Select", but we take any alpha chars as the name and anything else as separators
u16 encodeButtons(const char *buttons) {
	char tmp[32];
	int i, j=0, k;
	u16 btns = 0;

	for(i=0; buttons[i]; i++) {
		if(isalpha(buttons[i])) {
			if(j<sizeof(tmp)-1)
				tmp[j++] = buttons[i];
		}
		if(j > 0 && (!isalpha(buttons[i]) || buttons[i+1]=='\0')) {
			tmp[j] = '\0';
			for(k=0; k<10; k++) {
				if(0 == strcasecmp(buttonNames[k], tmp)) {
					btns |= (1 << k);
					break;
				}
			}
			if(k == 10) {
				printf("ERROR: '%s' isn't a recognized button.\n", tmp);
				return 0xffff;
			}
			j=0;
		}
	}
	return btns;
}

//process this code.bin file -- later we will modify it but for now we just print info
//returns a string on failure, NULL on success
//ciaName is just so we can dump the ROM to a suitable filename
const char* processCodeBin(const char *codeBin, const char *ciaName) {
	struct footer ftr;
	struct sectionDescriptor *sec;
	struct config cfg;
	long fpos;
	u32 cfgOffset;
	size_t nread;
	int nCfg, nErr, i;
	const char *result;
	FILE *fp = fopen(codeBin, "rb+");
	if(!fp) return "can't open code.bin";

	//footer is at the very end of the file
	if(0 != fseek(fp, -(ssize_t)sizeof(struct footer), SEEK_END)) { fclose(fp); return "can't seek to footer"; }
	fpos = ftell(fp);
	if(fpos < 0) { fclose(fp); return "bad seek to footer"; }
	nread = fread(&ftr, 1, sizeof(struct footer), fp);
	if(nread != sizeof(struct footer)) { fclose(fp); return "can't read footer"; }

	//print data before checking, so user can see it even if there's a problem
	printf("==== DUMPING INFO FROM FOOTER ====\n>> main footer >>\n");
	char magic[5];
	*((u32*)magic) = ftr.magic;
	magic[4] = '\0';
	printf("Magic: 0x%08x ('%4s')\n", ftr.magic, magic);
	if(ftr.magic != 0x4141432e) {
		printf("BAD magic value!\n");
		fclose(fp);
		return "bad footer magic value";
	}
	printf("Active: %d\n", ftr.active);
	if(ftr.active != 1) {
		printf("Footer active isn't 1!\n");
		fclose(fp);
		return "bad footer active value";
	}
	printf("Offset to descriptors: 0x%x\n", ftr.offset);
	printf("Number of descriptors: %d\n", ftr.nDesc>>4);

	//read the section descriptor array
	if(0 != fseek(fp, ftr.offset, SEEK_SET)) { fclose(fp); return "can't seek to descriptors"; }
	fpos = ftell(fp);
	if(fpos != ftr.offset) { fclose(fp); return "bad seek to descriptors"; }
	sec = alloca((ftr.nDesc>>4) * sizeof(struct sectionDescriptor));
	if(!sec) { fclose(fp); return "can't allocate memory (sec)"; }
	nread = fread(sec, sizeof(struct sectionDescriptor), ftr.nDesc>>4, fp);
	if(nread != ftr.nDesc>>4) { fclose(fp); return "can't read section table"; }

	//print sections, read and print configs
	nCfg = 0;
	nErr = 0;
	cfgOffset = 0xffffffff;
	for(i=0; i<ftr.nDesc>>4; i++) {
		printf(" >> section %d/%d >>\n", i+1, ftr.nDesc>>4);
		printf(" Type: %s (%d)\n", sectionTypeToString(sec[i].type), sec[i].type);
		printf(" Offset to data: 0x%x\n", sec[i].offset);
		printf(" Size of data: 0x%x\n", sec[i].size);
		printf(" Padding value: 0x%08x\n", sec[i].padding);
		//further processing only if this is a config
		if(sec[i].type == 1) {
			if(sec[i].size == sizeof(struct config) && sec[i].offset != 0 && sec[i].offset != 0xffffffff) {
				//read the config
				if(0 != fseek(fp, sec[i].offset, SEEK_SET)) { fclose(fp); return "can't seek to config"; }
				fpos = ftell(fp);
				if(fpos != sec[i].offset) { fclose(fp); return "bad seek to config"; }
				nread = fread(&cfg, 1, sizeof(struct config), fp);
				if(nread != sizeof(struct config)) { fclose(fp); return "can't read config"; }
				//print its info
				printf("  >> config data >>\n");
				printf("  Padding value: 0x%08x\n", cfg.padding0);
				printf("  ROM size: 0x%x\n", cfg.romSize);
				printf("  Save type: %s (0x%x)\n", saveTypeToString(cfg.saveType), cfg.saveType);
				printf("  Padding value: 0x%04x\n", cfg.padding1);
				printf("  Sleep buttons: 0x%04x => %s\n", cfg.sleepButtons, decodeButtons(cfg.sleepButtons));
				printf("   >> save chip config >>\n");
				printf("   Flash: bus cycles to erase the whole chip: %d\n", cfg.saveConfig.flashChipEraseCycles);
				printf("   Flash: bus cycles to erase a sector: %d\n", cfg.saveConfig.flashSectorEraseCycles);
				printf("   Flash: bus cycles to program a sector: %d\n", cfg.saveConfig.flashProgramCycles);
				printf("   EEPROM: bus cycles to perform a write: %d\n", cfg.saveConfig.eepromWriteCycles);
				printf("  LCD ghosting (01=lots; ff=none): %02x\n", cfg.lcdGhosting);
				printf("  Video LUT:\n");
				printVideoLUT(cfg.videoLUT, cfg.lcdGhosting);
				cfgOffset = sec[i].offset;
				++nCfg;
			} else if(sec[i].size != sizeof(struct config)) {
				printf("  !! Config section with WRONG size! Should be 0x324!\n");
				++nErr;
			} else {
				printf("  !! Config section with WRONG offset! Should not be 0 or 0xffffffff!\n");
				++nErr;
			}
		} else if(sec[i].type == 0) {
			if(sec[i].offset == 0) {
				if(dumpRom) {
					int romok=1;
					if(0 != fseek(fp, sec[i].offset, SEEK_SET)) romok=0;
					fpos = ftell(fp);
					if(fpos != sec[i].offset) romok=0;
					if(romok) {
						u8 *romdata = malloc(sec[i].size);
						if(romdata == NULL) romok=0;
						if(romok) {
							nread = fread(romdata, 1, sec[i].size, fp);
							if(nread != sec[i].size) romok=0;
							if(romok) {
								char romname[4096];
								strncpy(romname, ciaName, sizeof(romname));
								int ind=strlen(romname)-4;	//should put us at ".cia"
								if(0 == strcasecmp(&romname[ind], ".cia"))
									romname[ind] = '\0';	//lop off ".cia"
								strncat(romname, ".gba", sizeof(romname));
								FILE *romfp = fopen(romname, "wb");
								nread = fwrite(romdata, 1, sec[i].size, romfp);
								if(nread != sec[i].size) romok=0;
								fclose(romfp);
								if(romok)
									printf("  (raw GBA ROM data - dumped to '%s')\n", romname);
								else
									printf("  (raw GBA ROM data - failed to dump to '%s')\n", romname);
							}
							free(romdata);
						}
					}
				} else {
					printf("  (raw GBA ROM data)\n");
				}
			} else {
				printf("  !! ROM section with nonzero offset!\n  !! THIS WILL MAKE AGB_FIRM ERROR OUT!");
				++nErr;
			}
		} else {
			printf("  !! BAD SECTION TYPE %d!\n", sec[i].type);
			++nErr;
		}
	}
	printf("Number of config blocks: %d\n\n", nCfg);

	if(nErr == 0 && nCfg == 1) {
		//modify the config as requested
		if(setSleepButtons)
			cfg.sleepButtons = sleepButtons;
		if(setLcdGhosting)
			cfg.lcdGhosting = lcdGhosting;
		if(setVideoLUT)
			memcpy(cfg.videoLUT, videoLUT, sizeof(videoLUT));
		//write it back to code.bin if we changed it
		if(!onlyInfo) {
			if(0 != fseek(fp, cfgOffset, SEEK_SET)) { fclose(fp); return "can't seek to write config"; }
			fpos = ftell(fp);
			if(fpos != cfgOffset) { fclose(fp); return "bad seek to write config"; }
			nread = fwrite(&cfg, 1, sizeof(struct config), fp);
			if(nread != sizeof(struct config)) { fclose(fp); return "can't write config"; }
		}
		result = NULL;
	} else {
		if(!onlyInfo)
			printf("Cannot modify file with above problems!\n");
		result = "errors in config section";
	}

	fclose(fp);
	putchar('\n');
	return result;
}

//unpack and repack, calling processCodeBin with the path to the code.bin
const char* process(char *fname) {
	char cmd[8192];	//buffer to build command lines in
	char mainCxi[4096];	//name of main dumped cxi - official GBA VCs contain a second with a manual which we want to preserve but otherwise don't care about
	char dumpFile[4096];	//for enumerating dumped cxi's when rebuilding a cia
	char cmdPart[4096];	//additional buffer to build pieces of a command line in
	char fileNum[32], indNum[32];	//"file" and "index" numbers from the cxi name
	char newCiaName[4096];
	int i, j, nDumps;
	const char *resultStr = NULL;
	FILE *fp;
	printf("\n==> Processing %s\n", fname);

	//temp dir name stuff
	if(extractAll) {
		strncpy(tmpName, fname, sizeof(tmpName));
		i = strlen(tmpName) - 4;
		if(0 == strcasecmp(&tmpName[i], ".cia"))
			tmpName[i] = '\0';
		strncat(tmpName, ".dump", sizeof(tmpName));
	} else {
		strcpy(tmpName, "UNPACKTMP");
	}

	//clean & make the temp dir
	snprintf(cmd, sizeof(cmd), "rd /s /q \"%s\" 2>NUL", tmpName);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "mkdir \"%s\"", tmpName);
	system(cmd);

	//dump cia contents
	snprintf(cmd, sizeof(cmd), "progfiles\\ctrtool.exe --contents \"%s\\file\" \"%s\"", tmpName, fname);
	printf("==> %s\n", cmd);
	if(system(cmd)) return "ctrtool --contents failed";

	//find the main file -- NSUI uses a single cxi at 0:0, while Nintendo VCs have 0:2 and then a manual at 1:3
	//we can't assume a name, but it's probably safe to assume the largest file is the game
	//this dir command prints just the filenames of files, sorted largest to smallest, so we can just read line 1
	snprintf(cmd, sizeof(cmd), "dir \"%s\" /b /o-s", tmpName);
	fp = popen(cmd, "r");
	if(!fgets(mainCxi, sizeof(mainCxi), fp)) return "couldn't find cxi";
	pclose(fp);
	for(i=strlen(mainCxi)-1; i>=0 && isspace(mainCxi[i]); i--) mainCxi[i]='\0';	//trim newline/spaces from end

	//unpack the cxi
	snprintf(cmd, sizeof(cmd), "progfiles\\3dstool.exe -xtf cxi \"%s\\%s\" --header \"%s\\ncchheader.bin\" --exh \"%s\\exheader.bin\" --exefs \"%s\\exefs.bin\" --romfs \"%s\\romfs.bin\"",
			tmpName, mainCxi, tmpName, tmpName, tmpName, tmpName);
	printf("==> %s\n", cmd);
	if(system(cmd)) return "3dstool -xtf cxi failed";

	//unpack exefs
	snprintf(cmd, sizeof(cmd), "progfiles\\3dstool.exe -xtf exefs \"%s\\exefs.bin\" --exefs-dir \"%s\\exefs\" --header \"%s\\exefsheader.bin\"", tmpName, tmpName, tmpName);
	printf("==> %s\n", cmd);
	if(system(cmd)) return "3dstool -xtf exefs failed";
	//printf(" ^^^ NOTICE: \"ERROR: uncompress error\" and \"ERROR: extract file failed\" ARE NORMAL HERE. IGNORE THEM. ^^^\n\n\n");

	//process the extracted code.bin
	snprintf(cmd, sizeof(cmd), "%s\\exefs\\code.bin", tmpName);
	resultStr = processCodeBin(cmd, fname);
	if(resultStr) return resultStr;

	//we can stop here if we're just giving info; otherwise we need to rebuild a modified cia
	if(onlyInfo)
		return "Success!";

	//now we reverse the steps above to make a modified cia
	//loose files => exefs
	snprintf(cmd, sizeof(cmd), "progfiles\\3dstool.exe -ctf exefs \"%s\\newExefs.bin\" --exefs-dir \"%s\\exefs\" --header \"%s\\exefsheader.bin\"", tmpName, tmpName, tmpName);
	printf("==> %s\n", cmd);
	if(system(cmd)) return "3dstool -ctf exefs failed";

	//exefs etc => cxi
	snprintf(cmd, sizeof(cmd), "progfiles\\3dstool.exe -ctf cxi \"%s\\modified.cxi\" --header \"%s\\ncchheader.bin\" --exh \"%s\\exheader.bin\" --exefs \"%s\\newExefs.bin\" --romfs \"%s\\romfs.bin\"",
			tmpName, tmpName, tmpName, tmpName, tmpName);
	printf("==> %s\n", cmd);
	if(system(cmd)) return "3dstool -ctf cxi failed";

	//now we need to reassemble one or more cxi's into a cia
	//enumerate dumped contents in name order and parse the numbers out
	snprintf(cmd, sizeof(cmd), "dir \"%s\\file.*\" /b /on", tmpName);
	fp = popen(cmd, "r");
	strcpy(cmd, "progfiles\\makerom.exe -f cia");
	nDumps = 0;
	while(fgets(dumpFile, sizeof(dumpFile), fp)) {
		for(i=strlen(dumpFile)-1; i>=0 && isspace(dumpFile[i]); i--) dumpFile[i]='\0';	//trim newline/spaces from end

		//extract file and index numbers
		//locate first non-0 digit in the file number (5 jumps past "file.")
		for(i=5; dumpFile[i]=='0'; i++);
		if(dumpFile[i] == '\0') {
			fclose(fp);
			return "irregular dump file name";
		} else if(dumpFile[i] == '.') {
			//we reached the ending . before seeing a non-0 digit, number is 0
			strcpy(fileNum, "0");
		} else if(isxdigit(dumpFile[i])) {
			//found one or more digits, copy them
			for(j=0; j<sizeof(fileNum)-1 && isxdigit(dumpFile[i]); i++)
				fileNum[j++] = dumpFile[i];
			fileNum[j] = '\0';
		}
		if(dumpFile[i] != '.') {
			fclose(fp);
			return "irregular dump file name";
		}

		//we're now at the . between the file and index numbers, do index number the same way
		for(i++; dumpFile[i]=='0'; i++);
		if(dumpFile[i] == '\0' || dumpFile[i] == '.' || isspace(dumpFile[i])) {
			//we reached the ending . before seeing a non-0 digit, number is 0
			strcpy(indNum, "0");
		} else if(isxdigit(dumpFile[i])) {
			//found one or more digits, copy them
			for(j=0; j<sizeof(indNum)-1 && isxdigit(dumpFile[i]); i++)
				indNum[j++] = dumpFile[i];
			indNum[j] = '\0';
		}

		//construct -content part of command for this file
		snprintf(cmdPart, sizeof(cmdPart), " -content \"%s\\%s\":%s:%s",
				tmpName, 0==strcmp(dumpFile, mainCxi)?"modified.cxi":dumpFile, fileNum, indNum);
		strncat(cmd, cmdPart, sizeof(cmd));
	}
	fclose(fp);

	//generate a name for the modified cia
	strncpy(newCiaName, fname, sizeof(newCiaName));
	//remove extension
	for(i=strlen(newCiaName)-1; i>=0 && newCiaName[i]!='.'; i--) newCiaName[i]='\0';
	newCiaName[i]='\0';
	//add note as to what's changed
	strncat(newCiaName, " (edit", sizeof(newCiaName));
	if(setSleepButtons)
		strncat(newCiaName, "-sleepbtns", sizeof(newCiaName));
	if(setLcdGhosting)
		strncat(newCiaName, "-lcdghost", sizeof(newCiaName));
	if(setVideoLUT)
		strncat(newCiaName, "-filter", sizeof(newCiaName));
	strncat(newCiaName, ").cia", sizeof(newCiaName));

	//finish and run the makerom command
	snprintf(cmdPart, sizeof(cmdPart), " -o \"%s\"", newCiaName);
	strncat(cmd, cmdPart, sizeof(cmd));
	printf("==> %s\n", cmd);
	if(system(cmd)) return "makerom failed";

	return "Success!";
}

//delete the temp dir if we aren't extracting files
void cleanup(void) {
	if(!extractAll) {
		char cmd[8192];
		snprintf(cmd, sizeof(cmd), "rd /s /q \"%s\" 2>NUL", tmpName);
		system(cmd);
	}
}
