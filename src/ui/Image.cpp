#include "ui/Image.h"

#include "core/StringUtil.h"

extern "C" {
#include "stb_image.h"
#include "stb_image_write.h"
}

namespace nebula {

Status decodeImage(const std::string& bytes, Image& out) {
  out = Image();
  if (bytes.empty()) return Status(kInvalidArgument, "imagem vazia");
  if (bytes.size() > 0x7FFFFFFF) return Status(kLimitExceeded, "imagem grande demais");
  int w = 0, h = 0, n = 0;
  const stbi_uc* src = reinterpret_cast<const stbi_uc*>(bytes.data());
  if (!stbi_info_from_memory(src, static_cast<int>(bytes.size()), &w, &h, &n)) {
    return Status(kParseError, std::string("formato de imagem não reconhecido: ") +
                                   stbi_failure_reason());
  }
  if (w <= 0 || h <= 0 || w > kMaxImageDimension || h > kMaxImageDimension) {
    return Status(kLimitExceeded, "dimensões de imagem não suportadas: " +
                                      str::toString(w) + "x" + str::toString(h));
  }
  stbi_uc* data = stbi_load_from_memory(src, static_cast<int>(bytes.size()), &w, &h, &n, 4);
  if (data == NULL) {
    return Status(kParseError, std::string("falha ao decodificar imagem: ") + stbi_failure_reason());
  }
  out.width = w;
  out.height = h;
  out.pixels.resize(static_cast<size_t>(w) * h);
  for (size_t i = 0; i < out.pixels.size(); ++i) {
    const stbi_uc* p = data + i * 4;
    out.pixels[i] = makeColor(p[0], p[1], p[2], p[3]);
  }
  stbi_image_free(data);
  return Status::Ok();
}

Image downscale(const Image& src, int width, int height) {
  if (src.empty() || width <= 0 || height <= 0) return Image();
  if (width >= src.width && height >= src.height) return src;
  Image out(width, height, 0);
  for (int y = 0; y < height; ++y) {
    int y0 = static_cast<int>(static_cast<int64_t>(y) * src.height / height);
    int y1 = static_cast<int>(static_cast<int64_t>(y + 1) * src.height / height);
    if (y1 <= y0) y1 = y0 + 1;
    for (int x = 0; x < width; ++x) {
      int x0 = static_cast<int>(static_cast<int64_t>(x) * src.width / width);
      int x1 = static_cast<int>(static_cast<int64_t>(x + 1) * src.width / width);
      if (x1 <= x0) x1 = x0 + 1;
      uint32_t a = 0, r = 0, g = 0, b = 0, n = 0;
      for (int sy = y0; sy < y1; ++sy) {
        for (int sx = x0; sx < x1; ++sx) {
          Color c = src.at(sx, sy);
          uint32_t ca = colorA(c);
          a += ca;
          r += colorR(c) * ca;
          g += colorG(c) * ca;
          b += colorB(c) * ca;
          ++n;
        }
      }
      Color result = 0;
      if (a > 0) {
        result = makeColor(static_cast<uint8_t>(r / a), static_cast<uint8_t>(g / a),
                           static_cast<uint8_t>(b / a), static_cast<uint8_t>(a / n));
      }
      out.pixels[static_cast<size_t>(y) * width + x] = result;
    }
  }
  return out;
}

Image downscaleToCover(const Image& src, int width, int height) {
  if (src.empty() || width <= 0 || height <= 0) return src;
  // escala = max(w/sw, h/sh), limitada a 1 (só reduz).
  double sx = static_cast<double>(width) / src.width;
  double sy = static_cast<double>(height) / src.height;
  double s = sx > sy ? sx : sy;
  if (s >= 1.0) return src;
  int w = static_cast<int>(src.width * s + 0.5);
  int h = static_cast<int>(src.height * s + 0.5);
  if (w < width) w = width;
  if (h < height) h = height;
  return downscale(src, w, h);
}

namespace {
void appendToString(void* context, void* data, int size) {
  std::string* out = static_cast<std::string*>(context);
  out->append(static_cast<const char*>(data), static_cast<size_t>(size));
}
}  // namespace

Status encodePng(const Image& image, std::string& out) {
  out.clear();
  if (image.empty()) return Status(kInvalidArgument, "imagem vazia");
  std::vector<unsigned char> rgba(static_cast<size_t>(image.width) * image.height * 4);
  for (size_t i = 0; i < image.pixels.size(); ++i) {
    Color c = image.pixels[i];
    rgba[i * 4] = colorR(c);
    rgba[i * 4 + 1] = colorG(c);
    rgba[i * 4 + 2] = colorB(c);
    rgba[i * 4 + 3] = colorA(c);
  }
  int ok = stbi_write_png_to_func(appendToString, &out, image.width, image.height, 4,
                                  &rgba[0], image.width * 4);
  if (!ok) return Status(kInternal, "falha ao codificar PNG");
  return Status::Ok();
}

}  // namespace nebula
