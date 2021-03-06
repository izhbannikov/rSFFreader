/* Author: Matt Settles
   with original contributtion from:Kyu-Chul Cho (cho@vandals.uidaho.edu)
 * Last Modified: June 9, 2010
 */

#ifdef __cplusplus
extern "C" {
#endif

// Base rSFFreader includes
#include "rSFFreader.h"

// System Includes
#include <stdio.h>
#include <string.h>
#include <stdint.h>        // uint64_t, uint32_t, uint16_t
#include <stdlib.h>        // malloc(), free()
#include <arpa/inet.h>  // htons(), htonl()


//#include "encode.h"
#include "IRanges_interface.h"
#include "Biostrings_interface.h"


static int debug = 0;
static int flowgram_bytes_per_flow = 2;

long sff_file_size = 0;
long manifest_size = 0;


/***************************** Auxilary Functions *****************************/


// only version 1 is supported
int
isMagicNumberCorrect(uint32_t magic_number)
{
        if(0x2E736666==magic_number) return 1;
        else return 0;
}

// only version 1 is supported
int
isVersionInfoCorrect(char* version)
{
        if(version[0]==0 && version[1]==0 && version[2]==0 && version[3]==1)
                return 1;
        else return 0;
}



static void
debug_sffreader(int read, COMMONheader commonHeader,READheader header, READdata data)
{
    int i;
    Rprintf("read(%d) \n", read);
    Rprintf("read header length: %u\n", header.read_header_length);
    Rprintf("name length: %u\n", header.name_length);
    Rprintf("# of bases: %u\n", header.number_of_bases);
    Rprintf("clip qual left: %u\n", header.clip_qual_left);
    Rprintf("clip qual right: %u\n", header.clip_qual_right);
    Rprintf("clip adapter left: %u\n", header.clip_adapter_left);
    Rprintf("clip adapter right: %u\n", header.clip_adapter_right);
    Rprintf("Name:%s",data.name);
    Rprintf("\n");
    Rprintf("Flowgram: ");
    for(i=0; i<commonHeader.number_of_flows_per_read; i++) {
        Rprintf("%.2f\t", data.flows[i]);
    }
    Rprintf("\n");
    Rprintf("Flow Indexes: ");
    for(i=0; i<header.number_of_bases; i++) {
        Rprintf("%hu\t", data.index[i]);
    }
    Rprintf("\n");
    Rprintf("Bases:\t\t");
    for(i=0; i<header.number_of_bases; i++) {
        Rprintf("%c", data.bases[i]);
    }
    Rprintf("\n");
    Rprintf("Quality Scores: ");
    for(i=0; i<header.number_of_bases; i++) {
        Rprintf("%c", data.quality[i]);
    }
    Rprintf("\n");
}

static void
debug_headerreader(COMMONheader commonHeader)
{
    int i;
    Rprintf("magic number c:%d\n",
            isMagicNumberCorrect(commonHeader.magic_number));
    Rprintf("version: %c%c%c%c\n",
            commonHeader.version[0]+'0', commonHeader.version[1]+'0',
            commonHeader.version[2]+'0', commonHeader.version[3]+'0');
    Rprintf("correct version c:%d\n", isVersionInfoCorrect(commonHeader.version));
    Rprintf("index offset: %llu\n", commonHeader.index_offset);
    Rprintf("index length: %u\n", commonHeader.index_length);
    Rprintf("number of reads: %u\n", commonHeader.number_of_reads);
    Rprintf("commonHeader length: %u\n", commonHeader.commonHeader_length);
    Rprintf("key length: %u\n", commonHeader.key_length);
    Rprintf("number of flows per read: %u\n",
                commonHeader.number_of_flows_per_read);
    Rprintf("flowgram format code: %u\n",commonHeader.flowgram_format_code);
    Rprintf("flow chars:\n");
    for(i=0; i<commonHeader.number_of_flows_per_read; i++) {
        Rprintf("%c", commonHeader.flow_chars[i]);
    }
    Rprintf("\n");
    Rprintf("key:");
    for(i=0; i<commonHeader.key_length; i++) {
        Rprintf("%c", commonHeader.key_sequence[i]);
    }
    Rprintf("\n");
}

int
isBigEndian()
{
    uint64_t integerForTesting = UINT64_C(0x0123456789abcdef);
    unsigned char *ch = (unsigned char *) &integerForTesting;
    int returnValue;
    if (*ch == 0xef) // little endian
    {
        returnValue = 0;
    }
    else // big endian
    {
        returnValue = 1;
    }
    return returnValue;
}

/* convert 64 bit Big Endian integer to Native Endian(means current machine) */
// As far as I know, there is no standard function for 64 bit conversion
uint64_t
BE64toNA(uint64_t bigEndian)
{
    // if current machine uses Big Endian, then return the input value
    if(isBigEndian()) return bigEndian;
    else {    // else the machine must use little Endian. Convert to little Endian
        uint64_t littleEndian =
        ((bigEndian &   (0x00000000000000FF)) << 56) |
        ((bigEndian &   (0x000000000000FF00)) << 40) |
        ((bigEndian &   (0x0000000000FF0000)) << 24) |
        ((bigEndian &   (0x00000000FF000000)) << 8)  |
        ((bigEndian &   (0x000000FF00000000)) >> 8)  |
        ((bigEndian &   (0x0000FF0000000000)) >> 24) |
        ((bigEndian &   (0x00FF000000000000)) >> 40) |
        ((bigEndian &   (0xFF00000000000000)) >> 56);
        return littleEndian;
    }
}


int
commonHeaderPaddingSize(int number_of_flows_per_read, int key_length)
{
    int remaining = (7 + number_of_flows_per_read + key_length)%8;
    int padding_size;
    if(remaining==0) padding_size = 0;
    else padding_size = 8-remaining;
    return padding_size;
}

int
readHeaderPaddingSize(int read_header_len, int name_length)
{
    return read_header_len - name_length - 16;
}

int
readDataPaddingSize(int number_of_flows_per_read, int number_of_bases)
{
//     Rprintf("%i", (number_of_flows_per_read*flowgram_bytes_per_flow + (number_of_bases*3)));
    int remaining = ((number_of_flows_per_read*flowgram_bytes_per_flow) + (number_of_bases*3))%8;
    int padding_size;
    if(remaining==0) padding_size = 0;
    else padding_size = 8-remaining;
    return padding_size;
}

void
checkPadding (uint8_t *byte_padding, int padding_size)
{
    int i;
    for(i=0; i<padding_size; i++)
    {
        if(byte_padding[i] !=0 )
            Rf_error("The header does not have proper byte_padding");
    }
}

char
phredToChar(uint8_t score)
{
    return (char)((int)score + 33);
}

int
CharToPhred(char score)
{
    return ((int)score - 33);
}

/**************************** END Auxilary Functions **************************/

/**************************** SFF Header Functions ****************************/

/*
 * side effect: this function uses malloc.
 * ==> MUST call freeCommonHeader() function after calling readCommonHeader
 */
COMMONheader
readCommonHeader(const char *fname){
      int i, padding_size, fres;
    char ch;
    uint16_t uint16;
    uint8_t uint8;
    COMMONheader commonHeader;

    FILE* file = fopen (fname, "r");
    if(file==NULL)
        Rf_error("cannot open specified file: '%s'", fname);

    get_sff_file_size( file );
    
    /** Read Common Header Section **/
    int size = fread( &commonHeader.magic_number, sizeof(uint32_t), 1, file);
    commonHeader.magic_number = htonl(commonHeader.magic_number);
    fres = fread( commonHeader.version, sizeof(char), 4, file);

    fres = fread( &commonHeader.index_offset, sizeof(uint64_t), 1, file);
    commonHeader.index_offset = BE64toNA(commonHeader.index_offset);

    fres = fread( &commonHeader.index_length, sizeof(uint32_t), 1, file);
    commonHeader.index_length = htonl(commonHeader.index_length);

    fres = fread( &commonHeader.number_of_reads, sizeof(uint32_t), 1, file);
    commonHeader.number_of_reads = htonl(commonHeader.number_of_reads);

    fres = fread( &commonHeader.commonHeader_length, sizeof(uint16_t), 1, file);
    commonHeader.commonHeader_length = htons(commonHeader.commonHeader_length);

    fres = fread( &commonHeader.key_length, sizeof(uint16_t), 1, file);
    commonHeader.key_length = htons(commonHeader.key_length);

    fres = fread( &commonHeader.number_of_flows_per_read, sizeof(uint16_t), 1, file);
    commonHeader.number_of_flows_per_read = htons(commonHeader.number_of_flows_per_read);

    fres - fread( &commonHeader.flowgram_format_code, sizeof(uint8_t),1, file);
    // check flowgram_format_code
    if (commonHeader.flowgram_format_code != 1)
        Rf_error("Unknown flowgram format code value:%u",commonHeader.flowgram_format_code);
    // flow chars
    // malloc must be freed before the function ends
    commonHeader.flow_chars =
        (char*) malloc(sizeof(char)*(commonHeader.number_of_flows_per_read+1));
    if (commonHeader.flow_chars==NULL) Rf_error("cannot allocate memory");

    for(i=0; i<commonHeader.number_of_flows_per_read; i++) {
        fres = fread(&ch, sizeof(char), 1, file);
        commonHeader.flow_chars[i] = ch;
    }
    commonHeader.flow_chars[i] = '\0';

    // key sequence
    // malloc must be freed before the function ends
    commonHeader.key_sequence =
        (char*) malloc(sizeof(char)*(commonHeader.key_length+1));
    if (commonHeader.key_sequence==NULL) Rf_error("cannot allocate memory");

    for(i=0; i<commonHeader.key_length; i++) {
        fres = fread(&ch, sizeof(char), 1, file);
        commonHeader.key_sequence[i] = ch;
    }
    commonHeader.key_sequence[i] = '\0';
    
    commonHeader.manifest = readOneSFFManifest (file, commonHeader.index_offset);
    
    fclose(file);

    // if debug, print out header info
    if (debug == 1){
        debug_headerreader(commonHeader);
    }
    return commonHeader;
}


/* MUST called after using readCommonHeader(...) function */
void freeCommonHeader(COMMONheader commonHeader) {
    free(commonHeader.flow_chars);
    free(commonHeader.key_sequence);
}

void freeReadData(READdata readData) {
    free(readData.flows);
    free(readData.index);
    free(readData.name);
    free(readData.quality);
    free(readData.bases);
}

// Reads a single SFF file, converts the output to a R vector and returns the vector
SEXP
readOneSFFHeader (SEXP file)
{
    // we want to call readCommonHeader and convert to SEXP
    COMMONheader commonHeader = readCommonHeader(CHAR(file));
    SEXP headerList = allocVector(VECSXP, 13);
    SEXP eltnm;
    static const char *eltnames[] = {
        "filename", "magic_number", "version", "index_offset",
        "index_length", "number_of_reads", "header_length",
        "key_length", "number_of_flows_per_read", "flowgram_format_code", "flow_chars",
        "key_sequence","manifest"
    };
    PROTECT(headerList);
    SET_VECTOR_ELT(headerList, 0, mkString(CHAR(file)));
    SET_VECTOR_ELT(headerList, 1, Rf_ScalarInteger(commonHeader.magic_number));
    SET_VECTOR_ELT(headerList, 2, mkString(commonHeader.version));
    SET_VECTOR_ELT(headerList, 3, Rf_ScalarInteger(commonHeader.index_offset));
    SET_VECTOR_ELT(headerList, 4, Rf_ScalarInteger(commonHeader.index_length));
    SET_VECTOR_ELT(headerList, 5, Rf_ScalarInteger(commonHeader.number_of_reads));
    SET_VECTOR_ELT(headerList, 6, Rf_ScalarInteger(commonHeader.commonHeader_length));
    SET_VECTOR_ELT(headerList, 7, Rf_ScalarInteger(commonHeader.key_length));
    SET_VECTOR_ELT(headerList, 8,
                   Rf_ScalarInteger(commonHeader.number_of_flows_per_read));
    SET_VECTOR_ELT(headerList, 9, Rf_ScalarInteger(commonHeader.flowgram_format_code));
    SET_VECTOR_ELT(headerList, 10, mkString(commonHeader.flow_chars));
    SET_VECTOR_ELT(headerList, 11, mkString(commonHeader.key_sequence));
    SET_VECTOR_ELT(headerList, 12, commonHeader.manifest);

    PROTECT( eltnm = allocVector( STRSXP, 13 ) );
    for( int i = 0; i < 13; i++ )
        SET_STRING_ELT( eltnm, i, mkChar( eltnames[i] ) );
    namesgets( headerList, eltnm );
    UNPROTECT(1);
    freeCommonHeader(commonHeader);
    UNPROTECT(1);
    return headerList;
}

// Reads multiple SFF files, by calling readOneSFFHeader
SEXP
read_sff_header(SEXP files, SEXP verbose)
{
    int i, nfiles= 0,set_verbose;
    SEXP fname;
    if (!IS_CHARACTER(files))
        Rf_error("'%s' must be '%s'", "files", "character");

    set_verbose = LOGICAL(verbose)[0];

    nfiles = LENGTH(files);

    SEXP ans;
    PROTECT(ans = allocVector(VECSXP,nfiles));
    for (i = 0; i < nfiles; ++i) {
        R_CheckUserInterrupt();
        fname = STRING_ELT(files, i);
        if(set_verbose) {
            Rprintf("reading header for sff file:%s\n", CHAR(fname));
        }
        SET_VECTOR_ELT(ans,i,readOneSFFHeader(fname));
    }
    UNPROTECT(1);
    
    return ans;
}

int
count_reads_sum(SEXP files) //
{
    COMMONheader header;
    int i, nfile = LENGTH(files);
    int nreads = 0;

    for(i=0; i<nfile; i++) {
        header = readCommonHeader(CHAR(STRING_ELT(files,i)));
        nreads += header.number_of_reads;
        freeCommonHeader(header);
    }
    return nreads;
}

int * 
flowgram_sizes(SEXP files)
{
  int i, nfile = LENGTH(files);
  //int nflows[nfile];
  int *nflows = (int*) malloc(nfile*sizeof(int));
  
  
  for(i=0; i<nfile; i++) {
     nflows[i] = readCommonHeader(CHAR(STRING_ELT(files,i))).number_of_flows_per_read;
  }
  return nflows;
}
/*************************** END SFF Header Functions *************************/

/****************************************************************************
 * Reading SFF files.
 */

typedef struct irange_values{
    int *width;
    int *start;
	int length;
} IRANGE_VALUES;

typedef struct flowgram_values {
  double *flows;
  int *indices;
  int Ipos;
} FLOW_VALUES;

typedef struct sff_loader {
    void (*load_seqid)(struct sff_loader *loader,
        const cachedCharSeq *dataline);
    void (*load_seq)(struct sff_loader *loader,
        const cachedCharSeq *dataline);
    void (*load_qual)(struct sff_loader *loader,
        const cachedCharSeq *dataline);
    void (*load_qclip)(struct sff_loader *loader,
        int start, int width);
    void (*load_aclip)(struct sff_loader *loader,
        int start, int width);
    void (*load_flowgrams)(struct sff_loader *loader,
        const double *flows, const int *indices, int flow_length, int read_length);
    int nrec;
    void *ext;  /* loader extension (optional) */
} SFFloader;


typedef struct sff_loader_ext {
    CharAEAE ans_names_buf;
    cachedXVectorList cached_seq;
    cachedXVectorList cached_qual;
    IRANGE_VALUES cached_qual_clip;
    IRANGE_VALUES cached_adapt_clip;
    FLOW_VALUES flowgrams;
    const int *lkup_seq;
    int lkup_length_seq;
    const int *lkup_qual;
    int lkup_length_qual;
    int *read_header_lengths;
} SFF_loaderExt;

static void SFF_load_seqid(SFFloader *loader,
        const cachedCharSeq *dataline)
{
    SFF_loaderExt *loader_ext;
    CharAEAE *ans_names_buf;

    loader_ext = loader->ext;
    ans_names_buf = &(loader_ext->ans_names_buf);
    // This works only because dataline->seq is nul-terminated!
    append_string_to_CharAEAE(ans_names_buf, dataline->seq);
    return;
}

static void SFF_load_seq(SFFloader *loader, const cachedCharSeq *dataline)
{
    SFF_loaderExt *loader_ext;
    cachedCharSeq cached_ans_elt;

    loader_ext = loader->ext;
    cached_ans_elt = get_cachedXRawList_elt(&(loader_ext->cached_seq),
                        loader->nrec);

    /* cached_ans_elt.seq is a const char * so we need to cast it to
       char * before we can write to it */
    Ocopy_bytes_to_i1i2_with_lkup(0, cached_ans_elt.length - 1,
        (char *) cached_ans_elt.seq, cached_ans_elt.length,
        dataline->seq, dataline->length,
        loader_ext->lkup_seq, loader_ext->lkup_length_seq);
    return;
}

static void SFF_load_qual(SFFloader *loader, const cachedCharSeq *dataline)
{
    SFF_loaderExt *loader_ext;
    cachedCharSeq cached_ans_elt;

    loader_ext = loader->ext;
    cached_ans_elt = get_cachedXRawList_elt(&(loader_ext->cached_qual),
                        loader->nrec);
    /* cached_ans_elt.seq is a const char * so we need to cast it to
       char * before we can write to it */
    Ocopy_bytes_to_i1i2_with_lkup(0, cached_ans_elt.length - 1,
        (char *) cached_ans_elt.seq, cached_ans_elt.length,
        dataline->seq, dataline->length,
        loader_ext->lkup_qual, loader_ext->lkup_length_qual);
    return;
}

static void SFF_load_qclip(SFFloader *loader, int start, int width)
{
    SFF_loaderExt *loader_ext;
    loader_ext = loader->ext;
    
	IRANGE_VALUES clip_result;
	clip_result = loader_ext->cached_qual_clip;

	clip_result.start[(int)loader->nrec] = start;
	clip_result.width[(int)loader->nrec] = width;

//	Rprintf("record: %i\tclip points qual:%i %i\t",
//            loader->nrec,clip_result.start[loader->nrec],clip_result.start[loader->nrec]);

    return;
}

static void SFF_load_aclip(SFFloader *loader, int start, int width)
{
    SFF_loaderExt *loader_ext;
    loader_ext = loader->ext;

	IRANGE_VALUES clip_result;
	clip_result = loader_ext->cached_adapt_clip;

	clip_result.start[loader->nrec] = start;
	clip_result.width[loader->nrec] = width;

//	Rprintf("clip points adapt:%i %i\n",
//            clip_result.start[loader->nrec],clip_result.start[loader->nrec]);
    return;
}

static void SFF_load_flowgrams(struct sff_loader *loader, const double *flows, const int *indices, int flow_length, int flow_width)
{
    SFF_loaderExt *loader_ext;
    loader_ext = loader->ext;

    FLOW_VALUES flow_result;
	  flow_result = loader_ext->flowgrams;

	  // flow_result.flows[flow_length*loader->nrec] = *flows;
	  
    int i = 0, start_loc = flow_length*(loader->nrec);
    for (i=0; i<flow_length; i++) {
      flow_result.flows[start_loc+i] = flows[i];

      
        // Rprintf("Location:%i value:%f\n",start_loc+i, flows[i]);
    }
    
    for(i=0; i<flow_width; i++) {
        flow_result.indices[flow_result.Ipos+i] = indices[i];
        }
    
    //Fix length of indices
    loader_ext->flowgrams.Ipos += flow_width;
    //Rprintf("Ipos:%d\n",
    //loader_ext->flowgrams.Ipos);
    return;
     
}
        

void freeLoader(SFF_loaderExt loader) {
    free(loader.cached_qual_clip.width);
    free(loader.cached_qual_clip.start);
    free(loader.cached_adapt_clip.width);
    free(loader.cached_adapt_clip.start);
    free(loader.flowgrams.indices);
    free(loader.flowgrams.flows);
}

static SFF_loaderExt new_SFF_loaderExt(SEXP seq, SEXP qual, SEXP lkup_seq, SEXP lkup_qual, int nbases, int nflows)
{
    SFF_loaderExt loader_ext;

    loader_ext.ans_names_buf =
        new_CharAEAE(get_XVectorList_length(seq), 0);
    loader_ext.cached_seq = cache_XVectorList(seq);
    loader_ext.cached_qual = cache_XVectorList(qual);

    loader_ext.cached_qual_clip.width = (int *) R_alloc((long) get_XVectorList_length(seq), sizeof(int));
    loader_ext.cached_qual_clip.start = (int *) R_alloc((long) get_XVectorList_length(seq), sizeof(int));

    loader_ext.cached_adapt_clip.width = (int *) R_alloc((long) get_XVectorList_length(seq), sizeof(int));
    loader_ext.cached_adapt_clip.start = (int *) R_alloc((long) get_XVectorList_length(seq), sizeof(int));

    loader_ext.flowgrams.indices = (int *) R_alloc((long) get_XVectorList_length(seq)*nbases, sizeof(int));
    loader_ext.flowgrams.flows = (double *) R_alloc((long) get_XVectorList_length(seq)*nflows, sizeof(double));
    loader_ext.flowgrams.Ipos = 0;
    
    if (lkup_seq == R_NilValue) {
        loader_ext.lkup_seq = NULL;
    } else {
        loader_ext.lkup_seq = INTEGER(lkup_seq);
        loader_ext.lkup_length_seq = LENGTH(lkup_seq);
    }
    if (lkup_qual == R_NilValue) {
        loader_ext.lkup_qual = NULL;
    } else {
        loader_ext.lkup_qual = INTEGER(lkup_qual);
        loader_ext.lkup_length_qual = LENGTH(lkup_qual);
    }
    
    loader_ext.read_header_lengths = (int *) R_alloc((long) get_XVectorList_length(seq), sizeof(int));
    
    return loader_ext;
}

static SFFloader new_SFF_loader(int load_seqids, int load_flowgrams,
        SFF_loaderExt *loader_ext)
{
    SFFloader loader;

    loader.load_seqid = load_seqids ? &SFF_load_seqid : NULL;
    loader.load_flowgrams = load_flowgrams ? &SFF_load_flowgrams : NULL;
    loader.load_seq = SFF_load_seq;
    loader.load_qual = SFF_load_qual;
    loader.load_qclip = SFF_load_qclip;
    loader.load_aclip = SFF_load_aclip;
    loader.nrec = 0;
    loader.ext = loader_ext;
    return loader;
}


/******************************* Main Function ********************************/
/* Read a sff file and print a commonHeader information
 * Argument:(0)    SEXP string - file path
 * Return:        SEXP string - file path
 */
static void
readSFF( SEXP string, int *recno, SFFloader *loader )
{
    // C Structures
    COMMONheader commonHeader;
    READheader header;

    cachedCharSeq dataline;

    // C declarations
    int i, padding_size, fres, load_record;
    char ch;
    uint16_t uint16;
    uint8_t uint8;
//    static uint8_t byte_padding[8];

    const char *string2 = CHAR(string);

    FILE* file = fopen (string2, "r");
    if(file==NULL)
        Rf_error("cannot open specified file: '%s'", string2);
    
    
    
    commonHeader = readCommonHeader(string2);
    // add fseek statement to bypass the commonHeader
    fseek(file, commonHeader.commonHeader_length , SEEK_SET);

    READdata data;

    // for every read,
    for(int read=0; read<commonHeader.number_of_reads; read++) {
        
        // Read Header Section
        fres = fread( &header.read_header_length, sizeof(uint16_t), 1, file);
        header.read_header_length = htons(header.read_header_length);
        fres = fread( &header.name_length, sizeof(uint16_t), 1, file);
        header.name_length = htons(header.name_length);
        fres = fread( &header.number_of_bases, sizeof(uint32_t), 1, file);
        header.number_of_bases = htonl(header.number_of_bases);
        fres = fread( &header.clip_qual_left, sizeof(uint16_t), 1, file);
        header.clip_qual_left = htons(header.clip_qual_left);
        fres = fread( &header.clip_qual_right, sizeof(uint16_t), 1, file);
        header.clip_qual_right = htons(header.clip_qual_right);
        fres = fread( &header.clip_adapter_left, sizeof(uint16_t), 1, file);
        header.clip_adapter_left = htons(header.clip_adapter_left);
        fres = fread( &header.clip_adapter_right, sizeof(uint16_t), 1, file);
        header.clip_adapter_right = htons(header.clip_adapter_right);

        // Determine whether or not to load the record
        load_record = loader != NULL;

        // save clip points
        if (load_record && loader->load_seq != NULL) {
            loader->load_qclip(loader, (int)header.clip_qual_left,(int)header.clip_qual_right-(int)header.clip_qual_left+1);
            loader->load_aclip(loader, (int)header.clip_adapter_left,(int)header.clip_adapter_right-(int)header.clip_adapter_left+1);
        }


        data.name = malloc(sizeof(char)*(header.name_length+1));
        if(data.name==NULL) Rf_error("id: cannot allocate memory");

        fres = fread( data.name, sizeof(char),header.name_length, file);
        data.name[header.name_length] = '\0';


        dataline.length = strlen(data.name);
        dataline.seq = data.name;
        if (load_record && loader->load_seqid != NULL) {
            loader->load_seqid(loader, &dataline);
        }

        padding_size =
        readHeaderPaddingSize(header.read_header_length,
                        header.name_length);
        fseek(file, padding_size , SEEK_CUR);
        //Read Data Section
        // Flows
        data.flows = (double*) malloc(sizeof(double)*(commonHeader.number_of_flows_per_read));
        for(i=0; i<commonHeader.number_of_flows_per_read; i++) {
            fres = fread( &uint16, sizeof(uint16_t), 1, file);
            uint16 = htons(uint16);
            data.flows[i] = uint16/100.0;
        }
        // indexes
        data.index = (int*) malloc(sizeof(int)*(header.number_of_bases));
        int cindex = 0;
        for(i=0; i<header.number_of_bases; i++) {
            fres = fread( &uint8, sizeof(uint8_t), 1, file);
            data.index[i] = (int)uint8 + cindex;
            cindex = data.index[i];
        }
        
        // read data from the file
        if (load_record && loader->load_flowgrams != NULL) {
            loader->load_flowgrams(loader, data.flows, data.index, commonHeader.number_of_flows_per_read, header.number_of_bases);
        }
        
        // bases
        data.bases = (char*) malloc(sizeof(char)*(header.number_of_bases+1));
        if (data.bases==NULL) Rf_error("cannot allocate memory");
        for(i=0; i<header.number_of_bases; i++) {
            fres = fread( &ch, sizeof(char), 1, file);
            data.bases[i] = ch;
            if(ch!='A' && ch!='T' && ch!='G' && ch!='C' && ch!='N') Rf_error("base error at %i:%c ",i, ch);
        }
        data.bases[header.number_of_bases]= '\0';

//TODO: SAMS function to adapter find/clip and assign MID

        dataline.length = strlen(data.bases);
        dataline.seq = data.bases;
        if (load_record && loader->load_seq != NULL)
            loader->load_seq(loader, &dataline);

        //quality
        data.quality = (char*) malloc(sizeof(char)*(header.number_of_bases+1));
        if (data.quality==NULL) Rf_error("cannot allocate memory");
        for(i=0; i<header.number_of_bases; i++) {
            fres = fread( &uint8, sizeof(uint8_t), 1, file);
            data.quality[i] = phredToChar(uint8);
        }
        data.quality[header.number_of_bases] = '\0';

        dataline.length = strlen(data.quality);
        dataline.seq = data.quality;
        if (load_record && loader->load_qual != NULL)
            loader->load_qual(loader, &dataline);

        if (load_record)
            loader->nrec++;

        if (debug == 1){
            debug_sffreader(read,commonHeader, header, data);
        }

        padding_size =
        readDataPaddingSize(commonHeader.number_of_flows_per_read,
                        header.number_of_bases);
        fseek(file, padding_size , SEEK_CUR);
        freeReadData(data);
        (*recno)++;
    } // end of for(every read)

    //(*sffmanifest) = readOneSFFManifest(file);
    
    freeCommonHeader(commonHeader);
    
    
    
    fclose(file);
    return;
}

SEXP
sff_geometry(SEXP files)
{
    // C Structures
    COMMONheader commonHeader;
    READheader header;

    // C declarations
    int nrec, nfiles, i, recno, skip, padding_size, fres;
    int* nflows;
    const char *fname;
    nrec = recno = 0;
    static const char *names[] = {"number_of_reads","read_lengths", "flowgram_widths"};
    // R declarations
    SEXP ans, ans_width, eltnm, flow_width;

    if (!IS_CHARACTER(files))
        Rf_error("'%s' must be '%s'", "files", "character");

    nfiles = LENGTH(files);


    //  Retrieve number of records from the header(s)
    nrec = count_reads_sum(files);
    PROTECT(ans_width = NEW_INTEGER(nrec));

    nflows = flowgram_sizes(files);
    PROTECT(flow_width = NEW_INTEGER(nfiles));
        
    for (i = 0; i < nfiles; i++) {
      INTEGER(flow_width)[i] = nflows[i];
    }
    
    for (i = 0; i < nfiles; ++i) {
        R_CheckUserInterrupt();
        fname = CHAR(STRING_ELT(files, i));

        FILE* file = fopen (fname, "r");
        if(file==NULL)
            Rf_error("cannot open specified file: '%s'", fname);
        
        
        commonHeader = readCommonHeader(fname);
        // add fseek statement to bypass the commonHeader
        fseek(file, commonHeader.commonHeader_length , SEEK_SET);

        // for every read,
        for(int read=0; read<commonHeader.number_of_reads; read++) {
            // Read Header Section
            fres = fread( &header.read_header_length, sizeof(uint16_t), 1, file);
            header.read_header_length = htons(header.read_header_length);
            fres = fread( &header.name_length, sizeof(uint16_t), 1, file);
            header.name_length = htons(header.name_length);
            fres = fread( &header.number_of_bases, sizeof(uint32_t), 1, file);
            header.number_of_bases = htonl(header.number_of_bases);
            fres = fread( &header.clip_qual_left, sizeof(uint16_t), 1, file);
            header.clip_qual_left = htons(header.clip_qual_left);
            fres = fread( &header.clip_qual_right, sizeof(uint16_t), 1, file);
            header.clip_qual_right = htons(header.clip_qual_right);
            fres = fread( &header.clip_adapter_left, sizeof(uint16_t), 1, file);
            header.clip_adapter_left = htons(header.clip_adapter_left);
            fres = fread( &header.clip_adapter_right, sizeof(uint16_t), 1, file);
            header.clip_adapter_right = htons(header.clip_adapter_right);

            skip = ((commonHeader.number_of_flows_per_read*flowgram_bytes_per_flow) +
                    (header.number_of_bases*3))+header.read_header_length-16;
            fseek(file,skip, SEEK_CUR);

            padding_size =
            readDataPaddingSize(commonHeader.number_of_flows_per_read,
                            header.number_of_bases);
            fseek(file, padding_size , SEEK_CUR);
            INTEGER(ans_width)[recno] = (int)(header.number_of_bases);
            recno++;

        } // end of for(every read)

        freeCommonHeader(commonHeader);
        fclose(file);
    }
    PROTECT(ans = allocVector(VECSXP, 3));
    PROTECT(eltnm = allocVector( STRSXP, 3 ) );

    SET_VECTOR_ELT(ans, 0, ScalarInteger(nrec)); // also a scalar integer
    SET_VECTOR_ELT(ans, 1, ans_width);
    SET_VECTOR_ELT(ans, 2, flow_width);
    for( int i = 0; i < 3; i++ )
        SET_STRING_ELT( eltnm, i, mkChar( names[i] ));
    namesgets( ans, eltnm );
    UNPROTECT(4);
    return(ans);
}


SEXP
read_sff(SEXP files, SEXP use_names, SEXP use_flows, SEXP lkup_seq, SEXP lkup_qual, SEXP verbose)
{
    int i, nfiles, recno,load_seqids, load_flowgrams, set_verbose, ans_length, flowgram_maxwidth, nbases;
    SEXP fname, ans_geom, ans_names, header, nms, flowgram_width, base_geometry;
    SEXP  ans = R_NilValue, reads = R_NilValue, quals = R_NilValue,
            qual_clip = R_NilValue, adapt_clip = R_NilValue,
            flowgrams = R_NilValue, flow_indices = R_NilValue;
	  SEXP qclip_start, qclip_width, aclip_start, aclip_width;
    SFF_loaderExt loader_ext;
    SFFloader loader;
    

    if (!IS_CHARACTER(files))
        Rf_error("'%s' must be '%s'", "files", "character");

    nfiles = LENGTH(files);
    
    load_seqids = LOGICAL(use_names)[0];
    load_flowgrams = LOGICAL(use_flows)[0];
    set_verbose = LOGICAL(verbose)[0];
    //  Retrieve SFF(s) Geometry
    PROTECT(ans_geom = sff_geometry(files)); // Maintain maximum number of flows
    ans_length = INTEGER(VECTOR_ELT(ans_geom,0))[0];//number of READS
    if(set_verbose) Rprintf("Total number of reads to be read: %d\n", ans_length);

    flowgram_width = VECTOR_ELT(ans_geom, 2);
    flowgram_maxwidth = INTEGER(flowgram_width)[0];
    
    for (i=1; i<nfiles; i++) 
    {
      if (INTEGER(flowgram_width)[i] > flowgram_maxwidth) 
      {
        flowgram_maxwidth = INTEGER(flowgram_width)[i];
      }
    }
      
    base_geometry = VECTOR_ELT(ans_geom, 1);
    nbases = 0;
    for (i=0; i < ans_length; i++) {
      nbases += INTEGER(base_geometry)[i];
    }
    
    PROTECT(header = read_sff_header(files,verbose));
    PROTECT(reads = alloc_XRawList("DNAStringSet","DNAString",VECTOR_ELT(ans_geom,1)));
    PROTECT(quals = alloc_XRawList("BStringSet","BString",VECTOR_ELT(ans_geom,1)));
 
    loader_ext = new_SFF_loaderExt(reads, quals, lkup_seq,lkup_qual, nbases, flowgram_maxwidth); //Biostrings/XStringSet_io.c
    loader = new_SFF_loader(load_seqids, load_flowgrams, &loader_ext); //Biostrings/XStringSet_io.c
    
    
    recno = 0;
    
    //SEXP sffmanifest_array;
    //PROTECT(sffmanifest_array = allocVector(VECSXP,nfiles));
    for (i = 0; i < nfiles; i++) 
    {
        //SEXP sffmanifest;
        R_CheckUserInterrupt();
        fname = STRING_ELT(files, i);
        if(set_verbose) {
            Rprintf("reading file:%s\n", CHAR(fname));
        }
        readSFF(fname,&recno, &loader);
        //SET_VECTOR_ELT(sffmanifest_array,i,sffmanifest);
    }
    
    // load in the seq_ids
    if (load_seqids) {
        PROTECT(ans_names =
            new_CHARACTER_from_CharAEAE(&(loader_ext.ans_names_buf))); //IRanges
        set_XVectorList_names(reads, ans_names); //IRanges
//        set_XVectorList_names(quals, ans_names); //IRanges
        UNPROTECT(1);
    }



	PROTECT(qclip_start = NEW_INTEGER(ans_length));
	PROTECT(qclip_width = NEW_INTEGER(ans_length));
	memcpy(INTEGER(qclip_start), loader_ext.cached_qual_clip.start, sizeof(int) * ans_length);
	memcpy(INTEGER(qclip_width), loader_ext.cached_qual_clip.width, sizeof(int) * ans_length);
	PROTECT(qual_clip = new_IRanges("IRanges", qclip_start, qclip_width, R_NilValue));

	PROTECT(aclip_start = NEW_INTEGER(ans_length));
	PROTECT(aclip_width = NEW_INTEGER(ans_length));
	memcpy(INTEGER(aclip_start), loader_ext.cached_adapt_clip.start, sizeof(int) * ans_length);
	memcpy(INTEGER(aclip_width), loader_ext.cached_adapt_clip.width, sizeof(int) * ans_length);
	PROTECT(adapt_clip = new_IRanges("IRanges", aclip_start, aclip_width, R_NilValue));
  
  if (load_flowgrams) {
    PROTECT(flowgrams = allocVector(REALSXP, ans_length*flowgram_maxwidth));
    PROTECT(flow_indices = NEW_INTEGER(nbases));
    memcpy(REAL(flowgrams), loader_ext.flowgrams.flows, sizeof(double)* (ans_length*flowgram_maxwidth));
    memcpy(INTEGER(flow_indices), loader_ext.flowgrams.indices, sizeof(int) * nbases);
  }
  
  
    PROTECT(ans = NEW_LIST(7));
    SET_VECTOR_ELT(ans, 0, header);
    SET_VECTOR_ELT(ans, 1, reads); /* read */
    SET_VECTOR_ELT(ans, 2, quals); /* quality */
    SET_VECTOR_ELT(ans, 3, qual_clip); /* quality based clip points */
    SET_VECTOR_ELT(ans, 4, adapt_clip); /* adapter based clip points */
    SET_VECTOR_ELT(ans, 5, flowgrams); /* flowgrams */
    SET_VECTOR_ELT(ans, 6, flow_indices); /* flow indices across reads */
    //SET_VECTOR_ELT(ans, 7, sffmanifest_array);
    UNPROTECT(11);
    
    if (load_flowgrams) {
      UNPROTECT(2);
    }

    PROTECT(nms = NEW_CHARACTER(7));
    SET_STRING_ELT(nms, 0, mkChar("header"));
	  SET_STRING_ELT(nms, 1, mkChar("sread"));
    SET_STRING_ELT(nms, 2, mkChar("quality"));
    SET_STRING_ELT(nms, 3, mkChar("qualityClip"));
    SET_STRING_ELT(nms, 4, mkChar("adapterClip"));
    SET_STRING_ELT(nms, 5, mkChar("flowgrams"));
    SET_STRING_ELT(nms, 6, mkChar("flowIndices"));
    //SET_STRING_ELT(nms, 7, mkChar("manifest"));
    setAttrib(ans, R_NamesSymbol, nms);
	UNPROTECT(1);

    return ans;
}

/******************************* END Main Functions ********************************/

/******** Borrowing write_fastq from ShortRead *************/

/******************************* Write out phred qualities *************************/

char *
_cache_to_char(cachedXStringSet *cache, const int i,
               char *buf, const int width)
{
    cachedCharSeq roSeq = get_cachedXStringSet_elt(cache, i);
    if (roSeq.length > width)
        return NULL;
    strncpy(buf, roSeq.seq, roSeq.length);
    buf[roSeq.length] = '\0';
    return buf;
}

SEXP
write_phred_quality(SEXP id, SEXP quality, 
            SEXP fname, SEXP fmode, SEXP max_width)
{
    if (!(IS_S4_OBJECT(id) && 
          strcmp(get_classname(id), "BStringSet") == 0))
        Rf_error("'%s' must be '%s'", "id", "BStringSet");
    if (!(IS_S4_OBJECT(quality) && 
          strcmp(get_classname(quality), "BStringSet") == 0))
        Rf_error("'%s' must be '%s'", "quality", "BStringSet");
    const int len = get_XStringSet_length(id);
    if ((len != get_XStringSet_length(quality)))
        Rf_error("length() of %s must all be equal",
                 "'id', 'quality'");
    if (!(IS_CHARACTER(fname) && LENGTH(fname) == 1)) /* FIXME: nzchar */
        Rf_error("'%s' must be '%s'", "file", "character(1)");
    if (!(IS_CHARACTER(fmode) && LENGTH(fmode) == 1)) /* FIXME nchar()<3 */
        Rf_error("'%s' must be '%s'", "mode", "character(1)");
    if (!(IS_INTEGER(max_width) && LENGTH(max_width) == 1 &&
          INTEGER(max_width)[0] >= 0))
        Rf_error("'%s' must be %s", "max_width", "'integer(1)', >=0");
    const int width = INTEGER(max_width)[0];

    cachedXStringSet xid = cache_XStringSet(id),
                     xquality = cache_XStringSet(quality);

    FILE *fout = fopen(CHAR(STRING_ELT(fname, 0)), 
                       CHAR(STRING_ELT(fmode, 0)));
    if (fout == NULL)
        Rf_error("failed to open file '%s'", 
                 CHAR(STRING_ELT(fname, 0)));

    char *idbuf = (char *) R_alloc(sizeof(char), width + 1),
        *qualbuf = (char *) R_alloc(sizeof(char), width + 1);
    int i, j, phredval;
    
    for (i = 0; i < len; ++i) 
    {
        idbuf = _cache_to_char(&xid, i, idbuf, width);
        if (idbuf == NULL)
        {
			      fclose(fout);
			      Rf_error("failed to write record %d", i + 1);
		    }
		    fprintf(fout, ">%s\n",idbuf);
        
        qualbuf = _cache_to_char(&xquality, i, qualbuf, width);
        if (qualbuf == NULL)
        {
			    fclose(fout);
			    Rf_error("failed to write record %d", i + 1);
		    }
		    j = 0;
        while(qualbuf[j] != '\0') 
        {
			    if (j != 0) fprintf(fout," ");
              phredval = CharToPhred(qualbuf[j]);
	        fprintf(fout, "%i", phredval);
			    j++;
        }
		    fprintf(fout,"\n");
    }
    
    fclose(fout);
    return R_NilValue;
}

//===================Write sff functions================
SEXP write_sff(SEXP filename,
                SEXP magic, 
                SEXP version,
                SEXP index_offset,
                SEXP index_len,
                SEXP number_of_reads,
                SEXP header_len,
                SEXP key_len,
                SEXP flow_len,
                SEXP flowgram_format,
                SEXP flow,
                SEXP key,
                SEXP sread, 
                SEXP quality, 
                SEXP flowIndices, 
                SEXP flowgrams,
                SEXP names,
                SEXP qualityClip,
                SEXP adapterClip,
                SEXP manifest)
{
  //printf("%s\n",CHAR(STRING_ELT(filename,0)));
  /*printf("%d\n",INTEGER(magic)[0]);
  printf("%s\n",CHAR(STRING_ELT(version,0)));
  printf("%d\n",INTEGER(index_offset)[0]);
  printf("%d\n",INTEGER(index_len)[0]);
  printf("%d\n",INTEGER(number_of_reads)[0]);
  printf("%d\n",INTEGER(header_len)[0]);
  printf("%d\n",INTEGER(key_len)[0]);
  printf("%d\n",INTEGER(flow_len)[0]);
  printf("%d\n",INTEGER(flowgram_format)[0]);
  printf("%s\n",CHAR(STRING_ELT(flow,0)));
  printf("%s\n",CHAR(STRING_ELT(key,0)));
  */
  
  if (!(IS_S4_OBJECT(sread) && 
          strcmp(get_classname(sread), "DNAStringSet") == 0))
        Rf_error("'%s' must be '%s'", "sread", "DNAStringSet");
  
  if (!(IS_S4_OBJECT(quality) && 
          strcmp(get_classname(quality), "BStringSet") == 0)) // //BStringSet
        Rf_error("'%s' must be '%s'", "quality", "BStringSet");
  
  
  const int len = get_XStringSet_length(sread);
  
  
  if ((len != get_XStringSet_length(quality)))
  {
        Rf_error("length() of %s must all be equal",
                 "'sread', 'quality'");
  } 
  
  cachedXStringSet xsread = cache_XStringSet(sread);
  cachedXStringSet xquality = cache_XStringSet(quality);
  int width = 2048;
  char *sreadbuf = (char *) R_alloc(sizeof(char), width + 1);
  char *qualbuf = (char *) R_alloc(sizeof(char), width + 1);
  cachedCharSeq seq_i, qual_i;
  
  //Clip points
  cachedIRanges iqualityClip = cache_IRanges(qualityClip);
  cachedIRanges iadapterClip = cache_IRanges(adapterClip);
  int qualityClip_i_start, qualityClip_i_end, adapterClip_i_start, adapterClip_i_end ;
  
  //Common header
  FILE *fp = fopen( CHAR(STRING_ELT(filename,0)), "w");
  
  COMMONheader commonHeader;
  commonHeader.magic_number = INTEGER(magic)[0];
  //commonHeader.index_offset = INTEGER(index_offset)[0];
  commonHeader.index_length = INTEGER(index_len)[0];
  commonHeader.number_of_reads = INTEGER(number_of_reads)[0];
  commonHeader.commonHeader_length = INTEGER(header_len)[0];
  commonHeader.key_length = INTEGER(key_len)[0];
  commonHeader.number_of_flows_per_read = INTEGER(flow_len)[0];
  commonHeader.flowgram_format_code = INTEGER(flowgram_format)[0];
 
  commonHeader.flow_chars = (char*) malloc(sizeof(char)*(commonHeader.number_of_flows_per_read+1));
  if (commonHeader.flow_chars==NULL) Rf_error("cannot allocate memory");
 
  int i;
  for(i=0; i<commonHeader.number_of_flows_per_read; i++) 
  {
    commonHeader.flow_chars[i] = CHAR(STRING_ELT(flow,0))[i];
  }
  commonHeader.flow_chars[i] = '\0';
  // key sequence
  // malloc must be freed before the function ends
  commonHeader.key_sequence = (char*) malloc(sizeof(char)*(commonHeader.key_length+1));
  if (commonHeader.key_sequence==NULL) Rf_error("cannot allocate memory");
  for(i=0; i<commonHeader.key_length; i++) 
  {
    commonHeader.key_sequence[i] = CHAR(STRING_ELT(key,0))[i];
  }
  commonHeader.key_sequence[i] = '\0';

  //write_sff_common_header(fp, &commonHeader);
  int header_size = sizeof(commonHeader.magic_number)
                  + sizeof(char)*4 //for version
                  + sizeof(commonHeader.index_offset) 
                  + sizeof(commonHeader.index_length) 
                  + sizeof(commonHeader.number_of_reads) 
                  + sizeof(commonHeader.commonHeader_length) 
                  + sizeof(commonHeader.key_length)  
                  + sizeof(commonHeader.number_of_flows_per_read) 
                  + sizeof(commonHeader.flowgram_format_code)
                  + (sizeof(char) * commonHeader.number_of_flows_per_read )
                  + (sizeof(char) * commonHeader.key_length ) ;
    
    if ( !(header_size % PADDING_SIZE == 0) ) {
        header_size += PADDING_SIZE - (header_size % PADDING_SIZE);
    }
  //printf("%d\n",header_size);
  fseek(fp,header_size,SEEK_SET);
  //double *flows = (double*)malloc(sizeof(double)*INTEGER(flow_len)[0]*len);
  
  //flows = REAL(VECTOR_ELT(flowgrams,0));
  
  int index = 0;
  for(int ii=0; ii<len; ++ii)
  {
    // extract XString i from the XStringSet
    seq_i = get_cachedXStringSet_elt(&xsread, ii);
    qual_i = get_cachedXStringSet_elt(&xquality, ii);
    
    //Extract clip points
    qualityClip_i_start = get_cachedIRanges_elt_start(&iqualityClip, ii);
    qualityClip_i_end = get_cachedIRanges_elt_end(&iqualityClip, ii);
    adapterClip_i_start = get_cachedIRanges_elt_start(&iadapterClip, ii);
    adapterClip_i_end = get_cachedIRanges_elt_end(&iadapterClip, ii);
    
    READheader readHeader;
    readHeader.number_of_bases = seq_i.length;
    readHeader.clip_qual_left = qualityClip_i_start;
    readHeader.clip_qual_right = qualityClip_i_end;
    readHeader.clip_adapter_left = adapterClip_i_start;
    readHeader.clip_adapter_right = adapterClip_i_end;
    readHeader.name = CHAR(STRING_ELT(names,ii));
    readHeader.name_length = strlen(readHeader.name);
    
    readHeader.read_header_length = sizeof(readHeader.name_length)
                                    + sizeof(readHeader.number_of_bases)
                                    + sizeof(readHeader.clip_qual_left)
                                    + sizeof(readHeader.clip_qual_right)
                                    + sizeof(readHeader.clip_adapter_left)
                                    + sizeof(readHeader.clip_adapter_right)
                                    + (sizeof(char) * readHeader.name_length);
    
    
    
    if ( !( readHeader.read_header_length % 8 == 0) )
    {
      int remainder = 8 - (readHeader.read_header_length % 8);
      for(int i=0; i< remainder; ++i)
        readHeader.read_header_length += 1;
    }
    
    int nbases = readHeader.number_of_bases;
    int nlen = readHeader.name_length;
    
    write_sff_read_header(fp, &readHeader );
    
    //Read data
    READdata readData;
    
    int j;
    
    readData.flows = (double*)malloc( sizeof(double)*(INTEGER(flow_len)[0]) );
    for (j = 0; j < INTEGER(flow_len)[0]; ++j)
      readData.flows[j] = REAL(VECTOR_ELT(flowgrams,ii))[j];//flows[index+j];
    
    index += INTEGER(flow_len)[0];
    //printf("!\n");
    readData.index = (int*)malloc( sizeof(int)*(seq_i.length) );
    for (j = 0; j < seq_i.length; j++)
      readData.index[j] = INTEGER(VECTOR_ELT(flowIndices,ii))[j];//readData.indices[j] = (uint8_t)INTEGER(VECTOR_ELT(flowIndices,ii))[j];
    //printf("!\n");
    readData.quality = (char*)malloc( sizeof(char)*(qual_i.length+1) );
    for (j = 0; j < qual_i.length; j++)
      readData.quality[j] = qual_i.seq[j];
    readData.quality[j] = '\0';
    
    readData.bases = (char*)malloc( sizeof(char)*(seq_i.length+1) );
    for (j = 0; j < seq_i.length; j++)
      readData.bases[j] = DNAdecode(seq_i.seq[j]);
    readData.bases[j] = '\0';
    
    
    write_sff_read_data(fp, INTEGER(flow_len)[0], nbases, &readData, nlen);
    
    //commonHeader.index_offset = INTEGER(index_offset)[0];
        
    free(readData.bases);
    free(readData.quality);
    free(readData.flows);
    free(readData.index);
  }
  
  int msize = LENGTH( VECTOR_ELT(manifest,0) );
  
  char *mdata = (char*)malloc(sizeof(char)*msize);
  for (int i = 0; i < msize; i++) {
      mdata[i] = (char) RAW(VECTOR_ELT(manifest,0))[i];
  }
  
  commonHeader.index_offset = ftell(fp);//be64toh( ftell(fp) );
  //printf("%d\n",commonHeader.index_offset);
  write_manifest(fp, mdata, msize);
  fseek(fp, 0, SEEK_SET); // seek back to beginning of file
  write_sff_common_header(fp, &commonHeader);
  
  fclose(fp);
  
  free(mdata);
  free(commonHeader.flow_chars);
  free(commonHeader.key_sequence);
  
  return R_NilValue;
    
}

void write_sff_common_header(FILE *fp, COMMONheader *ch)
{
    char v[4] = {0x00,0x00,0x00,0x01};
    uint32_t mnum = ntohl(ch->magic_number);//be32toh(ch->magic_number);
    uint64_t ind_off = htonll(ch->index_offset);//be64toh(ch->index_offset);
    uint32_t ind_len = ntohl(ch->index_length);//be32toh(ch->index_length);
    uint32_t num_reads = ntohl(ch->number_of_reads);//be32toh(ch->number_of_reads);
    uint16_t head_len = ntohs(ch->commonHeader_length);//be16toh(ch->commonHeader_length);
    uint16_t k_len = ntohs(ch->key_length);//be16toh(ch->key_length);
    uint16_t f_len = ntohs(ch->number_of_flows_per_read);//be16toh(ch->number_of_flows_per_read);
    uint8_t f_frmt = 0x01;
    
    fwrite(&mnum, sizeof(uint32_t), 1, fp);
    fwrite(&v, sizeof(char) , 4, fp);
    fwrite(&ind_off, sizeof(uint64_t), 1, fp);
    fwrite(&ind_len, sizeof(uint32_t), 1, fp);
    fwrite(&num_reads, sizeof(uint32_t), 1, fp);
    fwrite(&head_len, sizeof(uint16_t), 1, fp);
    fwrite(&k_len, sizeof(uint16_t), 1, fp);
    fwrite(&f_len, sizeof(uint16_t), 1, fp);
    fwrite(&f_frmt, sizeof(uint8_t) , 1, fp);
    fwrite(ch->flow_chars, sizeof(char), ch->number_of_flows_per_read, fp);
    fwrite(ch->key_sequence, sizeof(char) , ch->key_length, fp);
    
    int header_size = sizeof(ch->magic_number)
                + sizeof(v)
                + sizeof(ch->index_offset)
                + sizeof(ch->index_length)
                + sizeof(ch->number_of_reads)
                + sizeof( ch->commonHeader_length )
                + sizeof(ch->key_length)
                + sizeof(ch->number_of_flows_per_read)
                + sizeof(ch->flowgram_format_code)
                + (sizeof(char) * ch->number_of_flows_per_read )
                + (sizeof(char) * ch->key_length );

    if ( !(header_size % PADDING_SIZE == 0) )
    {
      write_padding(fp, header_size); //printf("%d\n",PADDING_SIZE - (header_size % PADDING_SIZE));
    }
}

void write_sff_read_header(FILE *fp, READheader *rh) {
   rh->read_header_length=ntohs(rh->read_header_length);//be16toh(rh->read_header_length);
   rh->name_length= ntohs(rh->name_length); //be16toh(rh->name_length);
   rh->number_of_bases=ntohl(rh->number_of_bases);//be32toh(rh->number_of_bases);
   rh->clip_qual_left= ntohs(rh->clip_qual_left);//be16toh(rh->clip_qual_left);
   rh->clip_qual_right= ntohs(rh->clip_qual_right);//be16toh(rh->clip_qual_right);
   rh->clip_adapter_left= ntohs(rh->clip_adapter_left);//be16toh(rh->clip_adapter_left);
   rh->clip_adapter_right= ntohs(rh->clip_adapter_right);//be16toh(rh->clip_adapter_right);
   
   fwrite(&(rh->read_header_length) , sizeof(uint16_t), 1, fp);
   fwrite(&(rh->name_length) , sizeof(uint16_t), 1, fp);
   fwrite(&(rh->number_of_bases) , sizeof(uint32_t), 1, fp);
   fwrite(&(rh->clip_qual_left) , sizeof(uint16_t), 1, fp);
   fwrite(&(rh->clip_qual_right) , sizeof(uint16_t), 1, fp);
   fwrite(&(rh->clip_adapter_left) , sizeof(uint16_t), 1, fp);
   fwrite(&(rh->clip_adapter_right) , sizeof(uint16_t), 1, fp);
   //fwrite(rh->name , sizeof(char), htobe16(rh->name_length), fp);
   fwrite(rh->name , sizeof(char), htons(rh->name_length), fp);
   /*
   int header_size = sizeof(htobe16((*rh).read_header_length))
                  + sizeof(htobe16((*rh).name_length))
                  + sizeof(htobe32((*rh).number_of_bases))
                  + sizeof(htobe16((*rh).clip_qual_left))
                  + sizeof(htobe16((*rh).clip_qual_right))
                  + sizeof(htobe16((*rh).clip_adapter_left))
                  + sizeof(htobe16((*rh).clip_adapter_right))
                  + (sizeof(char) * htobe16((*rh).name_length));
   */
   int header_size = sizeof((*rh).read_header_length)
                  + sizeof((*rh).name_length)
                  + sizeof((*rh).number_of_bases)
                  + sizeof((*rh).clip_qual_left)
                  + sizeof((*rh).clip_qual_right)
                  + sizeof((*rh).clip_adapter_left)
                  + sizeof((*rh).clip_adapter_right)
                  + (sizeof(char) * htons((*rh).name_length));
   
   if ( !(header_size % PADDING_SIZE == 0) ) {
        write_padding(fp, header_size); 
   }
    
}

void write_sff_read_data(FILE *fp, int nflows, int nbases, READdata *rd, int name_len)
{
    uint16_t *flowgram;
    uint8_t *flow_index;
    
    /* sff files are in big endian notation so adjust appropriately */
    /* allocate the appropriate arrays */
    flowgram   = (uint16_t *) malloc( nflows * sizeof(uint16_t) );
    if (!flowgram) {
        fprintf(stderr,
                "Out of memory! Could not allocate for a read flowgram!\n");
        exit(1);
    }
    
    for (int i = 0; i < nflows; i++) {
       //flowgram[i] = be16toh(rd->flows[i]*100.0);
       flowgram[i] = ntohs(rd->flows[i]*100.0);
    }
    
    flow_index = (uint8_t *) malloc( nbases * sizeof(uint8_t)  );
    if (!flow_index) {
        fprintf(stderr,
                "Out of memory! Could not allocate for a read flow index!\n");
        exit(1);
    }
    
    flow_index[0] = rd->index[0];
    for(int i=1; i<nbases; i++) 
    {
        flow_index[i] =  (uint8_t)(rd->index[i] - rd->index[i-1])  ;
    }
    
    for(int i=0; i<nbases; i++)
    {
      rd->quality[i] = CharToPhred(rd->quality[i]);
    }
    
    fwrite(flowgram, sizeof(uint16_t), (size_t) nflows, fp);
    fwrite(flow_index, sizeof(uint8_t), (size_t) nbases, fp);
    fwrite(rd->bases, sizeof(char), (size_t) nbases, fp);
    fwrite(rd->quality, sizeof(uint8_t), (size_t) nbases, fp);
    //
    //the section should be a multiple of 8-bytes, if not,
    // it is zero-byte padded to make it so

    int data_size = (sizeof(uint16_t) * nflows) // flowgram size
                + (sizeof(uint8_t) * nbases) // flow_index size
                + (sizeof(char) * nbases) // bases size
                + (sizeof(char) * nbases); // quality size
                  
    if ( !(data_size % PADDING_SIZE == 0) )
    {
        write_padding(fp, data_size);
    }
}


void write_padding(FILE *fp, int header_size)
{
    int remainder = PADDING_SIZE - (header_size % PADDING_SIZE);
    uint8_t padding[remainder];
    int i;
    for(i=0; i< remainder; ++i)
      padding[i] = 0;
    
    
    fwrite(padding, sizeof(uint8_t), remainder, fp);
}


SEXP readOneSFFManifest (FILE *fp, long index_offset)
{
    long cur_pos = ftell(fp); // get current file pointer
    
    fseek ( fp, index_offset, SEEK_SET );
    
    long manifest_size = sff_file_size - index_offset;//ftell( fp );
    //printf("%d !!!\n",manifest_size);
    char* manifest_data = (char*)malloc( manifest_size * sizeof(char) );
    if (!manifest_data)
    {
        fprintf(stderr,
                "Out of memory! Could not allocate header flow string!\n");
        exit(1);
    }
    
    int fres = fread(manifest_data, sizeof(char), manifest_size, fp);
    
    SEXP mdata = allocVector(RAWSXP, manifest_size);
    memcpy(RAW(mdata), manifest_data, manifest_size);
    
    fseek(fp, cur_pos, SEEK_SET); //seek bac
    
    free(manifest_data);
    
    return mdata;
}


void write_manifest(FILE *fp, char *manifest, int msize)
{
  fwrite(manifest, sizeof(char), msize, fp);
}

void get_sff_file_size(FILE *fp)
{
  fseek(fp, 0, SEEK_END); // seek to end of file
  sff_file_size = ftell(fp); // get current file pointer
  fseek(fp, 0, SEEK_SET); // seek back to beginning of file
// proceed with allocating memory and reading the file
}

unsigned long long htonll(unsigned long long src) { 
  static int typ = TYP_INIT; 
  unsigned char c; 
  union { 
    unsigned long long ull; 
    unsigned char c[8]; 
  } x; 
  if (typ == TYP_INIT) { 
    x.ull = 0x01; 
    typ = (x.c[7] == 0x01ULL) ? TYP_BIGE : TYP_SMLE; 
  } 
  if (typ == TYP_BIGE) 
    return src; 
  x.ull = src; 
  c = x.c[0]; x.c[0] = x.c[7]; x.c[7] = c; 
  c = x.c[1]; x.c[1] = x.c[6]; x.c[6] = c; 
  c = x.c[2]; x.c[2] = x.c[5]; x.c[5] = c; 
  c = x.c[3]; x.c[3] = x.c[4]; x.c[4] = c; 
  return x.ull; 
} 


#ifdef __cplusplus
}
#endif

