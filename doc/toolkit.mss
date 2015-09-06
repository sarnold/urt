@make(Article)
@Device(PostScript)
@Disable(FigureContents)
@LibraryFile(Mathematics10)

@Modify(Description, LeftMargin +1.5inch, Indent -1.25inch, below 0.5 lines)

@Define(FigureFilename, Above 0, Below 0, Centered, Font BodyFont,FaceCode I )

@comment( These macros define how pictures are printed out.  Since the raster
	  images take a *long* time to print for the whole paper, this allows
	  them to be stubbed out.

	  Use the first form for printing drafts, the second for
	  actually printing the images.  Of course, this assumes you have
	  a PostScript output device. )

@form(RLEpicture="@blankSpace[@Parm(Size)]@FigureFilename[@Parm(PostScript)]")
@comment{
@form(RLEpicture="@Picture[ Size @Parm(Size),
			    PostScript=<@Parm(PostScript)> ]")
}


@Comment{This document uses "@F" for bold typewriter font.
	 I know it isn't documented, but it's exactly what I want.
	 If you're using something other than PostScript, the following
	 re-defines it to be the typewriter font:

	   @Textform(F="@T[@parm(Text)]")
	}

@comment[This font is much better than Times, but we can't
         assume all PostScript printers have it.
         @style(FontFamily=NewCenturySchoolbook)
        ]
@Style(FontFamily=TimesRoman)

@style(Spacing=1)  @comment(1.1?)
@comment[ no nuke page headings @PageHeading(Left "") ]

@MajorHeading(The Utah Raster Toolkit)

@begin(Center)
John W. Peterson
Rod G. Bogart
@i(and)
Spencer W. Thomas
@blankSpace(1 line)
University of Utah, Department of Computer Science@footnote(Originally presented at the third Usenix Workshop on Graphics, Monterey California, November 1986)
Salt Lake City, Utah
@end(Center)
@blankspace(1 inch)
@Heading(Abstract)
@begin(quotation)
The Utah Raster Toolkit is a set of programs for manipulating and
composing raster images.  These tools are based on the Unix concepts of
pipes and filters, and operate on images in much the same way as the
standard Unix tools operate on textual data.
The Toolkit uses a special run length encoding (RLE) format for
storing images and interfacing between the various programs.  This
reduces the disk space requirements for picture storage and provides a
standard header containing descriptive information about an image.
Some of the tools are able to work directly with the compressed
picture data, increasing their efficiency.  A
library of C routines is provided for reading and writing the RLE
image format, making the toolkit easy to extend.

This paper describes the individual tools, and gives several examples of
their use and how they work together.  Additional topics that arise in
combining  images, such as how to combine color table information from
multiple sources, are also discussed.
@end(Quotation)
@blankspace(3 lines)

@comment(This turns on two column mode - IT ALSO DIES WITH A BUS ERROR
	@begin[Text, columns=2, boxed, LineWidth = 3.2 inch, 
       		columnMargin = 0.5 inch])

@section(Introduction)

Over the past several years, the University of Utah Computer Graphics Lab has
developed several tools and techniques for generating, manipulating and
storing images.  At first this was done in a somewhat haphazard manner -
programs would generate and store images in various formats (often dependent
on particular hardware) and data was often not easily interchanged between
them.  More recently, some effort has been made to standardize the image
format, develop an organized set of tools for operating on the images, and
develop a subroutine library to make extending this set of tools easier.  This
paper is about the result of this effort, which we call the @i(Utah Raster
Toolkit.)  Using a single efficient image format, the toolkit provides a number
of programs for performing common operations on images.  These are in turn
based on a subroutine library for reading and writing the images.

@section(Origins)
The idea for the toolkit arose while we were developing an image
compositor.  The compositor (described in detail in section
@ref(comp)) allows images to be combined in various ways.  We found
that a number of simple and independent operations were frequently
needed before images could be composited together.

With the common image format, the subroutine library for manipulating
it, and the need for a number of independent tools, the idea of a
"toolkit" of image manipulating programs arose.  These tools are
combined using the Unix shell, operating on images
much like the standard Unix programs operate on textual data.  For
example, a Unix user would probably use the sequence of commands:
@ProgramExample[
	cat /etc/passwd | grep Smith | sort -t: +4.0 | lpr
]
to print a sorted list of all users named "Smith" in the system
password file.  Similarly, the Raster Toolkit user might do
something like:
@ProgramExample[
	cat image.rle | avg4 | repos -p 0 200 | getfb
]
to downfilter an image and place it on top of the frame buffer screen.  The
idea is similar to a method developed by Duff @cite(zcomp) for three
dimensional rendering.

