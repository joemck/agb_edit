/* agb_edit console UI specific functions */

#include <limits.h>
#include <float.h>
#include "console_ui.h"
#include "gbacia.h"
#include "videolut.h"

//ask the user something, present options, and return the one they picked
//question: prompt string to show the user (may contain multiple lines for multiple choice)
//keys: list of keys the user could press for each option
// ** keys is a string with multiple items separated by a null \0 byte **
// ** the end is marked with two of them, \0\0 **
// TIP: include '\n' in an entry in keys to make it the default choice if the user just hits enter
//Return value: the first key in the list containing the key the user pressed
//...or -1 if the user hit ^C, ^D or ^Z
static char prompt(const char *question, const char *keys) {
	int ch, i, groupkey;

	printf("%s\n? ", question);
	fflush(stdout);

	while(1) {
		ch = _getch();
		if(ch == '\r')
			ch = '\n';
		for(i=0, groupkey=0; keys[i]!='\0' || keys[i+1]!='\0'; i++) {
			if(keys[i] == '\0' || groupkey == 0)
				groupkey = keys[i];
			if(keys[i] == ch) {
				printf("%c\n", ch);
				return (char)groupkey;
			}
		}
		if(ch==3 || ch==4 || ch==26)	//3=^C, 4=^D, 26=^Z -- user is trying to get out
			return -1;
	}
}

//prompt for an integer -- supports min/max values and lets the user input hex or decimal
//min/max values are INCLUSIVE
//if ok is not null, it's set to 0 on failure or 1 on successful input
//the user may enter Q to quit, but only if ok is not null so we can report it
//when ok is null, the only times we return without getting a number are I/O errors or EOF
static int promptInt(const char *question, int minimum, int maximum, int *ok) {
	int answer = INT_MIN, isGood = 0, trim;
	char input[256];
	if(ok) *ok = 0;
	do {
		printf("\n%s\nMust be a whole number from %d to %d, in decimal or 0xHEX\n? ", question, minimum, maximum);
		fflush(stdout);
		if(!fgets(input, sizeof(input), stdin))	//EOF always means we must quit
			return INT_MIN;
		for(trim=0; trim<sizeof(input) && isspace(input[trim]); trim++);
		if(ok!=NULL && tolower(input[trim])=='q' && (input[trim+1]=='\0' || isspace(input[trim+1])))	//q=quit
			return INT_MIN;
		if(input[trim] == '0' && tolower(input[trim+1]) == 'x' && isxdigit(input[trim+2]))	//hex input
			isGood = (1 == sscanf(&input[trim+2], "%x", &answer));
		else
			isGood = (1 == sscanf(&input[trim], "%d", &answer));
		if(isGood) {
			if(answer < minimum) {
				isGood = 0;
				printf("That number is too small. The minimum is %d (inclusive)\n", minimum);
			} else if(answer > maximum) {
				isGood = 0;
				printf("That number is too big. The maximum is %d (inclusive)\n", maximum);
			}
		}
	} while(!isGood);

	if(ok) *ok = 1;
	return answer;
}

//prompt for a double -- as above but it's floating point, and no hex input
//minIsInclusive and maxIsInclusive should be 0 if you want the range to be exclusive, or 1 for inclusive
static double promptReal(const char *question, double minimum, int minIsInclusive, double maximum, int maxIsInclusive, int *ok) {
	double answer = -DBL_MAX;
	int isGood = 0, trim;
	char input[256];
	if(ok) *ok = 0;
	do {
		printf("\n%s\nIt can be any number in the range %c%.2lf .. %.2lf%c\n? ", question, minIsInclusive?'[':'(', minimum, maximum, maxIsInclusive?']':')');
		fflush(stdout);
		if(!fgets(input, sizeof(input), stdin))	//EOF always means we must quit
			return -DBL_MAX;
		for(trim=0; trim<sizeof(input) && isspace(input[trim]); trim++);
		if(ok!=NULL && tolower(input[trim])=='q' && (input[trim+1]=='\0' || isspace(input[trim+1])))	//q=quit
			return -DBL_MAX;
		isGood = (1 == sscanf(&input[trim], "%lf", &answer));
		if(isGood) {
			if((minIsInclusive && answer < minimum) || (!minIsInclusive && answer <= minimum)) {
				isGood = 0;
				printf("That number is too small. The minimum is %lf (%s)\n", minimum, minIsInclusive?"inclusive":"exclusive");
			} else if((maxIsInclusive && answer > maximum) || (!maxIsInclusive && answer >= maximum)) {
				isGood = 0;
				printf("That number is too big. The maximum is %lf (%s)\n", maximum, maxIsInclusive?"inclusive":"exclusive");
			}
		}
	} while(!isGood);

	if(ok) *ok = 1;
	return answer;
}

