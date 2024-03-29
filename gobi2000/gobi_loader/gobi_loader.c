/* Firmware loader for Qualcomm Gobi USB hardware */

/* Modified/Slightly rewritten by Madox <madox@madox.net> */

/* Copyright 2009 Red Hat <mjg@redhat.com> - heavily based on work done by
 * Alexander Shumakovitch <shurik@gwu.edu>
 *
 * crc-ccitt code derived from the Linux kernel, lib/crc-ccitt.c
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <malloc.h>

char cmd_devinit[] = {0x01, 0x51, 0x43, 0x4f, 0x4d, 0x20, 0x68, 0x69,
		 0x67, 0x68, 0x20, 0x73, 0x70, 0x65, 0x65, 0x64, 0x20, 
		 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f, 0x6c, 0x20,
		 0x68, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04,
		 0x30, 0xff, 0xff};

char cmd_devinit2k[] = {0x01, 0x51, 0x43, 0x4f, 0x4d, 0x20, 0x68, 0x69,
		 0x67, 0x68, 0x20, 0x73, 0x70, 0x65, 0x65, 0x64, 0x20, 
		 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f, 0x6c, 0x20,
		 0x68, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x05, 0x05,
		 0x30, 0xff, 0xff};
     
char cmd_complete[] = {0x29, 0xff, 0xff};

/*
 * This mysterious table is just the CRC of each possible byte. It can be
 * computed using the standard bit-at-a-time methods. The polynomial can
 * be seen in entry 128, 0x8408. This corresponds to x^0 + x^5 + x^12.
 * Add the implicit x^16, and you have the standard CRC-CCITT.
 */