@section(The RLE format)
The basis of all of these tools is a Run Length Encoded image format@cite(RLE).
This format is designed to provide an efficient, device
independent means of storing multi-level raster images.  It is not
designed for binary (bitmap) images.  It is built on several basic
concepts.  The central concept is the @i[channel].  A channel
corresponds to a single color, thus there are normally separate red, green and
blue channels.  Up to 255 color channels are
available for use; one channel is reserved for coverage ("alpha")
data.  Although the format supports arbitrarily deep channels, the
current implementation is restricted to 8 bits per channel.  An RLE
file is treated as a byte stream, making it independent of host byte ordering.

Image data is stored in an RLE file in a scanline form, with the data for
each channel of the scanline grouped together.  Runs of identical pixel
values are compressed into a count and a value.  However, sequences of
differing pixels are also stored efficiently (i.e, not as a sequence of single
pixel runs).

@subsection(The RLE header)
The file header contains a large amount of information about the image.
This includes: 
@begin(Itemize)
The size and position on the screen,

The number of channels saved and the number of bits per channel
(currently, only eight bits per channel is supported),

Several flags, indicating: how the background should be handled,
whether or not an alpha channel was saved, if picture comments were saved,

The size and number of channels in the color map, (if the color map is
supplied),

An optional background color,

An optional color map,

An optional set of comments.  The comment block contains any number of
null-terminated text strings.  These strings are conventionally of the
form "name=value", allowing for easy retrieval of specific information.
@end(Itemize)

@SubSection(The scanline data)
The scanline is the basic unit that programs read and write.
It consists of a sequence of operations, such as @i[Run],
@i[SetChannel], and @i[Pixels], describing the actual image.  An image is
stored starting at the lower left corner and proceeding upwards in order of
increasing scanline number.  Each operation and its associated data takes up
an even number of bytes, so that all operations begin on a 16 bit boundary.
This makes the implementation more efficient on many architectures.

Each operation is identified by an 8 bit opcode, and may have one or more
operands.  Single operand operations fit into a single 16 bit word if the
operand value is less than 256.  So that operand values are not limited to
the range 0..255, each operation has a @i[long] variant, in which the byte
following the opcode is ignored and the following word is taken as a 16 bit
quantity.  The long variant of an opcode is indicated by setting the bit 0x40
in the opcode (this allows for 64 opcodes, of which 6 have been used so far.)

The current set of opcodes include:

@begin(Description)
SkipLines@\Increment the @i[scanline number] by the operand value, terminating
the current scanline.

SetColor@\Set the @i[current channel] to the operand value.

SkipPixels@\Skip over pixels in the current scanline.
Pixels skipped will be left in the background color.

PixelData@\Following this opcode is a sequence of pixel values.  The length
of the sequence is given by the operand value.

Run@\This is the only two operand opcode.  The first operand is the length
(@i[N]) of the run.  The second operand is the pixel value, followed by a
filler byte if necessary@Footnote(E.g., a 16 bit pixel value would not need a
filler byte.).  The next @i[N] pixels in the scanline are set to
the given pixel value.

EOF@\This opcode has no operand, and indicates the end of the RLE file.  It
is provided so RLE files may be concatenated together and still be
correctly interpreted.  It is not required, a physical end of file also
indicates the end of the RLE data.
@End(Description)

@SubSection(Subroutine Interface)

Two similar subroutine interfaces are provided for reading and writing files
in the RLE format.  Both read or write a scanline worth of data at a time.  A
simple "row" interface communicates in terms of arrays of pixel values.  It
is simple to use, but slower than the "raw" interface, which uses arrays of
"opcode" values as its communication medium.

In both cases, the interface must be initialized by calling a setup function.
The two types of calls may be interleaved; for example, in a rendering
program, the background could be written using the "raw" interface, while
scanlines containing image data could be converted with the "row" interface.
The package allows multiple RLE streams to be open simultaneously, as is
necessary for use in a compositing tool, for example.  All data relevant to a
particular RLE stream is contained in a "globals" structure.  This
structure essentially echoes the information in the RLE header, along
with current state information about the RLE stream.

