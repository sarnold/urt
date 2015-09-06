/* 
 * getami.c - display a Utah RLE picture on a Commodore Amiga
 * 
 * Authors:	Eleftherios Koutsofios (ek@ulysses.att.com)
 *		Kriton Kyrimis (kyrimis%theseas@csi.forth.gr)
 */

#include <stdio.h>
#include <intuition/intuition.h>
#include <libraries/dos.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#ifndef NOIFFLIB
#include <iff.h>
#endif
#include <rle.h>
#include <stdlib.h>
#include <string.h>
#if !defined(MCH_AMIGA) && !defined(LATTICE)
#include <signal.h>
#endif
#ifdef LATTICE
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#ifndef NOIFFLIB
#include <iffpragmas.h>
#endif
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define index(r, g, b) ((((r) >> 4) << 8) | (((g) >> 4) << 4) | ((b) >> 4))
#define rdist(a, b) \
  ((ABS (((cusage[a].i >> 8) & 0xf) - ((cusage[b].i >> 8) & 0xf))) * 30)
#define gdist(a, b) \
  ((ABS (((cusage[a].i >> 4) & 0xf) - ((cusage[b].i >> 4) & 0xf))) * 59)
#define bdist(a, b) \
  ((ABS ((cusage[a].i & 0xf) - (cusage[b].i & 0xf))) * 11)
#define mapx(x) (x + offsetx)
#define mapy(y) (sh - (y) - 1 - offsety)
#define fmapy(y) (y + offsety)
#define inside(x, y) ((x) >= 0 && (y) >= 0 && (x) < sw && (y) < sh)
#define EXACT(x) (x)
#define RED(x) ((2 << 4) | x)
#define GREEN(x) ((3 << 4) | x)
#define BLUE(x) ((1 << 4) | x)
#define SQ(x) (SQtmp = (x), SQtmp * SQtmp)

#define MAX_X 362
#define MAX_Y_NTSC 241
#define MAX_Y_PAL 283

#define BASENAME(x)	(strrchr((x),'/') ? strrchr((x),'/')+1 : \
			(strrchr((x),':') ? strrchr((x),':')+1 : (x)))

struct GfxBase       *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
#ifndef NOIFFLIB
struct Library *IFFBase = NULL;
#endif

static struct NewScreen screen = {
  0, 0, 0, 0, 6, 1, 0, 0, CUSTOMSCREEN, NULL, "GetAmi", NULL, NULL
};

static struct Image img = {
  0, 0, 10, 10, 0, NULL, 0, 0, NULL
};

static struct Gadget CloseGadget = {
  NULL, 0, 0, 10, 10, GADGHCOMP | GADGIMAGE, RELVERIFY, BOOLGADGET, (APTR)&img,
  NULL, NULL, 0, NULL, 1, NULL
};

static struct NewWindow window = {
  0, 0, 0, 0, 1, 0, GADGETUP | MENUPICK | MENUVERIFY | REQSET | REQCLEAR,
  SMART_REFRESH | BACKDROP | BORDERLESS,
  &CloseGadget, NULL, NULL, NULL, NULL, 0, 0, 0, 0, CUSTOMSCREEN
};

static struct cuse {
  int i, j;
} cusage[4096];

static struct Window   *w1  = NULL, *w2 = NULL, *w3 = NULL;
static struct RastPort *rp1 = NULL, *rp2 = NULL, *rp3 = NULL;
static struct Screen   *s1 = NULL;
static struct Screen   *s2 = NULL;
static struct Screen   *s3 = NULL;
static struct ViewPort *vp1 = NULL, *vp2 = NULL, *vp3 = NULL;
static rle_pixel       *sl[3];
static rle_pixel      **cmap;
static unsigned int     R[32], G[32], B[32];
static char            *tfile, *infile = NULL;
static FILE            *ifd = NULL, *tfd = NULL;
static int              width, height, depth, nused, sw, sh, sd, isham;
#if 0
static int		nleft;
#endif
static int              offsetx, offsety;
static int		bw=0, threescreens=0, havereq=0, havemenu=0, flip=0;
static int		forceham=0;

static USHORT *Ptr = NULL;

static USHORT RenderPtr[] = {
  0x0000, 0x0000,
  0x7800, 0x3800,
  0x7000, 0x9000,
  0x7800, 0x8800,
  0x5C00, 0xA400,
  0x0E00, 0x1200,
  0x0700, 0x0900,
  0xC300, 0xC400,
  0xADD1, 0xADD1,
  0xA955, 0xA955,
  0xC9FF, 0xC9FF,
  0x0000, 0x0000,
  0x0BB8, 0x0BB8,
  0x0AA8, 0x0AA8,
  0x0AB8, 0x0AB8,
  0x0008, 0x0008,
  0x0038, 0x0038,
  0x0000, 0x0000
};

