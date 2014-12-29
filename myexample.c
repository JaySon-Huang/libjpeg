#include <stdlib.h>
#include <stdio.h>
#include <jpeglib.h>

#include <setjmp.h>

#include <assert.h>

struct error_mgr_t {
  struct jpeg_error_mgr pub;    /* "public" fields */

  jmp_buf setjmp_buffer;    /* for return to caller */
};
typedef struct error_mgr_t * error_ptr;

int
read_JPEG_file (const char * filename, const char *output_filename);

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

#define PUT_2B(array,offset,value)  \
        (array[offset]   = (char) ((value) & 0xFF), \
         array[offset+1] = (char) (((value) >> 8) & 0xFF))

#define PUT_4B(array,offset,value)  \
        (array[offset]   = (char) ((value) & 0xFF), \
         array[offset+1] = (char) (((value) >> 8) & 0xFF), \
         array[offset+2] = (char) (((value) >> 16) & 0xFF), \
         array[offset+3] = (char) (((value) >> 24) & 0xFF))

void write_bmp_header(
    j_decompress_ptr pcinfo, 
    FILE *output_file);

void write_pixel_data(
    j_decompress_ptr pcinfo, 
    unsigned char *output_buffer, 
    FILE *output_file);

int main(int argc, char const *argv[])
{
    const char *filename="lfs.jpg";
    const char *output_filename = "lfs.bmp";
    read_JPEG_file(filename, output_filename);
    return 0;
}

int
read_JPEG_file (const char * filename, const char *output_filename)
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
    (void) jpeg_start_decompress(&cinfo);

    FILE *outfile = NULL;
    if ((outfile = fopen(output_filename, "wb")) == NULL){
        fprintf(stderr, "Can't open %s\n", output_filename);
        return 0;
    }
    write_bmp_header(&cinfo, outfile);


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

    unsigned char *output_buffer = 
        (unsigned char *)malloc(cinfo.output_width * cinfo.output_components * cinfo.output_height);
    unsigned char *next_pos = output_buffer;
    memset(output_buffer, 0, sizeof(char)*cinfo.output_width * cinfo.output_components * cinfo.output_height);

    int row_stride;       /* physical row width in output buffer */
    JSAMPARRAY buffer;        /* Output row buffer */
    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(next_pos, *buffer, cinfo.output_width*cinfo.output_components);
        next_pos += cinfo.output_width*cinfo.output_components;
    }

    write_pixel_data(&cinfo, output_buffer, outfile);

    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return 1;
}

