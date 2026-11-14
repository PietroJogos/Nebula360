// =====================================================================
//  RASCUNHO - NUNCA COMPILADO NEM EXECUTADO.
//  Exige o Xbox 360 XDK (Microsoft), que não está disponível neste
//  ambiente. Ver src/platform/xbox360/README.md e docs/fases/FASE_2.md.
//  Este arquivo NÃO faz parte do build CMake.
// =====================================================================
//
// Ponto de entrada do Nébula 360 no console: monta os dispositivos, cria o
// dispositivo Direct3D, roda o App e envia o framebuffer do renderizador por
// software para a GPU a cada quadro.
#include <xtl.h>

#include "app/App.h"
#include "core/Log.h"
#include "core/StringUtil.h"
#include "library/Game.h"
#include "platform/Clock.h"
#include "platform/Launcher.h"
#include "platform/win32/Win32FileSystem.h"
#include "platform/win32/XInputMapping.h"
#include "ui/SoftwareRenderer.h"

// Exportações do kernel usadas por homebrew em consoles RGH/JTAG para dar
// nomes ("Hdd1:", "Usb0:") aos dispositivos. A declaração pode conflitar com
// tipos já definidos pelo XDK instalado: ajustar ao compilar.
extern "C" {
typedef struct _NB_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PCHAR Buffer;
} NB_STRING;
LONG __stdcall ObCreateSymbolicLink(NB_STRING* linkName, NB_STRING* deviceName);
VOID __stdcall RtlInitAnsiString(NB_STRING* destination, const char* source);
}

namespace nebula {

namespace {

void mountDevice(const char* link, const char* device) {
  NB_STRING l, d;
  RtlInitAnsiString(&l, link);
  RtlInitAnsiString(&d, device);
  LONG status = ObCreateSymbolicLink(&l, &d);
  NB_INFO("xbox360", "montagem " << link << " -> " << device << ": " << status);
}

void mountDevices() {
  mountDevice("\\??\\Hdd1:", "\\Device\\Harddisk0\\Partition1");
  mountDevice("\\??\\Usb0:", "\\Device\\Mass0");
  mountDevice("\\??\\Usb1:", "\\Device\\Mass1");
  mountDevice("\\??\\Dvd:", "\\Device\\Cdrom0");
}

class Xbox360Clock : public IClock {
 public:
  virtual uint64_t monotonicMs() NB_OVERRIDE { return GetTickCount(); }
  virtual int64_t unixTimeSeconds() NB_OVERRIDE {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    return static_cast<int64_t>((t - 116444736000000000ULL) / 10000000ULL);
  }
};

class Xbox360Launcher : public ILauncher {
 public:
  virtual bool canLaunch(const Game& game) const NB_OVERRIDE {
    return game.format == "xex";  // GOD/XBLA/XBE: próxima fase
  }
  virtual Status launch(const Game& game) NB_OVERRIDE {
    if (!canLaunch(game)) return Status(kUnsupported, "formato ainda não suportado: " + game.format);
    DWORD r = XLaunchNewImage(game.path.c_str(), 0);
    // Em caso de sucesso a função não retorna.
    return Status(kIoError, "XLaunchNewImage falhou: " + str::toString(static_cast<uint32_t>(r)));
  }
  virtual Status returnToDashboard() NB_OVERRIDE {
    // Volta ao aplicativo padrão (com DashLaunch, o "Default" do launch.ini).
    DWORD r = XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
    return Status(kIoError, "retorno ao dashboard falhou: " + str::toString(static_cast<uint32_t>(r)));
  }
};

// Envia o framebuffer 1280x720 (0xAARRGGBB) para a tela.
// O formato D3DFMT_LIN_A8R8G8B8 guarda A,R,G,B em ordem de memória, que é
// exatamente como um uint32 0xAARRGGBB fica numa CPU big-endian (Xenon).
class Presenter {
 public:
  Presenter() : d3d_(NULL), device_(NULL), texture_(NULL), vs_(NULL), ps_(NULL), decl_(NULL) {}
  bool init();
  void present(const Image& fb);

