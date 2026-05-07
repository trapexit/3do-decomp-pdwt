// PlayCPakStream.c - Cinepak stream player via the CPak subscriber.
#include "3do_types.h"

#include "controlsubscriber.h"
#include "cpaksubscriber.h"
#include "dataacq.h"
#include "datastreamdebug.h"
#include "datastreamlib.h"
#include "dsstreamheader.h"
#include "item.h"
#include "preparestream.h"
#include "saudiosubscriber.h"
#include "umemory.h"

#define REPORT_DS_FAILURE(dsResult)                              \
        do                                                       \
          {                                                        \
            PrintfDSError((int32)(dsResult));                      \
          } while(0)

struct Player
{
  PlayCPakUserFn userFn;
  void            *userContext;
  DSHeaderChunk hdr;
  DSDataBufPtr bufferList;
  AcqContextPtr acqContext;
  DSStreamCBPtr streamCBPtr;
  ScreenContext   *screenContextPtr;
  Item VBLIOReq;
  Item VRAMIOReq;
  Item messagePort;
  Item messageItem;
  Item endOfStreamMessageItem;
  CtrlContextPtr controlContextPtr;
  SAudioContextPtr audioContextPtr;
  CPakContextPtr cpakContextPtr;
  CPakRecPtr cpakChannelPtr;
  Item JoinSubsMsgPort;
  int32 dataAcqOpen;
  int32 dataStreamingOpen;
  int32 controlSubscriberOpen;
  int32 audioSubscriberOpen;
  int32 cpakSubscriberOpen;
};

int32
InitCPakPlayerFromStreamHeader(PlayerPtr ctx_,
                               uchar    *streamFileName_);
static
int32
_dismantle_player(PlayerPtr ctx_);

/*
 * PlayCPakStream() - Line 31
 * High-level Cinepak stream playback.
 * Address: 0x1FE0 - 0x22CC
 */
int32
PlayCPakStream(ScreenContext *screenContextPtr_,
               uchar         *streamFileName_,
               PlayCPakUserFn userFn_)
{
  int32 cleanupResult;
  int32 playerResult;
  int32 status;
  int32 streamStarted;
  Player playerContext;
  PlayerPtr ctx;
  DSRequestMsg EOSMessage;
  Bitmap      *bitmap;

  if(screenContextPtr_ == NULL ||
     screenContextPtr_->sc_curScreen < 0 ||
     screenContextPtr_->sc_curScreen >= screenContextPtr_->sc_nScreens ||
     screenContextPtr_->sc_curScreen >= 6 ||
     screenContextPtr_->sc_Screens[screenContextPtr_->sc_curScreen] <= 0 ||
     screenContextPtr_->sc_Bitmaps[screenContextPtr_->sc_curScreen] == NULL)
    {
      return -1;
    }

  ctx           = &playerContext;
  streamStarted = 0;
  status        = InitCPakPlayerFromStreamHeader(ctx, streamFileName_);
  if(status != 0)
    {
      return status;
    }

  ctx->screenContextPtr = screenContextPtr_;
  ctx->userFn           = userFn_;
  ctx->userContext      = NULL;

  status = DSStartStream(ctx->messageItem, NULL, ctx->streamCBPtr, 0);
  if(status != 0)
    {
      REPORT_DS_FAILURE(status);
      playerResult = status;
      goto CLEANUP;
    }
  streamStarted = 1;

  if(ctx->audioContextPtr == NULL)
    {
      status = DSSetClock(ctx->streamCBPtr, 0);
      if(status != 0)
        {
          REPORT_DS_FAILURE(status);
          playerResult = status;
          goto CLEANUP;
        }
    }

  status = DSWaitEndOfStream(ctx->endOfStreamMessageItem, &EOSMessage, ctx->streamCBPtr);
  if(status != 0)
    {
      REPORT_DS_FAILURE(status);
      playerResult = status;
      goto CLEANUP;
    }

  DisplayScreen(ctx->screenContextPtr->sc_Screens[ctx->screenContextPtr->sc_curScreen], 0);
  bitmap = ctx->screenContextPtr->sc_Bitmaps[ctx->screenContextPtr->sc_curScreen];

  playerResult = 0;
  while(TRUE)
    {
      if(ctx->userFn != NULL)
        {
          playerResult = (*ctx->userFn)(ctx);
          if(playerResult != 0)
            {
              break;
            }
        }

      if(PollForMsg(ctx->messagePort, NULL, NULL, NULL, &status))
        {
          break;
        }
      if(status < 0)
        {
          playerResult = status;
          break;
        }

      status = WaitVBL(ctx->VBLIOReq, 1);
      if(status < 0)
        {
          playerResult = status;
          break;
        }

      if(ctx->cpakContextPtr != NULL)
        {
          DrawCPakToBuffer(ctx->cpakContextPtr, ctx->cpakChannelPtr, bitmap);
          status = SendFreeCPakSignal(ctx->cpakContextPtr);
          if(status < 0)
            {
              playerResult = status;
              break;
            }
        }
    }

  status = DSStopStream(ctx->messageItem, NULL, ctx->streamCBPtr, SOPT_FLUSH);
  if(status != 0)
    {
      REPORT_DS_FAILURE(status);
      if(playerResult >= 0)
        {
          playerResult = status;
        }
    }
  streamStarted = 0;

  if(ctx->cpakContextPtr != NULL)
    {
      FlushCPakChannel(ctx->cpakContextPtr, ctx->cpakChannelPtr, 0);
    }

  cleanupResult = _dismantle_player(ctx);
  if(cleanupResult < 0 && playerResult >= 0)
    {
      playerResult = cleanupResult;
    }
  return playerResult;

CLEANUP:
  if(streamStarted)
    {
      cleanupResult = DSStopStream(ctx->messageItem, NULL, ctx->streamCBPtr, SOPT_FLUSH);
      if(cleanupResult < 0)
        {
          REPORT_DS_FAILURE(cleanupResult);
          if(playerResult >= 0)
            {
              playerResult = cleanupResult;
            }
        }
    }
  cleanupResult = _dismantle_player(ctx);
  if(cleanupResult < 0 && playerResult >= 0)
    {
      playerResult = cleanupResult;
    }
  return playerResult;
}


