// Procedures for working with the partition table

#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <string.h>
#include <stdlib.h>
#else
#include <windows.h>
#include "printf.h"
#endif

#include <zlib.h>

#include "ptable.h"
#include "hdlcio.h"
#include "util.h"
#include "signver.h"

int32_t lzma_decode(uint8_t* inbuf,uint32_t fsize,uint8_t* outbuf);

//******************************************************
// * search for the symbolic name of a section by its code
//******************************************************

void  find_pname(unsigned int id,unsigned char* pname) {

unsigned int j;
struct {
  char name[20];
  int code;
} pcodes[]={ 
  {"M3Boot",0x20000}, 
  {"M3Boot-ptable",0x10000}, 
  {"M3Boot_R11",0x200000}, 
  {"Ptable",0x10000},
  {"Ptable_ext_A",0x480000},
  {"Ptable_ext_B",0x490000},
  {"Fastboot",0x110000},
  {"Logo",0x130000},
  {"Kernel",0x30000},
  {"Kernel_R11",0x90000},
  {"DTS_R11",0x270000},
  {"VxWorks",0x40000},
  {"VxWorks_R11",0x220000},
  {"M3Image",0x50000},
  {"M3Image_R11",0x230000},
  {"DSP",0x60000},
  {"DSP_R11",0x240000},
  {"Nvdload",0x70000},
  {"Nvdload_R11",0x250000},
  {"Nvimg",0x80000},
  {"System",0x590000},
  {"System",0x100000},
  {"APP",0x570000}, 
  {"APP",0x5a0000}, 
  {"APP_EXT_A",0x450000}, 
  {"APP_EXT_B",0x460000},
  {"Oeminfo",0xa0000},
  {"CDROMISO",0xb0000},
  {"Oeminfo",0x550000},
  {"Oeminfo",0x510000},
  {"Oeminfo",0x1a0000},
  {"WEBUI",0x560000},
  {"WEBUI",0x5b0000},
  {"Wimaxcfg",0x170000},
  {"Wimaxcrf",0x180000},
  {"Userdata",0x190000},
  {"Online",0x1b0000},
  {"Online",0x5d0000},
  {"Online",0x5e0000},
  {"Ptable_R1",0x100},
  {"Bootloader_R1",0x101},
  {"Bootrom_R1",0x102},
  {"VxWorks_R1",0x550103},
  {"Fastboot_R1",0x104},
  {"Kernel_R1",0x105},
  {"System_R1",0x107},
  {"Nvimage_R1",0x66},
  {"WEBUI_R1",0x113},
  {"APP_R1",0x109},
  {"HIFI_R11",0x280000},
  {"Modem_fw",0x1e0000},
  {0,0}
};

for(j=0;pcodes[j].code != 0;j++) {
  if(pcodes[j].code == id) break;
}
if (pcodes[j].code != 0) strcpy(pname,pcodes[j].name); // name found - copy it into structure
else sprintf(pname,"U%08x",id); // name not found - substitute pseudame Uxxxxxxxx in blunt format
}

//*******************************************************************
// Calculate the size of the checksum block of the partition
//*******************************************************************
uint32_t crcsize(int n) { 
  return ptable[n].hd.hdsize-sizeof(struct pheader); 
  
}

//*******************************************************************
// get the size of the partition image
//*******************************************************************
uint32_t psize(int n) { 
  return ptable[n].hd.psize; 
  
}

//*******************************************************
// * Calculate the block header checksum
//*******************************************************
void calc_hd_crc16(int n) { 

ptable[n].hd.crc=0;   // clear the old CRC value
ptable[n].hd.crc=crc16((uint8_t*)&ptable[n].hd,sizeof(struct pheader));   
}


//*******************************************************
// * Calculate the block checksum of the partition
//*******************************************************
void calc_crc16(int n) {
  
uint32_t csize; // the size of the sum block in 16-bit words
uint16_t* csblock;  // pointer to the block being created
uint32_t off,len;
uint32_t i;
uint32_t blocksize=ptable[n].hd.blocksize; // the size of the block covered by the sum

// define the size and create the block
csize=psize(n)/blocksize;
if (psize(n)%blocksize != 0) csize++; // This is if the image size is not a multiple of blocksize
csblock=(uint16_t*)malloc(csize*2);

// loop of calculating the sums
for (i=0;i<csize;i++) {
 off=i*blocksize; // offset to the current block
 len=blocksize;
 if ((ptable[n].hd.psize-off)<blocksize) len=ptable[n].hd.psize-off; // for the last incomplete block 
 csblock[i]=crc16(ptable[n].pimage+off,len);
} 
// enter the parameters in the header
if (ptable[n].csumblock != 0) free(ptable[n].csumblock); // destroy the old block if it exists
ptable[n].csumblock=csblock;
ptable[n].hd.hdsize=csize*2+sizeof(struct pheader);
// recompute CRC header
calc_hd_crc16(n);
  
}