void write_bmp_header(
    j_decompress_ptr pcinfo, 
    FILE *output_file)
{
    char bmpfileheader[14];
    char bmpinfoheader[40];
    long headersize, bfSize;
    int bits_per_pixel, cmap_entries;
    int step;
    /* Compute colormap size and total file size */
    if (pcinfo->out_color_space == JCS_RGB) {
        if (pcinfo->quantize_colors) {
            /* Colormapped RGB */
            bits_per_pixel = 8;
            cmap_entries = 256;
        } else {
            /* Unquantized, full color RGB */
            bits_per_pixel = 24;
            cmap_entries = 0;
        }
    } else {
        /* Grayscale output.  We need to fake a 256-entry colormap. */
        bits_per_pixel = 8;
        cmap_entries = 256;
    }
    step = pcinfo->output_width * pcinfo->output_components;
    while ((step & 3) != 0) step++;

    /* File size */
    headersize = 14 + 40 + cmap_entries * 4; /* Header and colormap */
    bfSize = headersize + (long) step * (long) pcinfo->output_height;
    /* Set unused fields of header to 0 */
    memset(bmpfileheader, 0, sizeof(char)*14);
    memset(bmpinfoheader, 0 ,sizeof(char)*40);

    /* 填充BMP文件头 */
    /* 首部两个字节 'B'、'M' */
    bmpfileheader[0] = 0x42;
    bmpfileheader[1] = 0x4D;
    /* bfSize 4Bytes 整个BMP文件的大小（以位B为单位） */
    PUT_4B(bmpfileheader, 2, bfSize); 
    /* bfReserved1 2Bytes 保留，必须设置为0
       bfReserved2 2Bytes 保留，必须设置为0 */

    /* bfOffBits从文件头0000h开始到图像像素数据的字节偏移量（以字节Bytes为单位），
       以为位图的调色板长度根据位图格式不同而变化，可以用这个偏移量快速从文件中读取图像数据 */
    PUT_4B(bmpfileheader, 10, headersize); 

    /* 填充BMP信息头 */
    /* biSize 4Bytes BMP信息头结构体所需要的字节数（以字节为单位） */
    PUT_2B(bmpinfoheader, 0, 40);
    /* biWidth 4Bytes 说明图像的宽度（以像素为单位） */
    PUT_4B(bmpinfoheader, 4, pcinfo->output_width);
    /* biHeight 4Bytes 说明图像的高度（以像素为单位）。*/
    /*   这个值还有一个用处，指明图像是正向的位图还是倒向的位图。
         正数说明图像是倒向的即图像存储是由下到上；
         负数说明图像是倒向的即图像存储是由上到下。
         大多数BMP位图是倒向的位图，所以此值是正值。*/
    PUT_4B(bmpinfoheader, 8, pcinfo->output_height);
    /* biPlanes 2Bytes 为目标设备说明位面数，其值总设置为1 */
    PUT_2B(bmpinfoheader, 12, 1);
    /* biBitCount 2Bytes 说明一个像素点占几位（以比特位/像素位单位），其值可为1,4,8,16,24或32 */
    PUT_2B(bmpinfoheader, 14, bits_per_pixel);
    /* biCompression 4Bytes 这里留为0 */
    /* 说明图像数据的压缩类型，取值范围为：
        0 BI_RGB 不压缩（最常用）
        1 BI_RLE8 8比特游程编码（BLE），只 用于8位位图
        2 BI_RLE4 4比特游程编码（BLE），只 用于4位位图
        3 BI_BITFIELDS比特域  （BLE），只用 于16/32位位图 */

    /* biSizeImage 4Bytes 说明图像的大小，以字节为单位。当用BI_RGB格式时，总设置为0 */

    if (pcinfo->density_unit == 2) { /* if have density in dots/cm, then */
        /* biXPelsPerMeter 4Bytes 说明水平分辨率，用像素/米表示，有符号整数 */
        PUT_4B(bmpinfoheader, 24, (INT32) (pcinfo->X_density*100)); /* XPels/M */
        /* biYPelsPerMeter 4Bytes 说明垂直分辨率，用像素/米表示，有符号整数 */
        PUT_4B(bmpinfoheader, 28, (INT32) (pcinfo->Y_density*100)); /* XPels/M */
    }
    /* biClrUsed 4Bytes 说明位图实际使用的调色板索引数
         0：使用所有的调色板索引 */
    PUT_2B(bmpinfoheader, 32, cmap_entries);
    /* biClrImportant 4Bytes 说明对图像显示有重要影响的颜色索引的数目，如果是0，表示都重要。*/

    if (fwrite(bmpfileheader, 1, 14, output_file) != (size_t) 14) {
        fprintf(stderr, "Error write bmpfileheader\n");
        exit(1);
    }
    if (fwrite(bmpinfoheader, 1, 40, output_file) != (size_t) 40) {
        fprintf(stderr, "Error write bmpinfoheader\n");
        exit(1);
    }

    // BMP调色板数据
    if (cmap_entries > 0) {

    }
}

void write_pixel_data(
    j_decompress_ptr pcinfo, 
    unsigned char *output_buffer, 
    FILE *output_file)
{
    int row_width = pcinfo->output_width * pcinfo->output_components;
    int step = row_width;
    /* 像素的数据量并不一定完全等于图象的高度乘以宽度乘以每一像素的字节数，而是可能略大于这个值。
        原因是BMP文件采用了一种“对齐” 的机制，每一行像素数据的长度若不是4的倍数，则填充一些数据
        使它是4的倍数 */
    while ((step & 3) != 0) step++;

    // 要写入的数据缓冲区(一行)
    unsigned char *pdata = (unsigned char *)malloc(step);
    memset(pdata, 0, step);

    // 从下往上扫描
    unsigned char *tmp = output_buffer + row_width * (pcinfo->output_height - 1);
    for (int rows = 0; rows < pcinfo->output_height; rows++) {
        for (int cols = 0; cols < row_width; cols += 3) {
            // BMP 采用BGR而不是RGB
            pdata[cols + 2] = tmp[cols + 0];
            pdata[cols + 1] = tmp[cols + 1];
            pdata[cols + 0] = tmp[cols + 2];
        }
        tmp -= row_width;
        fwrite(pdata, 1, step, output_file);
    }

    free(pdata);
}
