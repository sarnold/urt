#import <appkit/Application.h>

// Myapp.h
// 
// Written by Vince DeMarco 
// 	demarco@cpsc.ucalgary.ca
//
// This program is In the Public Domain. If you make any improvements to this
// program please let me know

@interface Myapp:Application
{
    id                  openpanel;
    id                  savepanel;
    id                  savePanelAccessory;
    id                  compPopUP;
    int                 saveType;     /* 1 tiff 2 eps */
}

+ new;
- (int)appOpenFile:(const char *)fullPath type:(const char *)type;
- (BOOL)appAcceptsAnotherFile:sender;
- openfile:sender;
- print:sender;
- save:sender;
- saveTypeEPS:sender;
- saveTypeTiff:sender;
- (int)setCompType;
@end

