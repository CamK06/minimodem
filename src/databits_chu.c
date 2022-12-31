#include "databits.h"

/*
 * CHU time decoder
 */

#include <stdio.h>

#define CHU_FRAME_A 0x00
#define CHU_FRAME_B 0x01

static int chu_frametype = -1;
static int chu_ndata = 0;
static unsigned char chu_buf[10];

static int
decode_chu_reset()
{
    chu_frametype = 0;
    chu_ndata = 0;
    return 0;
}

// returns nbytes decoded
unsigned int
databits_decode_chu( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits )
{
    if ( ! dataout_p )	// databits processor reset: noop
	return decode_chu_reset();
    
    // Receive 10 bytes before continuing with decoding
    chu_buf[chu_ndata++] = bits;
    if(chu_ndata < 10)
        return 0;

    // Determine the frame type, if the first byte is equal to the inverse of the 6th byte, it's frame B
    // Check *all* bytes as a ghetto way of detecting FSK decode errors
    for(int i = 0; i < 5; i++)
        if(chu_buf[i] == (chu_buf[i+5] ^ 0xFF))
            chu_frametype = CHU_FRAME_B;
        else if(chu_buf[i] == chu_buf[i+5])
            chu_frametype = CHU_FRAME_A;
        else 
            return decode_chu_reset();
    printf("CHU frame type: %s\n", chu_frametype ? "B" : "A");
    // Swap the least and most significant nybbles of the first 5 bytes, the last 5 don't matter anymore
    for(int i = 0; i < 5; i++) {
        unsigned char tmp = chu_buf[i];
        chu_buf[i] = ((tmp & 0x0F) << 4) | ((tmp & 0xF0) >> 4);
    }

    // Decode the frame
    unsigned int dataout_n = 0;
    if(chu_frametype == CHU_FRAME_A) {
        // Check if the first byte is valid, A frames always start with 6 in the first nybble
        if((chu_buf[0] & 0xF0) != 0x60)
            return decode_chu_reset();
        printf("\nDecoded frame:\n");

        // Decode the time
        // TODO: Store these values/use them to update the system clock
        dataout_n += sprintf(dataout_p+dataout_n, "Day: %d%d%d\n", (chu_buf[0] & 0x0F), ((chu_buf[1] & 0xF0)>>4), (chu_buf[1] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "Hour: %d%d\n", ((chu_buf[2] & 0xF0)>>4), (chu_buf[2] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "Minute: %d%d\n", ((chu_buf[3] & 0xF0)>>4), (chu_buf[3] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "Second: %d%d\n", ((chu_buf[4] & 0xF0)>>4), (chu_buf[4] & 0x0F));
    }
    else if(chu_frametype == CHU_FRAME_B) {
        printf("\nDecoded frame:\n");

        // Decode the time stats
        dataout_n += sprintf(dataout_p+dataout_n, "DUT1: %d\n", (chu_buf[0] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "Year: %d%d%d%d\n", ((chu_buf[1] & 0xF0)>>4), (chu_buf[1] & 0x0F), ((chu_buf[2] & 0xF0)>>4), (chu_buf[2] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "TAI-UTC: %d%d\n", ((chu_buf[3] & 0xF0)>>4), (chu_buf[3] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "DST code: %d\n", (chu_buf[4] & 0x0F));
    }
    
    decode_chu_reset();
    return dataout_n;
}