//interactive video param editor -- starts with a straight line and lets the user
//change different parameters, displaying the LUT after each change
//also edits LCD ghosting
static void editVideoParams(void) {
	int isGood = 0;
	char str[4096];
	static const char *channelNames[4] = {"Red", "Green", "Blue", "ALL"};
	double red=0, green=0, blue=0;
	char choice, choice2;
	int print = 1, done = 0;
	int numInt;
	double numReal;

	printf(" ===== VIDEO PARAMETER EDITOR =====\n");
	lcdGhosting = 255;
	lutResetParams(1);

	do {
		if(print) {
			makeVideoLUT();
			printVideoLUT(videoLUT, lcdGhosting);
		}
		print = 1;
		lutGetWhitePointColor(&red, &green, &blue);
		snprintf(str, sizeof(str), "Do what?\n"
				"A - Change the color channel you're editing [%s]\n"
				"B - Brightness (intercept) [%.2lf]\n"
				"C - Contrast (slope) [%.2lf]\n"
				"D - Dark filter (sets contrast in dark-filter units)\n"
				"I - Change input gamma [%.2lf]\n"
				"O - Change output gamma [%.2lf]\n"
				"V - Invert colors amount (-1..1) [%.2lf]\n"
				"S - Solarize amount (0..1) [%.2lf]\n"
				"W - Set white point color [RGB %.2lf, %.2lf %.2lf]\n"
				"T - Set color temperature [%d k]\n"
				"X - Set maXimum/ceiling [%d]\n"
				"N - Set miNimum/floor [%d]\n"
				"G - Set LCD ghosting/anti-flicker/motion blur [%d]\n"
				"R - Reset params (does NOT reset ghosting)\n"
				"K - OK! Done! (Save changes)\n"
				"Q - Back to previous menu, abandon all parameter changes other than ghosting",
				channelNames[lutGetActiveChannel()],
				lutGetBrightness(), lutGetContrast(), lutGetGammaIn(), lutGetGammaOut(),
				lutGetInvert(), lutGetSolarize(), red, green, blue, lutGetColorTemp(),
				lutGetCeiling(), lutGetFloor(), lcdGhosting);
		choice = prompt(str, "Aa\0Bb\0Cc\0Dd\0IiLl1\0Oo0\0Vv\0Ss5\0Ww\0Tt\0Xx\0Nn\0Gg\0Rr\0Kk\0Qq\0");

		switch(choice) {
			case 'A':
				choice2 = prompt("Press R, G or B to select a channel, or A for ALL channels", "Rr\0Gg\0Bb\0Aa");
				if(choice2=='R' || choice2=='G' || choice2=='B' || choice2=='A')
					lutSetActiveChannel(choice2=='R'?CHANNEL_RED
							: choice2=='G'?CHANNEL_GREEN
							: choice2=='B'?CHANNEL_BLUE
							: CHANNEL_ALL);
				print = 0;
				break;
			case 'B':
				numReal = promptReal("Enter brightness (0=neutral)", -1.0, 1, 1.0, 1, &isGood);
				if(isGood) lutSetBrightness(numReal);
				else print = 0;
				break;
			case 'C':
				numReal = promptReal("Enter contrast (1=neutral)", 0.0, 0, 10.0, 1, &isGood);
				if(isGood) lutSetContrast(numReal);
				else print = 0;
				break;
			case 'D':
				numInt = promptInt("Enter dark filter value (0=bright; 255=black; Nintendo uses around 90)", 0, 255, &isGood);
				if(isGood) lutSetContrast(1.0 - numInt / 255.0);
				else print = 0;
				break;
			case 'I':
				numReal = promptReal("Enter input gamma (2.2=GBA gamma)", 0.0, 0, 5.0, 1, &isGood);
				if(isGood) lutSetGammaIn(numReal);
				else print = 0;
				break;
			case 'O':
				numReal = promptReal("Enter output gamma (2.2=GBA gamma, 1.54=3DS gamma)", 0.0, 0, 5.0, 1, &isGood);
				if(isGood) lutSetGammaOut(numReal);
				else print = 0;
				break;
			case 'V':
				numReal = promptReal("Enter invert amount (1=normal color; -1=fully inverted)", -1.0, 1, 1.0, 1, &isGood);
				if(isGood) lutSetInvert(numReal);
				else print = 0;
				break;
			case 'S':
				numReal = promptReal("Enter solarize amount (0=normal color; 1=fully solarized)", 0.0, 1, 1.0, 1, &isGood);
				if(isGood) lutSetSolarize(numReal);
				else print = 0;
				break;
			case 'W':
				red = promptReal("Enter white point red component", 0.0, 1, 1.0, 1, &isGood);
				if(isGood) green = promptReal("Enter white point green component", 0.0, 1, 1.0, 1, &isGood);
				if(isGood) blue = promptReal("Enter white point blue component", 0.0, 1, 1.0, 1, &isGood);
				if(isGood) lutSetWhitePointColor(red, green, blue);
				else print = 0;
				break;
			case 'T':
				numInt = promptInt("Enter color temperature (kelvin; 6500=neutral)", 1000, 25000, &isGood);
				if(isGood) lutSetColorTemp(numInt);
				else print = 0;
				break;
			case 'X':
				numInt = promptInt("Enter the max color component value (255=normal)", 0, 255, &isGood);
				if(isGood) lutSetCeiling(numInt);
				else print = 0;
				break;
			case 'N':
				numInt = promptInt("Enter the min color component value (0=normal)", 0, 255, &isGood);
				if(isGood) lutSetFloor(numInt);
				else print = 0;
				break;
			case 'G':
				numInt = promptInt("Enter LCD ghosting value (1=max; 255=none; Nintendo uses 80-90 or so)", 1, 255, &isGood);
				if(isGood) {
					lcdGhosting = numInt;
					setLcdGhosting = 1;
				} else print = 0;
				break;
			case 'R':
				choice2 = prompt("Do you want Gamma-corrected or Linear? [G / L]", "Gg\0LlIi1\0");
				lutResetParams(choice2 == 'G');
				break;
			case 'K':
				printf("OK - Will use this video LUT and LCD ghosting value\n");
				setVideoLUT = 1;
				done = 1;
				break;
			default:
				printf("Scrapping any video LUT changes\n");
				done = 1;
				break;
		}
	} while(!done);
}