/*
 * InitCPakPlayerFromStreamHeader() - Line 150
 * Initialize stream, data acquisition, and subscribers from the stream header.
 * Address: 0x22CC - 0x2B18
 */
int32
InitCPakPlayerFromStreamHeader(PlayerPtr ctx_,
                               uchar    *streamFileName_)
{
  int32 status;
  long subscriberIndex;
  long channelNum;
  SAudioCtlBlock ctlBlock;
  DSHeaderSubsPtr subsPtr;
  boolean fStreamHasAudio;

  fStreamHasAudio = FALSE;

  ctx_->bufferList             = NULL;
  ctx_->streamCBPtr            = NULL;
  ctx_->acqContext             = NULL;
  ctx_->messageItem            = 0;
  ctx_->endOfStreamMessageItem = 0;
  ctx_->messagePort            = 0;
  ctx_->controlContextPtr      = NULL;
  ctx_->audioContextPtr        = NULL;
  ctx_->cpakChannelPtr         = NULL;
  ctx_->cpakContextPtr         = NULL;
  ctx_->VBLIOReq               = 0;
  ctx_->VRAMIOReq              = 0;
  ctx_->JoinSubsMsgPort        = 0;
  ctx_->dataAcqOpen            = 0;
  ctx_->dataStreamingOpen      = 0;
  ctx_->controlSubscriberOpen  = 0;
  ctx_->audioSubscriberOpen    = 0;
  ctx_->cpakSubscriberOpen     = 0;

  ctx_->VBLIOReq = GetVBLIOReq();
  if(ctx_->VBLIOReq < 0)
    {
      return ctx_->VBLIOReq;
    }

  status = FindAndLoadStreamHeader(&ctx_->hdr, (char *)streamFileName_);
  if(status != 0)
    {
      goto CLEANUP;
    }

  if(ctx_->hdr.headerVersion != DS_STREAM_VERSION)
    {
      status = kPSVersionErr;
      goto CLEANUP;
    }

  ctx_->bufferList = CreateBufferList(ctx_->hdr.streamBuffers, ctx_->hdr.streamBlockSize);
  if(ctx_->bufferList == NULL)
    {
      status = kPSMemFullErr;
      goto CLEANUP;
    }

  ctx_->messagePort = NewMsgPort(NULL);
  if(ctx_->messagePort < 0)
    {
      status = ctx_->messagePort;
      goto CLEANUP;
    }

  ctx_->messageItem = CreateMsgItem(ctx_->messagePort);
  if(ctx_->messageItem < 0)
    {
      status = ctx_->messageItem;
      goto CLEANUP;
    }

  ctx_->endOfStreamMessageItem = CreateMsgItem(ctx_->messagePort);
  if(ctx_->endOfStreamMessageItem < 0)
    {
      status = ctx_->endOfStreamMessageItem;
      goto CLEANUP;
    }

  status = InitDataAcq(1);
  if(status != 0)
    {
      goto CLEANUP;
    }
  ctx_->dataAcqOpen = 1;

  status = NewDataAcq(&ctx_->acqContext, (char *)streamFileName_, ctx_->hdr.dataAcqDeltaPri);
  if(status != 0)
    {
      goto CLEANUP;
    }

  status = InitDataStreaming(1);
  if(status != 0)
    {
      goto CLEANUP;
    }
  ctx_->dataStreamingOpen = 1;

  status = NewDataStream(
    &ctx_->streamCBPtr,
    ctx_->bufferList,
    ctx_->hdr.streamBlockSize,
    ctx_->hdr.streamerDeltaPri,
    ctx_->hdr.numSubsMsgs);
  if(status != 0)
    {
      goto CLEANUP;
    }

  status = DSConnect(ctx_->messageItem, NULL, ctx_->streamCBPtr, ctx_->acqContext->requestPort);
  if(status != 0)
    {
      REPORT_DS_FAILURE(status);
      goto CLEANUP;
    }

  for(subscriberIndex = 0;
      subscriberIndex < DS_HDR_MAX_SUBSCRIBER &&
      ctx_->hdr.subscriberList[subscriberIndex].subscriberType != 0;
      subscriberIndex++)
    {
      subsPtr = ctx_->hdr.subscriberList + subscriberIndex;

      switch(subsPtr->subscriberType)
        {
        case FILM_CHUNK_TYPE:
          status = InitCPakSubscriber();
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }
          ctx_->cpakSubscriberOpen = 1;

          status = NewCPakSubscriber(&ctx_->cpakContextPtr, 1, subsPtr->deltaPriority);
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }

          status = DSSubscribe(
            ctx_->messageItem,
            NULL,
            ctx_->streamCBPtr,
            (DSDataType)FILM_CHUNK_TYPE,
            ctx_->cpakContextPtr->requestPort);
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }

          status = InitCPakCel(ctx_->streamCBPtr, ctx_->cpakContextPtr, &ctx_->cpakChannelPtr, 0,
                               TRUE);
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }
          break;

        case SNDS_CHUNK_TYPE:
          status = InitSAudioSubscriber();
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }
          ctx_->audioSubscriberOpen = 1;

          status = NewSAudioSubscriber(&ctx_->audioContextPtr, ctx_->streamCBPtr,
                                       subsPtr->deltaPriority);
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }

          status = DSSubscribe(
            ctx_->messageItem,
            NULL,
            ctx_->streamCBPtr,
            (DSDataType)SNDS_CHUNK_TYPE,
            ctx_->audioContextPtr->requestPort);
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }

          fStreamHasAudio = TRUE;
          break;

        case CTRL_CHUNK_TYPE:
          status = InitCtrlSubscriber();
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }
          ctx_->controlSubscriberOpen = 1;

          status = NewCtrlSubscriber(&ctx_->controlContextPtr, ctx_->streamCBPtr,
                                     subsPtr->deltaPriority);
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }

          status = DSSubscribe(
            ctx_->messageItem,
            NULL,
            ctx_->streamCBPtr,
            (DSDataType)CTRL_CHUNK_TYPE,
            ctx_->controlContextPtr->requestPort);
          if(status != 0)
            {
              REPORT_DS_FAILURE(status);
              goto CLEANUP;
            }
          break;

        default:
          status = kPSUnknownSubscriber;
          goto CLEANUP;
        }
    }
  if(subscriberIndex == DS_HDR_MAX_SUBSCRIBER)
    {
      status = kPSUnknownSubscriber;
      goto CLEANUP;
    }

  if(fStreamHasAudio)
    {
      if(ctx_->hdr.preloadInstList[0] != 0)
        {
          ctlBlock.loadTemplates.tagListPtr = ctx_->hdr.preloadInstList;
          status =
            DSControl(ctx_->messageItem, NULL, ctx_->streamCBPtr, SNDS_CHUNK_TYPE,
                      kSAudioCtlOpLoadTemplates, &ctlBlock);
          if(status != 0)
            {
              goto CLEANUP;
            }
        }

      for(channelNum = 1; channelNum < 32; channelNum++)
        {
          if(ctx_->hdr.enableAudioChan & (1UL << channelNum))
            {
              status = DSSetChannel(ctx_->messageItem, NULL, ctx_->streamCBPtr, SNDS_CHUNK_TYPE,
                                    channelNum,
                                    CHAN_ENABLED);
              if(status != 0)
                {
                  REPORT_DS_FAILURE(status);
                  goto CLEANUP;
                }
            }
        }

      ctlBlock.clock.channelNumber = ctx_->hdr.audioClockChan;
      status = DSControl(ctx_->messageItem, NULL, ctx_->streamCBPtr, SNDS_CHUNK_TYPE,
                         kSAudioCtlOpSetClockChan, &ctlBlock);
      if(status != 0)
        {
          REPORT_DS_FAILURE(status);
          goto CLEANUP;
        }
    }

  return 0;

