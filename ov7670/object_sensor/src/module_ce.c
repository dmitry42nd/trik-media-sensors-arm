#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <xdc/std.h>
#include <xdc/runtime/Diags.h>
#include <ti/sdo/ce/CERuntime.h>
#include <ti/sdo/ce/Server.h>

#include <linux/videodev2.h>

#include "trik_vidtranscode_cv.h"

#include "internal/module_ce.h"


#warning Check BUFALIGN usage!
#ifndef BUFALIGN
#define BUFALIGN 128
#endif

#define ALIGN_UP(v, a) ((((v)+(a)-1)/(a))*(a))


static bool s_verbose = false;


static int do_memoryAlloc(CodecEngine* _ce, size_t _srcBufferSize, size_t _dstBufferSize)
{
  memset(&_ce->m_allocParams, 0, sizeof(_ce->m_allocParams));
  _ce->m_allocParams.type = Memory_CONTIGPOOL;
  _ce->m_allocParams.flags = Memory_CACHED;
  _ce->m_allocParams.align = BUFALIGN;
  _ce->m_allocParams.seg = 0;

  _ce->m_srcBufferSize = ALIGN_UP(_srcBufferSize, BUFALIGN);
  if ((_ce->m_srcBuffer = Memory_alloc(_ce->m_srcBufferSize, &_ce->m_allocParams)) == NULL)
  {
    fprintf(stderr, "Memory_alloc(src, %zu) failed\n", _ce->m_srcBufferSize);
    _ce->m_srcBufferSize = 0;
    return ENOMEM;
  }

  _ce->m_dstBufferSize = ALIGN_UP(_dstBufferSize, BUFALIGN);
  if ((_ce->m_dstBuffer = Memory_alloc(_ce->m_dstBufferSize, &_ce->m_allocParams)) == NULL)
  {
    fprintf(stderr, "Memory_alloc(dst, %zu) failed\n", _ce->m_dstBufferSize);
    _ce->m_dstBufferSize = 0;

    Memory_free(_ce->m_srcBuffer, _ce->m_srcBufferSize, &_ce->m_allocParams);
    _ce->m_srcBuffer = NULL;
    _ce->m_srcBufferSize = 0;
    return ENOMEM;
  }
  memset(_ce->m_dstBuffer, 0, _ce->m_dstBufferSize);

  return 0;
}

static int do_memoryFree(CodecEngine* _ce)
{
  if (_ce->m_dstBuffer != NULL)
  {
    Memory_free(_ce->m_dstBuffer, _ce->m_dstBufferSize, &_ce->m_allocParams);
    _ce->m_dstBuffer = NULL;
    _ce->m_dstBufferSize = 0;
  }

  if (_ce->m_srcBuffer != NULL)
  {
    Memory_free(_ce->m_srcBuffer, _ce->m_srcBufferSize, &_ce->m_allocParams);
    _ce->m_srcBuffer = NULL;
    _ce->m_srcBufferSize = 0;
  }

  return 0;
}

static XDAS_Int32 do_convertPixelFormat(CodecEngine* _ce, uint32_t _format)
{
  (void)_ce;

  switch (_format)
  {
    case V4L2_PIX_FMT_RGB24:	return TRIK_VIDTRANSCODE_CV_VIDEO_FORMAT_RGB888;
    case V4L2_PIX_FMT_RGB565:	return TRIK_VIDTRANSCODE_CV_VIDEO_FORMAT_RGB565;
    case V4L2_PIX_FMT_RGB565X:	return TRIK_VIDTRANSCODE_CV_VIDEO_FORMAT_RGB565X;
    case V4L2_PIX_FMT_YUV32:	return TRIK_VIDTRANSCODE_CV_VIDEO_FORMAT_YUV444;
    case V4L2_PIX_FMT_YUYV:	return TRIK_VIDTRANSCODE_CV_VIDEO_FORMAT_YUV422;
    default:
      fprintf(stderr, "Unknown pixel format %c%c%c%c\n",
              _format&0xff, (_format>>8)&0xff, (_format>>16)&0xff, (_format>>24)&0xff);
      return TRIK_VIDTRANSCODE_CV_VIDEO_FORMAT_UNKNOWN;
  }
}

