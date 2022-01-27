
/* mpeg2dec.c, main(), initialization, option processing                    */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */
#include "BuildType.h"
#ifndef  GBA
#include <stdio.h>
#include <stdlib.h>
#endif // ! GBA


#ifdef GBA

#include <ctype.h>
#include <fcntl.h>

#define GLOBAL
#include "config.h"
#include "global.h"
#include "Aliemovie.h"
 /* private prototypes */
static int  video_sequence _ANSI_ARGS_((int* framenum));
static int Decode_Bitstream _ANSI_ARGS_((void));
static int  Headers _ANSI_ARGS_((void));
static void Initialize_Sequence _ANSI_ARGS_((void));
static void Initialize_Decoder _ANSI_ARGS_((void));
static void Deinitialize_Sequence _ANSI_ARGS_((void));
static void Process_Options();


//gba specific
 /* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*)0x4000000;
#define MODE0 0x00
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x400
#define BG3_ENABLE 0x800

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*)0x04000130;

/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

 /* the control registers for the four tile layers */
volatile unsigned short* bg0_control = (volatile unsigned short*)0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*)0x400000a;
volatile unsigned short* bg2_control = (volatile unsigned short*)0x400000c;
volatile unsigned short* bg3_control = (volatile unsigned short*)0x400000e;

/* palette is always 256 colors */
#define PALETTE_SIZE 256

/* the address of the color palette */
volatile unsigned short* bg_palette = (volatile unsigned short*)0x5000000;

/* define the timer control registers */
volatile unsigned short* timer0_data = (volatile unsigned short*)0x4000100;
volatile unsigned short* timer0_control = (volatile unsigned short*)0x4000102;

/* make defines for the bit positions of the control register */
#define TIMER_FREQ_1 0x0
#define TIMER_FREQ_64 0x2
#define TIMER_FREQ_256 0x3
#define TIMER_FREQ_1024 0x4
#define TIMER_ENABLE 0x80

/* the GBA clock speed is fixed at this rate */
#define CLOCK 16777216 
#define CYCLES_PER_BLANK 280896

/* turn DMA on for different sizes */
#define DMA_ENABLE 0x80000000
#define DMA_16 0x00000000
#define DMA_32 0x04000000

/* this causes the DMA destination to be the same each time rather than increment */
#define DMA_DEST_FIXED 0x400000

/* this causes the DMA to repeat the transfer automatically on some interval */
#define DMA_REPEAT 0x2000000

/* this causes the DMA repeat interval to be synced with timer 0 */
#define DMA_SYNC_TO_TIMER 0x30000000

/* pointers to the DMA source/dest locations and control registers */
volatile unsigned int* dma1_source = (volatile unsigned int*)0x40000BC;
volatile unsigned int* dma1_destination = (volatile unsigned int*)0x40000C0;
volatile unsigned int* dma1_control = (volatile unsigned int*)0x40000C4;

volatile unsigned int* dma2_source = (volatile unsigned int*)0x40000C8;
volatile unsigned int* dma2_destination = (volatile unsigned int*)0x40000CC;
volatile unsigned int* dma2_control = (volatile unsigned int*)0x40000D0;

volatile unsigned int* dma3_source = (volatile unsigned int*)0x40000D4;
volatile unsigned int* dma3_destination = (volatile unsigned int*)0x40000D8;
volatile unsigned int* dma3_control = (volatile unsigned int*)0x40000DC;

const uint RAWHEADER = 0x88FFFF00;
const uint LZCOMPRESSEDHEADER = 0x88FFFF01;
const uint INTERLACERLEHEADER = 0x88FFFF22;
const uint INTERLACERLEHEADER2 = 0x88FFFF23;
const uint DESCRIBEHEADER = 0x88FFFF03;
const uint LUTHEADER = 0x88FFFF02;
const uint DIFFHEADER = 0x88FFFF12;
const uint QUADDIFFHEADER = 0x88FFFF13;
const uint QUADDIFFHEADER2 = 0x88FFFF14;
const uint RLEHEADER = 0x88FFFF15;
const uint NINTYRLHEADERINTR = 0x88FFFF76;
typedef struct
{
	unsigned long thesize;
	unsigned long headervalue;
	unsigned long size;
	unsigned char* source;
}CompFrame;

//Frames don't have a type.
typedef struct
{
	unsigned long* address;
	unsigned long size;
}VidFrame;



/* copy data using DMA channel 3 (normal memory transfers) */
void memcpy16_dma(unsigned short* dest, unsigned short* source, int amount) {
	*dma3_source = (unsigned int)source;
	*dma3_destination = (unsigned int)dest;
	*dma3_control = DMA_ENABLE | DMA_16 | amount;
}

