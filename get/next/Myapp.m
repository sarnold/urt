#import "Myapp.h"
#import "RLE.h"
#import "RASF.h"
#import <rle.h>
#import <stdio.h>
#import <stdlib.h>
#import <libc.h>
#import <appkit/OpenPanel.h>
#import <appkit/SavePanel.h>
#import <appkit/PopUpList.h>
#import <appkit/Control.h>
#import <appkit/ActionCell.h>
#import <strings.h>
#import <appkit/tiff.h>

// Myapp.m
// 
// Written by Vince DeMarco 
// 	demarco@cpsc.ucalgary.ca
//
// This program is In the Public Domain. If you make any improvements to this
// program please let me know
// 

@implementation Myapp:Application
+ new
{
    self = [super new];
    [self activateSelf:NO];
    saveType = 1;                // Set Default type to save as Tiff
    return(self);
}

- (int)appOpenFile:(const char *)fullPath type:(const char *)type
{
    id                  ptr;
 
    [self activateSelf:NO];
    if (!strcmp(type,"rle"))
	ptr = [[RLE alloc]init];
    else
	ptr = [[RASF alloc]init];
    [ptr open:(char *)fullPath];
    return YES;
}

- (BOOL)appAcceptsAnotherFile:sender
{
    return YES;
}

- openfile:sender
{
    char                      temp[1024];
    char                      directory[1024];
    char                     *extension;
    id                        ptr;
    int                       loop;
    static const char  *const opentypes[] = {"rle","rasf",NULL};

    openpanel = [OpenPanel new];

    [openpanel allowMultipleFiles:YES];

    if ([openpanel runModalForTypes:opentypes]) {
	loop = 0;
	strncpy(directory,[openpanel directory],1024);
	while ([openpanel filenames][loop]) {
	    sprintf(temp,"%s/%s",directory,[openpanel filenames][loop]);
	    extension = rindex(temp,'.');
	    if (!strcmp(extension,".rle"))
		ptr = [[RLE alloc]init];
	    else
		ptr = [[RASF alloc]init];

	    [ptr open:temp];
	    ++loop;
	}
    }
    return(self);
}

- print:sender
{
    [[mainWindow contentView] printPSCode:sender];
    return(self);
}

- save:sender
{
    savepanel = [SavePanel new];
    [savepanel setAccessoryView:savePanelAccessory];
    [savepanel setRequiredFileType:"tiff"];
    [savepanel setTitle:"Save To TIFF"];
    [compPopUP setEnabled:YES];
    saveType = 1;

    if (mainWindow != nil){
	if ([savepanel runModal]){
	    if (saveType == 1){
		[[mainWindow contentView] saveAsTiff:[savepanel filename] usingCompression:[self setCompType]];
	    }else{
		[[mainWindow contentView] saveAsEPS:[savepanel filename]];		
	    }
	}
    }
    return(self);
}

- saveTypeEPS:sender
{
    [savepanel setRequiredFileType:"eps"]; 
    [savepanel setTitle:"Save To EPS"];
    [compPopUP setEnabled:NO];
    saveType = 2;
    return (self);
}

- saveTypeTiff:sender
{
    [savepanel setRequiredFileType:"tiff"]; 
    [savepanel setTitle:"Save To TIFF"];
    [compPopUP setEnabled:YES];
    saveType = 1;
    return(self);
}

- (int)setCompType
{

    const char *selected_Cell =  [[compPopUP selectedCell] title];

    switch(selected_Cell[0]){
	case 'L':
	     return(NX_TIFF_COMPRESSION_LZW);
	case 'P':
	     return(NX_TIFF_COMPRESSION_PACKBITS);
	case 'N':
	     return(NX_TIFF_COMPRESSION_NONE);
	case 'J':
	     return(NX_TIFF_COMPRESSION_JPEG);
    }
    return(NX_TIFF_COMPRESSION_LZW);
}
@end
