// DSPlayer.c - game player loop, main entry, and CEL/bitmap rendering.
#include "pdwt_app.h"

#include "3do_types.h"
#include "pdwt_input.h"
#include "pdwt_os.h"
#include "pdwt_scenes.h"
#include "pdwt_stream.h"
#include "pdwt_support.h"

#include "audio.h"
#include "celutils.h"
#include "event.h"
#include "item.h"
#include "operamath.h"
#include "operror.h"
#include "string.h"

#define BITMAP_ITEM_TYPE 0x203
#define BITMAP_BUFFER_MEM (MEMTYPE_STARTPAGE | MEMTYPE_VRAM)

#define BUTTON_STOP_A 0x08000000UL
#define BUTTON_STOP_B 0x04000000UL
#define BUTTON_STOP_C 0x02000000UL
#define BUTTON_DIGIT_1 0x40000000UL
#define BUTTON_DIGIT_2 0x80000000UL
#define BUTTON_DIGIT_3 0x10000000UL
#define BUTTON_DIGIT_4 0x20000000UL
#define BUTTON_ACCEPT 0x00800000UL

#define INTRO_CEL_PATH "$boot/miketest/ThreeDO.CEL"
#define JAN_CEL_PATH "$boot/miketest/JAN.CEL"

#define RENDER_CCB_FLAGS 0x57644000UL
#define RENDER_CCB_PIXC 0x1f001f81UL

/* Global state - matches original layout at 0x1EB30-0x1EBF8 */
Item VRAMIOReq;
Item VBLIOReq;
Boolean secretpass;
const uchar *passcode;
uchar secretcode[8];
int32 codenum;
int32 checksecret;

// Graphics buffers
Item itemRenderBitmap1;
Item itemRenderBitmap2;
Item itemRenderBitmap3;
Item itemRenderBitmap4;
Item itemRenderBitmap5;
BitmapPtr renderBitmap1;
BitmapPtr renderBitmap2;
BitmapPtr renderBitmap3;
BitmapPtr renderBitmap4;
BitmapPtr renderBitmap5;
ubyte    *bitmapBuffer1;
ubyte    *bitmapBuffer2;
ubyte    *bitmapBuffer3;
ubyte    *bitmapBuffer4;
ubyte    *bitmapBuffer5;

// Cel sprites
CCBPtr Cel1;
CCBPtr Cel2;
CCBPtr Cel3;
CCBPtr Cel4;
CCBPtr Cel5;

// Screen context
ScreenContext gScreenContext;
static int32 gStartupGraphicsOpen;
static int32 gStartupMathOpen;
static int32 gStartupAudioOpen;


static
void
_init_render_cel(CCB      *cel_,
                 BitmapPtr bitmap_,
                 int32     x_,
                 int32     y_,
                 int32     width_,
                 int32     height_);


static
void
_release_render_cel(CCBPtr *cel_);


static
void
_release_render_bitmap(Item      *item_,
                       BitmapPtr *bitmap_,
                       ubyte    **buffer_,
                       int32      bufferSize_);


static
void
_cleanup_render_resources(void);


static
void
_release_startup_resources(void);

static
Boolean
_init_render_bitmap(Item      *item_,
                    BitmapPtr *bitmap_,
                    ubyte    **buffer_,
                    int32      bufferSize_,
                    int32      height_,
                    Boolean  (*makeCels_)(void));


/*
 * _start_up() - Initialize 3DO system
 * Address 0x1250
 */
Boolean
_start_up(void)
{
  Item item;

  VRAMIOReq            = 0;
  VBLIOReq             = 0;
  gStartupGraphicsOpen = 0;
  gStartupMathOpen     = 0;
  gStartupAudioOpen    = 0;

  gScreenContext.sc_nScreens = 2;
  item                       = OpenGraphics(&gScreenContext, 2);
  if(item == 0)
    return 0;
  gStartupGraphicsOpen = 1;

  item = OpenMacLink();

  item = OpenSPORT();
  if(item == 0)
    {
      _release_startup_resources();
      return 0;
    }

  item = OpenMathFolio();
  if(item != 0)
    {
      _release_startup_resources();
      return 0;
    }
  gStartupMathOpen = 1;

  item = OpenAudioFolio();
  if(item != 0)
    {
      _release_startup_resources();
      return 0;
    }
  gStartupAudioOpen = 1;

  secretpass  = 0;
  codenum     = 0;
  checksecret = 0;

  if(!initbitmap())
    {
      _release_startup_resources();
      return 0;
    }

  return 1;
}


