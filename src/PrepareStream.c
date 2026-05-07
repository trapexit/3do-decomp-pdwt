// PrepareStream.c - stream buffer-list and header preparation.
#include "blockfile.h"
#include "item.h"
#include "preparestream.h"
#include "umemory.h"

#include "string.h"

#define ROUND_TO_LONG(x) (((x) + 3) & ~3)
#define MAX_BUFFER_LIST_BYTES 2147483647L

/*
 * CreateBufferList() - Line 25
 * Create the streamer data-buffer list using the SDK DSDataBuf layout.
 * Address: 0x1D98 - 0x1E38
 */
DSDataBufPtr
CreateBufferList(long numBuffers_,
                 long bufferSize_)
{
  DSDataBufPtr bp;
  DSDataBufPtr firstBp;
  long totalBufferSpace;
  long actualEntrySize;
  long roundedBufferSize;
  long bufferNum;

  if(numBuffers_ <= 0 || bufferSize_ <= 0)
    {
      return NULL;
    }

  if(bufferSize_ > MAX_BUFFER_LIST_BYTES - 3)
    {
      return NULL;
    }
  roundedBufferSize = ROUND_TO_LONG(bufferSize_);
  if(roundedBufferSize > MAX_BUFFER_LIST_BYTES - (long)sizeof(DSDataBuf))
    {
      return NULL;
    }
  actualEntrySize = (long)sizeof(DSDataBuf) + roundedBufferSize;
  if(numBuffers_ > MAX_BUFFER_LIST_BYTES / actualEntrySize)
    {
      return NULL;
    }
  totalBufferSpace = numBuffers_ * actualEntrySize;

  firstBp = (DSDataBufPtr)NewPtr(totalBufferSpace, MEMTYPE_ANY);
  if(firstBp == NULL)
    {
      return NULL;
    }

  for(bp = firstBp, bufferNum = 0; bufferNum < (numBuffers_ - 1); bufferNum++)
    {
      bp->next          = (DSDataBufPtr)(((char *)bp) + actualEntrySize);
      bp->permanentNext = bp->next;
      bp                = bp->next;
    }

  bp->next          = NULL;
  bp->permanentNext = NULL;
  return firstBp;
}


/*
 * FindAndLoadStreamHeader() - Line 67
 * Load the stream header from the first block of the stream file.
 * Address: 0x1E38 - 0x1FE0
 */
int32
FindAndLoadStreamHeader(DSHeaderChunkPtr headerPtr_,
                        char            *fileName_)
{
  int32 status;
  BlockFile blockFile;
  IOReq *ioReq;
  Item ioDoneReplyPort;
  Item ioReqItem;
  long     *pLong;
  char     *buffer;

  if(headerPtr_ == NULL || fileName_ == NULL)
    {
      return -1;
    }

  blockFile.fDevice = 0;
  ioDoneReplyPort   = 0;
  ioReqItem         = 0;
  buffer            = NULL;
  ioReq            = NULL;

  buffer = (char *)NewPtr(FIND_HEADER_BUFFER_SIZE, MEMTYPE_ANY);
  if(buffer == NULL)
    {
      return kPSMemFullErr;
    }

  status = OpenBlockFile(fileName_, &blockFile);
  if(status != 0)
    {
      goto BAILOUT;
    }

  ioDoneReplyPort = NewMsgPort(NULL);
  if(ioDoneReplyPort < 0)
    {
      status = ioDoneReplyPort;
      goto BAILOUT;
    }

  ioReqItem = CreateBlockFileIOReq(blockFile.fDevice, ioDoneReplyPort);
  if(ioReqItem < 0)
    {
      status = ioReqItem;
      goto BAILOUT;
    }

  status = AsynchReadBlockFile(&blockFile, ioReqItem, buffer, FIND_HEADER_BUFFER_SIZE, 0);
  if(status < 0)
    {
      goto BAILOUT;
    }

  status = WaitReadDoneBlockFile(ioReqItem);
  if(status < 0)
    {
      goto BAILOUT;
    }
  ioReq = (IOReq *)LookupItem(ioReqItem);
  if(ioReq == NULL)
    {
      status = -1;
      goto BAILOUT;
    }
  if(ioReq->io_Error < 0)
    {
      status = ioReq->io_Error;
      goto BAILOUT;
    }
  if(ioReq->io_Actual < (int32)sizeof(*headerPtr_))
    {
      status = kPSHeaderNotFound;
      goto BAILOUT;
    }

  CloseBlockFile(&blockFile);
  blockFile.fDevice = 0;

  pLong = (long *)buffer;
  if(*pLong == HEADER_CHUNK_TYPE)
    {
      memcpy(headerPtr_, pLong, sizeof(*headerPtr_));
      status = 0;
    }
  else
    {
      status = kPSHeaderNotFound;
    }

BAILOUT:
  if(buffer != NULL)
    {
      FreePtr(buffer);
    }
  if(blockFile.fDevice != 0)
    {
      CloseBlockFile(&blockFile);
    }
  if(ioReqItem > 0)
    {
      DeleteItem(ioReqItem);
    }
  if(ioDoneReplyPort > 0)
    {
      RemoveMsgPort(ioDoneReplyPort);
    }

  return status;
}
