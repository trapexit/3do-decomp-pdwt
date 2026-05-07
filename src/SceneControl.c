// SceneControl.c - scene control state machine and pause handling.
#include "pdwt_scene_control.h"

#include "pdwt_input.h"
#include "pdwt_log.h"

#include "celutils.h"

#include "stdlib.h"
#include "string.h"

#define PAUSE_CEL_COUNT 3
#define PAUSE_BORDER_PIXELS 3
#define PAUSE_HORIZONTAL_BORDER_COUNT 2
#define PAUSE_VERTICAL_BORDER_COUNT 3
#define PAUSE_SCREEN_WIDTH 320
#define PAUSE_SCREEN_HEIGHT 240
#define PAUSE_TEXT_WIDTH 48
#define SCENE_INPUT_MASK (ControlX | ControlStart | ControlRightShift)

typedef struct PauseCelConfig
{
  const char *path;
  int32 source_width;
  int32 source_height;
  int32 crop_x;
  int32 crop_y;
  int32 crop_width;
  int32 crop_height;
  int32 output_width;
  int32 output_height;
}
PauseCelConfig;

static const PauseCelConfig g_pause_cels[PAUSE_CEL_COUNT] =
  {
    {
      "$boot/miketest/sc13/SC13_12P.CEL",
      320,
      240,
      150,
      65,
      170,
      130,
      120,
      92
    },
    {
      "$boot/miketest/sc13/SC13_14P.CEL",
      160,
      240,
      15,
      35,
      145,
      170,
      82,
      96
    },
    {
      "$boot/miketest/sc13/SC13_15P.CEL",
      320,
      240,
      115,
      35,
      190,
      200,
      91,
      96
    }
  };


static
int32
_restore_scene_frame(PDWTSceneControl *control_);


static
int32
_display_pause_frame(PDWTSceneControl *control_);


static
int32
_pause_scene(PDWTSceneControl *control_);


static
int32
_resume_scene(PDWTSceneControl *control_);


static
int32
_service_scene(PDWTSceneControl *control_);


static
int32
_wait_scene_vbl(PDWTSceneControl *control_);


static
int32
_wait_while_paused(PDWTSceneControl *control_);

static PDWTSceneControl *gSceneControl;

static
int32
_restore_scene_frame(PDWTSceneControl *control_)
{
  if(control_ == NULL || control_->screenContext == NULL || control_->displayedScreen < 0)
    {
      return -1;
    }
  if(control_->screenContext->sc_Screens[control_->displayedScreen] <= 0)
    {
      return -1;
    }
  return DisplayScreen(control_->screenContext->sc_Screens[control_->displayedScreen], 0);
}


static
int32
_restore_pause_clip(Item  bitmap_item_,
                    int32 result_)
{
  int32 cleanup_result;

  cleanup_result = SetClipOrigin(bitmap_item_, 0, 0);
  if((cleanup_result < 0) && (result_ >= 0))
    {
      result_ = cleanup_result;
    }

  cleanup_result = SetClipWidth(bitmap_item_, PAUSE_SCREEN_WIDTH);
  if((cleanup_result < 0) && (result_ >= 0))
    {
      result_ = cleanup_result;
    }

  cleanup_result = SetClipHeight(bitmap_item_, PAUSE_SCREEN_HEIGHT);
  if((cleanup_result < 0) && (result_ >= 0))
    {
      result_ = cleanup_result;
    }

  return result_;
}


