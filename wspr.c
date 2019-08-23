/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// This code was modified from WsprryPi project - https://github.com/DanAnkers/WsprryPi
// Encodes a Type 1 WSPR Message (6 char callsign, 4 char locator, power level in dbm (ex. 20))




#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void code_msg(char[], unsigned long int*, unsigned long int*);      // encode callsign, locator and power
void pack_msg(unsigned long int, unsigned long int, unsigned char[]);// packed 50 bits in 11 bytes
void generate_parity(unsigned char[], unsigned char[]);  // generate 162 parity bits
void interleave(unsigned char[], unsigned char[]);  // interleave the 162 parity bits
void synchronise(unsigned char[], unsigned char[]);  // synchronize with a pseudo random pattern

#define POLYNOM_1 0xf2d05351	// polynoms for
#define POLYNOM_2 0xe4613c47	// parity generator

/*
WSPR encoding module:
Thanks to K1JT, G4JNT and PE1NZZ for publishing
helping infos.

Encoding process is in 5 steps:
   * bits packing of user message in 50 bits
   * store the 50 bits dans 11 octets (88 bits and only 81 useful)
   * convolutionnal encoding with two pariy generators (-> 162 bits)
   * interleaving of the 162 bits with bit-reverse technique
   * synchronisation with a psudo-random vector to obtain the
      162 symbols defining one frequency of 4.

 F8CHK 29/03/2011                              
*/

void code_msg(char usr_message[], unsigned long int *N, unsigned long int *M) {
    unsigned long int n, m;
    unsigned int i, j, power, callsign_length;

    char callsign[7] = ""; // callsign string
    char locator[5] = ""; // locator string
    char power_str[3] = ""; // power string

    strcpy(callsign, "      ");	// filling with spaces

    i = 0;
    while (usr_message[i] != ' ') {
        callsign[i] = islower(usr_message[i])?toupper(usr_message[i]):usr_message[i];	// extract callsign
        i++;
    }
    callsign_length = i;

    i++;
    j = 0;
    while (usr_message[i] != ' ') {
        locator[j++] = islower(usr_message[i])?toupper(usr_message[i++]):usr_message[i++];	// extract locator
    }
    locator[j] = 0;

    i++;
    j = 0;
    while (usr_message[i] != 0) {
        power_str[j++] = usr_message[i++];	// extract power
    }
    power_str[j] = 0;

    power = atoi (power_str);	// power needs to be an integer

    printf("\nCall: %s / Locator: %s / Power: %ddBm\n\n", callsign, locator, power);

    // Place a space in first position if third character is not a digit
    if (!isdigit (callsign[2])) {
        for (i = callsign_length; i > 0; i--) {
            callsign[i] = callsign[i - 1];
        }
        callsign[0] = ' ';
    }

    // callsign encoding:  
    // numbers have a value between 0 and 9 
    // and letters a value between 10 and 35
    // spaces a value of 36
    n = (callsign[0] >= '0' && callsign[0] <= '9' ? callsign[0] - '0' : callsign[0] == ' ' ? 36 : callsign[0] - 'A' + 10);
    n = n * 36 + (callsign[1] >= '0' && callsign[1] <= '9' ? callsign[1] - '0' : callsign[1] ==	' ' ? 36 : callsign[1] - 'A' + 10);
    n = n * 10 + (callsign[2] - '0');	// only number (0-9)
    n = 27 * n + (callsign[3] == ' ' ? 26 : callsign[3] - 'A');	// only space or letter
    n = 27 * n + (callsign[4] == ' ' ? 26 : callsign[4] - 'A');
    n = 27 * n + (callsign[5] == ' ' ? 26 : callsign[5] - 'A');

    // Locator encoding
    m = (179 - 10 * (locator[0] - 65) - (locator[2] - 48)) * 180 + 10 * (locator[1] - 65) + locator[3] - 48;

    // Power encoding
    m = m * 128 + power + 64;

    *N = n;
    *M = m;
}

void pack_msg(unsigned long int N, unsigned long int M, unsigned char c[]) {
    // Bit packing
    // Store in 11 characters because we need 81 bits for FEC correction
    c[0] = N >> 20; // Callsign
    c[1] = N >> 12;
    c[2] = N >> 4;
    c[3] = N;
    c[3] = c[3] << 4;

    c[3] = c[3] | (M >> 18); // locator and power
    c[4] = M >> 10;
    c[5] = M >> 2;
    c[6] = M & 0x03;
    c[6] = c[6] << 6;

    c[7] = 0; // always at 0
    c[8] = 0;
    c[9] = 0;
    c[10] = 0;
}