/* SQ temp variable */
static int SQtmp;

extern struct Window  *OpenWindow ();
extern struct Screen  *OpenScreen ();
extern struct Library *OpenLibrary ();
extern char *mktemp(char *);
extern rle_pixel **buildmap();

UBYTE handleIDCMP (void);

#define MENUITEMS 3

static struct IntuiText txt[] = {
  { 1, 0, JAM2, 0, 0, NULL, "Save", NULL },
  { 1, 0, JAM2, 0, 0, NULL, "Save As", NULL },
  { 1, 0, JAM2, 0, 0, NULL, "Quit", NULL }
};

static struct MenuItem actions[] = {
  { &actions[1], 0, 0, 68, 8, ITEMTEXT | ITEMENABLED | HIGHBOX | COMMSEQ, 0,
    (APTR)&txt[0], NULL, 'S', NULL, MENUNULL
  },
  { &actions[2], 0, 10, 68, 8, ITEMTEXT | ITEMENABLED | HIGHBOX | COMMSEQ, 0,
    (APTR)&txt[1], NULL, 'A', NULL, MENUNULL
  },
  { NULL, 0, 20, 68, 8, ITEMTEXT | ITEMENABLED | HIGHBOX | COMMSEQ, 0,
    (APTR)&txt[2], NULL, 'Q', NULL, MENUNULL
  }
};

static struct Menu menu = {
  NULL, 10, 0, 56, 0, MENUENABLED, "Actions", &actions[0]
};

static char fnam[80] = {'\0'};
static char ufnam[80] = {'\0'};

static struct StringInfo fgadinfo = {
  fnam, ufnam, 0, sizeof(fnam), 0, 0, 0, 0, 0, 0, NULL, 0, NULL
};
  
static struct IntuiText ftxt = {
  0, 1, JAM2, 9, 10, NULL, "File:", NULL
};

static SHORT fbcoord[] = {1, 1, 146, 1, 146, 26, 1, 26, 1, 1};

static struct Border fbord = {
  0, 0, 0, 1, JAM1, 5, fbcoord, NULL
};

static struct Gadget fgad = {
  NULL, 58, 10, 80, 8, GADGHCOMP, ENDGADGET, STRGADGET | REQGADGET,
  NULL, NULL, NULL, 0, (APTR)&fgadinfo, 0, NULL
};

#define FREQW 148
#define FREQH 28

static struct Requester freq = {
  NULL, 0, 0, FREQW, FREQH, 0, 0, &fgad, &fbord, &ftxt, NULL, 1, NULL,
  {NULL}, NULL, NULL, {NULL}
};

void main(int, char **);
int compare (struct cuse *, struct cuse *);
int find_most_used (void);
int display_rle (void);
void show_rgb (void);
/* Give our signal handler the same name as the default signal handler of
   the compiler we are using, thus saving a few bytes from the binary */
#ifdef MCH_AMIGA
void _abort (void);
#else
int CXBRK(void);
#define _abort CXBRK
#endif
void AdjustScreen(register struct Screen *, int *, int *);
void save(void);
void FixColors(void);
void ResetColors(void);
int CalcWidth(int);
int CalcHAMWidth(int);
int CalcHeight(int, int);

int curcol1, curcol2, curcol3;

#define OptSetAPen1(rp, col) curcol1 = (col); if(curcol1) SetAPen((rp), curcol1)
#define OptSetAPen2(rp, col) curcol2 = (col); if(curcol2) SetAPen((rp), curcol2)
#define OptSetAPen3(rp, col) curcol3 = (col); if(curcol3) SetAPen((rp), curcol3)

#define OptWritePixel1(rp, x, y) if(curcol1) WritePixel((rp), (x), (y))
#define OptWritePixel2(rp, x, y) if(curcol2) WritePixel((rp), (x), (y))
#define OptWritePixel3(rp, x, y) if(curcol3) WritePixel((rp), (x), (y))