@section(The tools)
@subsection(The image compositor - Comp)
@label(comp)
@F(Comp) implements an image compositor based on the compositing algorithms
presented in @cite(Alpha).  The compositing operations are based on the
presence of an alpha channel in the image.  This extra channel usually defines
a mask which represents a sort of a cookie-cutter for the image.  This is the 
case when alpha is 255 (full coverage) for pixels inside the shape, zero
outside, and between zero and 255 on the boundary.  
When the compositor operates on images of the cookie-cutter style, the
operations behave as follows:
@begin(Description)
A @b(over) B (the default)@\The result will be the union of the two
image shapes, with A obscuring B in the region of overlap.

A @b(atop) B@\The result shape is the same as image B, with A obscuring
B where the image shapes overlap. (Note that this differs from
"over" because the portion of A outside the shape of B will not
be in the result image.)

A @b(in) B@\The result is simply the image A cut by the shape of B.
None of the image data of B will be in the result.

A @b(out) B@\The result image is image A with the shape of B cut out.

A @b(xor) B@\The result is the image data from both images that is
outside the overlap region.  The overlap region will be blank.

A @b(plus) B@\The result is just the sum of the image data.  This
operation is actually independent of the alpha channels.

@end(Description)


The alpha channel can also represent a semi-transparent mask for the
image.  It would be similar to the cookie-cutter mask, except the
interior of the shape would have alpha values that represent partial
coverage; e.g. 128 is half coverage.  When one of the images to be
composited is a semi-transparent mask, the following operations have
useful results:
@begin(Description)
Semi-transparent A @b(over) B@\The image data of B will be blended with
that of A in the semi-transparent overlap region.  The resulting
alpha channel is as transparent as that of image B.

A @b(in) Semi-transparent B@\The image data of A is scaled by the
mask of B in the overlap region.  The alpha channel is
the same as the semi-transparent mask of B.
@end(Description)

If the picture to be composited doesn't have an alpha channel present, comp
assumes an alpha of 255 (i.e., full coverage) for non-background pixels and
an alpha of zero for background pixels.

Comp is able to take advantage of the size information for the two
images being composited.  For example, if a small picture is being
composited over a large backdrop, the actual compositing arithmetic
is only performed on a small portion of the image.  By looking at the
image size in the RLE header, comp performs the compositing operation
only where the images overlap.  For the rest of the image the
backdrop is copied (or not copied, depending on the compositing
operation).  The "raw" RLE read and write routines are used to perform
this copy operation, thus avoiding the cost of compressing and
expanding the backdrop as well.

@subsection(Basic image composition - Repos and Crop)
@F(Repos) and @F(crop) are the basic tools for positioning and
arranging images to be fed to the compositor.  Crop simply throws away
all parts of the image falling outside the rectangle specified.  Repos
positions an image to a specific location on the screen, or moves it
by an incremental amount.  Repos does not have to modify any of the
image data, it simply changes the position specification in the RLE
header.  In order to simplify the code, the Raster Toolkit does not
allow negative pixel coordinates in images.

@subsection(Changing image orientation and size - Flip, Fant and Avg4)
Two tools exist for changing the orientation of an image on the screen.
@F(Flip) rotates an image by 90 degrees right or left, turns an image
upside down, or reverse it from right to left.  @F(Fant) rotates an image an
arbitrary number of degrees from -45 to 45.  In order to get rotation beyond 
-45 to 45, flip and fant can be combined, for example:
@ProgramExample[
	flip -r < upright.rle | fant -a 10 > rotated.rle
]
rotates an image 100 degrees.  Fant is also able to scale images by an
arbitrary amount in X and Y.  A common use for this is to stretch or shrink
an image to correct for the aspect ratio of a particular frame buffer. Many
frame buffers designed for use with standard video hardware display a
picture of 512x480 pixels on a screen with the proportions 4:3, resulting in
an overall pixel aspect ratio of 6:5.  If the picture @F[kloo.rle] is
digitized or computed assuming a 1:1 aspect ratio (square pixels), the
command: 
@ProgramExample[
	cat kloo.rle | fant -s 1.0 1.2 | getfb
]
correctly displays the image on a frame buffer with a 6:5 aspect ratio.
Fant is implemented using a two-pass subpixel sampling algorithm@cite(fant). 
This algorithm performs the spatial transform (rotate and/or scale) first
on row by row basis, then on a column by column basis.  Because the
transformation is done on a subpixel level, fant does not introduce aliasing
artifacts into the image.