static int do_setupCodec(CodecEngine* _ce, const char* _codecName,
                         const ImageDescription* _srcImageDesc,
                         const ImageDescription* _dstImageDesc)
{
  if (_codecName == NULL || _srcImageDesc == NULL || _dstImageDesc == NULL)
    return EINVAL;

  if (s_verbose)
    fprintf(stderr, "VIDTRANSCODE_control(%c%c%c%c@%zux%zu[%zu] -> %c%c%c%c@%zux%zu[%zu])\n",
            (_srcImageDesc->m_format    )&0xff,
            (_srcImageDesc->m_format>> 8)&0xff,
            (_srcImageDesc->m_format>>16)&0xff,
            (_srcImageDesc->m_format>>24)&0xff,
            _srcImageDesc->m_width, _srcImageDesc->m_height, _srcImageDesc->m_lineLength,
            (_dstImageDesc->m_format    )&0xff,
            (_dstImageDesc->m_format>> 8)&0xff,
            (_dstImageDesc->m_format>>16)&0xff,
            (_dstImageDesc->m_format>>24)&0xff,
            _dstImageDesc->m_width, _dstImageDesc->m_height, _dstImageDesc->m_lineLength);

  TRIK_VIDTRANSCODE_CV_Params ceParams;
  memset(&ceParams, 0, sizeof(ceParams));
  ceParams.base.size = sizeof(ceParams);
  ceParams.base.numOutputStreams = 1;
  ceParams.base.formatInput = do_convertPixelFormat(_ce, _srcImageDesc->m_format);
  ceParams.base.formatOutput[0] = do_convertPixelFormat(_ce, _dstImageDesc->m_format);
  #define max(x, y) x > y ? x : y;
  ceParams.base.maxHeightInput = max(_srcImageDesc->m_height,_srcImageDesc->m_width);
  ceParams.base.maxWidthInput = max(_srcImageDesc->m_height,_srcImageDesc->m_width);
  ceParams.base.maxHeightOutput[0] = max(_dstImageDesc->m_height,_dstImageDesc->m_width);
  ceParams.base.maxWidthOutput[0] = max(_dstImageDesc->m_height,_dstImageDesc->m_width);
  ceParams.base.dataEndianness = XDM_BYTE;

  char* codec = strdup(_codecName);
  if ((_ce->m_vidtranscodeHandle = VIDTRANSCODE_create(_ce->m_handle, codec, &ceParams.base)) == NULL)
  {
    free(codec);
    fprintf(stderr, "VIDTRANSCODE_create(%s) failed\n", _codecName);
    return EBADRQC;
  }
  free(codec);

  TRIK_VIDTRANSCODE_CV_DynamicParams ceDynamicParams;
  memset(&ceDynamicParams, 0, sizeof(ceDynamicParams));
  ceDynamicParams.base.size = sizeof(ceDynamicParams);
  ceDynamicParams.base.keepInputResolutionFlag[0] = XDAS_FALSE;
  ceDynamicParams.base.outputHeight[0] = _dstImageDesc->m_height;
  ceDynamicParams.base.outputWidth[0] = _dstImageDesc->m_width;
  ceDynamicParams.base.keepInputFrameRateFlag[0] = XDAS_TRUE;
  ceDynamicParams.inputHeight = _srcImageDesc->m_height;
  ceDynamicParams.inputWidth = _srcImageDesc->m_width;
  ceDynamicParams.inputLineLength = _srcImageDesc->m_lineLength;
  ceDynamicParams.outputLineLength[0] = _dstImageDesc->m_lineLength;

  IVIDTRANSCODE_Status ceStatus;
  memset(&ceStatus, 0, sizeof(ceStatus));
  ceStatus.size = sizeof(ceStatus);
  XDAS_Int32 controlResult = VIDTRANSCODE_control(_ce->m_vidtranscodeHandle, XDM_SETPARAMS, &ceDynamicParams.base, &ceStatus);
  if (controlResult != IVIDTRANSCODE_EOK)
  {
    fprintf(stderr, "VIDTRANSCODE_control() failed: %"PRIi32"/%"PRIi32"\n", controlResult, ceStatus.extendedError);
    return EBADRQC;
  }

  return 0;
}

static int do_releaseCodec(CodecEngine* _ce)
{
  if (_ce->m_vidtranscodeHandle != NULL)
    VIDTRANSCODE_delete(_ce->m_vidtranscodeHandle);
  _ce->m_vidtranscodeHandle = NULL;

  return 0;
}