void
main(int argc, char **argv)
{
  int i, SW=0, SH=0, dump=0, load=0, dx, dy, max_x, max_y;
  ULONG signalmask, signals;
  UBYTE done;
  BYTE pal;
  FILE *f;
  struct Preferences prefs;
  extern char *optarg;
  extern int optind;

/* _abort (#define'd as CXBRK for Lattice) is the default signal handler.
   No need to call signal, unless we are using some compiler other than
   Manx/gcc or lattice
*/
#if !defined(MCH_AMIGA) && !defined(LATTICE)
  (void)signal(SIGINT, (void (*) (int))_abort);
#endif
  while ((i = getopt(argc, argv, "w:h:o:dlb3fH")) != EOF) {
    switch(i) {
      case 'w':
        SW = atoi(optarg);
        break;
      case 'h':
        SH = atoi(optarg);
        break;
      case 'o':
        strcpy(fnam, optarg);
        break;
      case 'd':
        dump = 1;
        break;
      case 'l':
        load = 1;
        break;
      case 'b':
        bw = 1;
        forceham = 0;
        threescreens = 0;
        break;
      case '3':
        threescreens = 1;
        bw = 0;
        forceham = 0;
        break;
      case 'f':
        flip = 1;
        break;
      case 'H':
        forceham = 1;
        bw = 0;
        threescreens = 0;
        break;
      case '?':
      default:
        fprintf(stderr,
	  "Usage: %s [-dlfb3H] [-w width] [-h height] [-o IFFfile] [file]\n",
	  BASENAME(argv[0]));
        exit(1);
    }
  }

  if (optind >= argc)
    ifd = stdin;
  else{
    infile = argv[optind];
    if ((ifd = fopen (infile, "r")) == NULL)
      fprintf (stderr, "cannot open input file %s\n", infile), _abort ();
  }

  if ((tfile = mktemp ("t:rle.XXX")) == NULL)
    fprintf (stderr, "cannot create a temp name for tfile\n"), _abort ();

  if ((tfd = fopen (tfile, "w+")) == NULL)
      fprintf (stderr, "cannot open output file %s\n", tfile), _abort ();

  if ((GfxBase =
       (struct GfxBase *) OpenLibrary ("graphics.library", 0L)) == NULL)
     fprintf (stderr, "Can't open graphics.library\n"), _abort ();
  pal = (GfxBase->DisplayFlags & PAL) ? 1 : 0;

  if ((IntuitionBase = (struct IntuitionBase *)
       OpenLibrary ("intuition.library", 0L)) == NULL)
    fprintf (stderr, "Can't open intuition.library\n"), _abort ();

#ifndef NOIFFLIB
  if ((IFFBase = OpenLibrary(IFFNAME, IFFVERSION)) == NULL) {
#endif
    actions[0].Flags &= ~ITEMENABLED;
    actions[1].Flags &= ~ITEMENABLED;
#ifndef NOIFFLIB
  }
#endif

  if (find_most_used () == -1)
    fprintf (stderr, "error in find_most_used\n"), _abort ();

  if (load) {
    f = fopen("cmap", "r");
    if (!f) {
      fprintf(stderr, "Can't find cmap file\n");
    }else{
      fread((char *)R, sizeof(R), 1, f);
      fread((char *)G, sizeof(G), 1, f);
      fread((char *)B, sizeof(B), 1, f);
    }
    fclose(f);
  }
  if (dump) {
    f = fopen("cmap", "w");
    if (!f) {
      fprintf(stderr, "Can't create cmap file\n");
    }else{
      fwrite((char *)R, sizeof(R), 1, f);
      fwrite((char *)G, sizeof(G), 1, f);
      fwrite((char *)B, sizeof(B), 1, f);
    }
    fclose(f);
  }
  fseek (tfd, 0L, 0);

  if (!forceham && (bw || threescreens || nused <= 32)) {
    isham = FALSE;
    sw = CalcWidth(width);
    sh = CalcHeight(height, pal);
    if (nused > 16) sd = 5;
    else if (nused > 8) sd = 4;
    else if (nused > 4) sd = 3;
    else if (nused > 2) sd = 2;
    else if (nused > 0) sd = 1;
    else
      fprintf (stderr, "found 0!! colors\n"), _abort ();
    if (SW) sw = SW;
    if (SH) sh = SH;
    screen.Width = sw, screen.Height = sh, screen.Depth = sd;
    if (pal) {
      screen.ViewModes =
	((sh > MAX_Y_PAL) ? LACE : 0) | ((sw > MAX_X) ? HIRES : 0);
    }else{
      screen.ViewModes =
	((sh > MAX_Y_NTSC) ? LACE : 0) | ((sw > MAX_X) ? HIRES : 0);
    }
    window.Width = sw, window.Height = sh;
  } else {
    isham = TRUE;
    sd = 6;
    sw = CalcHAMWidth(width);
    sh = CalcHeight(height, pal);
    if (SW) sw = SW;
    if (SH) sh = SH;
    screen.Width = sw, screen.Height = sh, screen.Depth = sd;
    if (pal) {
      screen.ViewModes = ((sh > MAX_Y_PAL) ? LACE : 0) | HAM;
    }else{
      screen.ViewModes = ((sh > MAX_Y_NTSC) ? LACE : 0) | HAM;
    }
    window.Width = sw, window.Height = sh;
  }
    
  for (i=0; i<MENUITEMS; i++) {
    if (screen.ViewModes & HIRES) {
      actions[i].Width += COMMWIDTH;
    }else{
      actions[i].Width += LOWCOMMWIDTH;
    }
  }

  GetPrefs(&prefs, sizeof(prefs));
  Ptr = (USHORT *)AllocMem(POINTERSIZE * sizeof(USHORT), MEMF_CHIP);
  if (!Ptr) {
    fprintf (stderr, "Can't allocate chip memory for pointer\n");
    _abort ();
  }
    
  if (threescreens) {
    if ((s3 = OpenScreen (&screen)) == NULL)
      fprintf (stderr, "Can't open screen 3\n"), _abort ();
    AdjustScreen(s3, &dx, &dy);
    ShowTitle (s3, FALSE);
    window.Screen = s3;
    if ((w3 = OpenWindow (&window)) == NULL)
      fprintf (stderr, "Can't open window 3\n"), _abort ();
    vp3 = &(s3->ViewPort);
    rp3 = w3->RPort;
    SetMenuStrip(w3, &menu);

    if ((s2 = OpenScreen (&screen)) == NULL)
      fprintf (stderr, "Can't open screen 2\n"), _abort ();
    AdjustScreen(s2, &dx, &dy);
    ShowTitle (s2, FALSE);
    window.Screen = s2;
    if ((w2 = OpenWindow (&window)) == NULL)
      fprintf (stderr, "Can't open window 2\n"), _abort ();
    vp2 = &(s2->ViewPort);
    rp2 = w2->RPort;
    SetMenuStrip(w2, &menu);
  }
  if ((s1 = OpenScreen (&screen)) == NULL)
    fprintf (stderr, "Can't open screen 1\n"), _abort ();
  AdjustScreen(s1, &dx, &dy);
  ShowTitle (s1, FALSE);
  window.Screen = s1;
  if ((w1 = OpenWindow (&window)) == NULL)
    fprintf (stderr, "Can't open window 1\n"), _abort ();
  vp1 = &(s1->ViewPort);
  rp1 = w1->RPort;
  SetMenuStrip(w1, &menu);

  max_y = (GfxBase->DisplayFlags & PAL) ? MAX_Y_PAL : MAX_Y_NTSC;
  if (s1->ViewPort.Modes & LACE){
    max_y *= 2;
  }
  max_x = MAX_X;
  if (s1->ViewPort.Modes & HIRES){
    max_x *= 2;
  }
  if (sw <= max_x) {
    freq.LeftEdge = (sw - FREQW) / 2;
  }else{
    freq.LeftEdge = (max_x - FREQW) / 2;
  }
  if (sh <= max_y) {
    freq.TopEdge = (sh - FREQH) / 2;
  }else{
    freq.TopEdge = (max_y - FREQH) / 2;
  }

  if (isham) nused = 16;
  for (i = 0; i < nused; i++) {
    if (threescreens) {
      SetRGB4 (vp1, (long) i, (long) i, 0L, 0L);
      SetRGB4 (vp2, (long) i, 0L, (long) i, 0L);
      SetRGB4 (vp3, (long) i, 0L, 0L, (long) i);
    }else{
      SetRGB4 (vp1, (long) i, (long) R[i], (long) G[i], (long) B[i]);
    }
  }

  memcpy(Ptr, RenderPtr, POINTERSIZE * sizeof(USHORT));
  SetPointer(w1, Ptr, 16, 16, dx - 1, dy);
  if (threescreens) {
    SetPointer(w2, Ptr, 16, 16, dx - 1, dy);
    SetPointer(w3, Ptr, 16, 16, dx - 1, dy);
  }

  display_rle ();

  memcpy(Ptr, prefs.PointerMatrix, POINTERSIZE * sizeof(USHORT));
  SetPointer(w1, Ptr, 16, 16, dx - 1 + prefs.XOffset, dy + prefs.YOffset);
  if (threescreens) {
    SetPointer(w2, Ptr, 16, 16, dx - 1 + prefs.XOffset, dy + prefs.YOffset);
    SetPointer(w3, Ptr, 16, 16, dx - 1 + prefs.XOffset, dy + prefs.YOffset);
  }
  
  if (fnam[0]) {
#ifndef NOIFFLIB
    if (IFFBase) save();
#else
    save();
#endif
  }else{
    done = (UBYTE)0;
    if (threescreens) {
      signalmask = (1L << w1->UserPort->mp_SigBit) |
		   (1L << w2->UserPort->mp_SigBit) |
		   (1L << w3->UserPort->mp_SigBit);
    }else{
      signalmask = 1L << w1->UserPort->mp_SigBit;
    }

    if (threescreens) {	/* Run at high priority to reduce flicker */
      SetTaskPri(FindTask(0L), 20L);
    }
    while (!done) {
      if (threescreens) {
        if (!(havemenu || havereq)) {
          ScreenToFront(s1);
        }
        done = handleIDCMP();
        if (!(havemenu || havereq)) {
	  ScreenToFront(s2);
        }
        done |= handleIDCMP();
        if (!(havemenu || havereq)) {
          ScreenToFront(s3);
        }
        done |= handleIDCMP();
      }
      if (!threescreens || havemenu || havereq) {
        signals = Wait(signalmask);
        if (signals & signalmask) {
          done = handleIDCMP();
        }
      }
    }
  }
  _abort ();
}