/*
 * MakeCels1-5() - Load cel sprite data
 * Addresses 0x608, 0x76C, 0x8C8, 0xA24, 0xB80
 */
static
void
_init_render_cel(CCB      *cel_,
                 BitmapPtr bitmap_,
                 int32     x_,
                 int32     y_,
                 int32     width_,
                 int32     height_)
{
  cel_->ccb_Flags     = RENDER_CCB_FLAGS;
  cel_->ccb_NextPtr   = 0;
  cel_->ccb_SourcePtr = bitmap_ != NULL ? (CelData *)bitmap_->bm_Buffer : 0;
  cel_->ccb_PLUTPtr   = 0;
  cel_->ccb_XPos      = x_;
  cel_->ccb_YPos      = y_;
  cel_->ccb_HDX       = 0x100000;
  cel_->ccb_HDY       = 0;
  cel_->ccb_VDX       = 0;
  cel_->ccb_VDY       = 0x10000;
  cel_->ccb_HDDX      = 0;
  cel_->ccb_HDDY      = 0;
  cel_->ccb_PIXC      = RENDER_CCB_PIXC;
  cel_->ccb_PRE0      = 0x16 | ((height_ - 1) << 6);
  cel_->ccb_PRE1      = (width_ - 1) | 0x9e0800;
  cel_->ccb_Width     = width_;
  cel_->ccb_Height    = height_;
}


static
void
_release_render_cel(CCBPtr *cel_)
{
  if(*cel_ != NULL)
    {
      FreeMem(*cel_, sizeof(CCB));
      *cel_ = 0;
    }
}


static
void
_release_render_bitmap(Item      *item_,
                       BitmapPtr *bitmap_,
                       ubyte    **buffer_,
                       int32      bufferSize_)
{
  if(*item_ > 0)
    {
      DeleteItem(*item_);
    }
  *item_   = 0;
  *bitmap_ = 0;

  if(*buffer_ != NULL)
    {
      FreeMem(*buffer_, bufferSize_);
      *buffer_ = 0;
    }
}


static
void
_cleanup_render_resources(void)
{
  _release_render_cel(&Cel1);
  _release_render_cel(&Cel2);
  _release_render_cel(&Cel3);
  _release_render_cel(&Cel4);
  _release_render_cel(&Cel5);

  _release_render_bitmap(&itemRenderBitmap1, &renderBitmap1, &bitmapBuffer1, 0x4b00);
  _release_render_bitmap(&itemRenderBitmap2, &renderBitmap2, &bitmapBuffer2, 0x4b00);
  _release_render_bitmap(&itemRenderBitmap3, &renderBitmap3, &bitmapBuffer3, 0x4b00);
  _release_render_bitmap(&itemRenderBitmap4, &renderBitmap4, &bitmapBuffer4, 0x7d00);
  _release_render_bitmap(&itemRenderBitmap5, &renderBitmap5, &bitmapBuffer5, 0x7d00);
}


static
void
_release_startup_resources(void)
{
  _cleanup_render_resources();

  if(VRAMIOReq > 0)
    {
      DeleteItem(VRAMIOReq);
      VRAMIOReq = 0;
    }
  if(VBLIOReq > 0)
    {
      DeleteItem(VBLIOReq);
      VBLIOReq = 0;
    }

  if(gStartupAudioOpen)
    {
      CloseAudioFolio();
      gStartupAudioOpen = 0;
    }
  if(gStartupMathOpen)
    {
      CloseMathFolio();
      gStartupMathOpen = 0;
    }
  if(gStartupGraphicsOpen)
    {
      CloseGraphics(&gScreenContext);
      gStartupGraphicsOpen = 0;
    }

  ShutDown();
}


