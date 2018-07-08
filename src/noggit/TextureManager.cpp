// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#include <noggit/TextureManager.h>
#include <noggit/Log.h> // LogDebug
#include <opengl/context.hpp>
#include <opengl/scoped.hpp>

#include <QtCore/QString>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLFramebufferObjectFormat>
#include <QtGui/QPixmap>
#include <QtOpenGL/QGLPixelBuffer>

#include <algorithm>

decltype (TextureManager::_) TextureManager::_;

void TextureManager::report()
{
  std::string output = "Still in the Texture manager:\n";
  _.apply ( [&] (std::string const& key, blp_texture const&)
            {
              output += " - " + key + "\n";
            }
          );
  LogDebug << output;
}

#include <cstdint>
//! \todo Cross-platform syntax for packed structs.
#pragma pack(push,1)
struct BLPHeader
{
  int32_t magix;
  int32_t version;
  uint8_t attr_0_compression;
  uint8_t attr_1_alphadepth;
  uint8_t attr_2_alphatype;
  uint8_t attr_3_mipmaplevels;
  int32_t resx;
  int32_t resy;
  int32_t offsets[16];
  int32_t sizes[16];
};
#pragma pack(pop)

#include <boost/thread.hpp>
#include <noggit/MPQ.h>

void blp_texture::bind()
{
  opengl::texture::bind();

  if (!finished)
  {
    return;
  }

  if (!_uploaded)
  {
    upload();
  }
}

void blp_texture::upload()
{
  if (_uploaded)
  {
    return;
  }

  int width = _width, height = _height;

  if (!_compression_format)
  {
    for (int i = 0; i < _data.size(); ++i)
    {
      width = std::max(1, width);
      height = std::max(1, height);

      gl.texImage2D(GL_TEXTURE_2D, i, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, _data[i].data());   

      width >>= 1;
      height >>= 1;
    }

    _data.clear();
  }
  else
  {
    for (int i = 0; i < _compressed_data.size(); ++i)
    {
      gl.compressedTexImage2D(GL_TEXTURE_2D, i, _compression_format.get(), width, height, 0, _compressed_data[i].size(), _compressed_data[i].data());

      width = std::max(width >> 1, 1);
      height = std::max(height >> 1, 1);
    }

    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, _compressed_data.size() - 1);
    _compressed_data.clear();
  }

  gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  _uploaded = true;
}

void blp_texture::loadFromUncompressedData(BLPHeader const* lHeader, char const* lData)
{
  unsigned int const* pal = reinterpret_cast<unsigned int const*>(lData + sizeof(BLPHeader));

  unsigned char const* buf;
  unsigned int *p;
  unsigned char const* c;
  unsigned char const* a;

  int alphabits = lHeader->attr_1_alphadepth;
  bool hasalpha = alphabits != 0;

  int width = _width, height = _height;

  for (int i = 0; i<16; ++i)
  {
    width = std::max(1, width);
    height = std::max(1, height);

    if (lHeader->offsets[i] && lHeader->sizes[i])
    {
      buf = reinterpret_cast<unsigned char const*>(&lData[lHeader->offsets[i]]);

      std::vector<uint32_t> data(lHeader->sizes[i]);

      int cnt = 0;
      p = data.data();
      c = buf;
      a = buf + width*height;
      for (int y = 0; y<height; y++)
      {
        for (int x = 0; x<width; x++)
        {
          unsigned int k = pal[*c++];
          k = ((k & 0x00FF0000) >> 16) | ((k & 0x0000FF00)) | ((k & 0x000000FF) << 16);
          int alpha = 0xFF;
          if (hasalpha)
          {
            if (alphabits == 8)
            {
              alpha = (*a++);
            }
            else if (alphabits == 1)
            {
              alpha = (*a & (1 << cnt++)) ? 0xff : 0;
              if (cnt == 8)
              {
                cnt = 0;
                a++;
              }
            }
          }

          k |= alpha << 24;
          *p++ = k;
        }
      }

      _data[i] = data;
    }
    else
    {
      return;
    }

    width >>= 1;
    height >>= 1;
  }
}