int
compare (struct cuse *a, struct cuse *b)
{
  if (a->j < b->j)
    return 1;
  else if (a->j > b->j)
    return -1;
  else
    return 0;
}
  
int
find_most_used (void)
{
  rle_hdr  rle_f_hdr;
  char               comi[4][100];
  char              *como;
  unsigned char      r, g, b;
  int		     ncolors, i, j, x, y, packed, pix, col;

  rle_f_hdr.rle_file = ifd;
  if (rle_get_setup(&rle_f_hdr) < 0)
    return -1;

  RLE_CLR_BIT (rle_f_hdr, RLE_ALPHA);
  for (i = 3; i < rle_f_hdr.ncolors; i++)
    RLE_CLR_BIT (rle_f_hdr, i);

  ncolors = rle_f_hdr.ncolors > 3 ? 3 : rle_f_hdr.ncolors;
  if (ncolors == 2)
    return -1;

  height = rle_f_hdr.ymax - rle_f_hdr.ymin + 1;
  width  = rle_f_hdr.xmax - rle_f_hdr.xmin + 1;

  depth = 0, packed = 0;
  if ((como = rle_getcom ("cy_depth", &rle_f_hdr)) != NULL) {
    depth = atoi (como);
    if ((como = rle_getcom ("cy_width", &rle_f_hdr)) != NULL)
      width = atoi (como);
    como = rle_getcom ("cy_packed", &rle_f_hdr);
    packed = como ? 1 : 0;
  }
  if (!depth)
    depth = ncolors * 8;

  for (i = 0; i < 3; i++)
    if ((sl[i] = (rle_pixel *) malloc(rle_f_hdr.xmax -
                                      rle_f_hdr.xmin + 1)) == NULL) {
      for (j = 0; j < i; j++)
        free ((char *) sl[j]);
      return -1;
    }

  rle_f_hdr.xmax -= rle_f_hdr.xmin, rle_f_hdr.xmin = 0;

  rle_dflt_hdr.dispatch   = RUN_DISPATCH;
  rle_dflt_hdr.alpha      = 0;
  rle_dflt_hdr.background = rle_f_hdr.background;
  rle_dflt_hdr.ncmap      = rle_f_hdr.ncmap;
  rle_dflt_hdr.cmaplen    = rle_f_hdr.cmaplen;
  rle_dflt_hdr.cmap       = rle_f_hdr.cmap;
  rle_dflt_hdr.comments   = NULL;
  rle_dflt_hdr.rle_file       = tfd;
  rle_dflt_hdr.xmin       = 0;
  rle_dflt_hdr.xmax       = width  - 1;
  rle_dflt_hdr.ymin       = 0;
  rle_dflt_hdr.ymax       = height - 1;
  rle_dflt_hdr.ncolors    = ncolors;
  for (i = 0; i < MIN (3, ncolors); i++)
    RLE_SET_BIT (rle_dflt_hdr, i);
  for (i = MIN (3, ncolors); i < ncolors; i++)
    RLE_CLR_BIT (rle_dflt_hdr, i);
  RLE_CLR_BIT (rle_dflt_hdr, RLE_ALPHA);

  if (packed) {
    sprintf (comi[0], "cy_depth=%d", depth), rle_putcom (comi[0], &rle_dflt_hdr);
    sprintf (comi[1], "cy_width=%d", width), rle_putcom (comi[1], &rle_dflt_hdr);
    strcpy (comi[2], "cy_packed=1"),         rle_putcom (comi[2], &rle_dflt_hdr);
  }

  cmap = buildmap (&rle_f_hdr, 3, 1.0, 0.0);

  rle_put_setup (&rle_dflt_hdr);

  while ((y = rle_getrow(&rle_f_hdr, sl)) <= rle_f_hdr.ymax) {
    rle_putrow (sl, width, &rle_dflt_hdr);
    y -= rle_f_hdr.ymin;
    if (ncolors == 3) {
      for (x = 0; x < width; x++)
        r = sl[0][x], g = sl[1][x], b = sl[2][x], cusage[index(r, g, b)].j++;
    } else if (ncolors == 1 && !packed) {
      for (x = 0; x < width; x++) {
        r = cmap[0][sl[0][x]];
	g = cmap[1][sl[0][x]];
	b = cmap[2][sl[0][x]];
	cusage[index(r, g, b)].j++;
      }
    } else {
      for (col = 0, x = 0; x < width; col = (col + 1) & 7, x++) {
        pix = (sl[0][x / 8] & (1 << (7 - col))) ? 0: 255;
        r = pix, g = pix, b = pix, cusage[index(r, g, b)].j++;
      }
    }
  }
  for (i = 0; i < 3; i++)
    free((char *) sl[i]);

  nused = 0;
  for (i = 0; i < 4096; i++) {
    cusage[i].i = i;
    if (cusage[i].j)
      nused++;
  }
  printf ("colors used: %d\n", nused);

  qsort ((char *) cusage, 4096, sizeof (struct cuse), compare);

  if (threescreens) {
    nused = 16;
    return 0;
  }
  if (bw) {
    for (i = 0; i < 16; i++) {
      R[i] = G[i] = B[i] = i;
      nused = 16;
    }
    return 0;
  }
  if (nused <= 32) {
    for (i = 0; i < 32; i++) {
      R[i] = (cusage[i].i >> 8) & 0xf;
      G[i] = (cusage[i].i >> 4) & 0xf;
      B[i] = cusage[i].i & 0xf;
    }
    return 0;
  }

#if 0
  nleft = nused;
  for (i = nused - 1; i >= 0; i--) {
    if (nleft <= 16)
      break;
    for (j = 0; j < i; j++)
      if (rdist (i, j) + gdist (i, j) + bdist (i, j) <= 1) {
        cusage[i].j = 0, nleft--;
        break;
      }
  }
  printf ("colors left %d\n", nleft);
#endif

  for (i = 0, j = 0; j < 16 && i < nused; i++)
    if (cusage[i].j) {
      R[j] = (cusage[i].i >> 8) & 0xf;
      G[j] = (cusage[i].i >> 4) & 0xf;
      B[j] = cusage[i].i & 0xf;
      j++;
    }
      
  return 0;
}