/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
	/* and the button register with the button constant we want */
	unsigned short pressed = *buttons & button;

	/* if this value is zero, then it's not pressed */
	if (pressed == 0) {
		return 1;
	}
	else {
		return 0;
	}
}

/* return a pointer to one of the 4 character blocks (0-3) */
volatile unsigned short* char_block(unsigned long block) {
	/* they are each 16K big */
	return (volatile unsigned short*)(0x6000000 + (block * 0x4000));
}

/* return a pointer to one of the 32 screen blocks (0-31) */
volatile unsigned short* screen_block(unsigned long block) {
	/* they are each 2K big */
	return (volatile unsigned short*)(0x6000000 + (block * 0x800));
}

/* the global interrupt enable register */
volatile unsigned short* interrupt_enable = (unsigned short*)0x4000208;

/* this register stores the individual interrupts we want */
volatile unsigned short* interrupt_selection = (unsigned short*)0x4000200;

/* this registers stores which interrupts if any occured */
volatile unsigned short* REG_IF = (unsigned short*)0x4000202;

/* the address of the function to call when an interrupt occurs */
volatile unsigned int* interrupt_callback = (unsigned int*)0x3007FFC;

/* this register needs a bit set to tell the hardware to send the vblank interrupt */
volatile unsigned short* display_interrupts = (unsigned short*)0x4000004;

/* the interrupts are identified by number, we only care about this one */
#define INTERRUPT_VBLANK 0x1

/* allows turning on and off sound for the GBA altogether */
volatile unsigned short* master_sound = (volatile unsigned short*)0x4000084;
#define SOUND_MASTER_ENABLE 0x80

/* has various bits for controlling the direct sound channels */
volatile unsigned short* sound_control = (volatile unsigned short*)0x4000082;

/* bit patterns for the sound control register */
#define SOUND_A_RIGHT_CHANNEL 0x100
#define SOUND_A_LEFT_CHANNEL 0x200
#define SOUND_A_FIFO_RESET 0x800
#define SOUND_B_RIGHT_CHANNEL 0x1000
#define SOUND_B_LEFT_CHANNEL 0x2000
#define SOUND_B_FIFO_RESET 0x8000

/* the location of where sound samples are placed for each channel */
volatile unsigned char* fifo_buffer_a = (volatile unsigned char*)0x40000A0;
volatile unsigned char* fifo_buffer_b = (volatile unsigned char*)0x40000A4;

/* global variables to keep track of how much longer the sounds are to play */
unsigned int channel_a_vblanks_remaining = 0;
unsigned int channel_a_total_vblanks = 0;
unsigned int channel_b_vblanks_remaining = 0;
#define INT_VBLANK 	0x0001
#define INT_HBLANK 	0x0002
#define INT_VCOUNT 	0x0004
#define INT_TIMER0 	0x0008
#define INT_TIMER1 	0x0010
#define INT_TIMER2 	0x0020
#define INT_TIMER3 	0x0040
#define INT_COM 	0x0080
#define INT_DMA0 	0x0100
#define INT_DMA1	0x0200
#define INT_DMA2 	0x0400
#define INT_DMA3 	0x0800
#define INT_BUTTON 	0x1000
#define INT_CART 	0x2000


#if OLD
static int  Get_Val _ANSI_ARGS_((char* argv[]));
#endif

/* #define DEBUG */

static void Clear_Options();
#ifdef DEBUG
static void Print_Options();
#endif

#define ARM __attribute__((__target__("arm")))
#define REG_IFBIOS (*(unsigned short*)(0x3007FF8))

//indicates if framebufer can be used as a buffer or not.
int canDmaImage;
int vblankcounter = 0;
void on_vblank() {

	/* disable interrupts for now and save current state of interrupt */
	*interrupt_enable = 0;
	unsigned short temp = *REG_IF;

	/* look for vertical refresh */
	if ((*REG_IF & INTERRUPT_VBLANK) == INTERRUPT_VBLANK) {

	
	
	}
	vblankcounter++;
	/* restore/enable interrupts */
	*REG_IF = temp;
	REG_IFBIOS |= 1;
	*interrupt_enable = 1;

}

