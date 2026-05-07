// LinkSupport.c - OS link layer: graphics, ports, memory, and messages.
#include "pdwt_os.h"

#include "3do_types.h"

#include "audio.h"
#include "compression.h"
#include "item.h"
#include "kernel.h"
#include "kernelnodes.h"
#include "msgport.h"
#include "operamath.h"
#include "umemory.h"

#ifdef NewPtr
  #undef NewPtr
#endif
#ifdef FreePtr
  #undef FreePtr
#endif

#include "pdwt_support.h"


#define VRAM_BANK_BITS (MEMTYPE_VRAM | MEMTYPE_STARTPAGE | MEMTYPE_BANKSELECT | MEMTYPE_BANK1)
#define SCREEN_BANK1_BITS (MEMTYPE_VRAM | MEMTYPE_STARTPAGE | MEMTYPE_BANKSELECT | MEMTYPE_BANK1)
#define SCREEN_BANK2_BITS (MEMTYPE_VRAM | MEMTYPE_STARTPAGE | MEMTYPE_BANKSELECT | MEMTYPE_BANK2)

typedef struct LinkScreen
{
  uint8 pad0[120];
  Bitmap *scr_TempBitmap;
} LinkScreen;

#define MAX_PORT_HANDLERS 32

// Item types formed as MKNODEID(KERNELNODE, nodetype)
#define THREAD_ITEM_TYPE MKNODEID(KERNELNODE, TASKNODE)     // 0x105
#define MESSAGE_ITEM_TYPE MKNODEID(KERNELNODE, MESSAGENODE) // 0x109
#define MSGPORT_ITEM_TYPE MKNODEID(KERNELNODE, MSGPORTNODE) // 0x10a

typedef struct PortHandlerRecord
{
  Item msgPortItem;
  void *msgDataPtr;
  int32 (*handler)(void *ctx,
                   void *msgDataPtr);
  void *handlerCtx;
} PortHandlerRecord;

static PortHandlerRecord gPortHandlers[MAX_PORT_HANDLERS];
static Item gScreenGroupItem        = 0;
static int32 gGraphicsFolioOpened    = 0;
static int32 gCompressionFolioOpened = 0;

static
List *
_current_task_mem_lists(void);


static
PortHandlerRecord *
_find_port_handler(Item msgPortItem_);


static
void
_unregister_port_handler(Item msgPortItem_);
static
List *
_current_task_mem_lists(void)
{
  if(KernelBase == 0 || CURRENTTASK == 0)
    {
      return 0;
    }
  return CURRENTTASK->t_FreeMemoryLists;
}


static
PortHandlerRecord *
_find_port_handler(Item msgPortItem_)
{
  int32 i;

  for(i = 0; i < MAX_PORT_HANDLERS; i++)
    {
      if(gPortHandlers[i].msgPortItem == msgPortItem_)
        {
          return &gPortHandlers[i];
        }
    }
  return NULL;
}


static
void
_unregister_port_handler(Item msgPortItem_)
{
  PortHandlerRecord *record;

  record = _find_port_handler(msgPortItem_);
  if(record != NULL)
    {
      record->msgPortItem = 0;
      record->msgDataPtr  = NULL;
      record->handler     = NULL;
      record->handlerCtx  = NULL;
    }
}


Item
OpenInferior(void)
{
  Err err;

  err = OpenCompressionFolio();
  if(err >= 0)
    {
      gCompressionFolioOpened = 1;
    }
  return err;
}


Item
OpenGraffolio(void)
{
  Item item;

  item = OpenGraphicsFolio();
  if(item >= 0)
    {
      gGraphicsFolioOpened = 1;
    }
  return item;
}


Item
OpenMathfolio(void)
{
  return OpenMathFolio();
}


Item
OpenAudiofolio(void)
{
  return OpenAudioFolio();
}


void
CloseGraphics(ScreenContextPtr sc_)
{
  (void)sc_;
  if(gScreenGroupItem > 0)
    {
      RemoveScreenGroup(gScreenGroupItem);
      DeleteScreenGroup(gScreenGroupItem);
      gScreenGroupItem = 0;
    }
  if(gGraphicsFolioOpened)
    {
      CloseGraphicsFolio();
      gGraphicsFolioOpened = 0;
    }
}


