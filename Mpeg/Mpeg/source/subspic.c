/* #define DEBUG */
/* subspic.c, Frame buffer substitution routines */

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
#include <fcntl.h>

#include "config.h"
#include "global.h"

/* private prototypes*/
void Read_Frame _ANSI_ARGS_((char *filename, 
  unsigned char *frame_buffer[], int framenum));
void Copy_Frame _ANSI_ARGS_((unsigned char *src, unsigned char *dst, 
  int width, int height, int parity, int incr));
int Read_Components _ANSI_ARGS_ ((char *filename, 
  unsigned char *frame[3], int framenum));
int Read_Component _ANSI_ARGS_ ((char *fname, unsigned char *frame, 
  int width, int height));
int Extract_Components _ANSI_ARGS_ ((char *filename,
  unsigned char *frame[3], int framenum));


/* substitute frame buffer routine */
void Substitute_Frame_Buffer (bitstream_framenum, sequence_framenum)
int bitstream_framenum;
int sequence_framenum;
{
  /* tracking variables */
  int previous_temporal_reference=-1;
  int previous_bitstream_framenum = -1;
  int previous_anchor_temporal_reference = -1;
  int previous_anchor_bitstream_framenum = -1;
  int previous_picture_coding_type = -1;
  int bgate = -1;
  
  /* local temporary variables */
  int substitute_display_framenum = -1;


#ifdef DEBUG
  customprint("SUB: seq fn(%d) bitfn(%d) tempref(%d) picstr(%d) type(%d)\n", 
    sequence_framenum, bitstream_framenum, temporal_reference, 
    picture_structure, picture_coding_type);
#endif

  /* we don't substitute at the first picture of a sequence */
  if((sequence_framenum!=0)||(Second_Field))
  {
    /* only at the start of the frame */
    if ((picture_structure==FRAME_PICTURE)||(!Second_Field))
    {
      if(picture_coding_type==P_TYPE)
      {
        /* the most recently decoded reference frame needs substituting */
        substitute_display_framenum = bitstream_framenum - 1;
        
        Read_Frame(Substitute_Picture_Filename, forward_reference_frame, 
          substitute_display_framenum);
      }
      /* only the first B frame in a consequitve set of B pictures
         loads a substitute backward_reference_frame since all subsequent
         B frames predict from the same reference pictures */
      else if((picture_coding_type==B_TYPE)&&(bgate!=1))
      {
        substitute_display_framenum = 
          (previous_temporal_reference - temporal_reference) 
            + bitstream_framenum - 1;

        Read_Frame(Substitute_Picture_Filename, backward_reference_frame, 
          substitute_display_framenum);
      }
    } /* P fields can predict from the two most recently decoded fields, even
         from the first field of the same frame being decoded */
    else if(Second_Field && (picture_coding_type==P_TYPE))
    {
      /* our favourite case: the IP field picture pair */
      if((previous_picture_coding_type==I_TYPE)&&(picture_coding_type==P_TYPE))
      {
        substitute_display_framenum = bitstream_framenum;
      }
      else /* our more generic P field picture pair */
      {
        substitute_display_framenum = 
          (temporal_reference - previous_anchor_temporal_reference) 
            + bitstream_framenum - 1;
      }

      Read_Frame(Substitute_Picture_Filename, current_frame, substitute_display_framenum);
    }
#ifdef DEBUG
    else if((picture_coding_type!=B_TYPE)||(picture_coding_type!=D_TYPE))
    {
      customprint("NO SUBS FOR THIS PICTURE\n");
    }
#endif
  }


  /* set b gate so we don't redundantly load next time around */
  if(picture_coding_type==B_TYPE)
    bgate = 1;
  else
    bgate = 0;

  /* update general tracking variables */
  if((picture_structure==FRAME_PICTURE)||(!Second_Field))
  {
    previous_temporal_reference  = temporal_reference;
    previous_bitstream_framenum  = bitstream_framenum;
  }
  
  /* update reference frame tracking variables */
  if((picture_coding_type!=B_TYPE) && 
    ((picture_structure==FRAME_PICTURE)||Second_Field))
  {
    previous_anchor_temporal_reference  = temporal_reference;
    previous_anchor_bitstream_framenum  = bitstream_framenum;
  }

  previous_picture_coding_type = picture_coding_type;

}


/* Note: fields are only read to serve as the same-frame reference for 
   a second field */
void Read_Frame(fname,frame,framenum)
char *fname;
unsigned char *frame[];
int framenum;
{
  int parity;
  int rerr = 0;
  int field_mode;

  if(framenum<0)
    customprint("ERROR: framenum (%d) is less than zero\n", framenum);


  if(Big_Picture_Flag)
    rerr = Extract_Components(fname, substitute_frame, framenum);
  else
    rerr = Read_Components(fname, substitute_frame, framenum);

  if(rerr!=0)
  {
    customprint("was unable to substitute frame\n");
  }

  /* now copy to the appropriate buffer */
  /* first field (which we are attempting to substitute) must be
     of opposite field parity to the current one */
  if((Second_Field)&&(picture_coding_type==P_TYPE))
  {
    parity      = (picture_structure==TOP_FIELD ? 1:0);      
    field_mode  = (picture_structure==FRAME_PICTURE ? 0:1);
  }
  else
  {
    /* Like frame structued pictures, B pictures only substitute an entire frame 
       since both fields always predict from the same frame (with respect 
       to forward/backwards directions) */
    parity = 0;
    field_mode = 0;
  }


  Copy_Frame(substitute_frame[0], frame[0], Coded_Picture_Width, 
    Coded_Picture_Height, parity, field_mode);
  
  Copy_Frame(substitute_frame[1], frame[1], Chroma_Width, Chroma_Height, 
    parity, field_mode);
  
  Copy_Frame(substitute_frame[2], frame[2], Chroma_Width, Chroma_Height,
    parity, field_mode);

#ifdef VERBOSE
  if(Verbose_Flag > NO_LAYER)
    customprint("substituted %s %d\n",
      (field_mode ? (parity?"bottom field":"bottom field"):"frame"), framenum);
#endif
}