unsigned short const crc_ccitt_table[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

unsigned short crc_ccitt_byte(unsigned short crc, const char c)
{
        return (crc >> 8) ^ crc_ccitt_table[(crc ^ c) & 0xff];
}

/**
 *	crc_ccitt - recompute the CRC for the data buffer
 *	@crc: previous CRC value
 *	@buffer: data pointer
 *	@len: number of bytes in the buffer
 */

unsigned short crc_ccitt(short crc, char const *buffer, size_t len)
{
	while (len--)
		crc = crc_ccitt_byte(crc, *buffer++);
	return crc;
}

void usage (char **argv)
{
	printf ("usage: %s serial_device firmware_dir\n", argv[0]);
}

void printhexstring (char *string, int len) {
  int i;
  for(i=0; i<len; i++) {
    printf("%02x ", (unsigned char)*(string+i));
    if((i+1)%16==0) {
      printf("\n");
    }
  }
  printf("\n");
}

int send_firmware(char fwtype, char *filename, int serialfd)
{
	int fwfd;
  char *fwdata = malloc(1024*1024);
  int fwsize;
  int len;
  
  struct stat file_data;
  
  char qdlresp[1024];
  int qdlresplen;
  
  if (!fwdata) {
		fprintf(stderr, "Failed to allocate memory for firmware\n");
		return -1;
	}
  
  //QCOM Commands
  char cmd_sendprep[] = {0x25, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0xff, 0xff};
  char cmd_sendinit[] = {0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff};
  
  //Load firmware file
  fwfd = open(filename, O_RDONLY);
	     
	if (fwfd == -1) {
    if ( fwtype == 0x0D ) {
      //UQCN firmware file may not exist for older devices
      printf("UQCN Firmware not file, assuming not Gobi2000...");
    } else {
      perror("Failed to open firmware: ");
      return -1;
    }
	}
  
  fstat(fwfd, &file_data);
  
  if (fwtype == 0x05) {
    //Quirk, first firmware file (0x05) has last 8 bytes truncated
    fwsize=file_data.st_size-8; 
  } else {
    fwsize=file_data.st_size;
  }
  
  //Fill out firmware type and size
  cmd_sendprep[1] = fwtype;
  *(int *)&cmd_sendprep[2] = fwsize;
  *(int *)&cmd_sendinit[7] = fwsize;
  
  //Calculate and fill out CRCs
  *(short *)&cmd_sendprep[sizeof(cmd_sendprep)-2] =
		~crc_ccitt(0xffff, cmd_sendprep, sizeof(cmd_sendprep)-2);
	*(short *)&cmd_sendinit[sizeof(cmd_sendinit)-2] =
		~crc_ccitt(0xffff, cmd_sendinit, sizeof(cmd_sendinit)-2);

  //Send commands, note the send prep needs to be wrapped by 0x7E
	write (serialfd, "\x7e", 1);
	write (serialfd, cmd_sendprep, sizeof(cmd_sendprep));
	write (serialfd, "\x7e", 1);
  
  printf("QDL protocol server request sent\n");
  printhexstring(cmd_sendprep, sizeof(cmd_sendprep));
  
  qdlresplen = read (serialfd, qdlresp, 1024);
  printf("QDL protocol server response received\n");  
  printhexstring(qdlresp, qdlresplen);  
  
	write (serialfd, cmd_sendinit, sizeof(cmd_sendinit));
  
  printf("QDL protocol server request sent\n");
  printhexstring(cmd_sendinit, sizeof(cmd_sendinit));    

  //Send the firmware file in 1 MiB chunks
	while (1) {
		len = read (fwfd, fwdata, 1024*1024);
		if (len == 1024*1024) {
			write (serialfd, fwdata, 1024*1024);
      printf("QDL protocol server sent %d bytes of image\n", 1024*1024); 
		} else {
      if (fwtype == 0x05) {
        //Quirk, first firmware file (0x05) has last 8 bytes truncated
        write (serialfd, fwdata, len-8);
        printf("QDL protocol server sent %d bytes of image\n", len-8); 
      } else {
        write (serialfd, fwdata, len);
        printf("QDL protocol server sent %d bytes of image\n", len); 
      }
			break;
		}
		write (serialfd, fwdata, 0);
	}
  
  sleep(1);
  
  qdlresplen = read (serialfd, qdlresp, 1024);
  printf("QDL protocol server response received\n");  
  printhexstring(qdlresp, qdlresplen);    
  
  return 0;
}

int main(int argc, char **argv)
{	
	int serialfd;
	int err;
	struct termios terminal_data;
  
  char qdlresp[1024];
  int qdlresplen;
  
	if (argc != 3) {
		usage(argv);
		return -1;
	}

	serialfd = open(argv[1], O_RDWR);

	if (serialfd == -1) {
		perror("Failed to open serial device: ");
		usage(argv);
		return -1;
	}

	err = chdir(argv[2]);
	if (err) {
		perror("Failed to change directory: ");
		usage(argv);
		return -1;
	}

  //Prepare Serial Port
	tcgetattr (serialfd, &terminal_data);
	cfmakeraw (&terminal_data);
	tcsetattr (serialfd, TCSANOW, &terminal_data);
  
  //Init the device
	*(short *)&cmd_devinit[sizeof(cmd_devinit)-2] =
		~crc_ccitt(0xffff, cmd_devinit, sizeof(cmd_devinit)-2);
  *(short *)&cmd_devinit2k[sizeof(cmd_devinit2k)-2] =
		~crc_ccitt(0xffff, cmd_devinit2k, sizeof(cmd_devinit2k)-2);    
	write (serialfd, "\x7e", 1);
	write (serialfd, cmd_devinit, sizeof(cmd_devinit));
	write (serialfd, "\x7e", 1);
  
  printf("QDL protocol server request sent\n");
  printhexstring(cmd_devinit, sizeof(cmd_devinit));
  
  sleep(1);
  qdlresplen = read (serialfd, qdlresp, 1024);
  printf("QDL protocol server response received\n");  
  printhexstring(qdlresp, qdlresplen);

  while(qdlresp[1] == 0x0d || qdlresplen == 0){
    //Failed, keep retrying with 2k init string
   	write (serialfd, "\x7e", 1);
    write (serialfd, cmd_devinit2k, sizeof(cmd_devinit2k));
    write (serialfd, "\x7e", 1);
    printf("QDL protocol server request sent (Retry as gobi2000)\n");
    printhexstring(cmd_devinit2k, sizeof(cmd_devinit2k));
    qdlresplen = read (serialfd, qdlresp, 1024);
    printf("QDL protocol server response received (Retry as gobi2000)\n");  
    printhexstring(qdlresp, qdlresplen);
    sleep(1);
  }

  //Send the firmware files
  err = send_firmware(0x05, "amss.mbn", serialfd);
  if (err) {
    perror("Error loading primary firmware.");
  }
  err = send_firmware(0x06, "apps.mbn", serialfd);
  if (err) {
    perror("Error loading secondary firmware.");
  }
  err = send_firmware(0x0D, "UQCN.mbn", serialfd);
  if (err) {
    perror("Error loading tertiary firmware.");
  }
    
  //Complete the load
	*(short *)&cmd_complete[sizeof(cmd_complete)-2] =
		~crc_ccitt(0xffff, cmd_complete, sizeof(cmd_complete)-2);

	write (serialfd, "\x7e", 1);
	write (serialfd, cmd_complete, sizeof(cmd_complete));
	write (serialfd, "\x7e", 1);
  
  printf("QDL protocol server request sent\n");
  printhexstring(cmd_complete, sizeof(cmd_complete));
  
  printf("Firmware Loading Complete\n");

	return 0;
}
