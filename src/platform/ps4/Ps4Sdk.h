#pragma once
// Minimal, self-contained declarations of the console system functions this app uses.
//
// They are declared here (instead of including toolchain headers) so the app does not
// depend on the exact header layout of a particular OpenOrbis release. Linking is by
// symbol name against the toolchain's stub libraries (libkernel, libSceVideoOut,
// libScePad, libSceUserService). If a future toolchain changes a signature, this is the
// only file to update.
#include <stddef.h>
#include <stdint.h>

extern "C" {

// libkernel
int sceKernelAllocateDirectMemory(int64_t searchStart, int64_t searchEnd, size_t length, size_t alignment,
                                  int type, int64_t* physAddrOut);
int sceKernelMapDirectMemory(void** addr, size_t length, int protection, int flags, int64_t directMemoryStart,
                             size_t maxPageSize);
size_t sceKernelGetDirectMemorySize(void);
uint64_t sceKernelGetProcessTime(void);  // microseconds since process start
int sceKernelUsleep(unsigned int microseconds);

// libSceVideoOut
struct Ps4VideoOutBufferAttribute {
  int32_t pixelFormat;
  int32_t tilingMode;
  int32_t aspectRatio;
  uint32_t width;
  uint32_t height;
  uint32_t pitchInPixel;
  uint32_t option;
  uint32_t reserved0;
  uint64_t reserved1;
};
int sceVideoOutOpen(int userId, int type, int index, const void* param);
int sceVideoOutClose(int handle);
void sceVideoOutSetBufferAttribute(Ps4VideoOutBufferAttribute* attribute, uint32_t pixelFormat,
                                   uint32_t tilingMode, uint32_t aspectRatio, uint32_t width, uint32_t height,
                                   uint32_t pitchInPixel);
int sceVideoOutRegisterBuffers(int handle, int startIndex, void* const* addresses, int bufferNum,
                               const Ps4VideoOutBufferAttribute* attribute);
int sceVideoOutSubmitFlip(int handle, int bufferIndex, uint32_t flipMode, int64_t flipArg);
int sceVideoOutSetFlipRate(int handle, int rate);
int sceVideoOutIsFlipPending(int handle);

// libSceUserService
int sceUserServiceInitialize(const void* params);
int sceUserServiceGetInitialUser(int* userId);

// libScePad
int scePadInit(void);
int scePadOpen(int userId, int type, int index, const void* param);
int scePadReadState(int handle, void* data);

#if defined(PS5UI_ENABLE_LAUNCH)
// libSceSystemService (experimental, privilege/firmware dependent)
int sceSystemServiceLaunchApp(const char* titleId, char* const argv[], void* param);
#endif
}

namespace ps4const {
constexpr int kUserIdSystem = 0xFF;
constexpr int kBusTypeMain = 0;
constexpr uint32_t kPixelFormatB8G8R8A8Srgb = 0x80000000u;
constexpr uint32_t kTilingLinear = 1;
constexpr uint32_t kAspect16x9 = 0;
constexpr uint32_t kFlipModeVsync = 1;
constexpr int kMemTypeWcGarlic = 3;
constexpr int kProtCpuGpuReadWrite = 0x33;
constexpr size_t kDirectMemAlign = 0x200000;
}  // namespace ps4const