static int makeValueRange(int _val, int _adj, int _min, int _max)
{
  _val += _adj;
  if (_val > _max)
    return _max;
  else if (_val < _min)
    return _min;
  else
    return _val;
}

static int makeValueWrap(int _val, int _adj, int _min, int _max)
{
  _val += _adj;
  while (_val > _max)
    _val -= (_max-_min+1);
  while (_val < _min)
    _val += (_max-_min+1);

  return _val;
}

static int do_transcodeFrame(CodecEngine* _ce,
                             const void* _srcFramePtr, size_t _srcFrameSize,
                             void* _dstFramePtr, size_t _dstFrameSize, size_t* _dstFrameUsed,
                             const TargetDetectParams* _targetDetectParams,
                             const TargetDetectCommand* _targetDetectCommand,
                             TargetLocation* _targetLocation,
                             TargetDetectParams* _targetDetectParamsResult)
{
  if (_ce->m_srcBuffer == NULL || _ce->m_dstBuffer == NULL)
    return ENOTCONN;
  if (   _srcFramePtr == NULL || _dstFramePtr == NULL
      || _targetDetectParams == NULL || _targetDetectCommand == NULL
      || _targetLocation == NULL || _targetDetectParamsResult == NULL)
    return EINVAL;
  if (_srcFrameSize > _ce->m_srcBufferSize || _dstFrameSize > _ce->m_dstBufferSize)
    return ENOSPC;


  TRIK_VIDTRANSCODE_CV_InArgs tcInArgs;
  memset(&tcInArgs, 0, sizeof(tcInArgs));
  tcInArgs.base.size = sizeof(tcInArgs);
  tcInArgs.base.numBytes = _srcFrameSize;
  tcInArgs.base.inputID = 1; // must be non-zero, otherwise caching issues appear
  tcInArgs.alg.detectHueFrom = makeValueWrap( _targetDetectParams->m_detectHue, -_targetDetectParams->m_detectHueTolerance, 0, 359);
  tcInArgs.alg.detectHueTo   = makeValueWrap( _targetDetectParams->m_detectHue, +_targetDetectParams->m_detectHueTolerance, 0, 359);
  tcInArgs.alg.detectSatFrom = makeValueRange(_targetDetectParams->m_detectSat, -_targetDetectParams->m_detectSatTolerance, 0, 100);
  tcInArgs.alg.detectSatTo   = makeValueRange(_targetDetectParams->m_detectSat, +_targetDetectParams->m_detectSatTolerance, 0, 100);
  tcInArgs.alg.detectValFrom = makeValueRange(_targetDetectParams->m_detectVal, -_targetDetectParams->m_detectValTolerance, 0, 100);
  tcInArgs.alg.detectValTo   = makeValueRange(_targetDetectParams->m_detectVal, +_targetDetectParams->m_detectValTolerance, 0, 100);
  tcInArgs.alg.autoDetectHsv = _targetDetectCommand->m_cmd;

  TRIK_VIDTRANSCODE_CV_OutArgs tcOutArgs;
  memset(&tcOutArgs,    0, sizeof(tcOutArgs));
  tcOutArgs.base.size = sizeof(tcOutArgs);

  XDM1_BufDesc tcInBufDesc;
  memset(&tcInBufDesc,  0, sizeof(tcInBufDesc));
  tcInBufDesc.numBufs = 1;
  tcInBufDesc.descs[0].buf = _ce->m_srcBuffer;
  tcInBufDesc.descs[0].bufSize = _srcFrameSize;

  XDM_BufDesc tcOutBufDesc;
  memset(&tcOutBufDesc, 0, sizeof(tcOutBufDesc));
  XDAS_Int8* tcOutBufDesc_bufs[1];
  XDAS_Int32 tcOutBufDesc_bufSizes[1];
  tcOutBufDesc.numBufs = 1;
  tcOutBufDesc.bufs = tcOutBufDesc_bufs;
  tcOutBufDesc.bufs[0] = _ce->m_dstBuffer;
  tcOutBufDesc.bufSizes = tcOutBufDesc_bufSizes;
  tcOutBufDesc.bufSizes[0] = _dstFrameSize;

#warning This memcpy is blocking high fps
  memcpy(_ce->m_srcBuffer, _srcFramePtr, _srcFrameSize);

  Memory_cacheWbInv(_ce->m_srcBuffer, _ce->m_srcBufferSize); // invalidate and flush *whole* cache, not only written portion, just in case
  Memory_cacheInv(_ce->m_dstBuffer, _ce->m_dstBufferSize); // invalidate *whole* cache, not only expected portion, just in case

  XDAS_Int32 processResult = VIDTRANSCODE_process(_ce->m_vidtranscodeHandle, &tcInBufDesc, &tcOutBufDesc, &tcInArgs.base, &tcOutArgs.base);
  if (processResult != IVIDTRANSCODE_EOK)
  {
    fprintf(stderr, "VIDTRANSCODE_process(%zu -> %zu) failed: %"PRIi32"/%"PRIi32"\n",
            _srcFrameSize, _dstFrameSize, processResult, tcOutArgs.base.extendedError);
    return EILSEQ;
  }

  if (tcOutArgs.base.encodedBuf[0].bufSize < 0)
  {
    *_dstFrameUsed = 0;
    fprintf(stderr, "VIDTRANSCODE_process(%zu -> %zu) returned negative buffer size\n",
            _srcFrameSize, _dstFrameSize);
  }
  else if ((size_t)(tcOutArgs.base.encodedBuf[0].bufSize) > _dstFrameSize)
  {
    *_dstFrameUsed = _dstFrameSize;
    fprintf(stderr, "VIDTRANSCODE_process(%zu -> %zu) returned too large buffer %zu, truncated\n",
            _srcFrameSize, _dstFrameSize, *_dstFrameUsed);
  }
  else
    *_dstFrameUsed = tcOutArgs.base.encodedBuf[0].bufSize;

#warning This memcpy is blocking high fps
  if(_ce->m_videoOutEnable)
    memcpy(_dstFramePtr, _ce->m_dstBuffer, *_dstFrameUsed);

  _targetLocation->m_targetX    = tcOutArgs.alg.targetX;
  _targetLocation->m_targetY    = tcOutArgs.alg.targetY;
  _targetLocation->m_targetSize = tcOutArgs.alg.targetSize;

  _targetDetectParamsResult->m_detectHue          = tcOutArgs.alg.detectHue;
  _targetDetectParamsResult->m_detectHueTolerance = tcOutArgs.alg.detectHueTolerance;
  _targetDetectParamsResult->m_detectSat          = tcOutArgs.alg.detectSat;
  _targetDetectParamsResult->m_detectSatTolerance = tcOutArgs.alg.detectSatTolerance;
  _targetDetectParamsResult->m_detectVal          = tcOutArgs.alg.detectVal;
  _targetDetectParamsResult->m_detectValTolerance = tcOutArgs.alg.detectValTolerance;

  return 0;
}