static int           x, y;
static unsigned char r, g, b;

int
display_rle (void)
{
  rle_hdr  rle_f_hdr;
  char              *como;
  int		     ncolors, i, j, packed, pix, col;

  rle_f_hdr.rle_file = tfd;
  if (rle_get_setup(&rle_f_hdr) < 0)
    return -1;

  RLE_CLR_BIT (rle_f_hdr, RLE_ALPHA);
  for (i = 3; i < rle_f_hdr.ncolors; i++)
    RLE_CLR_BIT (rle_f_hdr, i);

  ncolors = rle_f_hdr.ncolors > 3 ? 3 : rle_f_hdr.ncolors;
  if (ncolors == 2)
    return -1;

  height = rle_f_hdr.ymax - rle_f_hdr.ymin + 1;
  width  = rle_f_hdr.xmax - rle_f_hdr.xmin + 1;

  depth = 0, packed = 0;
  if ((como = rle_getcom ("cy_depth", &rle_f_hdr)) != NULL) {
    depth = atoi (como);
    if ((como = rle_getcom ("cy_width", &rle_f_hdr)) != NULL)
      width = atoi (como);
    como = rle_getcom ("cy_packed", &rle_f_hdr);
    packed = como ? 1 : 0;
  }
  if (!depth)
    depth = ncolors * 8;
  offsetx = (sw - width) / 2;
  offsety = (sh - height) / 2;

  for (i = 0; i < 3; i++)
    if ((sl[i] = (rle_pixel *) malloc(rle_f_hdr.xmax -
                                      rle_f_hdr.xmin + 1)) == NULL) {
      for (j = 0; j < i; j++)
        free ((char *) sl[j]);
      return -1;
    }

  rle_f_hdr.xmax -= rle_f_hdr.xmin, rle_f_hdr.xmin = 0;

  while ((y = rle_getrow(&rle_f_hdr, sl)) <= rle_f_hdr.ymax) {
    if (handleIDCMP()) {
      _abort();
    }
    y -= rle_f_hdr.ymin;
    if (ncolors == 3) {
      for (x = 0; x < width; x++)
        r = sl[0][x], g = sl[1][x], b = sl[2][x], show_rgb ();
    } else if (ncolors == 1 && !packed) {
      for (x = 0; x < width; x++) {
        r = cmap[0][sl[0][x]];
	g = cmap[1][sl[0][x]];
	b = cmap[2][sl[0][x]];
	show_rgb ();
      }
    } else {
      for (col = 0, x = 0; x < width; col = (col + 1) & 7, x++) {
        pix = (sl[0][x / 8] & (1 << (7 - col))) ? 0: 255;
        r = pix, g = pix, b = pix, show_rgb ();
      }
    }
  }
  for (i = 0; i < 3; i++)
    free((char *) sl[i]);

  return 0;
}