int32
OpenGraphics(ScreenContextPtr sc_,
             int32            nScreens_)
{
  TagArg tags[3];
  LinkScreen *screen;
  int32 err;
  int32 i;
  int32 pageSize;
  uint32     *clear;
  uint32 clearWords;


  if(sc_ == NULL || nScreens_ <= 0 || nScreens_ > 6)
    {
      return 0;
    }

  clear      = (uint32 *)sc_;
  clearWords = sizeof(*sc_) / sizeof(uint32);
  while(clearWords-- != 0)
    {
      *clear++ = 0;
    }

  tags[0].ta_Tag = CSG_TAG_SPORTBITS;
  tags[0].ta_Arg = (void *)SCREEN_BANK1_BITS;
  tags[1].ta_Tag = CSG_TAG_SCREENCOUNT;
  tags[1].ta_Arg = (void *)nScreens_;
  tags[2].ta_Tag = CSG_TAG_DONE;
  tags[2].ta_Arg = NULL;

  err = OpenGraphicsFolio();
  if(err < 0)
    {
      return 0;
    }
  gGraphicsFolioOpened = 1;

  gScreenGroupItem = CreateScreenGroup(&sc_->sc_Screens[0], tags);
  if(gScreenGroupItem < 0)
    {
      tags[0].ta_Arg   = (void *)SCREEN_BANK2_BITS;
      gScreenGroupItem = CreateScreenGroup(&sc_->sc_Screens[0], tags);
    }
  if(gScreenGroupItem < 0)
    {
      gScreenGroupItem = 0;
      CloseGraphics(sc_);
      return 0;
    }

  err = AddScreenGroup(gScreenGroupItem, 0);
  if(err < 0)
    {
      CloseGraphics(sc_);
      return 0;
    }

  sc_->sc_nScreens = nScreens_;
  for(i = 0; i < nScreens_; i++)
    {
      screen = (LinkScreen *)LookupItem(sc_->sc_Screens[i]);
      if(screen == 0 || screen->scr_TempBitmap == 0)
        {
          CloseGraphics(sc_);
          return 0;
        }

      sc_->sc_Bitmaps[i]     = screen->scr_TempBitmap;
      sc_->sc_BitmapItems[i] = screen->scr_TempBitmap->bm.n_Item;
      EnableHAVG(sc_->sc_Screens[i]);
      EnableVAVG(sc_->sc_Screens[i]);
    }

  pageSize                 = GetPageSize(MEMTYPE_VRAM);
  sc_->sc_nFrameBufferPages = (sc_->sc_Bitmaps[0]->bm_Width * 2 * sc_->sc_Bitmaps[0]->bm_Height +
                               pageSize - 1) / pageSize;
  sc_->sc_nFrameByteCount   = sc_->sc_nFrameBufferPages * pageSize;
  sc_->sc_curScreen         = 0;


  return 1;
}


Boolean
OpenMacLink(void)
{
  return 1;
}


Boolean
OpenSPORT(void)
{
  return 1;
}


void
ShutDown(void)
{
  if(gCompressionFolioOpened)
    {
      CloseCompressionFolio();
      gCompressionFolioOpened = 0;
    }
}


Item
GrabVRAM(Item *vramIOReq_)
{
  Item item;

  item = GetVRAMIOReq();
  if(vramIOReq_ != NULL)
    {
      *vramIOReq_ = item;
    }
  return item;
}


Item
OpenControlPad(int32 deviceNum_)
{
  return deviceNum_;
}


void *
PDWTAllocMem(int32  size,
             uint32 memType)
{
  List *memLists;

  memLists = _current_task_mem_lists();
  if(memLists == 0)
    {
      return 0;
    }
  return AllocMemFromMemLists(memLists, size, memType);
}


void
PDWTFreeMem(void *memBlock_,
            int32 size_)
{
  List *memLists;

  if(memBlock_ == NULL || size_ == 0)
    {
      return;
    }

  memLists = _current_task_mem_lists();
  if(memLists != 0)
    {
      FreeMemToMemLists(memLists, memBlock_, size_);
    }
}


void *
NewPtr(uint32 size,
       uint32 typebits)
{
  return Malloc(size, typebits);
}


void *
FreePtr(void *ptr)
{
  return Free(ptr);
}


int32
LinkDeleteItem(Item item_)
{
  _unregister_port_handler(item_);
  return DeleteItem(item_);
}


int32
LinkRegisterPortHandler(Item    msgPortItem_,
                        int32 (*handler_)(void *ctx,
                                          void *msgDataPtr),
                        void   *handlerCtx_)
{
  int32 i;
  PortHandlerRecord *record;

  if(msgPortItem_ <= 0 || handler_ == NULL)
    {
      return -1;
    }

  record = _find_port_handler(msgPortItem_);
  if(record == NULL)
    {
      for(i = 0; i < MAX_PORT_HANDLERS; i++)
        {
          if(gPortHandlers[i].msgPortItem == 0)
            {
              record              = &gPortHandlers[i];
              record->msgPortItem = msgPortItem_;
              break;
            }
        }
    }
  if(record == NULL)
    {
      return -1;
    }

  record->msgDataPtr = NULL;
  record->handler    = handler_;
  record->handlerCtx = handlerCtx_;
  return 0;
}


uint32
LinkGetMsgPortSignal(Item msgPortItem_)
{
  MsgPort *msgPortPtr;

  msgPortPtr = (MsgPort *)LookupItem(msgPortItem_);
  if(msgPortPtr == 0)
    {
      return 0;
    }
  return msgPortPtr->mp_Signal;
}


void *
LinkGetMsgData(Item msgItem)
{
  Message *messagePtr;

  messagePtr = (Message *)LookupItem(msgItem);
  if(messagePtr == 0)
    {
      return 0;
    }
  return messagePtr->msg_DataPtr;
}


uint8
LinkGetMsgFlags(Item msgItem_)
{
  Message *messagePtr;

  messagePtr = (Message *)LookupItem(msgItem_);
  if(messagePtr == 0)
    {
      return 0;
    }
  return messagePtr->msg.n_Flags;
}


int32
LinkSendMsg(Item  msgPortItem_,
            Item  msgItem_,
            void *msgDataPtr_,
            int32 msgDataSize_)
{
  PortHandlerRecord *record;

  record = _find_port_handler(msgPortItem_);
  if(record != NULL && record->handler != NULL)
    {
      (void)msgItem_;
      (void)msgDataSize_;
      record->msgDataPtr = msgDataPtr_;
      return record->handler(record->handlerCtx, msgDataPtr_);
    }

  return SendMsg(msgPortItem_, msgItem_, msgDataPtr_, msgDataSize_);
}