static int do_reportLoad(const CodecEngine* _ce, long long _ms)
{
  (void)_ms; // warn prevention

  Server_Handle ceServerHandle = Engine_getServer(_ce->m_handle);
  if (ceServerHandle == NULL)
  {
    fprintf(stderr, "Engine_getServer() failed\n");
    return ENOTCONN;
  }

  fprintf(stderr, "DSP load %d%%\n", (int)Server_getCpuLoad(ceServerHandle));

  Int sNumSegs;
  Server_Status sStatus = Server_getNumMemSegs(ceServerHandle, &sNumSegs);
  if (sStatus != Server_EOK)
    fprintf(stderr, "Server_getNumMemSegs() failed: %d\n", (int)sStatus);
  else
  {
    Int sSegIdx;
    for (sSegIdx = 0; sSegIdx < sNumSegs; ++sSegIdx)
    {
      Server_MemStat sMemStat;
      sStatus = Server_getMemStat(ceServerHandle, sSegIdx, &sMemStat);
      if (sStatus != Server_EOK)
      {
        fprintf(stderr, "Server_getMemStat() failed: %d\n", (int)sStatus);
        break;
      }

      fprintf(stderr, "DSP memory %#08x..%#08x, used %10u: %s\n",
              (unsigned)sMemStat.base, (unsigned)(sMemStat.base+sMemStat.size),
              (unsigned)sMemStat.used, sMemStat.name);
    }
  }

  return 0;
}




int codecEngineInit(bool _verbose)
{
  CERuntime_init(); /* init Codec Engine */

  s_verbose = _verbose;
  if (_verbose)
  {
    Diags_setMask("xdc.runtime.Main+EX1234567");
    Diags_setMask(Engine_MODNAME"+EX1234567");
  }

  return 0;
}

