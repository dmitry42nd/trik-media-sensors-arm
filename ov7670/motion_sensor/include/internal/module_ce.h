#ifndef TRIK_V4L2_DSP_FB_INTERNAL_MODULE_CE_H_
#define TRIK_V4L2_DSP_FB_INTERNAL_MODULE_CE_H_

#include <stdbool.h>

#include <xdc/std.h>
#include <ti/xdais/xdas.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/ce/osal/Memory.h>
#include <ti/sdo/ce/vidtranscode/vidtranscode.h>

#include "internal/common.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


typedef struct CodecEngineConfig // what user wants to set
{
  const char* m_serverPath;
  const char* m_codecName;
} CodecEngineConfig;

typedef struct CodecEngine
{
  Engine_Handle m_handle;

  Memory_AllocParams m_allocParams;
  size_t     m_srcBufferSize;
  void*      m_srcBuffer;

  size_t     m_dstBufferSize;
  void*      m_dstBuffer;

  VIDTRANSCODE_Handle m_vidtranscodeHandle;

  bool m_videoOutEnable;
} CodecEngine;




int codecEngineInit(bool _verbose);
int codecEngineFini();

int codecEngineOpen(CodecEngine* _ce, const CodecEngineConfig* _config);
int codecEngineClose(CodecEngine* _ce);
int codecEngineStart(CodecEngine* _ce, const CodecEngineConfig* _config,
                     const ImageDescription* _srcImageDesc,
                     const ImageDescription* _dstImageDesc);
int codecEngineStop(CodecEngine* _ce);

int codecEngineTranscodeFrame(CodecEngine* _ce,
                              const void* _srcFramePtr, size_t _srcFrameSize,
                              void* _dstFramePtr, size_t _dstFrameSize, size_t* _dstFrameUsed,
                              const TargetDetectParams* _targetDetectParams,
                              const TargetDetectCommand* _targetDetectCommand,
                              TargetLocation* _targetLocation,
                              TargetDetectParams* _targetDetectParamsResult);


int codecEngineReportLoad(const CodecEngine* _ce, long long _ms);


#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // !TRIK_V4L2_DSP_FB_INTERNAL_MODULE_CE_H_