Boolean
MakeCels1(void)
{
  Cel1 = (CCBPtr)AllocMem(sizeof(CCB), MEMTYPE_CEL);
  if(Cel1 == NULL)
    {
      return 0;
    }
  _init_render_cel(Cel1, renderBitmap1, 0x140000, 0x140000, 160, 30);
  return 1;
}


Boolean
MakeCels2(void)
{
  Cel2 = (CCBPtr)AllocMem(sizeof(CCB), MEMTYPE_CEL);
  if(Cel2 == NULL)
    {
      return 0;
    }
  _init_render_cel(Cel2, renderBitmap2, 0x500000, 0x5a0000, 160, 30);
  return 1;
}


Boolean
MakeCels3(void)
{
  Cel3 = (CCBPtr)AllocMem(sizeof(CCB), MEMTYPE_CEL);
  if(Cel3 == NULL)
    {
      return 0;
    }
  _init_render_cel(Cel3, renderBitmap3, 0x8c0000, 0xa00000, 160, 30);
  return 1;
}


Boolean
MakeCels4(void)
{
  Cel4 = (CCBPtr)AllocMem(sizeof(CCB), MEMTYPE_CEL);
  if(Cel4 == NULL)
    {
      return 0;
    }
  _init_render_cel(Cel4, renderBitmap4, 0x140000, 0x0f0000, 160, 50);
  return 1;
}


Boolean
MakeCels5(void)
{
  Cel5 = (CCBPtr)AllocMem(sizeof(CCB), MEMTYPE_CEL);
  if(Cel5 == NULL)
    {
      return 0;
    }
  _init_render_cel(Cel5, renderBitmap5, 0x8c0000, 0x7d0000, 160, 50);
  return 1;
}


static
Boolean
_init_render_bitmap(Item      *item_,
                    BitmapPtr *bitmap_,
                    ubyte    **buffer_,
                    int32      bufferSize_,
                    int32      height_,
                    Boolean  (*makeCels_)(void))
{
  TagArg bitmapTags[4];
  int32 iTag;

  *item_   = 0;
  *bitmap_ = 0;
  *buffer_ = (ubyte *)AllocMem(bufferSize_, BITMAP_BUFFER_MEM);
  if(*buffer_ == NULL)
    {
      return 0;
    }

  iTag                      = 0;
  bitmapTags[iTag].ta_Tag   = CBM_TAG_WIDTH;
  bitmapTags[iTag++].ta_Arg = (void *)160;
  bitmapTags[iTag].ta_Tag   = CBM_TAG_HEIGHT;
  bitmapTags[iTag++].ta_Arg = (void *)height_;
  bitmapTags[iTag].ta_Tag   = CBM_TAG_BUFFER;
  bitmapTags[iTag++].ta_Arg = *buffer_;
  bitmapTags[iTag].ta_Tag   = CBM_TAG_DONE;
  bitmapTags[iTag++].ta_Arg = 0;

  *item_ = CreateItem(BITMAP_ITEM_TYPE, bitmapTags);
  if(*item_ < 0)
    {
      FreeMem(*buffer_, bufferSize_);
      *buffer_ = 0;
      *item_   = 0;
      return 0;
    }

  *bitmap_ = (BitmapPtr)LookupItem(*item_);
  if(*bitmap_ == NULL)
    {
      DeleteItem(*item_);
      FreeMem(*buffer_, bufferSize_);
      *buffer_ = 0;
      *item_   = 0;
      return 0;
    }

  if(!makeCels_())
    {
      DeleteItem(*item_);
      FreeMem(*buffer_, bufferSize_);
      *item_   = 0;
      *buffer_ = 0;
      return 0;
    }

  return 1;
}


/*
 * initbitmap() - Initialize 5 render bitmaps
 * Address 0xCDC
 */
