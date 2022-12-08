#ifndef KILITE_AMALGAMATION
#include "common.h"
#endif

/* zip reader */
int32_t mz_zip_reader_is_open(void *handle);
int32_t mz_zip_reader_open(void *handle, void *stream);
int32_t mz_zip_reader_open_file(void *handle, const char *path);
int32_t mz_zip_reader_open_file_in_memory(void *handle, const char *path);
int32_t mz_zip_reader_open_buffer(void *handle, uint8_t *buf, int32_t len, uint8_t copy);
int32_t mz_zip_reader_close(void *handle);

/* Zip */

static int Zip_open(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{

}

/* Creating Zip file directly. */
// static int Zip_create(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
// {

// }

int Zip(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, open, Zip_open, lex, 1);
    KL_SET_METHOD(o, create, Zip_open, lex, 1);
    // KL_SET_METHOD(o, zip, Zip_create, lex, 1)
    SET_OBJ(r, o);
    return 0;
}
