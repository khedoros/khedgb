#include<stdio.h>

char ds[][4] =    {   "000",    "001", "010",    "011", "100", "101", "110","111"};
char r[][5]  =    {     "B",      "C",   "D",      "E",   "H",   "L","(HL)",  "A"};
char rp[][3] =    {    "BC",     "DE",  "HL",     "SP"};
char rp2[][3]=    {    "BC",     "DE",  "HL",     "AF"};
char cc[][3] =    {    "NZ",      "Z",  "NC",      "C",  "PO",  "PE",   "P",  "M"};
char alu[][7]=    {"ADD A,", "ADC A,", "SUB", "SBC A,", "AND", "XOR",  "OR", "CP"};
char rot[][5]=    {   "RLC",    "RRC",  "RL",     "RR", "SLA", "SRA", "SWAP","SRL"};

void decode(int pre,int x,int y,int z,int data);

#ifdef STAND_ALONE_DECODE
int main() {
	int x;
	int y;
	int z;
	printf("Z80 instruction decoding\n------------------------\n\nNo Prefix:\n");
	for(x=0;x<4;x++) {
		for(y=0;y<8;y++) {
			for(z=0;z<8;z++) {
				printf("%s%s%sb 0%d%d%d %03d 0x%02x: ",
                                        ds[x],ds[y],ds[z],
                                               x,y,z,
                                                      x*64+y*8+z,
                                                         x*64+y*8+z);
				decode(0,x,y,z,0);
			}
		}
	}
	printf("\n\nPrefix 0xCB:\n");
	for(x=0;x<4;x++) {
		for(y=0;y<8;y++) {
			for(z=0;z<8;z++) {
				printf("%s%s%sb 0%d%d%d %03d 0x%02x: ",
                                        ds[x],ds[y],ds[z],
                                               x,y,z,
                                                      x*64+y*8+z,
                                                         x*64+y*8+z);
				decode(0xcb,x,y,z,0);
			}
		}
	}
	return 0;
}
#endif