//*******************************************************************
// * Extract the partition from the file and add it to the partition table
//*
// in - the input file of the firmware
// The position in the file corresponds to the beginning of the section header
//*******************************************************************
void extract(FILE* in)  {

uint16_t hcrc,crc;
uint16_t* crcblock;
uint32_t crcblocksize;
uint8_t* zbuf;
long int zlen;
int res;

ptable[npart].zflag=0; 
// read the header in the structure
ptable[npart].offset=ftell(in);
fread(&ptable[npart].hd,1,sizeof(struct pheader),in); // header
// Look for the symbolic name of the section on the table
find_pname(ptable[npart].hd.code,ptable[npart].pname);

// load the checksum block
ptable[npart].csumblock=0;  // until the block is created
crcblock=(uint16_t*)malloc(crcsize(npart)); // allocate the temporary memory for the loadable block
crcblocksize=crcsize(npart);
fread(crcblock,1,crcblocksize,in);

// load the partition image
ptable[npart].pimage=(uint8_t*)malloc(psize(npart));
fread(ptable[npart].pimage,1,psize(npart),in);

// check the header CRC
hcrc=ptable[npart].hd.crc;
ptable[npart].hd.crc=0;  // the old CRC is not taken into account in the calculation
crc=crc16((uint8_t*)&ptable[npart].hd,sizeof(struct pheader));
if (crc != hcrc) {
    printf("\n! Section %s (%02x) - Header checksum error",ptable[npart].pname,ptable[npart].hd.code>>16);
    errflag=1;
}  
ptable[npart].hd.crc=crc;  // restore the CRC

// compute and check the CRC of the partition
calc_crc16(npart);
if (crcblocksize != crcsize(npart)) {
    printf("\n! Section %s (%02x) - is the wrong size of the checksum block",ptable[npart].pname,ptable[npart].hd.code>>16);
    errflag=1;
}    
  
else if (memcmp(crcblock,ptable[npart].csumblock,crcblocksize) != 0) {
    printf("\n! Section %s (%02x) -  an incorrect block checksum",ptable[npart].pname,ptable[npart].hd.code>>16);
    errflag=1;
}  
  
free(crcblock);


ptable[npart].ztype=' ';
// Definition of zlib-compression


if ((*(uint16_t*)ptable[npart].pimage) == 0xda78) {
  ptable[npart].zflag=ptable[npart].hd.psize;  // save the compressed size 
  zlen=52428800;
  zbuf=malloc(zlen);  // the buffer in 50M
  // unpack the partition image
  res=uncompress (zbuf, &zlen, ptable[npart].pimage, ptable[npart].hd.psize);
  if (res != Z_OK) {
    printf("\n! Error unpacking partition %s (%02x)\n",ptable[npart].pname,ptable[npart].hd.code>>16);
    errflag=1;
  }
  // create a new partition image buffer and copy the unpacked data into it
  free(ptable[npart].pimage);
  ptable[npart].pimage=malloc(zlen);
  memcpy(ptable[npart].pimage,zbuf,zlen);
  ptable[npart].hd.psize=zlen;
  free(zbuf);
  // allocate the checksums
  calc_crc16(npart);
  ptable[npart].hd.crc=crc16((uint8_t*)&ptable[npart].hd,sizeof(struct pheader));
  ptable[npart].ztype='Z';
}

// Definition of lzma-compression

if ((ptable[npart].pimage[0] == 0x5d) && (*(uint64_t*)(ptable[npart].pimage+5) == 0xffffffffffffffff)) {
  ptable[npart].zflag=ptable[npart].hd.psize;  // save the compressed size
  zlen=52428800;
  zbuf=malloc(zlen);  // the buffer in 50M
  // unpack the partition image
  zlen=lzma_decode(ptable[npart].pimage, ptable[npart].hd.psize, zbuf);
  if (zlen>52428800) {
    printf("\n Buffer size exceeded\n");
    exit(1);
  }  
  if (res == -1) {
    printf("\n! Error unpacking partition %s (%02x)\n",ptable[npart].pname,ptable[npart].hd.code>>16);
    errflag=1;
  }
  // create a new partition image buffer and copy the unpacked data into it
  free(ptable[npart].pimage);
  ptable[npart].pimage=malloc(zlen);
  memcpy(ptable[npart].pimage,zbuf,zlen);
  ptable[npart].hd.psize=zlen;
  free(zbuf);
  // allocate the checksums
  calc_crc16(npart);
  ptable[npart].hd.crc=crc16((uint8_t*)&ptable[npart].hd,sizeof(struct pheader));
  ptable[npart].ztype='L';
}
  
  
// move the section counter
npart++;

// move off, if necessary, forward to the border of the word
res=ftell(in);
if ((res&3) != 0) fseek(in,(res+4)&(~3),SEEK_SET);
}


