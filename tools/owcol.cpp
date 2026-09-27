#include "generator.hpp"
#include "blocks.hpp"
#include "specs.hpp"
#include <cstdio>
int main(){ static uint8_t b[CHUNK_VOL]; gen::generateColumn(1337,0,0,b);
 int lx=8,lz=8; printf("ow col(8,8): "); int y=0;
 while(y<WORLD_HEIGHT){ uint8_t v=b[lx+(lz<<4)+(y<<8)]; int y2=y;
   while(y2+1<WORLD_HEIGHT && b[lx+(lz<<4)+((y2+1)<<8)]==v) y2++;
   printf("[%d-%d]%s ",y,y2,BLOCK_DEFS[v].name); y=y2+1; }
 printf("\n"); return 0; }
