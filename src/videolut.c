/* agb_edit video LUT manipulation functions */

#define _ISOC99_SOURCE	//needed for log2
#include <math.h>
#include "videolut.h"

//not a "real" C header, just defines static const float blackbody_color[]
//I put it in a separate file because it is long
#include "blackbody_color.h"

//parametric LUT params -- the [3] is red, green and blue
static int activeChannel;
static double brightness[3];	//[-1..1], default 0
static double contrast[3];	//[0..10?], default 1, technically infinite, it's a slope
static double gammaIn[3], gammaOut[3];	//[0..5]? again infinite; default in 2.2 out 1.54?
static double invert[3];	//[-1..1], default 1; -1 is inverted, 0 is all gray
static double solarize[3];	//[0..1], default 0
static int maxval[3], minval[3];	//[0..255], default maxval 255 minval 0
static double whitepoint[3];	//[0..1], 1,1,1 is no color filter; typically set with a color temperature
static int colorTemp;	//just for if someone calls lutGetColorTemp


int lutGetActiveChannel(void) { return activeChannel; }
double lutGetBrightness(void) { return brightness[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
double lutGetContrast(void) { return contrast[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
double lutGetGammaIn(void) { return gammaIn[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
double lutGetGammaOut(void) { return gammaOut[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
double lutGetInvert(void) { return invert[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
double lutGetSolarize(void) { return solarize[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
int lutGetCeiling(void) { return maxval[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
int lutGetFloor(void) { return minval[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
double lutGetWhitePoint(void) { return whitepoint[activeChannel==CHANNEL_ALL?CHANNEL_RED:activeChannel]; }
void lutGetWhitePointColor(double *red, double *green, double *blue) {
	*red = whitepoint[CHANNEL_RED];
	*green = whitepoint[CHANNEL_GREEN];
	*blue = whitepoint[CHANNEL_BLUE];
}
int lutGetColorTemp(void) { return colorTemp; }

void lutResetParams(int gammaCorrected) {
	activeChannel = CHANNEL_ALL;
	brightness[0] = brightness[1] = brightness[2] = 0.0;
	contrast[0] = contrast[1] = contrast[2] = 1.0;
	gammaIn[0] = gammaIn[1] = gammaIn[2] = 2.2;
	gammaOut[0] = gammaOut[1] = gammaOut[2] = gammaCorrected?1.54:2.2;
	invert[0] = invert[1] = invert[2] = 1.0;
	solarize[0] = solarize[1] = solarize[2] = 0.0;
	maxval[0] = maxval[1] = maxval[2] = 255;
	minval[0] = minval[1] = minval[2] = 0;
	whitepoint[0] = whitepoint[1] = whitepoint[2] = 1.0;
	colorTemp = 6500;
}

void lutSetActiveChannel(int channel) {
	if(channel >= 0 && channel <= 3)
		activeChannel = channel;
	else
		printf("WARNING: Invalid channel ID %d, not changing active channel\n", channel);
}

void lutSetBrightness(double value) {
	if(activeChannel != CHANNEL_ALL)
		brightness[activeChannel] = value;
	else
		brightness[0] = brightness[1] = brightness[2] = value;
}

void lutSetContrast(double value) {
	if(activeChannel != CHANNEL_ALL)
		contrast[activeChannel] = value;
	else
		contrast[0] = contrast[1] = contrast[2] = value;
}

void lutSetGammaIn(double value) {
	if(activeChannel != CHANNEL_ALL)
		gammaIn[activeChannel] = value;
	else
		gammaIn[0] = gammaIn[1] = gammaIn[2] = value;
}

void lutSetGammaOut(double value) {
	if(activeChannel != CHANNEL_ALL)
		gammaOut[activeChannel] = value;
	else
		gammaOut[0] = gammaOut[1] = gammaOut[2] = value;
}

void lutSetInvert(double value) {
	if(activeChannel != CHANNEL_ALL)
		invert[activeChannel] = value;
	else
		invert[0] = invert[1] = invert[2] = value;
}

void lutSetSolarize(double value) {
	if(activeChannel != CHANNEL_ALL)
		solarize[activeChannel] = value;
	else
		solarize[0] = solarize[1] = solarize[2] = value;
}

void lutSetCeiling(int value) {
	if(activeChannel != CHANNEL_ALL)
		maxval[activeChannel] = value;
	else
		maxval[0] = maxval[1] = maxval[2] = value;
}

void lutSetFloor(int value) {
	if(activeChannel != CHANNEL_ALL)
		minval[activeChannel] = value;
	else
		minval[0] = minval[1] = minval[2] = value;
}

//allow setting white point manually to any color in addition to a temperature
void lutSetWhitePoint(double value) {
	if(activeChannel != CHANNEL_ALL)
		whitepoint[activeChannel] = value;
	else
		whitepoint[0] = whitepoint[1] = whitepoint[2] = value;
}

//allow setting all 3 at the same time since using a color picker to choose a filter color would be reasonable
void lutSetWhitePointColor(double red, double green, double blue) {
	whitepoint[0] = red;
	whitepoint[1] = green;
	whitepoint[2] = blue;
	colorTemp = -1;
}

//set white point based on color temperature - adapted from Luma3DS's screen filter code, which comes from redshift
void lutSetColorTemp(int kelvin) {
	colorTemp = kelvin;
	//blackbody_color has colors starting from 1000K through 25100K, in 100K intervals
	//clamp to that range
	if(kelvin > 25000) kelvin = 25000;
	if(kelvin < 1000) kelvin = 1000;
	//work out the amount between these 100K samples we are
	double fraction = (kelvin % 100) / 100.0;
	int baseIdx = ((kelvin - 1000) / 100) * 3;	//the table has a stride of 3
	//interpolate between sample points as needed for temperatures between samples
	for(int i=0; i<3; i++)
		whitepoint[i] = (1 - fraction) * blackbody_color[baseIdx+i] + fraction * blackbody_color[baseIdx+3+i];
}

/*calculate actual LUT byte array from parameters
 * formula as adjusted and tested in a graphing app
 * y=w(c^g_in(((1-s)x+s(1-abs(2x-1))-0.5)f_flip+0.5+b/c)^g_in)^1/g_out
 * w = whitepoint color
 * b = brightness [-1..1]
 * c = contrast [0..inf]
 * g_in = input gamma
 * g_out = output gamma
 * f_flip = invert [-1..1]
 * s = solarize (vee) [0..1] */
void makeVideoLUT(void) {
	int value;
	for(int x=0; x<256; x++) {
		for(int clr=0; clr<3; clr++) {
			value = (int) (255.0 * whitepoint[clr] * pow(pow(contrast[clr], gammaIn[clr]) * pow(invert[clr] * (solarize[clr] * (1 - fabs(2 * (x / 255.0) - 1)) + (x / 255.0) * (1 - solarize[clr]) - 0.5) + brightness[clr] / contrast[clr] + 0.5, gammaIn[clr]), 1 / gammaOut[clr]) + 0.5);
			if(value < minval[clr]) value = minval[clr];
			if(value > maxval[clr]) value = maxval[clr];
			videoLUT[3*x+clr] = (u8) value;
		}
	}
}
