#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#ifdef LATTICE
#include <proto/intuition.h>
#endif

#define MAX_X 362
#define MAX_Y_NTSC 241
#define MAX_Y_PAL 283

void
AdjustScreen(register struct Screen *screen, int *dx, int *dy)
{
  int c640, c320, r400, r200, w, h, max_x, max_y;
  struct Preferences prefs;
  extern struct GfxBase *GfxBase;

  GetPrefs(&prefs, sizeof(prefs));
  c640 = 640 + prefs.ColumnSizeChange;
  c320 = c640 / 2;
  r200 = 200 + prefs.RowSizeChange;
  r400 = r200 * 2;

  max_y = (GfxBase->DisplayFlags & PAL) ? MAX_Y_PAL : MAX_Y_NTSC;
  if (screen->ViewPort.Modes & LACE){
    max_y *= 2;
  }
  max_x = MAX_X;
  if (screen->ViewPort.Modes & HIRES){
    max_x *= 2;
  }
  if (screen->ViewPort.DWidth <= max_x) {
    w = screen->ViewPort.DWidth;
  }else{
    w = max_x;
  }
  if (screen->ViewPort.DHeight <= max_y) {
    h = screen->ViewPort.DHeight;
  }else{
    h = max_y;
  }

  if (screen->ViewPort.Modes & HIRES){
    if (w > c640) {
      screen->ViewPort.DxOffset += (c640 - w) / 2;
    }
    *dx = screen->ViewPort.DxOffset / 2;
  }else{
    if (w > c320) {
      screen->ViewPort.DxOffset += (c320 - w) / 2;
    }
    *dx = screen->ViewPort.DxOffset;
  }
  if (screen->ViewPort.Modes & LACE){
    if (h > r400) {
      screen->ViewPort.DyOffset += (r400 - h) / 2;
    }
    *dy = screen->ViewPort.DyOffset / 2;
  }else{
    if (h > r200) {
      screen->ViewPort.DyOffset += (r200 - h) / 2;
    }
    *dy = screen->ViewPort.DyOffset;
  }

  RemakeDisplay();
}
