#ifndef TRIK_V4L2_DSP_FB_INTERNAL_COMMON_H_
#define TRIK_V4L2_DSP_FB_INTERNAL_COMMON_H_

#include <stdbool.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define COLORS_NUM   100
#define COLORS_WIDTHM_MAX   10
#define COLORS_HEIGHTN_MAX  10

typedef struct MxnParams
{
  size_t m_m;
  size_t m_n;
} MxnParams;

typedef struct ImageDescription
{
  size_t m_width;
  size_t m_height;
  size_t m_lineLength;
  size_t m_imageSize;
  uint32_t m_format;
} ImageDescription;

typedef struct TargetDetectParams
{
  int m_detectHue;
  int m_detectHueTolerance;
  int m_detectSat;
  int m_detectSatTolerance;
  int m_detectVal;
  int m_detectValTolerance;
} TargetDetectParams;

typedef struct TargetDetectCommand
{
  int m_cmd;
} TargetDetectCommand;

typedef struct TargetLocation
{
  int m_targetX; 
  int m_targetY; 
  int m_targetSize;
} TargetLocation;

typedef struct TargetColors
{
  uint32_t m_colors[COLORS_NUM]; //treeColor
} TargetColors;

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus


#endif // !TRIK_V4L2_DSP_FB_INTERNAL_COMMON_H_
