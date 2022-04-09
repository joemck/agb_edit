#ifndef __CONSOLE_UI_H__
#define __CONSOLE_UI_H__

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <conio.h>	//XXX: Windows only. We use _getch; Linux uses getch from curses.
#include "gbacia.h"

//this just defines the size of the LUT graphs we print on the terminal
#define LUT_W 80
#define LUT_H 25

//function declarations
void printVideoLUT(u8 lut[3*256], int ghosting);
int doQuestionnaire(void);

#endif /* __CONSOLE_UI_H__ */