static
int32
_draw_pause_cel(Item                  bitmap_item_,
                const PauseCelConfig *config_,
                int32                 x_,
                int32                 y_)
{
  CCB *cel;
  Point quad[4];
  int32 clip_changed;
  int32 mapped_bottom;
  int32 mapped_left;
  int32 mapped_right;
  int32 mapped_top;
  int32 result;

  cel = PDWTLoadCel(config_->path, MEMTYPE_CEL);
  if(cel == NULL)
    {
      return -1;
    }

  mapped_left = -((config_->crop_x * config_->output_width) /
                  config_->crop_width);
  mapped_top = -((config_->crop_y * config_->output_height) /
                 config_->crop_height);
  mapped_right = (mapped_left +
                  ((config_->source_width * config_->output_width) /
                   config_->crop_width));
  mapped_bottom = (mapped_top +
                   ((config_->source_height * config_->output_height) /
                    config_->crop_height));

  cel->ccb_Flags |= CCB_LAST;
  cel->ccb_NextPtr = NULL;
  quad[0].pt_X = mapped_left;
  quad[0].pt_Y = mapped_top;
  quad[1].pt_X = mapped_right;
  quad[1].pt_Y = mapped_top;
  quad[2].pt_X = mapped_right;
  quad[2].pt_Y = mapped_bottom;
  quad[3].pt_X = mapped_left;
  quad[3].pt_Y = mapped_bottom;
  MapCel(cel, quad);

  clip_changed = 0;
  result = SetClipWidth(bitmap_item_, config_->output_width);
  if(result >= 0)
    {
      clip_changed = 1;
      result = SetClipHeight(bitmap_item_, config_->output_height);
    }

  if(result >= 0)
    {
      result = SetClipOrigin(bitmap_item_, x_, y_);
    }

  if(result >= 0)
    {
      result = DrawCels(bitmap_item_, cel);
    }

  if(clip_changed)
    {
      result = _restore_pause_clip(bitmap_item_, result);
    }

  UnloadCel(cel);
  return result;
}


static
int32
_display_pause_frame(PDWTSceneControl *control_)
{
  static const uint8 paused_text[] = "PAUSED";
  const PauseCelConfig *cel_config;
  ScreenContext *screen_context;
  Bitmap        *source;
  Bitmap        *destination;
  Font          *font;
  GrafCon graf_con;
  Rect panel;
  Color pixel;
  Item bitmap_item;
  int32 cel_x;
  int32 cel_y;
  int32 content_width;
  int32 font_height;
  int32 panel_height;
  int32 panel_left;
  int32 panel_top;
  int32 panel_width;
  int32 pause_screen;
  int32 result;
  int32 text_bottom;
  int32 text_x;
  int32 text_y;
  int32 x;
  int32 y;

  if((control_ == NULL) ||
     (control_->screenContext == NULL) ||
     (control_->vramIOReq <= 0))
    {
      return -1;
    }

  screen_context = control_->screenContext;
  if((screen_context->sc_nScreens < 2) ||
     (control_->displayedScreen < 0) ||
     (control_->displayedScreen > 1))
    {
      return -1;
    }

  pause_screen = (1 - control_->displayedScreen);
  bitmap_item  = screen_context->sc_BitmapItems[pause_screen];
  source       = screen_context->sc_Bitmaps[control_->displayedScreen];
  destination  = screen_context->sc_Bitmaps[pause_screen];
  if((source == NULL) ||
     (destination == NULL) ||
     (bitmap_item <= 0) ||
     (screen_context->sc_Screens[pause_screen] <= 0))
    {
      return -1;
    }

  result = CopyVRAMPages(control_->vramIOReq,
                         destination->bm_Buffer,
                         source->bm_Buffer,
                         screen_context->sc_nFrameBufferPages,
                         (uint32)-1);
  if(result < 0)
    {
      return result;
    }

  font = GetCurrentFont();
  if((font == NULL) || (font->font_CCB == NULL))
    {
      return -1;
    }

  cel_config = &g_pause_cels[(rand() % PAUSE_CEL_COUNT)];
  content_width = cel_config->output_width;
  if(content_width < PAUSE_TEXT_WIDTH)
    {
      content_width = PAUSE_TEXT_WIDTH;
    }

  font_height = (int32)font->font_Height;
  panel_width = (content_width +
                 (PAUSE_BORDER_PIXELS * PAUSE_HORIZONTAL_BORDER_COUNT));
  panel_height = (cel_config->output_height + font_height +
                  (PAUSE_BORDER_PIXELS * PAUSE_VERTICAL_BORDER_COUNT));
  panel_left = ((PAUSE_SCREEN_WIDTH - panel_width) / 2);
  panel_top  = ((PAUSE_SCREEN_HEIGHT - panel_height) / 2);
  cel_x = (panel_left + PAUSE_BORDER_PIXELS +
           ((content_width - cel_config->output_width) / 2));
  cel_y = (panel_top + PAUSE_BORDER_PIXELS);
  text_x = (panel_left + PAUSE_BORDER_PIXELS +
            ((content_width - PAUSE_TEXT_WIDTH) / 2));
  text_y = (cel_y + cel_config->output_height + PAUSE_BORDER_PIXELS);

  memset(&graf_con, 0, sizeof(graf_con));
  panel.rect_XLeft   = panel_left;
  panel.rect_YTop    = panel_top;
  panel.rect_XRight  = ((panel_left + panel_width) - 1);
  panel.rect_YBottom = ((panel_top + panel_height) - 1);
  SetFGPen(&graf_con, MakeRGB15(0, 0, 0));
  result = FillRect(bitmap_item, &graf_con, &panel);
  if(result < 0)
    {
      return result;
    }

  result = _draw_pause_cel(bitmap_item,
                           cel_config,
                           cel_x,
                           cel_y);
  if(result < 0)
    {
      return result;
    }

  MoveTo(&graf_con, text_x, text_y);
  result = DrawText8(&graf_con, bitmap_item, paused_text);
  if(result < 0)
    {
      return result;
    }

  text_bottom = (text_y + font_height);
  for(y = text_y; y < text_bottom; y++)
    {
      for(x = text_x; x < (text_x + PAUSE_TEXT_WIDTH); x++)
        {
          pixel = ReadPixel(bitmap_item, &graf_con, x, y);
          if((int32)pixel < 0)
            {
              return (int32)pixel;
            }

          if(pixel >= MakeRGB15(16, 16, 16))
            {
              SetFGPen(&graf_con, MakeRGB15(31, 31, 31));
            }
          else
            {
              SetFGPen(&graf_con, MakeRGB15(0, 0, 0));
            }

          result = WritePixel(bitmap_item, &graf_con, x, y);
          if(result < 0)
            {
              return result;
            }
        }
    }

  return DisplayScreen(screen_context->sc_Screens[pause_screen], 0);
}


