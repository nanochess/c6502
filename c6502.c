/*
 ** 6502 to IntyBASIC compiler
 **
 ** by Oscar Toledo G.
 **
 ** Creation date: Jul/03/2017.
 ** Revision date: Aug/09/2017. Avoids calculating unnecessary flags.
 */

#include <stdio.h>
#include <stdlib.h>

#define R(addr) rom[(addr) & 0x0fff]
#define C(addr) checked[(addr) & 0x0fff]

typedef unsigned char byte;

byte rom[4096];

/*
 ** bit 1-0 = step of process where it passed (it means code)
 ** bit 2 = 1 = label
 ** bit 3 = recalculates n
 ** bit 4 = recalculates z
 ** bit 5 = recalculates c
 ** bit 6 = recalculates v
 */
byte checked[4096];

#define LABEL  0x04
#define REN    0x08    /* Recalculates N */
#define REZ    0x10    /* Recalculates Z */
#define REC    0x20    /* Recalculates C */
#define REV    0x40    /* Recalculates V */
#define USC    0x80    /* Uses C */

int decimal;

FILE *output;
int step;

#define RA     1
#define RX     2
#define RY     3
#define RS     4

int has_nz;

/*
 ** Avoid store instructions
 */
int avoid(int address)
{
    while (1) {
        switch (R(address)) {
            case 0x84:
            case 0x94:
            case 0x85:
            case 0x95:
            case 0x86:
            case 0x96:
                address += 2;
                continue;
            case 0x99:
            case 0x8d:
                address += 3;
                continue;
            default:
                return address;
        }
    }
}

/*
 ** Analyze 6502 code
 */
