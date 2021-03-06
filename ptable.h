

// chapter header description structure
#ifndef WIN32
struct __attribute__ ((__packed__)) pheader {
#else
#pragma pack(push,1)
struct pheader {
#endif
 int32_t magic;    //   0xa55aaa55
 uint32_t hdsize;   // header size
 uint32_t hdversion;
 uint8_t unlock[8];
 uint32_t code;     // type of section
 uint32_t psize;    // the size of the data field
 uint8_t date[16];
 uint8_t time[16];  // date-time of assembly of the firmware
 uint8_t version[32];   // version of ?proshovki?
 uint16_t crc;   // CRC header
 uint32_t blocksize;  // the size of the CRC block of the firmware image
}; 
#ifdef WIN32
#pragma pack(pop)
#endif


// The partition table description structure

struct ptb_t{
  unsigned char pname[20];    // the literal name of the section
  struct pheader hd;  // header image
  uint16_t* csumblock; // a block of checksums
  uint8_t* pimage;   // the image of the partition
  uint32_t offset;   // offset in the file before the beginning of the section
  uint32_t zflag;     // sign of the compressed section
  uint8_t ztype;    // type of compression
};

//******************************************************
// * External arrays for storing the partition table
//******************************************************
extern struct ptb_t ptable[];
extern int npart; // number of partitions in the table

extern uint32_t errflag;

int findparts(FILE* in);
void  find_pname(unsigned int id,unsigned char* pname);
void findfiles (char* fdir);
uint32_t psize(int n);

extern int dload_id;
