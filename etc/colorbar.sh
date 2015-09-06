#!/bin/sh
scale=${1-1.0}
value=`echo $scale|awk '{print int(255 * $1)}'`
rlebg      0      0      0 -s 96 256 > 0
rlebg      0      0 $value -s 96 256 | repos -P  96 0 | rlecomp - over 0 > 1 ; rm 0
rlebg      0 $value      0 -s 96 256 | repos -P 192 0 | rlecomp - over 1 > 2 ; rm 1
rlebg      0 $value $value -s 96 256 | repos -P 288 0 | rlecomp - over 2 > 3 ; rm 2
rlebg $value      0      0 -s 96 256 | repos -P 384 0 | rlecomp - over 3 > 4 ; rm 3
rlebg $value      0 $value -s 96 256 | repos -P 480 0 | rlecomp - over 4 > 5 ; rm 4
rlebg $value $value      0 -s 96 256 | repos -P 576 0 | rlecomp - over 5 > 6 ; rm 5
rlebg $value $value $value -s 96 256 | repos -P 672 0 | rlecomp - over 6 > bars-$scale.rle
rm 6