void generate_parity (unsigned char c[], unsigned char symbols[]) {
    unsigned long int Reg0 = 0;	// 32 bits shift register
    unsigned long int Reg1 = 0;
    unsigned long int result0; 
    unsigned long int result1;
    int count1;	// to count the number
    int count2;	// of bits at one
    int bit_result = 0;
    int i;
    int j = 0;
    int k; 
    int l = 0;

    for (j = 0; j < 11; j++) { // each byte
        for (i = 7; i >= 0; i--) {
	    Reg0 = (Reg0 << 1);
	    Reg0 = Reg0 | (c[j] >> i); // each bit
	    Reg1 = Reg0;

	    result0 = Reg0 & POLYNOM_1; // first polynom
	    count1 = 0;

	    for (k = 0; k < 32; k++) { // how many bit at one?
	        bit_result = result0 >> k;
	        if ((bit_result & 0x01) == 1) {
                    count1++;
                }
	    }
	    if (count1 % 2 == 1) { // if number of one is odd
	        symbols[l] = 1; // parity = 1
            }
	    l++;

	    result1 = Reg1 & POLYNOM_2; // second polynom
	    count2 = 0;

	    for (k = 0; k < 32; k++) { // how many bit at one?
	        bit_result = result1 >> k;
	        if ((bit_result & 0x01) == 1) {
                    count2++;
                }
	    }
	    if (count2 % 2 == 1) { // if number of one is odd
                symbols[l] = 1;	// parity = 1
            }
	    l++;
	} // end of each bit (32) loop
    } // end of each byte (11) loop
}

void interleave(unsigned char symbols[], unsigned char symbols_interleaved[]) {
    int i, j, k, l, P;

    P = 0;
    while (P < 162) {
        for (k = 0; k <= 255; k++){  // bits reverse, ex: 0010 1110 --> 0111 0100
            i = k;
	    j = 0;
	    for (l = 7; l >= 0; l--){ // hard work is done here...
	        j = j | (i & 0x01) << l;
	        i = i >> 1;
	    }
	    if (j < 162) {
	        symbols_interleaved[j] = symbols[P++]; // range in interleaved table
            }
	}
    } // end of while, interleaved table is full
}

void synchronise(unsigned char symbols_interleaved[], unsigned char symbols_wspr[]) {
    unsigned int sync_word [162]={
        1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,0,0,0,0,1,0,0,1,0,1,0,0,
        0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,
        1,1,0,0,0,1,1,0,1,0,1,0,0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,0,1,0,0,0,1,
        1,1,0,0,0,0,0,1,0,1,0,0,1,1,0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0
    };

    for (int i = 0; i < 162; i++) {
        symbols_wspr[i] = sync_word[i] + 2 * symbols_interleaved[i];
    }
}

void code_wspr (char* wspr_message, unsigned char* wspr_symbols) {
    unsigned char symbols_parity[162] = ""; // contains 2*81 parity bits
    unsigned char symbols_interleaved[162] = ""; // contains parity bits after interleaving
    unsigned char c_packed[11]; // for bit packing

    unsigned long N; // for callsign
    unsigned long M; // for locator and power

    code_msg(wspr_message, &N, &M);
    pack_msg(N, M, c_packed);
    generate_parity(c_packed, symbols_parity);
    interleave(symbols_parity, symbols_interleaved);
    synchronise(symbols_interleaved, wspr_symbols);
}


int main(int argc, char *argv[])
{
    char wspr_message[20];          // user beacon message to encode
    unsigned char wspr_symbols[162] = {};

    if(argc != 4){
        printf("Usage: %s <callsign> <locator> <power in dBm>\n", argv[0]);
        printf("\te.g.: %s K6AWS EM73 20\n", argv[0]);
        return 1;
    }
    sprintf(wspr_message, "%s %s %s", argv[1], argv[2], argv[3]);

    code_wspr(wspr_message, wspr_symbols);

    int i;
    for (i = 0; i < 161; i++) {
        printf("%d, ", wspr_symbols[i]);
    }
    printf("%d\n\n\n", wspr_symbols[i]); // print last symbol without comma 
  
  return 0;
}
