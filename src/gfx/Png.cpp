#include "gfx/Png.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "core/Types.h"

namespace ps5ui {

namespace {

// ---- inflate (RFC 1951), modelled on the classic "puff" decoder -----------------

struct BitReader {
  const uint8_t* p;
  size_t n;
  size_t pos = 0;
  uint32_t bitBuf = 0;
  int bitCnt = 0;
  bool err = false;

  int bits(int need) {
    uint32_t val = bitBuf;
    while (bitCnt < need) {
      if (pos >= n) {
        err = true;
        return 0;
      }
      val |= static_cast<uint32_t>(p[pos++]) << bitCnt;
      bitCnt += 8;
    }
    bitBuf = val >> need;
    bitCnt -= need;
    return static_cast<int>(val & ((1u << need) - 1));
  }
};

constexpr int kMaxBits = 15;

struct Huffman {
  uint16_t count[kMaxBits + 1];
  uint16_t symbol[288];
};

int buildHuffman(Huffman& h, const uint16_t* length, int n) {
  std::memset(h.count, 0, sizeof(h.count));
  for (int s = 0; s < n; ++s) h.count[length[s]]++;
  if (h.count[0] == n) return 0;
  int left = 1;
  for (int len = 1; len <= kMaxBits; ++len) {
    left <<= 1;
    left -= h.count[len];
    if (left < 0) return left;  // over-subscribed
  }
  uint16_t offs[kMaxBits + 1];
  offs[1] = 0;
  for (int len = 1; len < kMaxBits; ++len) offs[len + 1] = static_cast<uint16_t>(offs[len] + h.count[len]);
  for (int s = 0; s < n; ++s)
    if (length[s] != 0) h.symbol[offs[length[s]]++] = static_cast<uint16_t>(s);
  return left;
}

int decodeSym(BitReader& br, const Huffman& h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len <= kMaxBits; ++len) {
    code |= br.bits(1);
    if (br.err) return -1;
    int count = h.count[len];
    if (code - count < first) return h.symbol[index + (code - first)];
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  return -1;
}

const uint16_t kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint16_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const uint16_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool inflateCodes(BitReader& br, std::vector<uint8_t>& out, const Huffman& lit, const Huffman& dist, size_t limit) {
  for (;;) {
    int sym = decodeSym(br, lit);
    if (sym < 0) return false;
    if (sym < 256) {
      if (out.size() >= limit) return false;
      out.push_back(static_cast<uint8_t>(sym));
    } else if (sym == 256) {
      return true;
    } else {
      sym -= 257;
      if (sym >= 29) return false;
      int len = kLenBase[sym] + br.bits(kLenExtra[sym]);
      int ds = decodeSym(br, dist);
      if (ds < 0 || ds >= 30) return false;
      size_t d = static_cast<size_t>(kDistBase[ds] + br.bits(kDistExtra[ds]));
      if (br.err || d > out.size() || out.size() + static_cast<size_t>(len) > limit) return false;
      size_t from = out.size() - d;
      for (int i = 0; i < len; ++i) out.push_back(out[from + static_cast<size_t>(i)]);
    }
  }
}

bool inflateStream(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t limit) {
  BitReader br{data, size};
  int last;
  do {
    last = br.bits(1);
    int type = br.bits(2);
    if (br.err) return false;
    if (type == 0) {
      br.bitBuf = 0;
      br.bitCnt = 0;
      if (br.pos + 4 > size) return false;
      unsigned len = data[br.pos] | (data[br.pos + 1] << 8);
      unsigned nlen = data[br.pos + 2] | (data[br.pos + 3] << 8);
      br.pos += 4;
      if (len != (~nlen & 0xFFFFu) || br.pos + len > size || out.size() + len > limit) return false;
      out.insert(out.end(), data + br.pos, data + br.pos + len);
      br.pos += len;
    } else if (type == 1) {
      static Huffman lit, dist;
      static bool built = false;
      if (!built) {
        uint16_t l[288];
        int s = 0;
        for (; s < 144; ++s) l[s] = 8;
        for (; s < 256; ++s) l[s] = 9;
        for (; s < 280; ++s) l[s] = 7;
        for (; s < 288; ++s) l[s] = 8;
        buildHuffman(lit, l, 288);
        for (s = 0; s < 30; ++s) l[s] = 5;
        buildHuffman(dist, l, 30);
        built = true;
      }
      if (!inflateCodes(br, out, lit, dist, limit)) return false;
    } else if (type == 2) {
      static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
      int nlen = br.bits(5) + 257, ndist = br.bits(5) + 1, ncode = br.bits(4) + 4;
      if (br.err || nlen > 286 || ndist > 30) return false;
      uint16_t lengths[320];
      std::memset(lengths, 0, sizeof(lengths));
      for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<uint16_t>(br.bits(3));
      Huffman lencode;
      if (buildHuffman(lencode, lengths, 19) != 0) return false;
      int index = 0;
      while (index < nlen + ndist) {
        int sym = decodeSym(br, lencode);
        if (sym < 0) return false;
        if (sym < 16) {
          lengths[index++] = static_cast<uint16_t>(sym);
        } else {
          int len = 0, rep;
          if (sym == 16) {
            if (index == 0) return false;
            len = lengths[index - 1];
            rep = 3 + br.bits(2);
          } else if (sym == 17) {
            rep = 3 + br.bits(3);
          } else {
            rep = 11 + br.bits(7);
          }
          if (br.err || index + rep > nlen + ndist) return false;
          while (rep--) lengths[index++] = static_cast<uint16_t>(len);
        }
      }
      if (lengths[256] == 0) return false;
      Huffman lit, dist;
      int e1 = buildHuffman(lit, lengths, nlen);
      if (e1 < 0 || (e1 > 0 && nlen - lit.count[0] != 1)) return false;
      int e2 = buildHuffman(dist, lengths + nlen, ndist);
      if (e2 < 0 || (e2 > 0 && ndist - dist.count[0] != 1)) return false;
      if (!inflateCodes(br, out, lit, dist, limit)) return false;
    } else {
      return false;
    }
  } while (!last);
  return true;
}

uint32_t be32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

int paeth(int a, int b, int c) {
  int p = a + b - c;
  int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  return pb <= pc ? b : c;
}

}  // namespace

bool decodePng(const uint8_t* data, size_t size, Image& out, int maxDimension) {
  out = Image();
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (!data || size < 8 + 25 || std::memcmp(data, sig, 8) != 0) return false;

  uint32_t w = 0, h = 0;
  int depth = 0, ctype = -1, interlace = 0;
  std::vector<uint8_t> idat, plte, trns;
  size_t pos = 8;
  bool sawIhdr = false, sawIend = false;
  while (pos + 12 <= size && !sawIend) {
    uint32_t len = be32(data + pos);
    const uint8_t* type = data + pos + 4;
    if (len > size - pos - 12) return false;
    const uint8_t* body = data + pos + 8;
    if (!std::memcmp(type, "IHDR", 4)) {
      if (len != 13) return false;
      w = be32(body);
      h = be32(body + 4);
      depth = body[8];
      ctype = body[9];
      interlace = body[12];
      sawIhdr = true;
    } else if (!std::memcmp(type, "PLTE", 4)) {
      plte.assign(body, body + len);
    } else if (!std::memcmp(type, "tRNS", 4)) {
      trns.assign(body, body + len);
    } else if (!std::memcmp(type, "IDAT", 4)) {
      idat.insert(idat.end(), body, body + len);
    } else if (!std::memcmp(type, "IEND", 4)) {
      sawIend = true;
    }
    pos += 12 + len;
  }
  if (!sawIhdr || w == 0 || h == 0 || static_cast<int>(w) > maxDimension || static_cast<int>(h) > maxDimension) return false;
  if (interlace != 0) return false;
  int channels;
  switch (ctype) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    default: return false;
  }
  if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) return false;
  if ((ctype == 2 || ctype == 4 || ctype == 6) && depth < 8) return false;
  if (ctype == 3 && (depth == 16 || plte.empty())) return false;

  const size_t bitsPerPixel = static_cast<size_t>(channels) * static_cast<size_t>(depth);
  const size_t stride = (static_cast<size_t>(w) * bitsPerPixel + 7) / 8;
  const size_t bpp = std::max<size_t>(1, bitsPerPixel / 8);
  const size_t rawSize = (stride + 1) * h;

  if (idat.size() < 6) return false;
  if ((idat[0] & 0x0F) != 8) return false;  // zlib: deflate only
  std::vector<uint8_t> raw;
  raw.reserve(rawSize);
  if (!inflateStream(idat.data() + 2, idat.size() - 2, raw, rawSize)) return false;
  if (raw.size() < rawSize) return false;

  // Undo scanline filters in place.
  for (size_t y = 0; y < h; ++y) {
    uint8_t* row = raw.data() + y * (stride + 1);
    uint8_t ft = row[0];
    uint8_t* cur = row + 1;
    const uint8_t* prev = (y == 0) ? nullptr : raw.data() + (y - 1) * (stride + 1) + 1;
    for (size_t i = 0; i < stride; ++i) {
      int a = (i >= bpp) ? cur[i - bpp] : 0;
      int b = prev ? prev[i] : 0;
      int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
      int v = cur[i];
      switch (ft) {
        case 0: break;
        case 1: v += a; break;
        case 2: v += b; break;
        case 3: v += (a + b) / 2; break;
        case 4: v += paeth(a, b, c); break;
        default: return false;
      }
      cur[i] = static_cast<uint8_t>(v);
    }
  }

  out.w = static_cast<int>(w);
  out.h = static_cast<int>(h);
  out.px.assign(static_cast<size_t>(w) * h, 0);
  for (size_t y = 0; y < h; ++y) {
    const uint8_t* row = raw.data() + y * (stride + 1) + 1;
    uint32_t* dst = out.px.data() + y * w;
    for (size_t x = 0; x < w; ++x) {
      uint32_t r = 0, g = 0, b = 0, a = 255;
      if (ctype == 3 || (ctype == 0 && depth < 8)) {
        size_t bit = x * static_cast<size_t>(depth);
        uint32_t v = (row[bit / 8] >> (8 - depth - (bit % 8))) & ((1u << depth) - 1);
        if (ctype == 3) {
          if (v * 3 + 2 >= plte.size()) return false;
          r = plte[v * 3];
          g = plte[v * 3 + 1];
          b = plte[v * 3 + 2];
          if (v < trns.size()) a = trns[v];
        } else {
          r = g = b = v * 255 / ((1u << depth) - 1);
        }
      } else {
        const uint8_t* px = row + x * (bitsPerPixel / 8);
        int step = depth == 16 ? 2 : 1;  // high byte of 16-bit samples
        switch (ctype) {
          case 0: r = g = b = px[0]; break;
          case 2: r = px[0]; g = px[step]; b = px[2 * step]; break;
          case 4: r = g = b = px[0]; a = px[step]; break;
          case 6: r = px[0]; g = px[step]; b = px[2 * step]; a = px[3 * step]; break;
        }
      }
      dst[x] = argb(a, r, g, b);
    }
  }
  return true;
}