CLEANUP:
  _dismantle_player(ctx_);
  return status;
}


/*
 * DismantlePlayer() - Line 405
 * Free resources associated with the Player structure.
 * Address: 0x2B18 - 0x2C34
 */
static
int32
_dismantle_player(PlayerPtr ctx_)
{
  int32 result;

  if(ctx_ == NULL)
    {
      return 0;
    }

  result = 0;
  if(ctx_->streamCBPtr != NULL)
    {
      result = DisposeDataStream(ctx_->messageItem, ctx_->streamCBPtr);
      // DisposeDataStream destroys the thread and its storage even when closing reports an error.
      ctx_->streamCBPtr = NULL;
    }
  if(ctx_->dataStreamingOpen)
    {
      CloseDataStreaming();
      ctx_->dataStreamingOpen = 0;
    }

  if(ctx_->acqContext != NULL)
    {
      DisposeDataAcq(ctx_->acqContext);
      ctx_->acqContext = NULL;
    }
  if(ctx_->dataAcqOpen)
    {
      CloseDataAcq();
      ctx_->dataAcqOpen = 0;
    }

  if(ctx_->controlContextPtr != NULL)
    {
      DisposeCtrlSubscriber(ctx_->controlContextPtr);
      ctx_->controlContextPtr = NULL;
    }
  if(ctx_->controlSubscriberOpen)
    {
      CloseCtrlSubscriber();
      ctx_->controlSubscriberOpen = 0;
    }

  if(ctx_->cpakChannelPtr != NULL)
    {
      DestroyCPakCel(ctx_->cpakContextPtr, ctx_->cpakChannelPtr, 0);
      ctx_->cpakChannelPtr = NULL;
    }

  if(ctx_->cpakContextPtr != NULL)
    {
      DisposeCPakSubscriber(ctx_->cpakContextPtr);
      ctx_->cpakContextPtr = NULL;
    }
  if(ctx_->cpakSubscriberOpen)
    {
      CloseCPakSubscriber();
      ctx_->cpakSubscriberOpen = 0;
    }

  if(ctx_->audioContextPtr != NULL)
    {
      DisposeSAudioSubscriber(ctx_->audioContextPtr);
      ctx_->audioContextPtr = NULL;
    }
  if(ctx_->audioSubscriberOpen)
    {
      CloseSAudioSubscriber();
      ctx_->audioSubscriberOpen = 0;
    }

  if(ctx_->bufferList != NULL)
    {
      FreePtr(ctx_->bufferList);
      ctx_->bufferList = NULL;
    }

  if(ctx_->messageItem > 0)
    {
      RemoveMsgItem(ctx_->messageItem);
      ctx_->messageItem = 0;
    }

  if(ctx_->endOfStreamMessageItem > 0)
    {
      RemoveMsgItem(ctx_->endOfStreamMessageItem);
      ctx_->endOfStreamMessageItem = 0;
    }

  if(ctx_->messagePort > 0)
    {
      RemoveMsgPort(ctx_->messagePort);
      ctx_->messagePort = 0;
    }

  if(ctx_->VBLIOReq > 0)
    {
      DeleteItem(ctx_->VBLIOReq);
      ctx_->VBLIOReq = 0;
    }

  return result;
}