void analyze(int address)
{
    int calc;
    
    while ((C(address) & 3) != step) {
        if (C(address) & LABEL) {
            if (step == 2)
                fprintf(output, "L%04X:\n", address);
        }
        if (step == 2) {
            if ((C(address) & 3) == 0) {
                fprintf(output, "\tDATA $%02X\n", R(address));
                address++;
                continue;
            }
        }
        C(address) = (C(address) & ~3) | step;
        switch (R(address)) {
            case 0x10:  /* BPL rel */
                address++;
                calc = R(address);
                if (calc >= 128)
                    calc -= 256;
                calc += address + 1;
                if (step == 1) {
                    C(calc) |= LABEL;
                    analyze(calc);
                } else {
                    fprintf(output, "\tIF n = 0 THEN GOTO L%04X\n", calc);
                }
                address++;
                break;
            case 0x30:  /* BMI rel */
                address++;
                calc = R(address);
                if (calc >= 128)
                    calc -= 256;
                calc += address + 1;
                if (step == 1) {
                    C(calc) |= LABEL;
                    analyze(calc);
                } else {
                    fprintf(output, "\tIF n THEN GOTO L%04X\n", calc);
                }
                address++;
                break;
            case 0x50:  /* BVC rel */
                address++;
                calc = R(address);
                if (calc >= 128)
                    calc -= 256;
                calc += address + 1;
                if (step == 1) {
                    C(calc) |= LABEL;
                    analyze(calc);
                } else {
                    fprintf(output, "\tIF v = 0 THEN GOTO L%04X\n", calc);
                }
                address++;
                break;
            case 0x60:  /* RTS */
                if (step == 1) {
                    return;
                } else {
                    fprintf(output, "\tRETURN\n");
                }
                address++;
                break;
            case 0x90:  /* BCC rel */
                address++;
                calc = R(address);
                if (calc >= 128)
                    calc -= 256;
                calc += address + 1;
                if (step == 1) {
                    C(calc) |= LABEL;
                    analyze(calc);
                } else {
                    fprintf(output, "\tIF c = 0 THEN GOTO L%04X\n", calc);
                }
                address++;
                break;
            case 0xa0:  /* LDY #imm */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ty = $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = y AND $80:z = y = 0\n");
                }
                address++;
                break;
            case 0xb0:  /* BCS rel */
                address++;
                calc = R(address);
                if (calc >= 128)
                    calc -= 256;
                calc += address + 1;
                if (step == 1) {
                    C(calc) |= LABEL;
                    analyze(calc);
                } else {
                    fprintf(output, "\tIF c THEN GOTO L%04X\n", calc);
                }
                address++;
                break;
            case 0xc0:  /* CPY #imm */
                C(address) |= REZ | REN | REC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = y - $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = #t AND $80:z = (#t AND 255) = 0\n");
                    fprintf(output, "\tc = (#t / 256) = 0\n");
                }
                address++;
                break;
            case 0xd0:  /* BNE rel */
                address++;
                calc = R(address);
                if (calc >= 128)
                    calc -= 256;
                calc += address + 1;
                if (step == 1) {
                    C(calc) |= LABEL;
                    analyze(calc);
                } else {
                    fprintf(output, "\tIF z = 0 THEN GOTO L%04X\n", calc);
                }
                address++;
                break;
            case 0xe0:  /* CPX #imm */
                C(address) |= REZ | REN | REC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = x - $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = #t AND $80:z = (#t AND 255) = 0\n");
                    fprintf(output, "\tc = (#t / 256) = 0\n");
                }
                address++;
                break;
            case 0xf0:  /* BEQ rel */
                address++;
                calc = R(address);
                if (calc >= 128)
                    calc -= 256;
                calc += address + 1;
                if (step == 1) {
                    C(calc) |= LABEL;
                    analyze(calc);
                } else {
                    fprintf(output, "\tIF z THEN GOTO L%04X\n", calc);
                }
                address++;
                break;
            case 0x20:  /* JSR abs */
                address++;
                if (step == 1) {
                    C(R(address) | R(address + 1) << 8) |= LABEL;
                    analyze(R(address) | R(address + 1) << 8);
                } else {
                    fprintf(output, "\tGOSUB L%04X\n", R(address) | R(address + 1) << 8);
                }
                address += 2;
                break;
            case 0xb1:  /* LDA ind,Y */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = zp(zp($%02X) + y)\t' !!!\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0xa2:  /* LDX #imm */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\tx = $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = x AND $80:z = x = 0\n");
                }
                address++;
                break;
            case 0x24:  /* BIT zpg */
                C(address) |= REZ | REN | REV;
                address++;
                if (step == 2) {
                    if ((C(avoid(address + 1)) & (REN)) != (REN))
                        fprintf(output, "\tn = zp($%02X) AND $80\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ)) != (REZ))
                        fprintf(output, "\tz = (zp($%02X) AND a) = 0\n", R(address));
                    fprintf(output, "\tv = zp($%02X) AND $40\n", R(address));
                }
                address++;
                break;
            case 0x84:  /* STY zpg */
                address++;
                if (step == 2) {
                    fprintf(output, "\tzp($%02X) = y\n", R(address));
                }
                address++;
                break;
            case 0x94:  /* STY zpg,x */
                address++;
                if (step == 2) {
                    fprintf(output, "\tzp($%02X + x) = y\n", R(address));
                }
                address++;
                break;
            case 0xa4:  /* LDY zpg */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ty = zp($%02X)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = y AND $80:z = y = 0\n");
                }
                address++;
                break;
            case 0xb4:  /* LDY zpg,x */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ty = zp($%02X + x)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = y AND $80:z = y = 0\n");
                }
                address++;
                break;
            case 0x05:  /* ORA zpg */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a OR zp($%02X)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x15:  /* ORA zpg,x */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a OR zp($%02X + x)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x25:  /* AND zpg */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a AND zp($%02X)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x35:  /* AND zpg,x */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a AND zp($%02X + x)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x45:  /* EOR zpg */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a XOR zp($%02X)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x55:  /* EOR zpg,x */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a XOR zp($%02X + x)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x65:  /* ADC zpg */
                C(address) |= REZ | REN | REC | USC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a + zp($%02X) + c\n", R(address));
                    fprintf(output, "\ta = #t\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                    fprintf(output, "\tc = #t / 256\n");
                }
                address++;
                break;
            case 0x75:  /* ADC zpg,x */
                C(address) |= REZ | REN | REC | USC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a + zp($%02X + x) + c\n", R(address));
                    fprintf(output, "\ta = #t\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                    fprintf(output, "\tc = #t / 256\n");
                }
                address++;
                break;
            case 0x85:  /* STA zpg */
                address++;
                if (step == 2) {
                    fprintf(output, "\tzp($%02X) = a\n", R(address));
                }
                address++;
                break;
            case 0x95:  /* STA zpg,X */
                address++;
                if (step == 2)
                    fprintf(output, "\tzp(x + $%02X) = a\n", R(address));
                address++;
                break;
            case 0xa5:  /* LDA zpg */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = zp($%02X)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0xb5:  /* LDA zpg,X */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = zp($%02X + x)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0xc5:  /* CMP zpg */
                C(address) |= REZ | REN | REC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a - zp($%02X)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = #t AND $80:z = (#t AND 255) = 0\n");
                    fprintf(output, "\tc = (#t / 256) = 0\n");
                }
                address++;
                break;
            case 0xd5:  /* CMP zpg,x */
                C(address) |= REZ | REN | REC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a - zp($%02X + x)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = #t AND $80:z = (#t AND 255) = 0\n");
                    fprintf(output, "\tc = (#t / 256) = 0\n");
                }
                address++;
                break;
            case 0xe5:  /* SBC zpg */
                C(address) |= REZ | REN | REC | USC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a + (zp($%02X) XOR $FF) + c\n", R(address));
                    fprintf(output, "\ta = #t\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                    fprintf(output, "\tc = #t / 256\n");
                }
                address++;
                break;
            case 0xf5:  /* SBC zpg,x */
                C(address) |= REZ | REN | REC | USC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a + (zp($%02X + x) XOR $FF) + c\n", R(address));
                    fprintf(output, "\ta = #t\n");
                    fprintf(output, "\tn = a AND $80:z = a = 0\n");
                    fprintf(output, "\tc = #t / 256\n");
                }
                address++;
                break;
            case 0x06:  /* ASL zpg */
                C(address) |= REZ | REC;
                address++;
                if (step == 2) {
                    if ((C(avoid(address + 1)) & (REC | USC)) != (REC))
                        fprintf(output, "\tc = zp($%02X) AND 128\n", R(address));
                    fprintf(output, "\tzp($%02X) = zp($%02X) * 2\n", R(address), R(address));
                    if ((C(avoid(address + 1)) & (REZ)) != (REZ))
                        fprintf(output, "\tz = zp($%02X) = 0\n", R(address));
                }
                address++;
                break;
            case 0x26:  /* ROL zpg */
                C(address) |= REZ | REC | USC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = c\n");
                    fprintf(output, "\tc = zp($%02X) AND $80\n", R(address));
                    fprintf(output, "\tzp($%02X) = zp($%02X) * 2\n", R(address), R(address));
                    if ((C(avoid(address + 1)) & (REZ)) != (REZ))
                        fprintf(output, "\tz = zp($%02X) = 0\n", R(address));
                }
                address++;
                break;
            case 0x46:  /* LSR zpg */
                C(address) |= REZ | REC;
                address++;
                if (step == 2) {
                    if ((C(avoid(address + 1)) & (REC | USC)) != (REC))
                        fprintf(output, "\tc = zp($%02X) AND 1\n", R(address));
                    fprintf(output, "\tzp($%02X) = zp($%02X) / 2\n", R(address), R(address));
                    if ((C(avoid(address + 1)) & (REZ)) != (REZ))
                        fprintf(output, "\tz = zp($%02X) = 0\n", R(address));
                }
                address++;
                break;
            case 0x66:  /* ROR zpg */
                C(address) |= REZ | REC | USC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = c\n");
                    fprintf(output, "\tc = zp($%02X) AND 1\n", R(address));
                    fprintf(output, "\tzp($%02X) = zp($%02X) / 2 + #t * 128\n", R(address), R(address));
                    if ((C(avoid(address + 1)) & (REZ)) != (REZ))
                        fprintf(output, "\tz = zp($%02X) = 0\n", R(address));
                }
                address++;
                break;
            case 0x86:  /* STX zpg */
                address++;
                if (step == 2) {
                    fprintf(output, "\tzp($%02X) = x\n", R(address));
                }
                address++;
                break;
            case 0x96:  /* STX zpg,y */
                address++;
                if (step == 2) {
                    fprintf(output, "\tzp($%02X + y) = x\n", R(address));
                }
                address++;
                break;
            case 0xa6:  /* LDX zpg */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\tx = zp($%02X)\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = x AND $80:z = x = 0\n");
                }
                address++;
                break;
            case 0xc6:  /* DEC zpg */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\tzp($%02X) = zp($%02X) - 1\n", R(address), R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = zp($%02X) AND $80:z = zp($%02X) = 0\n", R(address), R(address));
                }
                address++;
                break;
            case 0xe6:  /* INC zpg */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\tzp($%02X) = zp($%02X) + 1\n", R(address), R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = zp($%02X) AND $80:z = zp($%02X) = 0\n", R(address), R(address));
                }
                address++;
                break;
            case 0xf6:  /* INC zpg,x */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\tzp($%02X + x) = zp($%02X + x) + 1\n", R(address), R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = zp($%02X + x) AND $80:z = zp($%02X + x) = 0\n", R(address), R(address));
                }
                address++;
                break;
            case 0x18:  /* CLC */
                C(address) |= REC;
                if (step == 2)
                    fprintf(output, "\tc = 0\n");
                address++;
                break;
            case 0x38:  /* SEC */
                C(address) |= REC;
                if (step == 2)
                    fprintf(output, "\tc = 1\n");
                address++;
                break;
            case 0x78:  /* SEI */
                if (step == 2)
                    fprintf(output, "\t' SEI\n");
                address++;
                break;
            case 0x88:  /* DEY */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\ty = y - 1\n");
                    fprintf(output, "\tn = y AND $80:z = y = 0\n");
                }
                address++;
                break;
            case 0x98:  /* TYA */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\ta = y\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0xa8:  /* TAY */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\ty = a\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = y AND $80:z = y = 0\n");
                }
                address++;
                break;
            case 0xc8:  /* INY */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\ty = y + 1\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = y AND $80:z = y = 0\n");
                }
                address++;
                break;
            case 0xd8:  /* CLD */
                if (step == 2)
                    fprintf(output, "\t' Entering binary mode\n");
                decimal = 0;
                address++;
                break;
            case 0xe8:  /* INX */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\tx = x + 1\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = x AND $80:z = x = 0\n");
                }
                address++;
                break;
            case 0xf8:  /* SED */
                if (step == 2)
                    fprintf(output, "\t' Entering decimal mode\n");
                decimal = 1;
                address++;
                break;
            case 0x09:  /* ORA #imm */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a OR $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x29:  /* AND #imm */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a AND $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x39:  /* AND abs,y */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\ta = a AND L%04X(y)\n", calc);
                    if ((C(avoid(address + 2)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address += 2;
                break;
            case 0x49:  /* EOR #imm */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = a XOR $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x69:  /* ADC #imm */
                C(address) |= REZ | REN | REC | USC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a + $%02X + c\n", R(address));
                    fprintf(output, "\ta = #t\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                    fprintf(output, "\tc = #t / 256\n");
                }
                address++;
                break;
            case 0x79:  /* ADC abs,y */
                C(address) |= REZ | REN | REC | USC;
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\t#t = a + L%04X(y) + c\n", calc);
                    fprintf(output, "\ta = #t\n");
                    if ((C(avoid(address + 2)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                    fprintf(output, "\tc = #t / 256\n");
                }
                address += 2;
                break;
            case 0x99:  /* STA abs,y */
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\tL%04X(y) = a\n", calc);
                }
                address += 2;
                break;
            case 0xa9:  /* LDA #imm */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    fprintf(output, "\ta = $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0xb9:  /* LDA abs,y */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\ta = L%04X(y)\n", calc);
                    if ((C(avoid(address + 2)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address += 2;
                break;
            case 0xc9:  /* CMP #imm */
                C(address) |= REZ | REN | REC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a - $%02X\n", R(address));
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = #t AND $80:z = (#t AND 255) = 0\n");
                    fprintf(output, "\tc = (#t / 256) = 0\n");
                }
                address++;
                break;
            case 0xe9:  /* SBC #imm */
                C(address) |= REZ | REN | REC | USC;
                address++;
                if (step == 2) {
                    fprintf(output, "\t#t = a + ($%02X XOR $FF) + c\n", R(address));
                    fprintf(output, "\ta = #t\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                    fprintf(output, "\tc = #t / 256\n");
                }
                address++;
                break;
            case 0x0a:  /* ASL */
                C(address) |= REZ | REC;
                if (step == 2) {
                    if ((C(avoid(address + 1)) & (REC | USC)) != (REC))
                        fprintf(output, "\tc = a AND 128\n");
                    fprintf(output, "\ta = a * 2\n");
                    if ((C(avoid(address + 1)) & (REZ)) != (REZ))
                        fprintf(output, "\tz = a = 0\n");
                }
                address++;
                break;
            case 0x2a:  /* ROL */
                C(address) |= REZ | REC | USC;
                if (step == 2) {
                    fprintf(output, "\t#t = c\n");
                    fprintf(output, "\tc = a AND 1\n");
                    fprintf(output, "\ta = a * 2 + #t\n");
                    if ((C(avoid(address + 1)) & (REZ)) != (REZ))
                        fprintf(output, "\tz = a = 0\n");
                }
                address++;
                break;
            case 0x4a:  /* LSR */
                C(address) |= REZ | REC;
                if (step == 2) {
                    if ((C(avoid(address + 1)) & (REC | USC)) != (REC))
                        fprintf(output, "\tc = a AND 1\n");
                    fprintf(output, "\ta = a / 2\n");
                    if ((C(avoid(address + 1)) & (REZ)) != (REZ))
                        fprintf(output, "\tz = a = 0\n");
                }
                address++;
                break;
            case 0x8a:  /* TXA */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\ta = x\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address++;
                break;
            case 0x9a:  /* TXS */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\ts = x\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = x AND $80:z = x = 0\n");
                }
                address++;
                break;
            case 0xaa:  /* TAX */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\tx = a\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = x AND $80:z = x = 0\n");
                }
                address++;
                break;
            case 0xca:  /* DEX */
                C(address) |= REZ | REN;
                if (step == 2) {
                    fprintf(output, "\tx = x - 1\n");
                    if ((C(avoid(address + 1)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = x AND $80:z = x = 0\n");
                }
                address++;
                break;
            case 0xea:  /* NOP */
                if (step == 2) {
                    fprintf(output, "\t' NOP\n");
                }
                address++;
                break;
            case 0x4c:  /* JMP abs */
                address++;
                if (step == 1) {
                    C(R(address) | R(address + 1) << 8) |= LABEL;
                    analyze(R(address) | R(address + 1) << 8);
                    return;
                } else {
                    fprintf(output, "\tGOTO L%04X\n\n", R(address) | R(address + 1) << 8);
                }
                address += 2;
                break;
            case 0x7d:  /* ADC abs,x */
                C(address) |= REZ | REN | REC | USC;
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\t#t = a + L%04X(x) + c\n", calc);
                    fprintf(output, "\ta = #t\n");
                    if ((C(avoid(address + 2)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                    fprintf(output, "\tc = #t / 256\n");
                }
                address += 2;
                break;
            case 0x8d:  /* STA abs */
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\tL%04X(0) = a\n", calc);
                }
                address += 2;
                break;
            case 0xad:  /* LDA abs */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\ta = L%04X(0)\n", calc);
                    if ((C(avoid(address + 2)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address += 2;
                break;
            case 0xbd:  /* LDA abs,x */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\ta = L%04X(x)\n", calc);
                    if ((C(avoid(address + 2)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = a AND $80:z = a = 0\n");
                }
                address += 2;
                break;
            case 0xbe:  /* LDX abs,y */
                C(address) |= REZ | REN;
                address++;
                if (step == 2) {
                    calc = R(address) | R(address + 1) << 8;
                    C(calc) |= LABEL;
                    fprintf(output, "\tx = L%04X(y)\n", calc);
                    if ((C(avoid(address + 2)) & (REZ | REN)) != (REZ | REN))
                        fprintf(output, "\tn = x AND $80:z = x = 0\n");
                }
                address += 2;
                break;
            default:
                fprintf(stderr, "Unhandled opcode $%02x at $%04x\n", R(address), address);
                return;
        }
    }
}

/*
 ** Main program
 */
int main(int argc, char *argv[])
{
    FILE *input;
    int start;
    
    fprintf(stderr, "6502 to IntyBASIC compiler. http://nanochess.org/\n\n");
    if (argc != 3) {
        fprintf(stderr, "Usage:\n\n");
        fprintf(stderr, "    c6502 input.rom output.bas\n\n");
        fprintf(stderr, "Only 4K ROM supported and it will generate\n");
        fprintf(stderr, "non-working programs. Sorry :P\n\n");
        exit(1);
    }
    input = fopen(argv[1], "rb");
    if (input == NULL) {
        fprintf(stderr, "Failure to open input file: %s\n", argv[1]);
        exit(1);
    }
    output = fopen(argv[2], "w");
    if (output == NULL) {
        fclose(input);
        fprintf(stderr, "Failure to open output file: %s\n", argv[2]);
        exit(1);
    }
    if (fread(rom, 1, sizeof(rom), input) != sizeof(rom)) {
        fclose(input);
        fprintf(stderr, "Input ROM size isn't 4 kilobytes\n");
        exit(1);
    }
    fclose(input);
    start = rom[0x0ffc] | (rom[0x0ffd] << 8);
    fprintf(stderr, "Starting analysis at %04X\n...\n", start);
    step = 1;
    analyze(start);
    step = 2;
    analyze(start);
    fclose(output);
    exit(0);
}