@F(Avg4) downfilters an RLE image into a resulting image of 1/4th the
size, by simply averaging four pixel values in the input image to
produce a single pixel in the output.  If the original image does not
contain an alpha channel, avg4 creates one by counting the number of
non-zero pixels in each group of four input pixels and using the count
to produce a coverage value.  While the alpha channel produced this
way is crude (only four levels of coverage) it is enough to make a
noticeable improvement in the edges of composited images.  One use for
avg4 is to provide anti-aliasing for rendering programs that perform
no anti-aliasing of their own.  For example, suppose @F[huge.rle] is a 4k
x 4k pixel image rendered without anti aliasing and without an alpha
channel.  Executing the commands:
@ProgramExample[
	cat huge.rle | avg4 | avg4 | avg4 > small.rle
]
produces an image @F[small.rle] with 64 (8x8) samples per pixel and an
alpha channel with smooth edges.  

Images generated from this approach are as good as those produced by direct
anti-aliasing algorithms such as the A-buffer @cite(Abuffer).
However, a properly implemented A-buffer renderer produces images nearly an
order of magnitude faster.


@subsection(Color map manipulation - Ldmap and Applymap)

As mentioned previously, RLE files may optionally contain a color map for the
image.  @F(Ldmap) is used to create or modify a color map within an RLE file.
Ldmap is able to create some standard color maps, such as linear maps with
various ramps, or maps with various gamma corrections.  Color maps may also be
read from a simple text file format, or taken from other RLE files.  Ldmap
also performs @i(map composition), where one color map is used as in index
into the other.  An example use for this is to apply a gamma correction to an
image already having a non-linear map.  

@F(Applymap) applies the color map in an rle file
to the pixel values in the file.  For example, if the color map
in @F[kloo.rle] contained a color map with the entries (for the red channel):
@begin(Figure)
@verbatim[
    Index   Red color map
	0:	5
	1: 	7
	2:	9
]
@end(Figure)

Then a (red channel) pixel value of zero would be displayed with an
intensity of five (assuming the display program used the color map in
@F[kloo.rle]).  When kloo.rle is passed through applymap,
@ProgramExample[
	cat kloo.rle | applymap > kloo2.rle
]
pixels that had a value of zero in @f[kloo.rle] now have a value of
five in @f[kloo2.rle], pixel values of one would now be seven, etc.
When displaying the images on a frame buffer, @f[kloo2.rle] appears
the same with a linear map loaded as @F(kloo.rle) does with its special
color map loaded.

One use for these tools is merging images with different compensation
tables.  For example, suppose image @F[gam.rle] was computed so that
it requires a gamma corrected color table (stored in the RLE file) to
be loaded to look correct on a particular monitor, and image @F(lin.rle)
was computed with the gamma correction already taken into
consideration (so it looks correct with a linear color table loaded).
If these two images are composited together without taking into
consideration these differences, the results aren't correct (part of
the resulting image is either too dim or washed out).  However,
we can use applymap to "normalize" the two images to using a linear
map with:
@ProgramExample[
cat gam.rle | applymap | comp - lin.rle | ldmap -l > result.rle
]

@subsection( Generating backgrounds )
Unlike most of the toolkit programs which act as filters, @F(background) just
produces output.  Background either produces a simple flat field, or it
produces a field with pixel intensities ramped in the vertical
direction.  Most often background is used simply to provide a colored
backdrop for an image.
Background is another example of a program that takes advantage of the "raw"
RLE facilities.  Rather than generating the scanlines and compressing them,
background simply generates the opcodes required to produce each scanline.

@subsection( Converting full RGB images to eight bits - to8 and tobw )
Although most of the time we prefer to work with full 24 bit per pixel images,
(32 bits with alpha) we often need the images represented with eight bits per
pixel.  Many inexpensive frame buffers (such as the AED 512) and many personal
color workstations (VaxStation GPX, Color Apollos and Suns) are
equipped with eight bit displays.

@F(To8) converts a 24 bit image to an eight bit image by applying a dither
matrix to the pixels.  This basically trades spatial resolution for color
resolution.  The resulting image has eight bits of data per pixel, and also
contains a special color map for displaying the dithered image (since most
eight bit frame buffers still use 24 bit wide color table entries).