void
show_rgb (void)
{
  int  xx, yy, i, j;
  long d, td, d1, d2, d3, rd, gd, bd;

  static unsigned char pr, pg, pb;
  
  xx = mapx (x);
  if (!flip) {
    yy = mapy (y);
  }else{
    yy = fmapy (y);
  }
  if (!inside (xx, yy))
    return;

  if (bw) {
#if 0
    r = g = b = 0.35 * r + 0.55 * g + 0.10 * b;
#else
    r = g = b = (35 * r + 55 * g + 10 * b) / 100;
#endif
  }
#ifndef AZTEC_C
  r >>= 4, g >>= 4, b >>= 4;
#else /* avoid a manx bug */
  r = (r >> 4) & 0xf, g = (g >> 4) & 0xf, b = (b >> 4) & 0xf;
#endif
  if (threescreens) {
    OptSetAPen1 (rp1, (long) r); OptWritePixel1 (rp1, (long) xx, (long) yy);
    OptSetAPen2 (rp2, (long) g); OptWritePixel2 (rp2, (long) xx, (long) yy);
    OptSetAPen3 (rp3, (long) b); OptWritePixel3 (rp3, (long) xx, (long) yy);
    return;
  }
  if (!isham) {
    for (i = 0; i < nused; i++) {
      if (R[i] == (int) r && G[i] == (int) g && B[i] == (int) b) {
        OptSetAPen1 (rp1, (long) i); OptWritePixel1 (rp1, (long) xx, (long) yy);
        break;
      }
    }
    return;
  }
  for (i = 0, d = 3276700, j = -1; i < 16; i++) {
    td = SQ (r - R[i]) + SQ (g - G[i]) + SQ (b - B[i]);
    if (td < d)
      d = td, j = i;
    if (d == 0)
      break;
  }
  if (d == 0 || x == 0) {
    OptSetAPen1 (rp1, (long) EXACT (j));
    pr = R[j], pg = G[j], pb = B[j];
  } else {
    rd = SQ (r - pr), gd = SQ (g - pg), bd = SQ (b - pb);
    d1 = rd + gd, d2 = rd + bd, d3 = gd + bd;
    if (d <= d1 && d <= d2 && d <= d3) {
      OptSetAPen1 (rp1, (long) EXACT (j));
      pr = R[j], pg = G[j], pb = B[j];
    } else if (d1 <= d2 && d1 <= d3) {
      OptSetAPen1 (rp1, (long) BLUE (b));
      pb = b;
    } else if (d2 <= d3 && d2 <= d1) {
      OptSetAPen1 (rp1, (long) GREEN (g));
      pg = g;
    } else {
      OptSetAPen1 (rp1, (long) RED (r));
      pr = r;
    }
  }
  OptWritePixel1 (rp1, (long) xx, (long) yy);
}

