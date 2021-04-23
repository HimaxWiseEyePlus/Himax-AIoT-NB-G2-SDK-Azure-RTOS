#include "spi_master_protocol.h"
#include "hx_drv_tflm.h"
#include "inc/hii.h"
#include "inc/hx_drv_pmu.h"
#include "inc/tflitemicro_algo.h"
#include "inc/azure_iothub.h"
#include "inc/pmu.h"

#include "tx_api.h"
//datapath boot up reason flag
volatile uint8_t g_bootup_md_detect = 0;
//#define SPI_SEND

#define IMAGE_HEIGHT 	480
#define IMAGE_WIDTH		640
struct_algoResult algo_result;
uint32_t g_imgsize;
unsigned char *g_img_cur_addr_pos;

#ifdef SPI_SEND
static int open_spi()
{
	int ret ;
#ifndef SPI_MASTER_SEND
	ret = hx_drv_spi_slv_open();
	hx_drv_uart_print("SPI slave ");
#else
	ret = hx_drv_spi_mst_open();
	hx_drv_uart_print("SPI master ");
#endif
    return ret;
}

static int spi_write(uint32_t addr, uint32_t size, SPI_CMD_DATA_TYPE data_type)
{
#ifndef SPI_MASTER_SEND
	return hx_drv_spi_slv_protocol_write_simple_ex(addr, size, data_type);
#else
	return hx_drv_spi_mst_protocol_write_sp(addr, size, data_type);
#endif
}
#endif

hx_drv_sensor_image_config_t g_pimg_config;
static bool is_initialized = false;
GetImage(int image_width,int image_height, int channels) {
	int ret = 0;
	//xprintf("is_initialized : %d \n",is_initialized);
	  if (!is_initialized) {
	    if (hx_drv_sensor_initial(&g_pimg_config) != HX_DRV_LIB_PASS) {
	    	xprintf("hx_drv_sensor_initial error\n");
	      return ERROR;
	    }
#ifdef SPI_SEND
	    if (hx_drv_spim_init() != HX_DRV_LIB_PASS) {
	      return ERROR;
	    }
	    ret = open_spi();
#endif
	    is_initialized = true;
		xprintf("is_initialized : %d \n",is_initialized);
	  }

	  //capture image by sensor
	  hx_drv_sensor_capture(&g_pimg_config);

	  g_img_cur_addr_pos = (unsigned char *)g_pimg_config.jpeg_address;
	  g_imgsize = g_pimg_config.jpeg_size;
	  xprintf("g_pimg_config.jpeg_address:0x%x size : %d \n",g_pimg_config.jpeg_address,g_pimg_config.jpeg_size);
#ifdef SPI_SEND
	  //send jpeg image data out through SPI
	  ret = spi_write(g_pimg_config.jpeg_address, g_pimg_config.jpeg_size, DATA_TYPE_JPG);
	  //ret = spi_write(g_pimg_config.raw_address, g_pimg_config.raw_size, DATA_TYPE_RAW_IMG);
#endif
	  return OK;
}

int img_cnt = 0;

void tflitemicro_start() {
	GetImage(IMAGE_WIDTH,IMAGE_HEIGHT,1);

#ifdef TFLITE_MICRO_GOOGLE_PERSON
	xprintf("tflitemicro_algo_run\n");
	img_cnt++;
	xprintf("### img_cnt:%d ###\n",img_cnt);
	tflitemicro_algo_run(g_pimg_config.raw_address, g_pimg_config.img_width, g_pimg_config.img_height, &algo_result);
	//xprintf("humanPresence %d frame_count %d\n",algo_result.humanPresence,algo_result.frame_count);
	xprintf("[MetaData]\nhumanPresence:%d\n",algo_result.humanPresence);
	xprintf("det_box_x:%d\ndet_box_y:%d\ndet_box_width:%d\ndet_box_height:%d\n",\
			algo_result.ht[0].upper_body_bbox.x,\
			algo_result.ht[0].upper_body_bbox.y, algo_result.ht[0].upper_body_bbox.width, \
			algo_result.ht[0].upper_body_bbox.height);
#endif

	/* azure_active_event
	 * ALGO_EVENT_SEND_RESULT_TO_CLOUD :Send Algorithm Metadata.
	 * ALGO_EVENT_SEND_IMAGE_TO_CLOUD  :Send Image.
	 * ALGO_EVENT_SEND_RESULT_AND_IMAGE:Send Metadata and Image.
	 *
	 * Example:Send Metadata After Capture 5 image.
	 * */
	//if(algo_result.humanPresence){
	if(img_cnt == 5){// example
		azure_active_event = ALGO_EVENT_SEND_RESULT_TO_CLOUD;
		img_cnt= 0;
	}
}

void setup(){
#ifdef TFLITE_MICRO_GOOGLE_PERSON
	xprintf("### tflitemicro_algo_init... ###\n");
	tflitemicro_algo_init();
#endif

#ifdef NB_IOT_BOARD
	/*Azure TX Task. */
	xprintf("#############################################################################\n");
	xprintf("**** Enter TX Thread ****\n");
	xprintf("#############################################################################\n");
	nbiot_task_define();
#endif

}

hx_drv_sensor_image_config_t g_pimg_config;
void hx_aiot_nb()
{
	//EnterToPMU(); //for measure power
	setup();
}

