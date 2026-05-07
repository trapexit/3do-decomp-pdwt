#pragma once

#include "3do_types.h"

Item
OpenInferior(void);
Item
OpenGraffolio(void);
Item
OpenMathfolio(void);
Item
OpenAudiofolio(void);
Boolean
OpenMacLink(void);
Boolean
OpenSPORT(void);
int32
OpenGraphics(ScreenContextPtr screenContext_,
             int32            nScreens_);
void
CloseGraphics(ScreenContextPtr screenContext_);
void
ShutDown(void);
Item
GrabVRAM(Item *vramIOReq_);
Item
OpenControlPad(int32 deviceNum_);
#ifndef NewPtr
void   *
NewPtr(uint32 size,
       uint32 typeBits);
#endif
#ifndef FreePtr
void   *
FreePtr(void *ptr);
#endif

int32
LinkDeleteItem(Item item_);
int32
LinkRegisterPortHandler(Item    msgPortItem_,
                        int32 (*handler_)(void *ctx,
                                          void *msgDataPtr),
                        void   *handlerCtx_);
uint32
LinkGetMsgPortSignal(Item msgPortItem_);
void  *
LinkGetMsgData(Item msgItem);
uint8
LinkGetMsgFlags(Item msgItem_);
int32
LinkSendMsg(Item  msgPortItem_,
            Item  msgItem_,
            void *msgDataPtr_,
            int32 msgDataSize_);