#ifdef MCH_AMIGA
void
_abort (void)
#else
int
CXBRK (void)
#endif
{
  if (ifd && ifd != stdin)
    fclose (ifd);

  if (tfd) {
    fclose (tfd);
    unlink (tfile);
  }

  if (w1) {
    if (w1->MenuStrip)
      ClearMenuStrip(w1);
    CloseWindow (w1);
  }

  if (w2) {
    if (w2->MenuStrip)
      ClearMenuStrip(w2);
    CloseWindow (w2);
  }

  if (w3) {
    if (w3->MenuStrip)
      ClearMenuStrip(w3);
    CloseWindow (w3);
  }

  if (s1)
    CloseScreen (s1);

  if (s2)
    CloseScreen (s2);

  if (s3)
    CloseScreen (s3);

  if (Ptr)
    FreeMem(Ptr, POINTERSIZE * sizeof(USHORT));

  if (GfxBase)
    CloseLibrary ((struct Library *)GfxBase);

  if (IntuitionBase)
    CloseLibrary (IntuitionBase);

#ifndef NOIFFLIB
  if (IFFBase)
    CloseLibrary (IFFBase);
#endif

  exit (0);
#ifndef MCH_AMIGA	/* Make Lattice shut up */
  return 1;
#endif
}

UBYTE
handleIDCMP(void)
{
  UBYTE flag = 0;
  struct IntuiMessage *message = NULL;
  USHORT code, selection;
  ULONG class, itemnum;
  int t;

  while ((message = (struct IntuiMessage *)GetMsg(w1->UserPort)) ||
      (threescreens && (message=(struct IntuiMessage *)GetMsg(w2->UserPort))) ||
      (threescreens && (message=(struct IntuiMessage *)GetMsg(w3->UserPort)))) {
    class = message->Class;
    code = message->Code;
    switch (class) {
      case GADGETUP:
        flag = 1;
        break;
      case REQSET:
        FixColors();
        if (threescreens) ScreenToFront(s1);
	ActivateGadget(&fgad, w1, &freq);
	break;
      case REQCLEAR:
	save();
	havereq = 0;
	ResetColors();
	break;
      case MENUVERIFY:
        havemenu = 1;
	FixColors();
        break;
      case MENUPICK:
        selection = code;
        while (selection != MENUNULL) {
          itemnum = ITEMNUM(selection);
          switch (itemnum) {
            case 0:
              if (ifd == stdin) {
	        if (threescreens) ScreenToFront(s1);
		havereq = 1;
		Request(&freq, w1);
	      }else{
	        strcpy(fnam, infile);
	        t = strlen(fnam);
	        if ( (t >= 4) && (strcmp(&fnam[t-4], ".rle") == 0)) {
	          fnam[t-4] = '\000';
	        }
	        strcat(fnam, ".ilbm");
	      }
	      save();
              break;
            case 1:
	      if (threescreens) ScreenToFront(s1);
	      havereq = 1;
	      Request(&freq, w1);
              break;
            case 2:
              flag = 1;
              break;
            default:
              break;
          }
          selection = ((struct MenuItem *)ItemAddress
          		(&menu, (LONG)selection))->NextSelect;
        }
        havemenu = 0;
        ResetColors();
        break;
      default:
        break;
    }
    ReplyMsg((struct Message *)message);
  }
  return flag;
}

