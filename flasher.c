#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#include "getopt.h"
#include "printf.h"
#include "buildno.h"
#endif

#include "hdlcio.h"
#include "ptable.h"
#include "flasher.h"
#include "util.h"

#define true 1
#define false 0


//***************************************************
// * The error code store
//***************************************************
int errcode;


//***************************************************
// * Displays the command error code
//***************************************************
void printerr() {
  
if (errcode == -1) printf ("- timeout of the command \n");
else printf("- error code %02x\n", errcode);
}

// ************************************************ ***
// Send the start command to the partition
//
// code - 32-bit section code
// size - the full size of the recordable partition
//
// * result:
// false - error
// true - the command is accepted by the modem
// ************************************************ ***
int dload_start (uint32_t code, uint32_t size) {

uint32_t iolen;  
uint8_t replybuf[4096];
  
#ifndef WIN32
static struct __attribute__ ((__packed__))  {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t code;
  uint32_t size;
  uint8_t pool[3];
} cmd_dload_init =  {0x41,0,0,{0,0,0}};
#ifdef WIN32
#pragma pack(pop)
#endif


cmd_dload_init.code=htonl(code);
cmd_dload_init.size=htonl(size);
iolen=send_cmd((uint8_t*)&cmd_dload_init,sizeof(cmd_dload_init),replybuf);
errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2)) {
  if (iolen == 0) errcode=-1;
  return false;
}  
else return true;
}  

//***************************************************
// Send block section
// 
// blk - # blocks
// pimage - address of the start of the partition image in memory
// 
// * result:
// false - error
// true - the command is accepted by the modem
//***************************************************
int dload_block(uint32_t part, uint32_t blk, uint8_t* pimage) {

uint32_t res,blksize,iolen;
uint8_t replybuf[4096];

#ifndef WIN32
static struct __attribute__ ((__packed__)) {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t blk;
  uint16_t bsize;
  uint8_t data[fblock];
} cmd_dload_block;  
#ifdef WIN32
#pragma pack(pop)
#endif

blksize = fblock; // the initial value of the block size
res = ptable [part] .hd.psize-blk * fblock; // the size of the remaining piece to the end of the file
if (res<fblock) blksize = res; // adjust the size of the last block

// the command code
cmd_dload_block.cmd=0x42;
// block number
cmd_dload_block.blk=htonl(blk+1);
// block size
cmd_dload_block.bsize=htons(blksize);
// a piece of data from the partition image
memcpy(cmd_dload_block.data,pimage+blk*fblock,blksize);
// send the block to the modem
iolen=send_cmd((uint8_t*)&cmd_dload_block,sizeof(cmd_dload_block)-fblock+blksize,replybuf); // send the command

errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2))  {
  if (iolen == 0) errcode=-1;
  return false;
}
return true;
}

  
//***************************************************
// End the partition
// 
// code - section code
// size - the size of the partition
// 
// * result:
// false - error
// true - the command is accepted by the modem
//***************************************************
int dload_end(uint32_t code, uint32_t size) {

uint32_t iolen;
uint8_t replybuf[4096];

#ifndef WIN32
static struct __attribute__ ((__packed__)) {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t size;
  uint8_t garbage[3];
  uint32_t code;
  uint8_t garbage1[11];
} cmd_dload_end;
#ifdef WIN32
#pragma pack(pop)
#endif


cmd_dload_end.cmd=0x43;
cmd_dload_end.code=htonl(code);
cmd_dload_end.size=htonl(size);
iolen=send_cmd((uint8_t*)&cmd_dload_end,sizeof(cmd_dload_end),replybuf);
errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2)) {
  if (iolen == 0) errcode=-1;
  return false;
}  
return true;
}  



//***************************************************
// * Record all sections from the table in the modem
//***************************************************
void flash_all() {

int32_t part;
uint32_t blk,maxblock;

printf ("\n ## ---- The name of the section ---- is written");
// The main cycle of recording partitions
for(part=0;part<npart;part++) {
printf("\n");  
//  printf("\n02i %s)",part,ptable[part].pname);
 // start partition command
 if (!dload_start(ptable[part].hd.code,ptable[part].hd.psize)) {
   printf("\r! Rejected the title of the partition %i (%s)", part, ptable [part] .pname);
   printerr();
   exit(-2);
 }  
    
 maxblock = (ptable [part] .hd.psize + (fblock-1)) / fblock; // number of blocks in the section
 // Block transfer of the partition image
 for(blk=0;blk<maxblock;blk++) {
  // Output the percentage recorded
  printf("\r%02i  %-20s  %i%%",part,ptable[part].pname,(blk+1)*100/maxblock); 

    // Send the next block
  if (!dload_block(part,blk,ptable[part].pimage)) {
   printf("\n! The %i block of %i (%s)", blk, part, ptable[part].pname);
   printerr();
   exit(-2);
  }  
 }    

// close the section
 if (!dload_end(ptable[part].hd.code,ptable[part].hd.psize)) {
   printf("\n! Error closing section %i (%s)", part, ptable[part].pname);
   printerr();
   exit(-2);
 }  
} // end of loop by sections
}
