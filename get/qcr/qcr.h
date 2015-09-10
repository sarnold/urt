#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#define RED 0
#define GREEN 1
#define BLUE 2

/* Command defs for QCR-Z Film Recorder */

/* These are for 8 bit Look Up Tables */

#define LUT_LOAD	0x00
#define LUT_VERIFY	0x01

#define NEUTRAL_LUT	0x1d
#define NEUTRAL_VERIFY	0x1e


/* These commands are for the 12 bit LUTs */

#define LOAD_12_LUT	0x4c
#define VERIFY_12	0x4e
#define LOAD_12_NEUTRAL 0x4d
#define VERIFY_12_NEUTRAL 0x4f

/* Start a color image recording, reading Red, Blue than Green */

#define THREE_PASS_PIXEL 0x06

/* For a three pass RLE image */
#define THREE_PASS_RLE  0x16


/* These are for single passes of data, fully specified (Non-RLE) */
#define NEUTRAL_PIXEL	0x02
#define RED_PIXEL	0x03
#define GREEN_PIXEL	0x04
#define BLUE_PIXEL	0x05

/* These are for single passes of data, run length format */
#define NEUTRAL_RLE	0x12
#define RED_RLE		0x13
#define GREEN_RLE	0x14
#define BLUE_RLE	0x15

#define TEST_PAT_0	0x0a
#define TEST_PAT_1	0x0b

#define SET_LINE_DELAY	0x0c
#define GET_VERSION_ID	0x0d
#define IDLENGTH	14
#define SET_DIMENSIONS	0x0e

#define MIRROR_DISABLE	0x30
#define MIRROR_ENABLE	0x31

#define WARBLE_DISABLE	0x21
#define WARBLE_ENABLE	0x22

#define RED_REPEAT_ON	0x39
#define RED_REPEAT_OFF	0x38

#define AUTO_SHUTTER_OFF 0x0f
#define AUTO_SHUTTER_ON 0x10

#define AUTO_FILTER_OFF 0x18
#define AUTO_FILTER_ON  0x19

#define QCR_BELL	0x11

#define AUTO_CAL_OFF	0x1f
#define AUTO_CAL_ON	0x20

#define END_WARBLE_OFF	0x21
#define END_WARBLE_ON	0x22

#define LOAD_INTERN_LUT	0x23

#define LUT_LINEAR		1 /* Canned luts */
#define LUT_Polaroid59		2
#define LUT_Ektachrome100 	3
#define LUT_Ektachrome100_4K 	4
#define LUT_Polaroid59_4K 	5
#define LUT_Polaroid809_4K 	6

#define EXTENDED_STATUS	0x24
#define MODULE_ID	0x25

#define START_CAL	0x26
#define START_CAL_ALL	0x27

#define SHUTTER_CLOSE	0x1a
#define SHUTTER_OPEN	0x1b

#define BLACK_JUMP_OFF	0x28
#define BLACK_JUMP_ON	0x29

#define RESOLUTION_2K	0x2a
#define RESOLUTION_4K	0x2b

#define GET_BRIGHTNESS	0x2c
#define SET_BRIGHTNESS	0x2d

#define ROTATE_90_OFF	0x2e
#define ROTATE_90_ON	0x2f

#define RETURN_BRIGHT_LEVELS	0x2c
#define	LOAD_BRIGHT_LEVELS	0x2d

#define	RETURN_BRIGHT_TABLE	0x3b
#define LOAD_BRIGHT_TABLE	0x3a

/* Special for QCR-Z */

#define RETURN_STATUS		0x3c
#define SET_FRAME_COUNTER	0x3d
#define GET_FRAME_COUNTER	0x3e
#define RES_2K_SAME_STATUS	0x3f
#define RES_4K_SAME_STATUS	0x40
#define REWIND_FILM		0x41
#define ADVANCE_FILM		0x42
#define MULT_RGB_LUTS	        0x43
#define MULT_GRAY_LUTS		0x44
#define LOAD_FILM		0x45
#define UNLOAD_FILM		0x46
#define NOTCH_FILM		0x47
#define DISABLE_HALF_FRAME	0x48
#define ENABLE_HALF_FRAME	0x49
#define DISABLE_180_ROTATE	0x4a
#define ENABLE_180_ROTATE	0x4b
