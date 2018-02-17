#include<iostream>
#include<fstream>
#include<cstdint>
#include<cstdio>
#include<cassert>

using namespace std;

int main(int argc, char *argv[]) {
    ifstream in1;
    ifstream in2;
    ifstream in3;
    if(argc == 4) {
        printf("Opening %s and %s as tile data, %s as bg and pal data.\n", argv[1], argv[2], argv[3]);
        in1.open(argv[1]);
        in2.open(argv[2]);
        in3.open(argv[3]);
    }
    if(!in1.is_open() || !in2.is_open() || ! in3.is_open()) {
        cout<<"Fail."<<endl;
        return 1;
    }
    uint8_t raw_tiles[0x2000] = {0};
    uint16_t bgmap[0x400] = {0};
    uint16_t pals[0x40] = {0};
    uint8_t tiles[8*8*256] = {0};
    in1.read(reinterpret_cast<char *>(&raw_tiles[0]), 0x1000);
    in1.close();
    in2.read(reinterpret_cast<char *>(&raw_tiles[0x1000]), 0x1000);
    in2.close();
    in3.read(reinterpret_cast<char *>(&bgmap[0]), 0x800);
    //in3.read(reinterpret_cast<char *>(&bgmap[0]), 0x800);
    //in3.read(reinterpret_cast<char *>(&pals[0]), 0x80);
    in3.close();

    for(int i=0;i<0x40;i++) {
        pals[i]=i;
    }
    //Decode tile data into tiles array, and print decoded images
    for(int tile=0;tile<256;tile++) {
        printf("Tile %d\n", tile);
        int offset = tile * 32;
        for(int line = 0; line < 8; line++) {
            offset += line * 2;
            uint8_t byte0 = raw_tiles[offset];
            uint8_t byte1 = raw_tiles[offset+1];
            uint8_t byte2 = raw_tiles[offset+16];
            uint8_t byte3 = raw_tiles[offset+17];
            for(int pix = 0;pix<8;pix++) {
                uint8_t col = (byte0&1) | 2*(byte1&1) | 4*(byte2&1) | 8*(byte3&1);
                byte0>>=1;
                byte1>>=1;
                byte2>>=1;
                byte3>>=1;
                printf("%01x%01x", col, col);
                //of<<int(col)<<" ";
                tiles[64*tile+8*line+pix] = col;
            }
            printf("\n");
            //of<<endl;
        }
        printf("\n");
    }

    //print out raw bgmap data
    for(int i=0;i<32;i++) {
        for(int j=0;j<32;j++)
            printf("%04x ", bgmap[i*32+j]);
        printf("\n");
    }

    //print out palette data
    for(int i=0;i<4;i++) {
        for(int j=0;j<16;j++) {
            printf("%04x ", pals[i*16+j]);
        }
        printf("\n");
    }

    //try to output the screen
    ofstream of("output.pgm");
    of<<"P3\n256 256\n31\n";
    for(int yt = 0; yt < 32; yt++) {
        for(int yp = 0; yp < 8; yp++) {
            for(int xt = 0; xt < 32; xt++) {
                int bg_attrib = bgmap[yt*32+xt];
                int tile = bg_attrib&0x3ff;
                printf("(%d, %d, tile: %d, pal: ", yt, xt, tile);
                tile&=0xff; //quell misbehavior
                int pal = ((bg_attrib&0x1c00)>>10);
                printf("%d\n", pal);
                if(pal<0) pal = 0;
                if(pal>3) pal=3;
                //assert(tile>=0);
                //assert(tile<256);
                //assert(pal>=0);
                //assert(pal<4);
                //printf("%04x ", tile);
                for(int xp=0;xp<8;xp++) {
                   int palindex = tiles[64*tile+8*yp+xp];
                    int color = pals[pal*16+palindex];
                    int r = (color & 0x1f);
                    color>>=5;
                    int g = (color & 0x1f);
                    color>>=5;
                    int b = (color & 0x1f);
                    of<<r<<" "<<g<<" "<<b<<" ";
                }
            }
            //printf("\n");
            of<<endl;
        }
    }

    of.close();
    return 0;
}