void Setup()
{
	
	/* create custom interrupt handler for vblank - whole point is to turn off sound at right time
	   * we disable interrupts while changing them, to avoid breaking things */
	*interrupt_enable = 0;
	*interrupt_callback = (unsigned int)&on_vblank;
	*interrupt_selection |= INTERRUPT_VBLANK;
	*display_interrupts |= 9;//;
	*interrupt_enable = 1;

	/* clear the sound control initially */
	*sound_control = 0;
}
void VBlankIntrWait()
{

	asm("swi 0x05");

}
int main()
{
	(*(unsigned short*)0x4000000) = 0x403;
	Setup();
#ifdef DEBUG
	Print_Options();
#endif
	Process_Options();
	ld = &base; /* select base layer context */

	/* open MPEG base layer bitstream file(s) */
	/* NOTE: this is either a base layer stream or a spatial enhancement stream */

	base.fileBuf = Aliemovie;

	base.fileSize = Aliemovie_size;

	Initialize_Buffer();

	if (Show_Bits(8) == 0x47)
	{
		sprintf(Error_Text, "Decoder currently does not parse transport streams\n");
		Error(Error_Text);
	}

	next_start_code();
	int code = Show_Bits(32);

	switch (code)
	{
	case SEQUENCE_HEADER_CODE:
		break;
	case PACK_START_CODE:
		System_Stream_Flag = 1;
	case VIDEO_ELEMENTARY_STREAM:
		System_Stream_Flag = 1;
		break;
	default:
		sprintf(Error_Text, "Unable to recognize stream type\n");
		Error(Error_Text);
		break;
	}

	base.fileBuf = Aliemovie;//Reset
	Initialize_Buffer();
	base.fileBuf = Aliemovie;//Reset


	Initialize_Buffer();


	Initialize_Decoder();

	int	ret = Decode_Bitstream();


	return 0;
}

/* IMPLEMENTAION specific rouintes */
static void Initialize_Decoder()
{
	int i;

	/* Clip table */
	if (!(Clip = (unsigned char*)malloc(1024)))
		Error("Clip[] malloc failed\n");

	Clip += 384;

	for (i = -384; i < 640; i++)
		Clip[i] = (i < 0) ? 0 : ((i > 255) ? 255 : i);


     Initialize_Fast_IDCT();

}
static int Table_6_20[3] = { 6,8,12 };
/* mostly IMPLEMENTAION specific rouintes */
static void Initialize_Sequence()
{
	int cc, size;


	/* check scalability mode of enhancement layer */
	if (Two_Streams && (enhan.scalable_mode != SC_SNR) && (base.scalable_mode != SC_DP))
		Error("unsupported scalability mode\n");

	/* force MPEG-1 parameters for proper decoder behavior */
	/* see ISO/IEC 13818-2 section D.9.14 */
	if (!base.MPEG2_Flag)
	{
		progressive_sequence = 1;
		progressive_frame = 1;
		picture_structure = FRAME_PICTURE;
		frame_pred_frame_dct = 1;
		chroma_format = CHROMA420;
		matrix_coefficients = 5;
	}

	/* round to nearest multiple of coded macroblocks */
	/* ISO/IEC 13818-2 section 6.3.3 sequence_header() */
	mb_width = (horizontal_size + 15) / 16;
	mb_height = (base.MPEG2_Flag && !progressive_sequence) ? 2 * ((vertical_size + 31) / 32)
		: (vertical_size + 15) / 16;

	Coded_Picture_Width = 16 * mb_width;
	Coded_Picture_Height = 16 * mb_height;

	/* ISO/IEC 13818-2 sections 6.1.1.8, 6.1.1.9, and 6.1.1.10 */
	Chroma_Width = (chroma_format == CHROMA444) ? Coded_Picture_Width
		: Coded_Picture_Width >> 1;
	Chroma_Height = (chroma_format != CHROMA420) ? Coded_Picture_Height
		: Coded_Picture_Height >> 1;

	/* derived based on Table 6-20 in ISO/IEC 13818-2 section 6.3.17 */
	block_count = Table_6_20[chroma_format - 1];

	for (cc = 0; cc < 3; cc++)
	{
		if (cc == 0)
			size = Coded_Picture_Width * Coded_Picture_Height;
		else
			size = Chroma_Width * Chroma_Height;

		if (!(backward_reference_frame[cc] = (unsigned char*)malloc(size)))
			Error("backward_reference_frame[] malloc failed\n");

		if (!(forward_reference_frame[cc] = (unsigned char*)malloc(size)))
			Error("forward_reference_frame[] malloc failed\n");

		if (!(auxframe[cc] = (unsigned char*)malloc(size)))
			Error("auxframe[] malloc failed\n");

		if (Ersatz_Flag)
			if (!(substitute_frame[cc] = (unsigned char*)malloc(size)))
				Error("substitute_frame[] malloc failed\n");


		if (base.scalable_mode == SC_SPAT)
		{
			/* this assumes lower layer is 4:2:0 */
			if (!(llframe0[cc] = (unsigned char*)malloc((lower_layer_prediction_horizontal_size * lower_layer_prediction_vertical_size) / (cc ? 4 : 1))))
				Error("llframe0 malloc failed\n");
			if (!(llframe1[cc] = (unsigned char*)malloc((lower_layer_prediction_horizontal_size * lower_layer_prediction_vertical_size) / (cc ? 4 : 1))))
				Error("llframe1 malloc failed\n");
		}
	}

	/* SCALABILITY: Spatial */
	if (base.scalable_mode == SC_SPAT)
	{
		if (!(lltmp = (short*)malloc(lower_layer_prediction_horizontal_size * ((lower_layer_prediction_vertical_size * vertical_subsampling_factor_n) / vertical_subsampling_factor_m) * sizeof(short))))
			Error("lltmp malloc failed\n");
	}

#ifdef DISPLAY
	if (Output_Type == T_X11)
	{
		Initialize_Display_Process("");
		Initialize_Dither_Matrix();
	}
#endif /* DISPLAY */

}

