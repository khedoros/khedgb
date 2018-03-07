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
                                printf("\n");
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
                                printf("\n");
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
					printf("NOP");
					break;
				case 0x1:
					//Different than Z80
					printf("LD ($%04x), SP",data);
					break;
				case 0x2:
					//Different than Z80
					printf("STOP 0");
					break;
				case 0x3:
					printf("JR $%02x",data);
					break;
				default: /* 4..7 */
					printf("JR %s, $%02x", cc[y-4], data);
					break;
				}
				break;
			case 0x1:
				if(!q) {
					printf("LD %s, $%04x",rp[p],data);
				}
				else {
					printf("ADD HL, %s",rp[p]);
				}
				break;
			case 0x2:
				switch(p) {
				case 0x0:
					if(!q) {
						printf("LD (BC), A");
					}
					else {
						printf("LD A, (BC)");
					}
					break;
				case 0x1:
					if(!q) {
						printf("LD (DE), A");
					}
					else {
						printf("LD A, (DE)");
					}
					break;
				case 0x2:
					if(!q) {
						//Different than z80
						//printf("LD (nn), HL");
						printf("LDI (HL), A");
					}
					else {
						//Different than z80
						//printf("LD HL, (nn)");
						printf("LDI A, (HL)");
					}
					break;
				case 0x3:
					if(!q) {
						//Different than z80
						//printf("LD (nn), A");
						printf("LDD (HL), A");
					}
					else {
						//Different than z80
						//printf("LD A, (nn)");
						printf("LDD A, (HL)");
					}
					break;
				}
				break;
			case 0x3:
				if(!q) {
					printf("INC %s", rp[p]);
				}
				else {
					printf("DEC %s", rp[p]);
				}
				break;
			case 0x4:
				printf("INC %s", r[y]);
				break;
			case 0x5:
				printf("DEC %s", r[y]);
				break;
			case 0x6:
				printf("LD %s, $%02x", r[y],data);
				break;
			case 0x7:
				switch(y) {
				case 0x0:
					printf("RLCA");
					break;
				case 0x1:
					printf("RRCA");
					break;
				case 0x2:
					printf("RLA");
					break;
				case 0x3:
					printf("RRA");
					break;
				case 0x4:
					printf("DAA");
					break;
				case 0x5:
					printf("CPL");
					break;
				case 0x6:
					printf("SCF");
					break;
				case 0x7:
					printf("CCF");
					break;
				}
				break;
			}
		}
		else if(x==1) {
			if(y==6 && z==6) {
				printf("HALT");
			}
			else {
				printf("LD %s, %s", r[y], r[z]);
			}
		}
		else if(x==2) {
			printf("%s %s", alu[y], r[z]);
		}
		else if(x==3) {
			switch(z) {
			case 0x0:
				switch(y) {
				case 4:
					printf("LD (FF00+$%02x), A",data);
					break;
				case 5:
					printf("ADD SP, $%02x",data);
					break;
				case 6:
					printf("LD A, (FF00+$%02x)",data);
					break;
				case 7:
					printf("LD HL, SP+$%02x",data);
					break;
				default:
					printf("RET %s", cc[y]);
					break;
				}
				break;
			case 0x1:
				if(!q) {
					printf("POP %s", rp2[p]);
				}
				else {
					switch(p) {
					case 0x0:
						printf("RET");
						break;
					case 0x1: //Different from z80
						//printf("EXX");
						printf("RETI");
						break;
					case 0x2:
						printf("JP HL");
						break;
					case 0x3:
						printf("LD SP, HL");
						break;
					}
				}
				break;
			case 0x2:
				switch(y) {
				case 0x4:
					printf("LD (FF00+C), A");
					break;
				case 0x5:
					printf("LD ($%04x), A",data);
					break;
				case 0x6:
					printf("LD A, (FF00+C)");
					break;
				case 0x7:
					printf("LD A, ($%04x)",data);
					break;
				default:
					printf("JP %s, $%04x", cc[y],data);
					break;
				}
				break;
			case 0x3:
				switch(y) {
				case 0x0:
					printf("JP $%04x",data);
					break;
				case 0x1:
					printf("CB Prefix");
					break;
				case 0x2: //Different from x80
					//printf("OUT (n), A");
					printf("---- (diff)");
					break;
				case 0x3:
					//printf("IN A, (n)");
					printf("---- (diff)");
					break;
				case 0x4: //Different from z80
					//printf("EX (SP), HL");
					printf("---- (diff)");
					break;
				case 0x5: //Different from z80
					//printf("EX DE, HL");
					printf("---- (diff)");
					break;
				case 0x6:
					//printf("X: %d Y: %d Z: %d ",x,y,z);
					printf("DI");
					break;
				case 0x7:
					printf("EI");
					break;
				}
				break;
			case 0x4:
				if(y<4) {
					printf("CALL %s, $%04x", cc[y],data);
				}
				else {
					printf("---- (diff)");
				}
				break;
			case 0x5:
				if(!q) {
					printf("PUSH %s", rp2[p]);
				}
				else {
					switch(p) {
					case 0x0:
						printf("CALL $%04x",data);
						break;
					case 0x1: //Different from z80
						//printf("DD Prefix");
						printf("---- (diff)");
						break;
					case 0x2: //Different from z80
						//printf("ED Prefix");
						printf("---- (diff)");
						break;
					case 0x3: //Different from z80
						//printf("FD Prefix");
						printf("---- (diff)");
						break;
					}
				}
				break;
			case 0x6:
				printf("%s $%02x", alu[y],data);
				break;
			case 0x7:
				printf("RST %02X", y*8);
				break;
			}
		}
		//printf("");
	}
	else if(prefix==0xCB) {
		if(!x) {
			printf("%s %s %s", rot[y], r[z], (y==6)?"(diff)":"");
		}
		else if(x==1) {
			printf("BIT %02x, %s", y, r[z]);
		}
		else if(x==2) {
			printf("RES %02x, %s", y, r[z]);
		}
		else if(x==3) {
			printf("SET %02x, %s", y, r[z]);
		}
	}
	else {
		printf("Prefix \"0x%02x\" doesn't exist in the Game Boy's CPU.", prefix);
	}
}