//print video LUT data of 256 3-byte entries
void printVideoLUT(u8 lut[3*256], int ghosting) {
	int x, y, i, color;
	char graph[LUT_W][LUT_H];

	//raw hex dump of all the data in order, with spaces between RGB triplets
	for(i=0; i<3*256; i+=3)
		printf("%s%02x %02x %02x", i==0?"":"  ", lut[i], lut[i+1], lut[i+2]);

	printf("\nGraphical representation of video LUT:\n");
	//now generate a graph to give a quick visualization of the LUT
	//draw border and fill graph with spaces
	for(y=0; y<LUT_H; y++) {
		for(x=0; x<LUT_W; x++) {
			if(y == 0 || y == LUT_H-1) {
				if(x == 0 || x == LUT_W-1) {
					graph[x][y] = '+';
				} else {
					graph[x][y] = '-';
				}
			} else if(x == 0 || x == LUT_W-1) {
				graph[x][y] = '|';
			} else {
				graph[x][y] = ' ';
			}
		}
	}
	//now draw the graph in the array
	for(i=0; i<3*256; i+=3) {
		x = ((i/3) * (LUT_W-1) + 127) / 255;
		if(x<0 || x>=LUT_W) printf("WARN: BAD X %d (i=%d)\n", x, i);
		for(color=0; color<3; color++) {
			y = (lut[i+color] * (LUT_H-1) + 127) / 255;
			if(y<0 || y>=LUT_H) printf("WARN: BAD Y %d (i=%d color=%d, value=%d)\n", y, i, color, lut[i+color]);
			if((isalpha(graph[x][y]) && graph[x][y]!="RGB"[color]) || graph[x][y]=='*')
				graph[x][y] = '*';
			else
				graph[x][y] = "RGB"[color];
		}
	}
	//print it
	for(y=LUT_H-1; y>=0; y--) {
		for(x=0; x<LUT_W; x++) {
			putchar(graph[x][y]);
		}
		putchar('\n');
	}
	printf("LCD Ghosting: %d (0x%02x)\n\n", ghosting, ghosting);
}

