#ifndef GXEPD_DISPLAY_H
#define GXEPD_DISPLAY_H

#include <SPI.h>
#include <Wire.h>

#include <GxEPD.h>

#include <GxGDE0213B1/GxGDE0213B1.cpp>

#include <Fonts/FreeMono9pt7b.h>
#include "FAPercent14pt.h"
#include "FATachometer14pt.h"
#include "FAThermomenter-full14pt.h"
#include "FontSourceCodeProRegular7pt.h"

#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

static const uint8_t line_height = 42;
static const uint8_t value_column = 38;

static const GFXfont *fpercent = &FAPercent14pt;
static const GFXfont *ftachometer = &FATachometer14pt;
static const GFXfont *fthermometer = &FAThermometerFull14pt;

const GFXfont *fsmall = &FreeMono9pt7b;
const GFXfont *fsmall7pt = &SourceCodePro_Regular7pt7b;

#endif