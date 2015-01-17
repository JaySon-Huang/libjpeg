#include <stdlib.h>
#include <stdio.h>
#include <jpeglib.h>

#include <setjmp.h>

#include <assert.h>

typedef struct backing_store_struct * backing_store_ptr;
typedef struct backing_store_struct {
  /* Methods for reading/writing/closing this backing-store object */
  void (*read_backing_store) (j_common_ptr cinfo,
                     backing_store_ptr info,
                     void FAR * buffer_address,
                     long file_offset, long byte_count);
  void (*write_backing_store) (j_common_ptr cinfo,
                      backing_store_ptr info,
                      void FAR * buffer_address,
                      long file_offset, long byte_count);
  void (*close_backing_store) (j_common_ptr cinfo,
                      backing_store_ptr info);

  /* Private fields for system-dependent backing-store management */
  /* For a typical implementation with temp files, we need: */
  FILE * temp_file;     /* stdio reference to temp file */
  char temp_name[64]; /* name of temp file */
} backing_store_info;

// `jvirt_barray_control`类型定义
// 此类型用在获取 进行DCT变换后的量化系数 的方法的返回值中
struct jvirt_barray_control {
  JBLOCKARRAY mem_buffer;   /* => the in-memory buffer 指向内存缓冲区 */
  JDIMENSION rows_in_array; /* total virtual array height  */
  JDIMENSION blocksperrow;  /* width of array (and of memory buffer) */
  JDIMENSION maxaccess;     /* max rows accessed by access_virt_barray */
  JDIMENSION rows_in_mem;   /* height of memory buffer 在内存中内容的高度 */
  JDIMENSION rowsperchunk;  /* allocation chunk size in mem_buffer */
  JDIMENSION cur_start_row; /* first logical row # in the buffer 内存中内容的第一行的行号 */
  JDIMENSION first_undef_row;   /* row # of first uninitialized row 第一个没有初始化的行的行号，可以看作是含数据的最后一行的后一行 */
  boolean pre_zero;     /* pre-zero mode requested? */
  // 有 `dirty` 字段, 应该有相应的读函数来处理读出数据. 在本文件844行
  boolean dirty;        /* do current buffer contents need written? 是否"脏"数据,需要写回? */
  boolean b_s_open;     /* is backing-store data valid? 有没有恢复的数据 */
  jvirt_barray_ptr next;    /* link to next virtual barray control block 下一块数据块 */
  backing_store_info b_s_info;  /* System-dependent control info 恢复数据的内容 */
};


struct error_mgr_t {
  struct jpeg_error_mgr pub;    /* "public" fields */

  jmp_buf setjmp_buffer;    /* for return to caller */
};
typedef struct error_mgr_t * error_ptr;

int
read_JPEG_file (const char * filename);

METHODDEF(void)
my_error_exit (j_common_ptr pcinfo)
{
  /* pcinfo->err really points to a error_mgr_t struct, so coerce pointer */
  error_ptr myerr = (error_ptr) pcinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*pcinfo->err->output_message) (pcinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

int main(int argc, char const *argv[])
{
    const char *filename="lfs.jpg";
    read_JPEG_file(filename);
    return 0;
}

int
read_JPEG_file (const char * filename)
{
    FILE * infile = NULL;
    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        return 0;
    }

    struct jpeg_decompress_struct cinfo;
    struct error_mgr_t jerr;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }
    
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    (void) jpeg_read_header(&cinfo, TRUE);

    printf("Width:%4d, Height:%4d\n", cinfo.image_width, cinfo.image_height );
    printf("Components:%2d\n", cinfo.num_components);
    printf("Color space:");
    switch (cinfo.jpeg_color_space)
    {
    case JCS_GRAYSCALE:
        printf("Grey\n");
        break;
    case JCS_RGB:
        printf("RGB\n");
        break;
    case JCS_YCbCr:
        printf("YCbCr(YUV/YCC)\n");
        break;

    default:
        assert(0);
        break;
    }
    jvirt_barray_ptr *coef_arrays = jpeg_read_coefficients(&cinfo);
    for (int ci=0; ci != cinfo.num_components; ++ci) {
        printf("\n======\n");
        // compptr指向第ci个颜色分量的信息
        jpeg_component_info *compptr = cinfo.comp_info + ci;
        printf("component: %d\n", compptr->component_id);
        printf("MCU width,height: (%2d,%2d)\n", compptr->MCU_width, compptr->MCU_height);

        jvirt_barray_ptr com_coef_array = coef_arrays[ci];
        /*
        JBLOCKARRAY block_array = (cinfo.mem->access_virt_barray)
            ((j_common_ptr) &cinfo, com_coef_array,
                0, com_coef_array->rows_in_array,
                FALSE);
        会导致 num_rows > ptr->maxaccess, 导致`JERR_BAD_VIRTUAL_ACCESS`.具体原因不明.
        */
        // 粗暴地访问内存
        printf("Cur jvirt_barray_ptr @%p\n", com_coef_array);
        printf("  rows_in_mem: %d\n", com_coef_array->rows_in_mem);
        printf("  rows_in_array: %d\n", com_coef_array->rows_in_array);
        printf("  blocks_per_row: %d\n", com_coef_array->blocksperrow);
        printf("  cur_start_row: %d\n", com_coef_array->cur_start_row);
        JBLOCKARRAY block_array = com_coef_array->mem_buffer;
        for (int row=0; row != com_coef_array->rows_in_mem; ++row){
            for (int col=0; col != com_coef_array->blocksperrow; ++col){
                JCOEF *pcoef = block_array[row][col];
                // printf("[");
                // for (int i=0; i != DCTSIZE2; ++i){
                //   if (i!=0&&i%8==0){ printf("\n"); }
                //   printf("%3d,\n", pcoef[i]);
                // }
                // printf("]\n");
            }
        }
        printf("%d Coef blocks in Components %d. \n",
            (com_coef_array->rows_in_mem)*(com_coef_array->blocksperrow)
            , ci);
    }

    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return 1;
}