static
int32
_pause_scene(PDWTSceneControl *control_)
{
  int32 result;

  result = DecisionAudioPause(control_->audio);
  if(result < 0)
    {
      return result;
    }
  PDWTSceneClockPause(control_->clock);

  result = _display_pause_frame(control_);
  if(result < 0)
    {
      PDWTSceneClockResume(control_->clock);
      DecisionAudioResume(control_->audio);
      return result;
    }

  control_->paused = 1;
  return PDWT_SCENE_CONTROL_CONTINUE;
}


static
int32
_resume_scene(PDWTSceneControl *control_)
{
  int32 result;

  result = _restore_scene_frame(control_);
  if(result < 0)
    {
      return result;
    }
  result = DecisionAudioResume(control_->audio);
  if(result < 0)
    {
      return result;
    }
  PDWTSceneClockResume(control_->clock);
  control_->paused = 0;
  return PDWT_SCENE_CONTROL_CONTINUE;
}


static
int32
_service_scene(PDWTSceneControl *control_)
{
  uint32 pressed;
  uint32 rawButtons;
  int32 result;

  if(control_ == NULL || !control_->active)
    {
      return -1;
    }

  result = DecisionAudioService(control_->audio);
  if(result < 0)
    {
      return result;
    }

  GetJoypad();
  if(controlPadLastError < 0)
    {
      return controlPadLastError;
    }
  rawButtons = controlPadPrevious.cped_ButtonBits;
  pressed    = rawButtons & ~control_->previousButtons;
  control_->previousButtons = rawButtons & SCENE_INPUT_MASK;

  if((pressed & ControlX) != 0)
    {
      return PDWT_SCENE_CONTROL_SKIP;
    }
  if((pressed & ControlRightShift) != 0)
    {
      return PDWT_SCENE_CONTROL_STEP_BACK;
    }
  if((pressed & ControlStart) != 0)
    {
      if(control_->paused)
        {
          return _resume_scene(control_);
        }
      return _pause_scene(control_);
    }
  return PDWT_SCENE_CONTROL_CONTINUE;
}