@F(Tobw) converts a picture from 24 bits of red, green, and blue to an
eight bit gray level image.  It uses the standard television YIQ
transformation of
@begin(MathDisplay)
	graylevel#=#@r[0.35]#*#red#+#@r[0.55]#*#green#+#@r[0.10]#*#blue.
@end(MathDisplay)
These images are often preferred when displaying the image on an eight bit
frame buffer and shading information is more important than color.

@begin(Comment)

*** This section needs a for-real mathematical description.  Spencer
*** couldn't come up with one off hand. ***

@subsection( Compressing dynamic range of image - unexp )
Some image synthesis programs may generate images where the full
dynamic range is not known ahead of time.
@end(Comment)

@section( Interfacing to the RLE toolkit )
In order to display RLE images, a number of programs are provided for
displaying the pictures on various devices.  These display programs all read
from standard input, so they are conveniently used as the end of a raster
toolkit pipeline.  The original display program, @F(getfb), displays images
on our ancient Grinnell GMR-27 frame buffer.  Many display programs have
since been developed:
@begin(Description)
@F[getcx]@\displays images on a Chromatics CX1500

@F[getiris]@\displays an image on an Iris workstation (via ethernet)

@F[getX]@\for the X window system

@F[getap]@\for the Apollo display manager

@F[gethp]@\for the Hewlett-Packard Series 300 workstation color display

@F[rletops]@\converts an RLE file to gray-level PostScript
output@footnote(@f[Rletops] was used to produce the figures in this paper.)
@end(Description)

The @F[getX], @F[getap] and @F[gethp] programs automatically perform the
dithering required to convert a 24 bit RLE image to eight bits (for
color nodes) or one bit (for bitmapped workstations).  Since all of
these workstations have high resolution (1Kx1K) displays, the trade of
spatial resolution for color resolution produces very acceptable
results.  Even on bitmapped displays the image quality is good enough
to get a reasonable idea of an image's appearance.

Two programs, @F(painttorle) and @F(rletopaint) are supplied to convert
MacPaint images to RLE files.  This offers a simple way to add text or
graphic annotation to an RLE image (although the resolution is somewhat low).

@Section( Examples )
A typical use for the toolkit is to take an image generated
with a rendering program, add a background to it, and display the
result on a frame buffer:
@programExample[
rlebg 250 250 250 -v | comp image.rle - | getfb
]

What follows are some more elaborate applications:

@subSection( Making fake shadows )
In this exercise we take the dart (Figure @ref(original_dart)a) and stick it
into the infamous mandrill, making a nice (but completely fake) shadow
along the way.
@Figure{
@RLEPicture(Size=3 inch, PostScript=[pics/dart_and_strtch.ps])
@caption[@b(a:) Original dart image. @b(b:) Rotated and stretched dart.]
@tag(Original_dart)
}

First the image of the dart is rotated by 90 degrees (using @F[rleflip
-l] and then stretched in the X direction (using @F[fant -s 1.3 1.0])
to another image we can use as a shadow template, figure
@ref(Original_dart)b.  Then we take this image, @F[dart_stretch_rot.rle], 
and do:
@programExample[
rlebg 0 0 0 175 \
   | comp -o in - dart_stretch_rot.rle > dart_shadow.rle
]

This operation uses the stretched dart as a cookie-cutter, and the
shape is cut out of a black image, with a coverage mask (alpha
channel) of 175.  Thus when we composite the shadow (figure
@ref(DartShadow)a) over the mandrill image, it lets 32% of the mandrill
through ((255 - 175) / 255 = 32%) with the rest being black.
@Figure{
@RLEPicture(Size=3 inch, PostScript=[pics/dart_shadow_and_monkey.ps])
@caption[@b(a:) Dart shadow mask. @b(b:) Resulting skewered baboon.]
@tag(DartShadow)
@tag(stuck_monkey)
}

Now all we have left to do is to composite the dart and the shadow
over the mandrill image, and the result is figure @ref(stuck_monkey)b.
Note that since the original dart image was properly anti-aliased, no aliasing
artifacts were introduced in the final image.

@subsection( Cutting holes )

In this example we start out out with a single bullet hole, created
with the same modelling package that produced the
dart.@footnote(The bullet hole was modeled with cubic B-splines.  
Normal people probably would have painted something like this...)  
First the hole (which originally filled the screen) is 
downfiltered to a small size, with
@programExample[
avg4 < big_hole.rle | avg4 | avg4 > smallhole.rle
]
@Figure{
@RLEPicture(Size=3 inch, PostScript=[pics/bullet_holes_and_shot_turb.ps])
@caption[@b(a:) A set of bullet holes. @b(b:) A shot-up turbine blade.]
@tag(blade_with_holes)
@tag(multiple_holes)
}

Now by invoking @f[repos] and @f[comp] several times in a row, 
a set of shots is built up (Figure @ref(multiple_holes)a)

To shoot up the turbine blade, the @i[atop] operator of comp is used.  This
works much like @i[over], except the bullet holes outside of the turbine
blade don't appear in the resulting image (Figure
@ref(blade_with_holes)b)  Note how the top left corner is just grazed.

