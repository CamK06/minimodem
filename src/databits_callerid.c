/*
 * databits_callerid.c
 *
 * Copyright (C) 2012 Kamal Mostafa <kamal@whence.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "databits.h"

#include <stdio.h>

/*
 * Caller-ID (USA SDMF/MDMF) databits decoder
 *
 * Reference: http://melabs.com/resources/callerid.htm
 */

#define CID_MSG_MDMF		0x80
#define CID_MSG_SDMF		0x04

#define CID_DATA_DATETIME	0x01
#define CID_DATA_PHONE		0x02
#define CID_DATA_PHONE_NA	0x04
#define CID_DATA_NAME		0x07
#define CID_DATA_NAME_NA	0x08

static const char *cid_datatype_names[] = {
    "unknown0:", "Time:", "Phone:", "unknown3:",
    "Phone:", "unknown5:", "unknown6:", "Name:",
    "Name:"
};

static int cid_msgtype = 0;
static int cid_ndata = 0;
static unsigned char cid_buf[256];


static unsigned int
decode_mdmf_callerid( char *dataout_p, unsigned int dataout_size )
{
    unsigned int dataout_n = 0;
    unsigned int cid_i = 0;
    unsigned int cid_msglen = cid_buf[1];

    unsigned char *m = cid_buf + 2;
    while ( cid_i < cid_msglen ) {

	unsigned int cid_datatype = *m++;
	if ( cid_datatype >= CID_DATA_NAME_NA ) {
	    // FIXME: bad datastream -- print something here
	    return databits_decode_callerid(0, 0, 0, 0);	// reset
	}

	unsigned int cid_datalen = *m++;
	if ( m + 2 + cid_datalen >= cid_buf + sizeof(cid_buf) ) {
	    // FIXME: bad datastream -- print something here
	    return databits_decode_callerid(0, 0, 0, 0);	// reset
	}

	// dataout_n += sprintf(dataout_p+dataout_n, "CID: %d (%d)\n", cid_datatype, cid_datalen);

	dataout_n += sprintf(dataout_p+dataout_n, "%-6s ",
				cid_datatype_names[cid_datatype]);

	if ( cid_datatype == CID_DATA_DATETIME )
	    dataout_n += sprintf(dataout_p+dataout_n, "%.2s/%.2s %.2s:%.2s\n",
				m+0, m+2, m+4, m+6);
	else if ( cid_datatype == CID_DATA_PHONE && cid_datalen == 10 )
	    dataout_n += sprintf(dataout_p+dataout_n, "%.3s-%.3s-%.4s\n",
				m+0, m+3, m+6);
	else
	    dataout_n += sprintf(dataout_p+dataout_n, "%.*s\n",
				cid_datalen, m);

	m     += cid_datalen;
	cid_i += cid_datalen + 2;
    }

    return dataout_n;
}


static unsigned int
decode_sdmf_callerid( char *dataout_p, unsigned int dataout_size )
{
    unsigned int dataout_n = 0;
    unsigned int cid_msglen = cid_buf[1];

    unsigned char *m = cid_buf + 2;

    dataout_n += sprintf(dataout_p+dataout_n, "%-6s ",
			    cid_datatype_names[CID_DATA_DATETIME]);
    dataout_n += sprintf(dataout_p+dataout_n, "%.2s/%.2s %.2s:%.2s\n",
			    m+0, m+2, m+4, m+6);
    m += 8;

    dataout_n += sprintf(dataout_p+dataout_n, "%-6s ",
			    cid_datatype_names[CID_DATA_PHONE]);
    unsigned int cid_datalen = cid_msglen - 8;
    if ( cid_datalen == 10 )
	dataout_n += sprintf(dataout_p+dataout_n, "%.3s-%.3s-%.4s\n",
			    m+0, m+3, m+6);
    else
	dataout_n += sprintf(dataout_p+dataout_n, "%.*s\n",
			    cid_datalen, m);

    return dataout_n;
}


// FIXME: doesn't respect dataout_size at all!
/* returns nbytes decoded */
unsigned int
databits_decode_callerid( char *dataout_p, unsigned int dataout_size,
	unsigned int bits, unsigned int n_databits )
{
    if ( ! dataout_p ) {	// databits processor reset
	cid_msgtype = 0;
	cid_ndata = 0;
	return 0;
    }

    if ( cid_msgtype == 0 ) {
	if ( bits == CID_MSG_MDMF )
	    cid_msgtype = CID_MSG_MDMF;
	else if ( bits == CID_MSG_SDMF )
	    cid_msgtype = CID_MSG_SDMF;
	cid_buf[cid_ndata++] = bits;
	return 0;
    }

    if ( cid_ndata >= sizeof(cid_buf) ) {
	// FIXME? buffer overflow; do what here?
	return databits_decode_callerid(0, 0, 0, 0);	// reset
    }

    cid_buf[cid_ndata++] = bits;

    // Collect input bytes until we've collected as many as the message
    // length byte says there will be, plus two (the message type byte
    // and the checksum byte)
    unsigned int cid_msglen = cid_buf[1];
    if ( cid_ndata < cid_msglen + 2)
	return 0;

    // Now we have a whole CID message in cid_buf[] -- decode it

    // FIXME: check the checksum

    unsigned int dataout_n = 0;

    dataout_n += sprintf(dataout_p+dataout_n, "CALLER-ID\n");

    if ( cid_msgtype == CID_MSG_MDMF )
	dataout_n += decode_mdmf_callerid(dataout_p+dataout_n,
						dataout_size-dataout_n);
    else
	dataout_n += decode_sdmf_callerid(dataout_p+dataout_n,
						dataout_size-dataout_n);

    // All done; reset for the next one
    databits_decode_callerid(0, 0, 0, 0);

    return dataout_n;
}
