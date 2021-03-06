#include "aok.h"
#include <zlib.h>

#define CHUNK 16384
struct Zfile
{
	FILE *f;
	int inflating;
	z_stream strm;
	uchar inbuf[CHUNK];
	uchar outbuf[CHUNK];
	int nout;
	uchar *outp;
	int pos;
};

/*
static void
dumpbytes(void *buf, int n)
{
	uchar *b;
	int i;
	b = (uchar*)buf;
	for(i = 0; i < n; i++){
		printf("%02X ", b[i]);
		if(i%16 == 15)
			printf("\n");
	}
	printf("\n");
}
*/

void
zfclose(Zfile *zf)
{
	if(zf){
		fclose(zf->f);
		if(zf->inflating)
			inflateEnd(&zf->strm);
		free(zf);
	}
}

Zfile*
zfopen(const char *path, const char *mode)
{
	FILE *f;
	Zfile *zf;
	int ret;

	f = fopen(path, mode);
	if(f == nil)
		return nil;
	zf = malloc(sizeof(Zfile));
	zf->f = f;
	zf->inflating = 0;
	zf->strm.zalloc = Z_NULL;
	zf->strm.zfree = Z_NULL;
	zf->strm.opaque = Z_NULL;
	zf->strm.avail_in = 0;
	zf->strm.next_in = Z_NULL;
	ret = inflateInit2(&zf->strm, -15);
	if (ret != Z_OK){
		zfclose(zf);
		return nil;
	}
	zf->inflating = 1;
	zf->strm.avail_out = CHUNK;
	zf->strm.next_out = zf->outbuf;
	zf->outp = zf->outbuf;
	zf->nout = 0;
	zf->pos = 0;

	return zf;
}

int
zread(Zfile *zf, void *dst, int n)
{
	int nread, status;
	nread = 0;

	for(;;){
		if(n <= 0 || !zf->inflating)
			return nread;
	
		/* all requested data uncompressed already */
		if(n <= zf->nout){
			memcpy(dst, zf->outp, n);
			zf->pos += n;
			zf->outp += n;
			zf->nout -= n;
			return nread+n;
		}
	
		/* partially uncompressed already */
		if(zf->nout > 0){
			memcpy(dst, zf->outp, zf->nout);
			zf->pos += zf->nout;
			nread += zf->nout;
			dst += zf->nout;
			n -= zf->nout;
			zf->nout = 0;
		}
	
		/* uncompress more data */
	
		/* need more input */
		if(zf->strm.next_in == Z_NULL){
			zf->strm.avail_in = fread(zf->inbuf, 1, CHUNK, zf->f);
			if(ferror(zf->f) || zf->strm.avail_in == 0){
		endinflate:
				inflateEnd(&zf->strm);
				zf->inflating = 0;
				return nread;
			}
			zf->strm.next_in = zf->inbuf;
		}
	
		/* uncompress */
		zf->strm.avail_out = CHUNK;
		zf->strm.next_out = zf->outbuf;
		status = inflate(&zf->strm, Z_NO_FLUSH);
		assert(status != Z_STREAM_ERROR);
		switch(status){
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				goto endinflate;
		}
		zf->outp = zf->outbuf;
		zf->nout = CHUNK - zf->strm.avail_out;
		if(zf->strm.avail_out){
			zf->strm.avail_in = 0;
			zf->strm.next_in = Z_NULL;
		}
	}
}

int
ztell(Zfile *zf)
{
	return zf->pos;
}