void blp_texture::loadFromCompressedData(BLPHeader const* lHeader, char const* lData)
{
  //                         0 (0000) & 3 == 0                1 (0001) & 3 == 1                    7 (0111) & 3 == 3
  const int alphatypes[] = { GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT };
  const int blocksizes[] = { 8, 16, 0, 16 };

  int alpha_type = lHeader->attr_2_alphatype & 3;
  GLint format = alphatypes[alpha_type];
  _compression_format = format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ? (lHeader->attr_1_alphadepth == 1 ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT) : format;

  // do every mipmap level
  for (int i = 0; i < 16; ++i)
  {
    if (lHeader->offsets[i] && lHeader->sizes[i])
    {
      char const* start = lData + lHeader->offsets[i];
      _compressed_data[i] = std::vector<uint8_t>(start, start + lHeader->sizes[i]);
    }
    else
    {
      return;
    }
  }
}

const std::string& blp_texture::filename()
{
  return _filename;
}

blp_texture::blp_texture(const std::string& filenameArg)
  : AsyncObject(filenameArg)
  , _filename(filenameArg)
{
}

void blp_texture::finishLoading()
{
  bool exists = MPQFile::exists(_filename);
  if (!exists)
  {
    LogError << "file not found: '" << _filename << "'" << std::endl;
  }

  MPQFile f(exists ? _filename : "textures/shanecube.blp");
  if (f.isEof())
  {
    finished = true;
    throw std::runtime_error ("File " + _filename + " does not exists");
  }

  char const* lData = f.getPointer();
  BLPHeader const* lHeader = reinterpret_cast<BLPHeader const*>(lData);
  _width = lHeader->resx;
  _height = lHeader->resy;

  if (lHeader->attr_0_compression == 1)
  {
    loadFromUncompressedData(lHeader, lData);
  }
  else if (lHeader->attr_0_compression == 2)
  {
    loadFromCompressedData(lHeader, lData);
  }
  else
  {
    finished = true;
    throw std::logic_error ("unimplemented BLP colorEncoding");
  }

  f.close();
  finished = true;
}

namespace noggit
{
  QPixmap render_blp_to_pixmap ( std::string const& blp_filename
                               , int width
                               , int height
                               )
  {
    opengl::context::save_current_context const context_save (::gl);

    QOpenGLContext context;
    context.create();

    QOpenGLFramebufferObjectFormat fmt;
    fmt.setSamples(1);
    fmt.setInternalTextureFormat(GL_RGBA8);

    QOffscreenSurface surface;
    surface.create();

    context.makeCurrent (&surface);

    opengl::context::scoped_setter const context_set (::gl, &context);

    opengl::scoped::texture_setter<0, GL_TRUE> const texture0;
    blp_texture texture (blp_filename);
    texture.finishLoading();
    texture.upload();

    width = width == -1 ? texture.width() : width;
    height = height == -1 ? texture.height() : height;

    QOpenGLFramebufferObject pixel_buffer (width, height, fmt);
    pixel_buffer.bind();

    gl.viewport (0, 0, width, height);
    gl.matrixMode (GL_PROJECTION);
    gl.loadIdentity();
    gl.ortho (0.0f, width, height, 0.0f, 1.0f, -1.0f);
    gl.matrixMode (GL_MODELVIEW);
    gl.loadIdentity();

    gl.clearColor(.0f, .0f, .0f, .0f);
    gl.clear(GL_COLOR_BUFFER_BIT);

    gl.begin (GL_TRIANGLE_FAN);
    gl.texCoord2f (0.0f, 0.0f);
    gl.vertex2f (0.0f, 0.0f);
    gl.texCoord2f (1.0f, 0.0f);
    gl.vertex2f (width, 0.0f);
    gl.texCoord2f (1.0f, 1.0f);
    gl.vertex2f (width, height);
    gl.texCoord2f (0.0f, 1.0f);
    gl.vertex2f (0.0f, height);
    gl.end();

    QPixmap pixmap (QPixmap::fromImage (pixel_buffer.toImage()));

    if (pixmap.isNull())
    {
      throw std::runtime_error
        ("failed rendering " + blp_filename + " to pixmap");
    }
    return pixmap;
  }
}
