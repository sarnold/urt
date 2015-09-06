/*
	Show3.c: display three IFF files in rapid succession continuously.
	Based on showiff.c by Christian A. Weber.
	Modified by Kriton Kyrimis.
*/

#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#ifdef LATTICE
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <iffpragmas.h>
#endif
#include <iff.h>

struct Library *IntuitionBase = NULL, *IFFBase = NULL, *OpenLibrary();
struct GfxBase *GfxBase = NULL;

struct NewScreen ns =
{
  0,0,0,0,0,0,0, NULL, CUSTOMSCREEN | SCREENQUIET, NULL, (STRPTR)"Show3", NULL,
  NULL
};

struct Screen *screen[3] = {NULL, NULL, NULL}, *OpenScreen();
APTR ifffile = NULL;
char *fname[3] = {NULL, NULL, NULL};
int fsiz = 0;
void CenterScreen(register struct Screen *, int *, int *);

void
Fail(char *text, int status)
{
	int i;

	if(ifffile) CloseIFF(ifffile);
	if(screen[0]) CloseScreen(screen[0]);
	if(screen[1]) CloseScreen(screen[1]);
	if(screen[2]) CloseScreen(screen[2]);
	if (fsiz) {
	  for (i=0; i<3; i++) {
	    if (fname[i]) FreeMem(fname[i], fsiz);
	  }
	}
	if (*text) {
	  printf("%s\n",text);
	}
	if(IFFBase) {
	  if (i = IffError()) {
	    printf("IffError = %ld\n", i);
	  }
          CloseLibrary(IFFBase);	/* MUST ALWAYS BE CLOSED !! */
        }
	if(IntuitionBase) CloseLibrary(IntuitionBase);
	if(GfxBase) CloseLibrary((struct Library *)GfxBase);
	exit(status);
}

main(int argc, char *argv[])
{
  long count,i;
  int dx, dy;
  WORD colortable[128];
  struct BitMapHeader *bmhd;

  if(!strcmp(argv[1],"?") || ((argc !=4) && (argc != 2))) {
    printf("Usage: %s redfile greenfile bluefile\n"
	   "       %s filename (.[rgb] will be appended)\n",
	   argv[0], argv[0]);
    exit(1);
  }
  if (argc == 4) {
    for (i=0; i<3; i++) {
      fname[i] = argv[i+1];
    }
  }else{
    fsiz = strlen(argv[1]) + 3;
    for (i=0; i<3; i++) {
      fname[i] = AllocMem(fsiz, 0L);
      if (!fname[i]) {
        Fail("Can't allocate memory", 1);
      }
      strcpy(fname[i], argv[1]);
      strcat(fname[i], ((i == 0) ? ".r" : ((i == 1) ? ".g" : ".b")));
    }
  }
  GfxBase = (struct GfxBase *)OpenLibrary("graphics.library",0L);
  if (!GfxBase) {
    Fail("Can't open graphics.library", 1);
  }
  IntuitionBase = OpenLibrary("intuition.library",0L);
  if (!IntuitionBase) {
    Fail("Can't open intuition.library", 1);
  }
  if(!(IFFBase = OpenLibrary(IFFNAME,IFFVERSION))) {
    Fail("Copy the iff.library to your LIBS: directory!", 1);
  }

  for (i=0; i<3; i++) {
    if(!(ifffile=OpenIFF(fname[i]))){
      Fail("Error opening file", 1);
    }
    if(!(bmhd=GetBMHD(ifffile))) {
      Fail("BitMapHeader not found", 1);
    }
    ns.Width      = bmhd->w;
    ns.Height     = bmhd->h;
    ns.Depth      = bmhd->nPlanes;
    ns.ViewModes  = GetViewModes(ifffile);

    if(!(screen[i] = OpenScreen(&ns))) {
      Fail("Can't open screen!", 1);
    }
    CenterScreen(screen[i], &dx, &dy);

    count = GetColorTab(ifffile,colortable);
    if(count>32L) count = 32L; /* Some HAM pictures have 64 colors ?! */
    LoadRGB4(&(screen[i]->ViewPort),colortable,count);

    if(!DecodePic(ifffile,&screen[i]->BitMap)) {
      Fail("Can't decode picture", 1);
    }
    CloseIFF(ifffile);
    ifffile = NULL;
  }
  SetTaskPri(FindTask(0L), 20L);	/* Run at high priority to reduce */
  for(;;) {				/* flicker! */
    if(!((*((UBYTE*)0xbfe001))&64)) break;	/* I know it's a hack */
    ScreenToFront(screen[0]);
    if(!((*((UBYTE*)0xbfe001))&64)) break;	/* I know it's a hack */
    ScreenToFront(screen[1]);
    if(!((*((UBYTE*)0xbfe001))&64)) break;	/* I know it's a hack */
    ScreenToFront(screen[2]);
  }
  Fail("", 0); /* Close the whole stuff */
}