void Error(text)
char* text;
{
	fprintf(stderr, text);
	exit(1);
}

/* Trace_Flag output */
void Print_Bits(code, bits, len)
int code, bits, len;
{
	int i;
	for (i = 0; i < len; i++)
		printf("%d", (code >> (bits - 1 - i)) & 1);
}



/* option processing */
static void Process_Options()
{

	Output_Type = -1;


	/* command-line options are proceeded by '-' */
	Main_Bitstream_Filename = 1;


	Output_Type = 7;

		Display_Progressive_Flag = 0;




}





static int Headers()
{
	int ret;

	ld = &base;


	/* return when end of sequence (0) or picture
	   header has been parsed (1) */

	ret = Get_Hdr();


	return ret;
}



static int Decode_Bitstream()
{
	int ret;
	int Bitstream_Framenum;

	Bitstream_Framenum = 0;

	for (;;)
	{

#ifdef VERIFY
		Clear_Verify_Headers();
#endif /* VERIFY */

		ret = Headers();

		if (ret == 1)
		{
			ret = video_sequence(&Bitstream_Framenum);
			VBlankIntrWait();
		}
		else
			return(ret);
	}

}


static void Deinitialize_Sequence()
{
	int i;

	/* clear flags */
	base.MPEG2_Flag = 0;

	for (i = 0; i < 3; i++)
	{
		free(backward_reference_frame[i]);
		free(forward_reference_frame[i]);
		free(auxframe[i]);

		if (base.scalable_mode == SC_SPAT)
		{
			free(llframe0[i]);
			free(llframe1[i]);
		}
	}

	if (base.scalable_mode == SC_SPAT)
		free(lltmp);

#ifdef DISPLAY
	if (Output_Type == T_X11)
		Terminate_Display_Process();
#endif
}


static int video_sequence(Bitstream_Framenumber)
int* Bitstream_Framenumber;
{
	int Bitstream_Framenum;
	int Sequence_Framenum;
	int Return_Value;

	Bitstream_Framenum = *Bitstream_Framenumber;
	Sequence_Framenum = 0;

	Initialize_Sequence();

	/* decode picture whose header has already been parsed in
	   Decode_Bitstream() */


	Decode_Picture(Bitstream_Framenum, Sequence_Framenum);

	/* update picture numbers */
	if (!Second_Field)
	{
		Bitstream_Framenum++;
		Sequence_Framenum++;
	}

	/* loop through the rest of the pictures in the sequence */
	while ((Return_Value = Headers()))
	{
		Decode_Picture(Bitstream_Framenum, Sequence_Framenum);

		if (!Second_Field)
		{
			Bitstream_Framenum++;
			Sequence_Framenum++;
		}
	}

	/* put last frame */
	if (Sequence_Framenum != 0)
	{
		Output_Last_Frame_of_Sequence(Bitstream_Framenum);
	}

	Deinitialize_Sequence();

#ifdef VERIFY
	Clear_Verify_Headers();
#endif /* VERIFY */

	* Bitstream_Framenumber = Bitstream_Framenum;
	return(Return_Value);
}



static void Clear_Options()
{
	Verbose_Flag = 0;
	Output_Type = 0;
	Output_Picture_Filename = " ";
	hiQdither = 0;
	Output_Type = 0;
	Frame_Store_Flag = 0;
	Spatial_Flag = 0;
	Lower_Layer_Picture_Filename = " ";
	Reference_IDCT_Flag = 0;
	Trace_Flag = 0;
	Quiet_Flag = 0;
	Ersatz_Flag = 0;
	Substitute_Picture_Filename = " ";
	Two_Streams = 0;
	Enhancement_Layer_Bitstream_Filename = " ";
	Big_Picture_Flag = 0;
	Main_Bitstream_Flag = 0;
	Main_Bitstream_Filename = " ";
	Verify_Flag = 0;
	Stats_Flag = 0;
	User_Data_Flag = 0;
}


#endif //  GBA