Boolean
initbitmap(void)
{
  _cleanup_render_resources();

  if(!_init_render_bitmap(&itemRenderBitmap1, &renderBitmap1, &bitmapBuffer1, 0x4b00, 60,
                          MakeCels1))
    goto fail;
  if(!_init_render_bitmap(&itemRenderBitmap2, &renderBitmap2, &bitmapBuffer2, 0x4b00, 60,
                          MakeCels2))
    goto fail;
  if(!_init_render_bitmap(&itemRenderBitmap3, &renderBitmap3, &bitmapBuffer3, 0x4b00, 60,
                          MakeCels3))
    goto fail;
  if(!_init_render_bitmap(&itemRenderBitmap4, &renderBitmap4, &bitmapBuffer4, 0x7d00, 100,
                          MakeCels4))
    goto fail;
  if(!_init_render_bitmap(&itemRenderBitmap5, &renderBitmap5, &bitmapBuffer5, 0x7d00, 100,
                          MakeCels5))
    goto fail;

  return 1;

fail:
  _cleanup_render_resources();
  return 0;
}


/*
 * mySecretFun() - Password verification callback
 * Address 0x12E8
 */
int32
mySecretFun(PlayerPtr ctx_)
{
  static uint32 buttons;

  (void)ctx_;

  buttons = GetJoypad();
  if(controlPadLastError < 0)
    {
      return controlPadLastError;
    }

  if(!secretpass)
    {
      if(codenum > 6)
        {
          codenum = 0;
        }

      if((buttons & BUTTON_DIGIT_1) != 0 && codenum < 6)
        {
          secretcode[codenum++] = '1';
        }
      if((buttons & BUTTON_DIGIT_2) != 0 && codenum < 6)
        {
          secretcode[codenum++] = '2';
        }
      if((buttons & BUTTON_DIGIT_3) != 0 && codenum < 6)
        {
          secretcode[codenum++] = '3';
        }
      if((buttons & BUTTON_DIGIT_4) != 0 && codenum < 6)
        {
          secretcode[codenum++] = '4';
        }

      if((buttons & BUTTON_ACCEPT) != 0)
        {
          secretcode[codenum] = 0;
          checksecret         = strcmp((char *)secretcode, (char *)passcode);
          if(checksecret == 0)
            {
              secretpass = 1;
            }
          else
            {
              codenum = 0;
            }
        }
    }

  if((buttons & BUTTON_STOP_A) != 0)
    {
      return 1;
    }
  if((buttons & BUTTON_STOP_B) != 0)
    {
      return 1;
    }
  if((buttons & BUTTON_STOP_C) != 0)
    {
      return 1;
    }

  return 0;
}


/*
 * myUserFunction() - Main interaction callback
 * Address 0x14B8
 */
int32
myUserFunction(PlayerPtr ctx_)
{
  static uint32 button;

  (void)ctx_;

  button = GetJoypad();
  if(controlPadLastError < 0)
    {
      return controlPadLastError;
    }
  if((button & BUTTON_STOP_A) != 0)
    {
      return 1;
    }
  if((button & BUTTON_STOP_B) != 0)
    {
      return 1;
    }
  if((button & BUTTON_STOP_C) != 0)
    {
      return 1;
    }

  return 0;
}


/*
 * Streamin() - JAN cel transition
 * Address 0x1530
 */
void
Streamin(uint32 arg0_)
{
  CCB  *ccb;
  Point corner[4];

  ccb = (CCB *)arg0_;

  gScreenContext.sc_curScreen = 1 - gScreenContext.sc_curScreen;
  SetVRAMPages(
    VRAMIOReq,
    gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
    0,
    gScreenContext.sc_nFrameBufferPages,
    -1);
  DisplayScreen(gScreenContext.sc_Screens[gScreenContext.sc_curScreen], 0);

  corner[0].pt_X = 0;
  corner[0].pt_Y = 0;
  corner[1].pt_X = 320;
  corner[1].pt_Y = 0;
  corner[2].pt_X = 320;
  corner[2].pt_Y = 240;
  corner[3].pt_X = 0;
  corner[3].pt_Y = 240;

  while(corner[0].pt_X < 120)
    {
      gScreenContext.sc_curScreen = 1 - gScreenContext.sc_curScreen;

      corner[0].pt_X++;
      corner[0].pt_Y++;
      corner[1].pt_X--;
      corner[1].pt_Y++;
      corner[2].pt_X--;
      corner[2].pt_Y--;
      corner[3].pt_X++;
      corner[3].pt_Y--;

      MapCel(ccb, corner);
      SetVRAMPages(
        VRAMIOReq,
        gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
        0,
        gScreenContext.sc_nFrameBufferPages,
        -1);
      DrawCels(gScreenContext.sc_BitmapItems[gScreenContext.sc_curScreen], ccb);
      DisplayScreen(gScreenContext.sc_Screens[gScreenContext.sc_curScreen], 0);
      WaitVBL(VBLIOReq, 1);
    }
}


