import sys
def printf(format, *args):
        sys.stdout.write(format % args)

string = 'CE ED 66 66 CC 0D 00 0B 03 73 00 83 00 0C 00 0D 00 08 11 1F 88 89 00 0E DC CC 6E E6 DD DD D9 99 BB BB 67 63 6E 0E EC CC DD DC 99 9F BB B9 33 3E'.replace(' ','')
rstring = '3C42B9A5B9A5423C'
#print string
hex2bin = ['    ','   1','  1 ','  11',' 1  ',' 1 1',' 11 ',' 111','1   ','1  1','1 1 ','1 11','11  ','11 1','111 ','1111']
index=0
out=[[0 for i in range(12)] for i in range(8)]
for majrow in [0,1]:
    for col in xrange(0,12):
        for minrow in [0,1,2,3]:
            pstr = hex2bin[int(string[majrow*48+col*4+minrow],16)]
            out[majrow*4+minrow][col] = pstr

rrow=0
for row in out:
    printf("%s%s", "".join(row).replace('1','1111').replace(' ','    '),'    ')
    if rrow < 16:
        print hex2bin[int(rstring[rrow],16)],hex2bin[int(rstring[rrow+1],16)]
    printf("%s%s", "".join(row).replace('1','1111').replace(' ','    '),'    ')
    if rrow < 16:
        printf("%s%s\n", hex2bin[int(rstring[rrow+2],16)],hex2bin[int(rstring[rrow+3],16)])
    else:
        printf("         \n")
    rrow+=4





rstring = '3C 42 B9 A5 B9 A5 42 3C'

00111100
01000010
10111001

