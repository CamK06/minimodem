#include "databits.h"

/*
 * CHU time decoder
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#define CHU_FRAME_A 0x00
#define CHU_FRAME_B 0x01

bool chu_do_systime = false;
int chu_seconds_offset = 2;
static int chu_frametype = -1;
static int chu_ndata = 0;
static int chu_year = 0;
static int chu_dst = 0;
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

    // Swap the least and most significant nybbles of the first 5 bytes, the last 5 bytes don't matter anymore
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

        // Decode the date-time info
        int day = (chu_buf[0] & 0x0F)*100 + ((chu_buf[1] & 0xF0)>>4)*10 + (chu_buf[1] & 0x0F);
        int hour = ((chu_buf[2] & 0xF0)>>4)*10 + (chu_buf[2] & 0x0F);
        int minute = ((chu_buf[3] & 0xF0)>>4)*10 + (chu_buf[3] & 0x0F);
        int second = ((chu_buf[4] & 0xF0)>>4)*10 + (chu_buf[4] & 0x0F) + chu_seconds_offset;

        // Convert the date-time info into a format we can work with
        time_t t_utc = time(NULL);
        struct tm *tm = gmtime(&t_utc);
        tm->tm_mon = 0; // mktime treats tm_mday as 0-365 (what we got from CHU) if the month is 0
        tm->tm_mday = day;
        tm->tm_hour = hour;
        tm->tm_min = minute;
        tm->tm_sec = second;
        if(chu_year > 0) // If we got the year from CHU use it, otherwise system year will be used
            tm->tm_year = chu_year - 1900;
        t_utc = mktime(tm);
        
        // Convert t_utc to local time
        time_t t_local = t_utc;
        t_local -= __timezone;

        // Print the time
        dataout_n += sprintf(dataout_p+dataout_n, "\nUTC Time: %s", ctime(&t_utc));
        dataout_n += sprintf(dataout_p+dataout_n, "Local Time: %s", ctime(&t_local));

        // Set the system time
        if(chu_do_systime) {
            struct timeval tv;
            tv.tv_sec = t_local;
            tv.tv_usec = 0;
            if(settimeofday(&tv, NULL))
                dataout_n += sprintf(dataout_p+dataout_n, "System clock set successfully\n");
            else
                dataout_n += sprintf(dataout_p+dataout_n, "Failed to set system clock\n");
        }
    }
    else if(chu_frametype == CHU_FRAME_B) {

        chu_year = ((chu_buf[1] & 0xF0)>>4)*1000 + (chu_buf[1] & 0x0F)*100 + ((chu_buf[2] & 0xF0)>>4)*10 + (chu_buf[2] & 0x0F);
        chu_dst = (chu_buf[4] & 0x0F);

        dataout_n += sprintf(dataout_p+dataout_n, "DUT1: %d\n", (chu_buf[0] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "Year: %d%d%d%d\n", ((chu_buf[1] & 0xF0)>>4), (chu_buf[1] & 0x0F), ((chu_buf[2] & 0xF0)>>4), (chu_buf[2] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "TAI-UTC: %d%d\n", ((chu_buf[3] & 0xF0)>>4), (chu_buf[3] & 0x0F));
        dataout_n += sprintf(dataout_p+dataout_n, "DST code: %d\n", (chu_buf[4] & 0x0F));
    }
    
    decode_chu_reset();
    return dataout_n;
}