Image resizeBox(const Image& src, int dw, int dh) {
  Image out;
  if (!src.valid() || dw <= 0 || dh <= 0) return out;
  out.w = dw;
  out.h = dh;
  out.px.resize(static_cast<size_t>(dw) * dh);
  for (int y = 0; y < dh; ++y) {
    int sy0 = static_cast<int>(static_cast<int64_t>(y) * src.h / dh);
    int sy1 = std::max(sy0 + 1, static_cast<int>(static_cast<int64_t>(y + 1) * src.h / dh));
    for (int x = 0; x < dw; ++x) {
      int sx0 = static_cast<int>(static_cast<int64_t>(x) * src.w / dw);
      int sx1 = std::max(sx0 + 1, static_cast<int>(static_cast<int64_t>(x + 1) * src.w / dw));
      uint32_t a = 0, r = 0, g = 0, b = 0, n = 0;
      for (int yy = sy0; yy < sy1 && yy < src.h; ++yy)
        for (int xx = sx0; xx < sx1 && xx < src.w; ++xx) {
          uint32_t p = src.px[static_cast<size_t>(yy) * src.w + xx];
          a += p >> 24;
          r += (p >> 16) & 255;
          g += (p >> 8) & 255;
          b += p & 255;
          ++n;
        }
      if (n == 0) n = 1;
      out.px[static_cast<size_t>(y) * dw + x] = argb(a / n, r / n, g / n, b / n);
    }
  }
  return out;
}

}  // namespace ps5ui
