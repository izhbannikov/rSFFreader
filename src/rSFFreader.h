#ifndef _RSFFREADER_H_
#define _RSFFREADER_H_

#ifdef __cplusplus
extern "C" {
#endif

// R includes
//#include <R.h>
#include <Rdefines.h>
//#include <Rinternals.h> // Rprintf, SEXP
#include <R_ext/Rdynload.h>

// System includes
#include <zlib.h>
#include <stdint.h>		// uint64_t, uint32_t, uint16_t
//#include <architecture/byte_order.h>

#define PADDING_SIZE 8

#define TYP_INIT 0 
#define TYP_SMLE 1 
#define TYP_BIGE 2 

/***************************** SFF file structures *****************************/
typedef struct CommonHeader {
    uint32_t magic_number;
    char version[4];
    uint64_t index_offset;
    uint32_t index_length;
    uint32_t number_of_reads;
    uint16_t commonHeader_length;
    uint16_t key_length;
    uint16_t number_of_flows_per_read;
    uint8_t flowgram_format_code;
    char *flow_chars;// flow_chars omitted
    char *key_sequence;// key_sequence omitted
    SEXP manifest;
    // eight_byte_padding is not necessary
} COMMONheader;

typedef struct ReadHeader {
    uint16_t read_header_length;
    uint16_t name_length;
    uint32_t number_of_bases;
    uint16_t clip_qual_left;
    uint16_t clip_qual_right;
    uint16_t clip_adapter_left;
    uint16_t clip_adapter_right;
    const char *name;
    // eight_byte_padding
} READheader;

typedef struct ReadData {
    double* flows;
    int*   index;
    char*  name;
    char*  quality;
    char*  bases;
} READdata;


SEXP read_sff(
		SEXP files,
		SEXP use_names,
    SEXP use_flowgrams,
		SEXP lkup_seq,
		SEXP lkup_qual,
		SEXP verbose
);

SEXP read_sff_header(
		SEXP files,
		SEXP verbose
);

SEXP sff_geometry(
		SEXP files
);

SEXP write_phred_quality(
		SEXP id,
		SEXP quality, 
		SEXP fname,
		SEXP fmode,
		SEXP max_width
);

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
                SEXP manifest);
                
SEXP readOneSFFManifest (FILE *fp, long index_offset);
                
void write_sff_common_header(FILE *fp, COMMONheader *ch) ;
void write_sff_read_header(FILE *fp, READheader *rh);
void write_sff_read_data(FILE *fp, int nflows, int nbases, READdata *rd, int name_len);
void write_padding(FILE *fp, int header_size) ;
void write_manifest(FILE *fp, char *manifest, int msize);
void get_sff_file_size(FILE *fp);
unsigned long long htonll(unsigned long long src);

#ifdef __cplusplus
}
#endif

#endif /* _RSFFHEADER_H_ */