/*
 * main() - Main program entry
 * Address 0x1714
 *
 * This is the FULL main game loop reconstructed from disassembly
 */
int
main(int    argc_,
     char **argv_)
{
  int32 result;
  int32 gameResult;
  int32 eventErr;
  int32 eventOpen;
  int32 i;
  CCB  *cel;

  (void)argc_;
  (void)argv_;


  // Initialize system
  if(!_start_up())
    {
      return -1;
    }

  controlPadPrevious.cped_ButtonBits = 0;
  passcode                           = (const uchar *)"124324";
  secretpass                         = 0;
  codenum                            = 0;
  checksecret                        = 0;

  for(i = 0; i < 6; i++)
    {
      secretcode[i] = '0';
    }
  secretcode[i] = 0;

  VBLIOReq = GetVBLIOReq();
  VRAMIOReq = GetVRAMIOReq();
  if(VBLIOReq < 0 || VRAMIOReq < 0)
    {
      _release_startup_resources();
      return -1;
    }
  eventOpen  = 0;
  result     = 0;
  gameResult = 0;


  cel = LoadCel(INTRO_CEL_PATH, MEMTYPE_CEL);
  if(cel == NULL)
    {
      gameResult = -1;
      goto cleanup;
    }
  else
    {
      cel->ccb_Flags |= CCB_LAST;
      SetVRAMPages(
        VRAMIOReq,
        gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
        0,
        gScreenContext.sc_nFrameBufferPages,
        -1);
      DrawCels(gScreenContext.sc_BitmapItems[gScreenContext.sc_curScreen], cel);
      DisplayScreen(gScreenContext.sc_Screens[gScreenContext.sc_curScreen], 0);
      WaitVBL(VBLIOReq, 0x168);
      UnloadCel(cel);
    }

  for(;;)
    {
      for(;;)
        {
          eventErr = InitEventUtility(1, 0, 1);
          if(eventErr < 0)
            {
              PrintfSysErr(eventErr);
              result    = eventErr;
              eventOpen = 0;
            }
          else
            {
              eventOpen = 1;
            }

          SetVRAMPages(
            VRAMIOReq,
            gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
            0,
            gScreenContext.sc_nFrameBufferPages,
            -1);
          DisplayScreen(gScreenContext.sc_Screens[gScreenContext.sc_curScreen], 0);

          if(eventOpen)
            {
              result = playdatastream((uchar *)"kirinweaver", myUserFunction);
            }
          if(eventOpen)
            {
              eventErr = KillEventUtility();
              eventOpen = 0;
              if(eventErr < 0 && result >= 0)
                {
                  result = eventErr;
                }
            }
          if(result < 0)
            {
              gameResult = result;
              goto cleanup;
            }

          SetVRAMPages(
            VRAMIOReq,
            gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
            0,
            gScreenContext.sc_nFrameBufferPages,
            -1);
          DisplayScreen(gScreenContext.sc_Screens[gScreenContext.sc_curScreen], 0);

          if(result == 0)
            {
              eventErr = InitEventUtility(1, 0, 1);
              if(eventErr < 0)
                {
                  PrintfSysErr(eventErr);
                  result    = eventErr;
                  eventOpen = 0;
                }
              else
                {
                  eventOpen = 1;
                  result = playdatastreams((uchar *)"janp1weaver", mySecretFun);
                }
              if(eventOpen)
                {
                  eventErr = KillEventUtility();
                  eventOpen = 0;
                  if(eventErr < 0 && result >= 0)
                    {
                      result = eventErr;
                    }
                }
              if(result < 0)
                {
                  gameResult = result;
                  goto cleanup;
                }
            }

          if(result == 0)
            {
              if(secretpass)
                {
                  SetVRAMPages(
                    VRAMIOReq,
                    gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
                    0,
                    gScreenContext.sc_nFrameBufferPages,
                    -1);

                  eventErr = InitEventUtility(1, 0, 1);
                  if(eventErr < 0)
                    {
                      PrintfSysErr(eventErr);
                      result    = eventErr;
                      eventOpen = 0;
                    }
                  else
                    {
                      eventOpen = 1;
                      result = playdatastream((uchar *)"janp2weaver", myUserFunction);
                    }
                  if(eventOpen)
                    {
                      eventErr = KillEventUtility();
                      eventOpen = 0;
                      if(eventErr < 0 && result >= 0)
                        {
                          result = eventErr;
                        }
                    }
                }
              else
                {
                  SetVRAMPages(
                    VRAMIOReq,
                    gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
                    0,
                    gScreenContext.sc_nFrameBufferPages,
                    -1);

                  eventErr = InitEventUtility(1, 0, 1);
                  if(eventErr < 0)
                    {
                      PrintfSysErr(eventErr);
                      result    = eventErr;
                      eventOpen = 0;
                    }
                  else
                    {
                      eventOpen = 1;
                      result = playdatastream((uchar *)"janp3weaver", myUserFunction);
                    }
                  if(eventOpen)
                    {
                      eventErr = KillEventUtility();
                      eventOpen = 0;
                      if(eventErr < 0 && result >= 0)
                        {
                          result = eventErr;
                        }
                    }
                }
            }

          if(result < 0)
            {
              gameResult = result;
              goto cleanup;
            }

          cel = LoadCel(JAN_CEL_PATH, MEMTYPE_CEL);
          if(cel == NULL)
            {
              gameResult = -1;
              goto cleanup;
            }
          else
            {
              cel->ccb_Flags |= CCB_LAST;
              Streamin((uint32)cel);
              UnloadCel(cel);
            }

          SetVRAMPages(
            VRAMIOReq,
            gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
            0,
            gScreenContext.sc_nFrameBufferPages,
            -1);

          eventErr = InitEventUtility(1, 0, 1);
          if(eventErr < 0)
            {
              PrintfSysErr(eventErr);
              result    = eventErr;
              eventOpen = 0;
            }
          else
            {
              eventOpen = 1;
              result = playdatastream((uchar *)"titleweaver", myUserFunction);
            }
          if(eventOpen)
            {
              eventErr = KillEventUtility();
              eventOpen = 0;
              if(eventErr < 0 && result >= 0)
                {
                  result = eventErr;
                }
            }

          if(result < 0)
            {
              gameResult = result;
              goto cleanup;
            }

          if(result == 1)
            {
              break;
            }
        }

      SetVRAMPages(
        VRAMIOReq,
        gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
        0,
        gScreenContext.sc_nFrameBufferPages,
        -1);
      DisplayScreen(gScreenContext.sc_Screens[gScreenContext.sc_curScreen], 0);

      gScreenContext.sc_curScreen = 1 - gScreenContext.sc_curScreen;
      SetVRAMPages(
        VRAMIOReq,
        gScreenContext.sc_Bitmaps[gScreenContext.sc_curScreen]->bm_Buffer,
        0,
        gScreenContext.sc_nFrameBufferPages,
        -1);
      DisplayScreen(gScreenContext.sc_Screens[gScreenContext.sc_curScreen], 0);

      gameResult = playplumber();
      if(gameResult < 0)
        {
          goto cleanup;
        }
      if(gameResult == 0)
        {
          break;
        }
    }

cleanup:
  if(eventOpen)
    {
      eventErr = KillEventUtility();
      if(eventErr < 0 && gameResult >= 0)
        {
          gameResult = eventErr;
        }
    }
  _release_startup_resources();

  return gameResult < 0 ? -1 : 0;
}
