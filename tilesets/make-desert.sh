#! /bin/bash

IMAGEW=`identify -format "%w" desert_tileset.bmp`
IMAGEH=`identify -format "%h" desert_tileset.bmp`
convert desert_tileset2.tga desert.rgba
printf "tilew 8 tileh 16\n" ${IMAGEW} ${IMAGEH} > desert.header
cat desert.header data desert.rgba > desert.tile
cp desert.tile ../tiles