void decode(int prefix,int x,int y,int z,int data) {
	int p,q;
	p=y/2;
	q=y%2;
	if(!prefix) {
		if(x==0) {
			switch(z) {
			case 0x0:
				switch(y) {
				case 0x0:
					printf("NOP\n");
					break;
				case 0x1:
					//Different than Z80
					printf("LD (a16), SP (diff)\n");
					break;
				case 0x2:
					//Different than Z80
					printf("STOP 0 (diff)\n");
					break;
				case 0x3:
					printf("JR r8\n");
					break;
				default: /* 4..7 */
					printf("JR %s, r8\n", cc[y-4]);
					break;
				}
				break;
			case 0x1:
				if(!q) {
					printf("LD %s, nn\n",rp[p]);
				}
				else {
					printf("ADD HL, %s\n",rp[p]);
				}
				break;
			case 0x2:
				switch(p) {
				case 0x0:
					if(!q) {
						printf("LD (BC), A\n");
					}
					else {
						printf("LD A, (BC)\n");
					}
					break;
				case 0x1:
					if(!q) {
						printf("LD (DE), A\n");
					}
					else {
						printf("LD A, (DE)\n");
					}
					break;
				case 0x2:
					if(!q) {
						//Different than z80
						//printf("LD (nn), HL\n");
						printf("LDI (HL), A (diff)\n");
					}
					else {
						//Different than z80
						//printf("LD HL, (nn)\n");
						printf("LDI A, (HL) (diff)\n");
					}
					break;
				case 0x3:
					if(!q) {
						//Different than z80
						//printf("LD (nn), A\n");
						printf("LDD (HL), A (diff)\n");
					}
					else {
						//Different than z80
						//printf("LD A, (nn)\n");
						printf("LDD A, (HL) (diff)\n");
					}
					break;
				}
				break;
			case 0x3:
				if(!q) {
					printf("INC %s\n", rp[p]);
				}
				else {
					printf("DEC %s\n", rp[p]);
				}
				break;
			case 0x4:
				printf("INC %s\n", r[y]);
				break;
			case 0x5:
				printf("DEC %s\n", r[y]);
				break;
			case 0x6:
				printf("LD %s, n\n", r[y]);
				break;
			case 0x7:
				switch(y) {
				case 0x0:
					printf("RLCA\n");
					break;
				case 0x1:
					printf("RRCA\n");
					break;
				case 0x2:
					printf("RLA\n");
					break;
				case 0x3:
					printf("RRA\n");
					break;
				case 0x4:
					printf("DAA\n");
					break;
				case 0x5:
					printf("CPL\n");
					break;
				case 0x6:
					printf("SCF\n");
					break;
				case 0x7:
					printf("CCF\n");
					break;
				}
				break;
			}
		}
		else if(x==1) {
			if(y==6 && z==6) {
				printf("HALT\n");
			}
			else {
				printf("LD %s, %s\n", r[y], r[z]);
			}
		}
		else if(x==2) {
			printf("%s %s\n", alu[y], r[z]);
		}
		else if(x==3) {
			switch(z) {
			case 0x0:
				switch(y) {
				case 4:
					printf("LD (FF00+n), A (diff)\n");
					break;
				case 5:
					printf("ADD SP, dd (diff)\n");
					break;
				case 6:
					printf("LD A, (FF00+n) (diff)\n");
					break;
				case 7:
					printf("LD HL, SP+dd (diff)\n");
					break;
				default:
					printf("RET %s\n", cc[y]);
					break;
				}
				break;
			case 0x1:
				if(!q) {
					printf("POP %s\n", rp2[p]);
				}
				else {
					switch(p) {
					case 0x0:
						printf("RET\n");
						break;
					case 0x1: //Different from z80
						//printf("EXX\n");
						printf("RETI (diff)\n");
						break;
					case 0x2:
						printf("JP HL\n");
						break;
					case 0x3:
						printf("LD SP, HL\n");
						break;
					}
				}
				break;
			case 0x2:
				switch(y) {
				case 0x4:
					printf("LD (FF00+C), A (diff)\n");
					break;
				case 0x5:
					printf("LD (nn), A (diff)\n");
					break;
				case 0x6:
					printf("LD A, (FF00+C) (diff)\n");
					break;
				case 0x7:
					printf("LD A, (nn) (diff)\n");
					break;
				default:
					printf("JP %s, nn\n", cc[y]);
					break;
				}
				break;
			case 0x3:
				switch(y) {
				case 0x0:
					printf("JP nn\n");
					break;
				case 0x1:
					printf("CB Prefix\n");
					break;
				case 0x2: //Different from x80
					//printf("OUT (n), A\n");
					printf("---- (diff)\n");
					break;
				case 0x3:
					//printf("IN A, (n)\n");
					printf("---- (diff)\n");
					break;
				case 0x4: //Different from z80
					//printf("EX (SP), HL\n");
					printf("---- (diff)\n");
					break;
				case 0x5: //Different from z80
					//printf("EX DE, HL\n");
					printf("---- (diff)\n");
					break;
				case 0x6:
					printf("X: %d Y: %d Z: %d ",x,y,z);
					printf("DI\n");
					break;
				case 0x7:
					printf("EI\n");
					break;
				}
				break;
			case 0x4:
				if(y<4) {
					printf("CALL %s, nn\n", cc[y]);
				}
				else {
					printf("---- (diff)\n");
				}
				break;
			case 0x5:
				if(!q) {
					printf("PUSH %s\n", rp2[p]);
				}
				else {
					switch(p) {
					case 0x0:
						printf("CALL nn\n");
						break;
					case 0x1: //Different from z80
						//printf("DD Prefix\n");
						printf("---- (diff)\n");
						break;
					case 0x2: //Different from z80
						//printf("ED Prefix\n");
						printf("---- (diff)\n");
						break;
					case 0x3: //Different from z80
						//printf("FD Prefix\n");
						printf("---- (diff)\n");
						break;
					}
				}
				break;
			case 0x6:
				printf("%s n\n", alu[y]);
				break;
			case 0x7:
				printf("RST %02X\n", y*8);
				break;
			}
		}
		//printf("\n");
	}
	else if(prefix==0xCB) {
		if(!x) {
			printf("%s %s %s\n", rot[y], r[z], (y==6)?"(diff)":"");
		}
		else if(x==1) {
			printf("BIT %02x, %s\n", y, r[z]);
		}
		else if(x==2) {
			printf("RES %02x, %s\n", y, r[z]);
		}
		else if(x==3) {
			printf("SET %02x, %s\n", y, r[z]);
		}
	}
	else {
		printf("Prefix \"0x%02x\" doesn't exist in the Game Boy's CPU.\n", prefix);
	}
}
