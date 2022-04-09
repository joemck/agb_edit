#ifndef __VIDEOLUT_H__
#define __VIDEOLUT_H__

/* Stuff for generating a custom video LUT
 * Internally the LUT is represented by a set of parameters that go with a
 * formula that defines the curve of the LUT. Importing and exporting the
 * actual LUT as a list of numbers is also supported, as is importing and
 * exporting all the parameters as a string.
 */

#include "gbacia.h"

#define CHANNEL_RED 0
#define CHANNEL_GREEN 1
#define CHANNEL_BLUE 2
#define CHANNEL_ALL 3

int lutGetActiveChannel(void);
double lutGetBrightness(void);
double lutGetContrast(void);
double lutGetGammaIn(void);
double lutGetGammaOut(void);
double lutGetInvert(void);
double lutGetSolarize(void);
int lutGetCeiling(void);
int lutGetFloor(void);
double lutGetWhitePoint(void);
void lutGetWhitePointColor(double *red, double *green, double *blue);
int lutGetColorTemp(void);

void lutResetParams(int gammaCorrected);
void lutSetActiveChannel(int channel);
void lutSetBrightness(double brightness);
void lutSetContrast(double contrast);
void lutSetGammaIn(double gammaIn);
void lutSetGammaOut(double gammaOut);
void lutSetInvert(double invert);
void lutSetSolarize(double solarize);
void lutSetCeiling(int ceiling);
void lutSetFloor(int floor);
void lutSetWhitePoint(double value);
void lutSetWhitePointColor(double red, double green, double blue);
void lutSetColorTemp(int kelvin);

void makeVideoLUT(void);	//calculate actual LUT byte array from parameters

#endif /* __VIDEOLUT_H__ */