static
int32
_wait_scene_vbl(PDWTSceneControl *control_)
{
  int32 wasPaused;
  int32 result;

  wasPaused = control_->paused;
  result = WaitVBL(control_->vblIOReq, 1);
  if(result < 0)
    {
      return result;
    }
  result = _service_scene(control_);
  if(result != PDWT_SCENE_CONTROL_CONTINUE)
    {
      return result;
    }

  // Keep the restored scene visible for one complete resumed field.
  if(wasPaused && !control_->paused)
    {
      result = WaitVBL(control_->vblIOReq, 1);
      if(result < 0)
        {
          return result;
        }
      return _service_scene(control_);
    }
  return PDWT_SCENE_CONTROL_CONTINUE;
}


static
int32
_wait_while_paused(PDWTSceneControl *control_)
{
  int32 result;

  while(control_->paused)
    {
      result = _wait_scene_vbl(control_);
      if(result != PDWT_SCENE_CONTROL_CONTINUE)
        {
          return result;
        }
    }
  return PDWT_SCENE_CONTROL_CONTINUE;
}


void
PDWTSceneControlInit(PDWTSceneControl *control_)
{
  if(control_ == NULL)
    {
      return;
    }

  memset(control_, 0, sizeof(*control_));
  control_->displayedScreen = -1;
}


int32
PDWTSceneControlBegin(PDWTSceneControl *control_,
                      ScreenContext    *screenContext_,
                      Item              vramIOReq_,
                      Item              vblIOReq_,
                      DecisionAudio    *audio_,
                      PDWTSceneClock   *clock_)
{
  if(control_ == NULL || screenContext_ == NULL || vramIOReq_ <= 0 || vblIOReq_ <= 0 ||
     audio_ == NULL || clock_ == NULL || gSceneControl != 0)
    {
      return -1;
    }

  PDWTSceneControlInit(control_);
  control_->screenContext   = screenContext_;
  control_->audio           = audio_;
  control_->clock           = clock_;
  control_->vramIOReq       = vramIOReq_;
  control_->vblIOReq        = vblIOReq_;
  control_->displayedScreen = screenContext_->sc_curScreen;
  control_->active          = 1;
  gSceneControl            = control_;

  GetJoypad();
  if(controlPadLastError < 0)
    {
      PDWTSceneControlEnd(control_);
      return controlPadLastError;
    }
  // Do not carry X, pause, or step-back presses into a newly started scene.
  control_->previousButtons = controlPadPrevious.cped_ButtonBits & SCENE_INPUT_MASK;
  return PDWT_SCENE_CONTROL_CONTINUE;
}


void
PDWTSceneControlEnd(PDWTSceneControl *control_)
{
  if(control_ == NULL)
    {
      return;
    }
  if(control_->active && control_->paused)
    {
      _restore_scene_frame(control_);
    }
  if(gSceneControl == control_)
    {
      gSceneControl = 0;
    }
  control_->paused = 0;
  control_->active = 0;
}


int32
PDWTSceneControlIsActive(void)
{
  return gSceneControl != 0 && gSceneControl->active;
}


void
PDWTSceneControlPresent(int32 screen_)
{
  if(PDWTSceneControlIsActive() && screen_ >= 0 && screen_ < 2)
    {
      gSceneControl->displayedScreen = screen_;
    }
}