 private:
  IDirect3D9* d3d_;
  IDirect3DDevice9* device_;
  IDirect3DTexture9* texture_;
  IDirect3DVertexShader9* vs_;
  IDirect3DPixelShader9* ps_;
  IDirect3DVertexDeclaration9* decl_;
};

const char* const kVertexShader =
    "struct V { float4 p : POSITION; float2 t : TEXCOORD0; };"
    "V main(V v) { return v; }";
const char* const kPixelShader =
    "sampler s : register(s0);"
    "float4 main(float2 t : TEXCOORD0) : COLOR { return tex2D(s, t); }";

struct Vertex {
  float x, y, z, w, u, v;
};

bool Presenter::init() {
  d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
  if (d3d_ == NULL) return false;
  D3DPRESENT_PARAMETERS pp;
  ZeroMemory(&pp, sizeof(pp));
  pp.BackBufferWidth = kScreenWidth;
  pp.BackBufferHeight = kScreenHeight;
  pp.BackBufferFormat = D3DFMT_A8R8G8B8;
  pp.BackBufferCount = 1;
  pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
  if (FAILED(d3d_->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp,
                                &device_))) {
    return false;
  }
  if (FAILED(device_->CreateTexture(kScreenWidth, kScreenHeight, 1, 0, D3DFMT_LIN_A8R8G8B8, 0,
                                    &texture_, NULL))) {
    return false;
  }
  ID3DXBuffer* code = NULL;
  if (FAILED(D3DXCompileShader(kVertexShader, strlen(kVertexShader), NULL, NULL, "main", "vs_2_0", 0,
                               &code, NULL, NULL))) {
    return false;
  }
  device_->CreateVertexShader(static_cast<DWORD*>(code->GetBufferPointer()), &vs_);
  code->Release();
  if (FAILED(D3DXCompileShader(kPixelShader, strlen(kPixelShader), NULL, NULL, "main", "ps_2_0", 0,
                               &code, NULL, NULL))) {
    return false;
  }
  device_->CreatePixelShader(static_cast<DWORD*>(code->GetBufferPointer()), &ps_);
  code->Release();
  D3DVERTEXELEMENT9 elements[] = {
      {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
      {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
      D3DDECL_END()};
  device_->CreateVertexDeclaration(elements, &decl_);
  return vs_ != NULL && ps_ != NULL && decl_ != NULL;
}

void Presenter::present(const Image& fb) {
  D3DLOCKED_RECT lr;
  if (SUCCEEDED(texture_->LockRect(0, &lr, NULL, 0))) {
    for (int y = 0; y < fb.height; ++y) {
      memcpy(static_cast<BYTE*>(lr.pBits) + y * lr.Pitch, &fb.pixels[y * fb.width], fb.width * 4);
    }
    texture_->UnlockRect(0);
  }
  const Vertex quad[4] = {{-1, 1, 0, 1, 0, 0}, {1, 1, 0, 1, 1, 0}, {-1, -1, 0, 1, 0, 1}, {1, -1, 0, 1, 1, 1}};
  device_->BeginScene();
  device_->SetVertexDeclaration(decl_);
  device_->SetVertexShader(vs_);
  device_->SetPixelShader(ps_);
  device_->SetTexture(0, texture_);
  device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));
  device_->EndScene();
  device_->Present(NULL, NULL, NULL, NULL);
}

}  // namespace
}  // namespace nebula

void __cdecl main() {
  using namespace nebula;
  mountDevices();
  Win32FileSystem fs;
  Xbox360Clock clock;
  Xbox360Launcher launcher;
  SoftwareRenderer renderer;
  Presenter presenter;
  if (!presenter.init()) return;

  App app(fs, clock, launcher, kPlatformXbox360);
  AppPaths paths;
  paths.dataDir = "Hdd1:\\Nebula";
  paths.assetsDir = "game:\\assets";  // pasta do próprio XEX
  if (!app.init(paths).ok()) return;

  for (;;) {
    XINPUT_STATE state;
    InputState input;
    if (XInputGetState(0, &state) == ERROR_SUCCESS) input = inputFromXInput(state.Gamepad);
    app.tick(input);
    app.render(renderer);
    presenter.present(renderer.framebuffer());
  }
}