int codecEngineFini()
{
  return 0;
}


int codecEngineOpen(CodecEngine* _ce, const CodecEngineConfig* _config)
{
  if (_ce == NULL || _config == NULL)
    return EINVAL;

  if (_ce->m_handle != NULL)
    return EALREADY;

  Engine_Error ceError;
  Engine_Desc desc;
  Engine_initDesc(&desc);
  desc.name = "dsp-server";
  desc.remoteName = strdup(_config->m_serverPath);
  errno = 0;

  ceError = Engine_add(&desc);
  if (ceError != Engine_EOK)
  {
    free(desc.remoteName);
    fprintf(stderr, "Engine_add(%s) failed: %d/%"PRIi32"\n", _config->m_serverPath, errno, ceError);
    return ENOMEM;
  }
  free(desc.remoteName);

  if ((_ce->m_handle = Engine_open("dsp-server", NULL, &ceError)) == NULL)
  {
    fprintf(stderr, "Engine_open(%s) failed: %d/%"PRIi32"\n", _config->m_serverPath, errno, ceError);
    return ENOMEM;
  }

  return 0;
}

int codecEngineClose(CodecEngine* _ce)
{
  if (_ce == NULL)
    return EINVAL;

  if (_ce->m_handle == NULL)
    return EALREADY;

  Engine_close(_ce->m_handle);
  _ce->m_handle = NULL;

  return 0;
}


int codecEngineStart(CodecEngine* _ce, const CodecEngineConfig* _config,
                     const ImageDescription* _srcImageDesc,
                     const ImageDescription* _dstImageDesc)
{
  int res;

  if (_ce == NULL || _config == NULL || _srcImageDesc == NULL || _dstImageDesc == NULL)
    return EINVAL;

  if (_ce->m_handle == NULL)
    return ENOTCONN;

  if ((res = do_memoryAlloc(_ce, _srcImageDesc->m_imageSize, _dstImageDesc->m_imageSize)) != 0)
    return res;

  if ((res = do_setupCodec(_ce, _config->m_codecName, _srcImageDesc, _dstImageDesc)) != 0)
  {
    do_memoryFree(_ce);
    return res;
  }

  return 0;
}

int codecEngineStop(CodecEngine* _ce)
{
  if (_ce == NULL)
    return EINVAL;

  if (_ce->m_handle == NULL)
    return ENOTCONN;

  do_releaseCodec(_ce);
  do_memoryFree(_ce);

  return 0;
}

int codecEngineTranscodeFrame(CodecEngine* _ce,
                              const void* _srcFramePtr, size_t _srcFrameSize,
                              void* _dstFramePtr, size_t _dstFrameSize, size_t* _dstFrameUsed,
                              const TargetDetectParams* _targetDetectParams,
                              const TargetDetectCommand* _targetDetectCommand,
                              TargetLocation* _targetLocation,
                              TargetDetectParams* _targetDetectParamsResult)
{
  int res;

  if (_ce == NULL || _targetDetectParams == NULL || _targetDetectCommand == NULL || _targetLocation == NULL || _targetDetectParamsResult == NULL)
    return EINVAL;

  if (_ce->m_handle == NULL)
    return ENOTCONN;

  res = do_transcodeFrame(_ce,
                          _srcFramePtr, _srcFrameSize,
                          _dstFramePtr, _dstFrameSize, _dstFrameUsed,
                          _targetDetectParams,
                          _targetDetectCommand,
                          _targetLocation,
                          _targetDetectParamsResult);

  if (s_verbose)
  {
    fprintf(stderr, "Transcoded frame %p[%zu] -> %p[%zu/%zu]\n",
            _srcFramePtr, _srcFrameSize, _dstFramePtr, _dstFrameSize, *_dstFrameUsed);
    if (_targetLocation->m_targetSize > 0)
      fprintf(stderr, "Target detected at %d x %d @ %d\n",
              _targetLocation->m_targetX,
              _targetLocation->m_targetY,
              _targetLocation->m_targetSize);
  }

  return res;
}

int codecEngineReportLoad(const CodecEngine* _ce, long long _ms)
{
  if (_ce == NULL)
    return EINVAL;

  if (_ce->m_handle == NULL)
    return ENOTCONN;

  return do_reportLoad(_ce, _ms);
}