void
save()
{
#ifndef NOIFFLIB
  char *name;

  if (*fnam == '\000')
    return;

  if (IFFBase) {
    ResetColors();
    if (threescreens) {
      name = malloc(strlen(fnam) + 4);
      if (!name) {
        fprintf(stderr, "malloc failed\n");
        return;
      }
      strcpy(name, fnam);
      strcat(name, ".r");
      SaveBitMap(name, s1->BitMap,vp1->ColorMap->ColorTable,(isham ? 0x81 : 1));
      strcpy(name, fnam);
      strcat(name, ".g");
      SaveBitMap(name, s2->BitMap,vp2->ColorMap->ColorTable,(isham ? 0x81 : 1));
      strcpy(name, fnam);
      strcat(name, ".b");
      SaveBitMap(name, s3->BitMap,vp3->ColorMap->ColorTable,(isham ? 0x81 : 1));
      free(name);
    }else{
      SaveBitMap(fnam, s1->BitMap,vp1->ColorMap->ColorTable,(isham ? 0x81 : 1));
    }
    FixColors();
  }
#endif
}

void
FixColors()
{
  if (threescreens) {
    OptSetAPen1(rp1, 1L); OptSetAPen2(rp2, 1L); OptSetAPen3(rp3, 1L);
    SetBPen(rp1, 0L); SetBPen(rp2, 0L); SetBPen(rp3, 0L);
    SetRGB4(vp1, 1L, 15L, 15L, 15L);
    SetRGB4(vp2, 1L, 15L, 15L, 15L);
    SetRGB4(vp3, 1L, 15L, 15L, 15L);
  }else{
    OptSetAPen1(rp1, 1L);
    SetBPen(rp1, 0L);
    SetRGB4(vp1, 1L, (R[0]+8) % 16, (G[0]+8) % 16, (B[0]+8) % 16);
  }
}

void
ResetColors()
{
  if (threescreens) {
    SetRGB4(vp1, 1L, 1L, 1L, 1L);
    SetRGB4(vp2, 1L, 1L, 1L, 1L);
    SetRGB4(vp3, 1L, 1L, 1L, 1L);
  }else{
    SetRGB4(vp1, 1L, (long)R[1], (long)G[1], (long)B[1]);
  }
}

int
CalcWidth(int width)
{
  if (width <= 320) return 320;
  if (width <= MAX_X) return width;
  if (width <= 640) return 640;
  return width;
}

int
CalcHAMWidth(int width)
{
  if (width <= 320) return 320;
  return width;
}

int
CalcHeight(int height, int pal)
{
  if (pal) {
    if (height <= 256) return 256;
    if (height <= MAX_Y_PAL) return height;
    if (height <= 512) return 512;
    return height;
  }else{
    if (height <= 200) return 200;
    if (height <= MAX_Y_NTSC) return height;
    if (height <= 400) return 400;
    return height;
  }
}

/*
   No-one in his right mind would want to run this program from the Workbench!
*/
#ifdef MCH_AMIGA
void
_wb_parse()
{}
#endif