//*******************************************************
// * Find partitions in the firmware file
//* 
// * returns the number of found partitions
//*******************************************************
int findparts(FILE* in) {

// BIN prefixed buffer
uint8_t prefix[0x5c];
int32_t signsize;
int32_t hd_dload_id;

// Header start title of the section
const unsigned int dpattern=0xa55aaa55;
unsigned int i;


// search for the beginning of the chain of sections in the file
while (fread(&i,1,4,in) == 4) {
  if (i == dpattern) break;
}
if (feof(in)) {
  printf("\n В There are no partitions found in the file - the file does not contain an image of the firmware\n");
  exit(0);
}  

// the current position in the file must be no closer than 0x60 from the beginning - the size of the header of the entire file
if (ftell(in)<0x60) {
    printf("\n The header of the file has an incorrect size\n");
    exit(0);
}    
fseek(in,-0x60,SEEK_CUR); // we leave on the beginning of the BIN-file

// extract the prefix
fread(prefix,0x5c,1,in);
hd_dload_id=*((uint32_t*)&prefix[0]);
// if forced dload_id is not installed - select it from the header
if (dload_id == -1) dload_id=hd_dload_id;
if (dload_id > 0xf) {
  printf("\n The incorrect firmware type code (dload_id) in the header is - %x",dload_id);
  exit(0);
}  
printf("\n Firmware file code: %x (%s)\n",hd_dload_id,fw_description(hd_dload_id));

// поиск остальных разделов

do {
  printf("\r Partition search # %i",npart); fflush(stdout);	
  if (fread(&i,1,4,in) != 4) break; // конец файла
  if (i!=dpattern) break; 			// sample not found - end of the chain of sections
  fseek(in, -4, SEEK_CUR); 			// go back to the beginning of the title
  extract(in); 						// extract the partition
} while(1);
printf("\r                                 \r");


// look for a digital signature
signsize=serach_sign();
if (signsize == -1) printf("\n Digital signature: not found");
else {
  printf("\n Digital signature: %i bytes",signsize);
  printf("\n Public key hash: %s",signver_hash);
}
if (((signsize == -1) && (dload_id>7)) ||
    ((signsize != -1) && (dload_id<8))) 
    printf("\n ! ATTENTION: The presence of a digital signature does not correspond to the firmware type code: %02x",dload_id);


return npart;
}


//*******************************************************
// * Search for partitions in a multi-file mode
//*******************************************************
void findfiles (char* fdir) {

char filename[200];  
FILE* in;
  
printf("\n Search for partition image files...\n\n ##   Size      ID        Name          File\n-----------------------------------------------------------------\n");

for (npart=0;npart<30;npart++) {
    if (find_file(npart, fdir, filename, &ptable[npart].hd.code, &ptable[npart].hd.psize) == 0) break; // конец поиска - раздела с таким ID не нашли
    // get the symbolic name of the section
    find_pname(ptable[npart].hd.code,ptable[npart].pname);
    printf("\n %02i  %8i  %08x  %-14.14s  %s",npart,ptable[npart].hd.psize,ptable[npart].hd.code,ptable[npart].pname,filename);fflush(stdout);
    
    // allocate memory for the partition image
    ptable[npart].pimage=malloc(ptable[npart].hd.psize);
    if (ptable[npart].pimage == 0) {
      printf("\n! Memory allocation error, partition #%i, size = %i bytes\n",npart,ptable[npart].hd.psize);
      exit(0);
    }
    
    // read the image into the buffer
    in=fopen(filename,"r");
    if (in == 0) {
      printf("\n Error opening file %s",filename);
      return;
    } 
    fread(ptable[npart].pimage,ptable[npart].hd.psize,1,in);
    fclose(in);
      
}
if (npart == 0) {
 printf("\n! No files were found with the partition image in the directory %s",fdir);
 exit(0);
} 
}

