#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/videodev2.h> // pixel formats

#include "internal/module_fb.h"



static int do_fbOutputOpen(FBOutput* _fb, const char* _path)
{
  int res;

  if (_fb == NULL || _path == NULL)
    return EINVAL;

  _fb->m_fd = open(_path, O_RDWR|O_SYNC, 0);
  if (_fb->m_fd < 0)
  {
    res = errno;
    fprintf(stderr, "open(%s) failed: %d\n", _path, res);
    _fb->m_fd = -1;
    return res;
  }

  return 0;
}

static int do_fbOutputClose(FBOutput* _fb)
{
  int res;
  if (_fb == NULL)
    return EINVAL;

  if (close(_fb->m_fd) != 0)
  {
    res = errno;
    fprintf(stderr, "close() failed: %d\n", res);
    return res;
  }

  return 0;
}

static int do_fbOutputSetFormat(FBOutput* _fb)
{
  int res;

  if (_fb == NULL)
    return EINVAL;

  memset(&_fb->m_fbFixInfo, 0, sizeof(_fb->m_fbFixInfo));
  memset(&_fb->m_fbVarInfo, 0, sizeof(_fb->m_fbVarInfo));

  if (ioctl(_fb->m_fd, FBIOGET_FSCREENINFO, &_fb->m_fbFixInfo) != 0)
  {
    res = errno;
    fprintf(stderr, "ioctl(FBIOGET_FSCREENINFO) failed: %d\n", res);
    return res;
  }

  if (ioctl(_fb->m_fd, FBIOGET_VSCREENINFO, &_fb->m_fbVarInfo) != 0)
  {
    res = errno;
    fprintf(stderr, "ioctl(FBIOGET_VSCREENINFO) failed: %d\n", res);
    return res;
  }

  return 0;
}

static int do_fbOutputUnsetFormat(FBOutput* _fb)
{
  if (_fb == NULL)
    return EINVAL;

  memset(&_fb->m_fbFixInfo, 0, sizeof(_fb->m_fbFixInfo));
  memset(&_fb->m_fbVarInfo, 0, sizeof(_fb->m_fbVarInfo));

  return 0;
}

static int do_fbOutputGetFormat(FBOutput* _fb, ImageDescription* _imageDesc)
{
  if (_fb == NULL || _imageDesc == NULL)
    return EINVAL;

  _imageDesc->m_width      = _fb->m_fbVarInfo.xres;
  _imageDesc->m_height     = _fb->m_fbVarInfo.yres;
  _imageDesc->m_lineLength = _fb->m_fbFixInfo.line_length;
  _imageDesc->m_imageSize  = _fb->m_fbFixInfo.smem_len;

#warning TODO check and get framebuffer format!
  _imageDesc->m_format = V4L2_PIX_FMT_RGB565X;

  return 0;
}


static int do_fbOutputMmap(FBOutput* _fb)
{
  int res;

  if (_fb == NULL)
    return EINVAL;

  _fb->m_fbSize = _fb->m_fbFixInfo.smem_len;
  _fb->m_fbPtr = mmap(NULL, _fb->m_fbSize,
                      PROT_READ|PROT_WRITE, MAP_SHARED,
                      _fb->m_fd, 0);
  if (_fb->m_fbPtr == MAP_FAILED)
  {
    res = errno;
    fprintf(stderr, "mmap(%zu) failed: %d\n", _fb->m_fbSize, res);
    return res;
  }

  return 0;
}

static int do_fbOutputMunmap(FBOutput* _fb)
{
  int res = 0;
  if (_fb->m_fbPtr != MAP_FAILED)
  {
    if (munmap(_fb->m_fbPtr, _fb->m_fbSize) != 0)
    {
      res = errno;
      fprintf(stderr, "munmap(%p, %zu) failed: %d\n", _fb->m_fbPtr, _fb->m_fbSize, res);
    }
    _fb->m_fbPtr = MAP_FAILED;
    _fb->m_fbSize = 0;
  }

  return res;
}

static int do_fbOutputGetFrame(FBOutput* _fb, void** _framePtr, size_t* _frameSize)
{
  if (_fb == NULL || _framePtr == NULL || _frameSize == NULL)
    return EINVAL;

  if (_fb->m_fbPtr == NULL)
    return ENOTCONN;

  *_framePtr = _fb->m_fbPtr;
  *_frameSize = _fb->m_fbSize;

  return 0;
}




int fbOutputInit(bool _verbose)
{
  (void)_verbose;
  return 0;
}

int fbOutputFini()
{
  return 0;
}

int fbOutputOpen(FBOutput* _fb, const FBConfig* _config)
{
  int res = 0;

  if (_fb == NULL)
    return EINVAL;
  if (_fb->m_fd != -1)
    return EALREADY;

  res = do_fbOutputOpen(_fb, _config->m_path);
  if (res != 0)
    goto exit;

  res = do_fbOutputSetFormat(_fb);
  if (res != 0)
    goto exit_close;

  res = do_fbOutputMmap(_fb);
  if (res != 0)
    goto exit_unset_format;

  return 0;


 exit_unset_format:
  do_fbOutputUnsetFormat(_fb);
 exit_close:
  do_fbOutputClose(_fb);
 exit:
  return res;
}

int fbOutputClose(FBOutput* _fb)
{
  if (_fb == NULL)
    return EINVAL;
  if (_fb->m_fd == -1)
    return EALREADY;

  do_fbOutputMunmap(_fb);
  do_fbOutputUnsetFormat(_fb);
  do_fbOutputClose(_fb);

  return 0;
}

int fbOutputStart(FBOutput* _fb)
{
  if (_fb == NULL)
    return EINVAL;
  if (_fb->m_fd == -1)
    return ENOTCONN;

  return 0;
}

int fbOutputStop(FBOutput* _fb)
{
  if (_fb == NULL)
    return EINVAL;
  if (_fb->m_fd == -1)
    return ENOTCONN;

  return 0;
}

int fbOutputGetFrame(FBOutput* _fb, void** _framePtr, size_t* _frameSize)
{
  if (_fb == NULL)
    return EINVAL;
  if (_fb->m_fd == -1)
    return ENOTCONN;

  return do_fbOutputGetFrame(_fb, _framePtr, _frameSize);
}

int fbOutputPutFrame(FBOutput* _fb)
{
  if (_fb == NULL)
    return EINVAL;
  if (_fb->m_fd == -1)
    return ENOTCONN;

  return 0;
}

int fbOutputGetFormat(FBOutput* _fb, ImageDescription* _imageDesc)
{
  if (_fb == NULL || _imageDesc == NULL)
    return EINVAL;
  if (_fb->m_fd == -1)
    return ENOTCONN;

  return do_fbOutputGetFormat(_fb, _imageDesc);
}


