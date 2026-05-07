#pragma once

// Project compatibility types for reconstructed source.
#include "graphics.h"
#include "types.h"

typedef boolean Boolean;

typedef struct Player Player;
typedef Player       *PlayerPtr;

typedef struct ScreenContext
{
  int32 sc_nScreens;
  int32 sc_curScreen;
  int32 sc_nFrameBufferPages;
  int32 sc_nFrameByteCount;
  Item sc_Screens[6];
  Item sc_BitmapItems[6];
  Bitmap *sc_Bitmaps[6];
}
ScreenContext;
typedef ScreenContext *ScreenContextPtr;

typedef Bitmap *BitmapPtr;
typedef CCB    *CCBPtr;

typedef struct PicData
{
  uchar name[15];
  uchar _pad;
  int32 time;
}
PicData;
typedef PicData *PicDataPtr;

typedef struct DecData
{
  uchar name[15];
  uchar _pad;
  Coord x;
  Coord y;
  Coord w;
  Coord h;
}
DecData;
typedef DecData *DecDataPtr;

typedef int32 (*PlayCPakUserFn)(PlayerPtr ctx);

int32
PlayCPakStream(ScreenContext *screenContextPtr_,
               uchar         *streamFileName_,
               PlayCPakUserFn userFn_);