int32
PDWTSceneControlPoll(void)
{
  int32 result;

  if(!PDWTSceneControlIsActive())
    {
      return -1;
    }

  result = _service_scene(gSceneControl);
  if(result != PDWT_SCENE_CONTROL_CONTINUE)
    {
      return result;
    }
  return _wait_while_paused(gSceneControl);
}


int32
PDWTSceneControlWaitFields(Item  vblIOReq_,
                           int32 fields_)
{
  int32 field;
  int32 result;

  if(!PDWTSceneControlIsActive())
    {
      result = WaitForJoypad(vblIOReq_, fields_, ControlRightShift);
      if(result == PDWT_INPUT_STOP)
        {
          return PDWT_SCENE_CONTROL_STEP_BACK;
        }
      return result;
    }
  if(fields_ < 0)
    {
      return -1;
    }

  field = 0;
  while(field < fields_)
    {
      result = _wait_scene_vbl(gSceneControl);
      if(result != PDWT_SCENE_CONTROL_CONTINUE)
        {
          return result;
        }
      if(!gSceneControl->paused)
        {
          field++;
        }
    }
  return PDWT_SCENE_CONTROL_CONTINUE;
}


int32
PDWTSceneControlWaitClock(PDWTSceneClock *clock_,
                          Item            vblIOReq_,
                          int32           fields_)
{
  int32 result;

  if(!PDWTSceneControlIsActive() || clock_ != gSceneControl->clock)
    {
      result = PDWTSceneClockWait(clock_, vblIOReq_, fields_, ControlRightShift);
      if(result == PDWT_INPUT_STOP)
        {
          return PDWT_SCENE_CONTROL_STEP_BACK;
        }
      return result;
    }
  if(!clock_->active)
    {
      return PDWTSceneControlWaitFields(vblIOReq_, fields_);
    }

  PDWTSceneClockAdvance(clock_, fields_);
  while(!PDWTSceneClockReached(clock_))
    {
      result = _wait_scene_vbl(gSceneControl);
      if(result != PDWT_SCENE_CONTROL_CONTINUE)
        {
          return result;
        }
    }

  result = _service_scene(gSceneControl);
  if(result != PDWT_SCENE_CONTROL_CONTINUE)
    {
      return result;
    }
  return _wait_while_paused(gSceneControl);
}


int32
PDWTSceneControlWaitClockEnd(PDWTSceneClock *clock_,
                             Item            vblIOReq_)
{
  int32 result;

  if(!PDWTSceneControlIsActive() || clock_ != gSceneControl->clock)
    {
      result = PDWTSceneClockWaitForEnd(clock_, vblIOReq_, ControlRightShift);
      if(result == PDWT_INPUT_STOP)
        {
          return PDWT_SCENE_CONTROL_STEP_BACK;
        }
      return result;
    }
  if(!clock_->active)
    {
      return PDWTSceneControlPoll();
    }

  while(!PDWTSceneClockEndReached(clock_))
    {
      result = _wait_scene_vbl(gSceneControl);
      if(result != PDWT_SCENE_CONTROL_CONTINUE)
        {
          return result;
        }
    }

  result = _service_scene(gSceneControl);
  if(result != PDWT_SCENE_CONTROL_CONTINUE)
    {
      return result;
    }
  return _wait_while_paused(gSceneControl);
}


int32
PDWTSceneControlWaitAudio(DecisionAudio *audio_,
                          Item           vblIOReq_)
{
  int32 result;

  (void)vblIOReq_;

  if(!PDWTSceneControlIsActive() || audio_ != gSceneControl->audio)
    {
      return -1;
    }

  while(DecisionAudioIsActive(audio_))
    {
      result = _wait_scene_vbl(gSceneControl);
      if(result != PDWT_SCENE_CONTROL_CONTINUE)
        {
          return result;
        }
    }

  result = _service_scene(gSceneControl);
  if(result != PDWT_SCENE_CONTROL_CONTINUE)
    {
      return result;
    }
  return _wait_while_paused(gSceneControl);
}