But there's still one catch.  Suppose we want to display the shot-up
turbine blade over a background of some sort.  We want to be see the background
through the holes.  To do this we come up with another mask to cut
away the centers of the bullet holes already on the turbine blade so
we can see through them (Figure @ref(hole_center_masks)a).
@Figure{
@RLEPicture(Size=3 inch, PostScript=[pics/center_masks_and_fin_turb.ps])
@caption[@b(a:) Cut-away masks for the bullet hole centers. 
@b(b:) Finished turbine blade]
@tag(blade_with_background)
@tag(hole_center_masks)
}
Now we can see through the blade (Figure @ref(blade_with_background)b).

@subsection( Integrating digitized data )
In this example, we take a raw digitized negative and turn it into a useful
frame buffer image.  Figure @ref(raw_pahriah)a shows the original image from 
the scanner.  First the image is cropped to remove the excess digitized 
portion, then rotated into place with @f(flip -r), resulting in Figure 
@ref(raw_pahriah)b.

@Figure{
@RLEPicture(Size=3 inch, PostScript=[pics/scanned_and_cropped.ps])
@caption[@b(a:) The raw digitized negative.  @b(b:) Cropped and rotated image]
@tag(raw_pahriah)
}
To get the image to appear as a positive on the frame buffer, 
a negative linear color map (stored as text file) is loaded, then this is
composed with a gamma correction map of 2.2, and finally applied to the
pixels with the command:
@programExample[
ldmap -f neg.cmap < neg_pahriah.rle | ldmap -a -g 2.2 \
   | applymap > pahriah.rle
]
The result, Figure @ref(pahriah_final) now appears correct with a linear
color map loaded.

@Figure{
@RLEPicture(Size=3 inch, PostScript=[pics/pahriah_final.ps])
@caption[Final image of the old Pahriah ghost town]
@tag(pahriah_final)
}

@section(Future Work)
Needless to say, a project such as the Raster Toolkit is open ended in
nature, and new tools are easily added.  For example, most of the
tools we have developed to date deal with image synthesis and
composition, primarily because that is our research orientation.
Additional tools could be added to assist work in areas such as vision
research or image processing (for examples, see @cite(burt, stockham)).

Another interesting application would be a visual "shell" for invoking
the tools.  Currently, arguments to programs like @F[crop] and
@F[repos] are specified by tediously finding the numbers with the
frame buffer cursor and then typing them into a shell.  The visual
shell would allow the various toolkit operations to be interactively selected
from a menu.  Such a shell could probably work directly with an
existing workstation window system such as X @cite(Xwin) to provide
a friendly environment for using the toolkit.

@section(Conclusions)
@comment{THIS IS IRRELEVANT?
The operations provided by the Raster Toolkit have been around for many
years, and indeed some systems (such as the Pixar Image computer, the
Quantel Paintbox, etc) implement them at full video rates using
special purpose hardware.}
The Raster Toolkit provides a flexible, simple and easily extended set
of tools for anybody working with images in a Unix-based environment.
We have ported the toolkit to a wide variety of Unix systems, including
Gould UTX, HP-UX, Sun Unix, Apollo Domain/IX, 4.2 and 4.3 BSD, etc.
The common interfaces of the RLE format and the subroutine library
make it easy to interface the toolkit to a wide variety of image sources
and displays.

@newpage
@section(Acknowledgments)
This work was supported in part by the National Science Foundation
(DCR-8203692 and DCR-8121750), the Defense Advanced Research Projects Agency
(DAAK11-84-K-0017), the Army Research Office (DAAG29-81-K-0111), and
the Office of Naval Research (N00014-82-K-0351).
All opinions, findings, conclusions or recommendations expressed in this
document are those of the authors and do not necessarily reflect the views of
the sponsoring agencies.