int Read_Components(filename, frame, framenum) 
char *filename;
unsigned char *frame[3];
int framenum;
{
  int err = 0;
  char outname[FILENAME_LENGTH];
  char name[FILENAME_LENGTH];

  scustomprint(outname,filename,framenum);


  scustomprint(name,"%s.Y",outname);
  err += Read_Component(name, frame[0], Coded_Picture_Width, 
    Coded_Picture_Height);

  scustomprint(name,"%s.U",outname);
  err += Read_Component(name, frame[1], Chroma_Width, Chroma_Height);

  scustomprint(name,"%s.V",outname);
  err += Read_Component(name, frame[2], Chroma_Width, Chroma_Height);

  return(err);
}


int Read_Component(Filename, Frame, Width, Height)
char *Filename;
unsigned char *Frame;
int Width;
int Height;
{
    while (1);
  int Size;
  int Bytes_Read;
  int Infile;

  Size = Width*Height;
//
//#ifdef DEBUG
//  customprint("SUBS: reading %s\n", filename);
//#endif
//
//  if(!(Infile=open(Filename,O_RDONLY|O_BINARY))<0)
//  {
//    customprint("ERROR: unable to open reference filename (%s)\n", Filename);
//	return(-1);
//  }
//
//  Bytes_Read = rawread(Infile, Frame, Size);
//  
//  if(Bytes_Read!=Size)
//  {
//    customprint("was able to read only %d bytes of %d of file %s\n",
//      Bytes_Read, Size, Filename);
//  }
// 
//  close(Infile); 
  while (1);
  return(0);
}


/* optimization: do not open the big file each time. Open once at start
   of decoder, and close at the very last frame */

/* Note: "big" files were used in E-mail exchanges almost exclusively by the 
   MPEG Committee's syntax validation and conformance ad-hoc groups from 
   the year 1993 until 1995 */
int Extract_Components(filename, frame, framenum) 
char *filename;
unsigned char *frame[3];
int framenum;
{

#ifndef GBA
/*  int err = 0; */
  FILE *fd;
  int line;
  int size, offset;


  if (!(fd = fopen(filename,"rb")))
  {
    scustomprint(Error_Text,"Couldn't open %s\n",filename);
    return(-1);
  }

  /* compute size of each frame (in bytes) */
  size = (Coded_Picture_Width*Coded_Picture_Height);

  if(chroma_format==CHROMA444)
    size = (size * 3);
  else if(chroma_format==CHROMA422)
    size = (size * 2);
  else if(chroma_format==CHROMA420)
    size = ((size*3)>>1);
  else
    customprint("ERROR: chroma_format (%d) not recognized\n", chroma_format);


  /* compute distance into "big" file */
  offset = size*framenum;

#ifdef DEBUG
  customprint("EXTRACTING: frame(%d) offset(%d), size (%d) from %s\n", 
    framenum, offset, size, filename);
#endif

  /* seek to location in big file where desired frame begins */
  /* note: this offset cannot exceed a few billion bytes due to the */
  /*       obvious limitations of 32-bit integers */
  fseek(fd, offset, 0);

  /* Y  */
  for (line=0; line<Coded_Picture_Height; line++)
  {
    fread(frame[0]+(line*Coded_Picture_Width),1,Coded_Picture_Width,fd);
  }

  /* Cb */
  for (line=0; line<Chroma_Height; line++)
  {
    fread(frame[1]+(line*Chroma_Width),1,Chroma_Width,fd);
  }

  /* Cr */
  for (line=0; line<Chroma_Height; line++)
  {
    fread(frame[2]+(line*Chroma_Width),1,Chroma_Width,fd);
  }


  fclose(fd);
#endif
  return(0);
}


void Copy_Frame(src, dst, width, height, parity, field_mode)
unsigned char *src;
unsigned char *dst;
int width;
int height;
int parity;        /* field parity (top or bottom) to overwrite */
int field_mode;    /* 0 = frame, 1 = field                      */
{
  int row, col;
  int s, d;
  int incr;

  s = d = 0;

#ifdef DEBUG
  customprint("COPYING (w=%d, h=%d, parity=%d, field_mode=%d)\n",
    width,height,parity,field_mode);
#endif /* DEBUG */

  if(field_mode)
  {
    incr = 2;

    if(parity==0)
      s += width;
  }
  else
  {
    incr = 1;
  }

  for(row=0; row<height; row+=incr) 
  {
    for(col=0; col<width; col++)
    {
      dst[d+col] = src[s+col];
    }
    
    d += (width*incr);
    s += (width*incr);
  }

}