//ask the user questions and set the above globals - returns 0 if the user chooses to quit
int doQuestionnaire(void) {
	char result;
	char input[1024];
	int trim, isGood, darken;

	result = prompt("What do you want to do?\n"
			"A - Analyze cia(s) [Default; just pressing enter will select this]\n"
			"P - Preset quick fix cia(s): Remove ghosting & dark filter; gamma correct;\n"
			"      sleep buttons L+R+Select will be pressed when the lid closes\n"
			"E - Edit cia(s)\n"
			"X - eXtract files from cia(s)\n"
			"D - Dump GBA ROM(s)\n"
			"Q - Quit without doing anything",
			"Aa\n\0Pp\0Ee\0Xx\0Dd\0Qq\0");

	if(result == 'A') {
		//analyze / only print info
		onlyInfo = 1;
		return 1;

	} else if(result == 'X') {
		//extract everything
		onlyInfo = 1;
		extractAll = 1;
		return 1;

	} else if(result == 'D') {
		//dump the .gba ROM file
		onlyInfo = 1;
		dumpRom = 1;
		return 1;

	} else if(result == 'P') {
		//do default edits -- new LUT, gamma 2.2 => 1.54, ghosting=ff, sleep buttons=L R Select
		//sleep buttons
		setSleepButtons = 1;
		sleepButtons = BTN_L | BTN_R | BTN_SELECT;
		//no ghosting
		setLcdGhosting = 1;
		lcdGhosting = 0xff;
		//set up video LUT
		setVideoLUT = 1;
		lutResetParams(1);	//sets up parameters for default gamma-corrected, full-brightness LUT
		makeVideoLUT();	//builds a LUT from the parameters
		return 1;

	} else if(result == 'E') {
		//prompt for edits to make
		printf(
				"\nFor each of these yes/no questions, you can respond:\n"
				"Y - Yes, change this in all input files. Overwrite anything already set.\n"
				"N - No, leave the current setting for this in all input files.\n"
				"Q - Cancel and quit the program.\n"
				"Or press enter for the default answer of No.\n\n");

		printf(
				"You can set a button combo you want to send the game when you close the lid.\n"
				"If you applied a sleep patch or the game has a built-in sleep feature, you\n"
				"should set it to that. Or you can do other things like send Start to pause the\n"
				"game when you shut the lid.\n\n");
		result = prompt("Set a lid-close button combo?", "Yy\0Nn\n\0Qq\0");
		if(result == 'Y') {
			setSleepButtons = 1;
			do {
				printf("\nEnter the sleep button combo you want, separated with spaces or +.\n"
						"Valid buttons are Up Down Left Right A B Start Select L R.\n? ");
				fflush(stdout);
				if(!fgets(input, sizeof(input), stdin))
					return 0;
				if(tolower(input[0]) == 'q' && (input[1]=='\0' || isspace(input[1])))
					return 0;
				sleepButtons = encodeButtons(input);
			} while(sleepButtons == 0xffff);
		} else if(result == 'Q' || result == -1) {
			return 0;
		}

		result = prompt("\nDo you want to edit video parameters? This includes LCD ghosting, dark filter,\n"
				"color temperature / blue light filter, and various effects.", "Yy\0Nn\n\0Qq\0");
		if(result == 'Y') {
			editVideoParams();
		} else if(result == 'Q' || result == -1) {
			return 0;
		}

		printf("\nSummary:\n");
		if(setSleepButtons)
			printf(" - Sleep buttons will be set to %s\n", decodeButtons(sleepButtons));
		if(setLcdGhosting)
			printf(" - LCD ghosting will be set to %d (0x%x)\n", lcdGhosting, lcdGhosting);
		if(setVideoLUT)
			printf(" - Video LUT will be set to what you made above\n");
		
		if(!setSleepButtons && !setLcdGhosting && !setVideoLUT) {
			printf(" - No changes made, nothing to do\n\n");
			onlyInfo = 1;
			return 0;	//change to 1 and it will analyze if you don't make any changes
		} else {
			result = prompt("\nCHANGES WILL BE MADE! DO YOU WANT TO PROCEED?", "Yy\0Nn\nQq\0");
			return (result == 'Y');
		}

	} else if(result == 'Q' || result == -1) {
		return 0;

	} else {
		printf("BUG: Unhandled option key '%c' - PLEASE REPORT THIS BUG!\n", result);
		return 0;
	}
}
