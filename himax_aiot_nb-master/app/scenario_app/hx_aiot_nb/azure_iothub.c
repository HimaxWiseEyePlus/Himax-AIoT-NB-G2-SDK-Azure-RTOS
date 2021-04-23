/*
 * azure_iothub.c
 *
 *  Created on: 2021¦~4¤ë18¤é
 *      Author: 903990
 */


#if 1
#include <addons/azure_iot/nx_azure_iot_hub_client.h>
#include <addons/azure_iot/nx_azure_iot_json_reader.h>
#include <addons/azure_iot/nx_azure_iot_json_writer.h>
#include <addons/azure_iot/nx_azure_iot_provisioning_client.h>
#include <nx_api.h>
#include <nxd_dns.h>
#include <nx_secure_tls_api.h>

#include "inc/nx_azure_iot_cert.h"
#include "inc/nx_azure_iot_ciphersuites.h"
#include "inc/azure_iothub.h"
#include "inc/pmu.h"

#include "azure/core/az_span.h"
#include "azure/core/az_version.h"
#include "tx_port.h"
#include "hx_drv_iomux.h"
#include "external/nb_iot/wnb303r/wnb303r.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define SAMPLE_DHCP_DISABLE

/* unit:BYTE */
#define SEND_PKG_MAX_SIZE 	256

/* Define Enable PMU Mode. */
//#define ENABLE_PMU
/* Setup PMU Sleep time. */
#define PMU_SLEEP_MS	15000 /* unit:ms*/

#define NBIOT_ATCMD_RETRY_MAX_TIMES		10

/* Define AZ IoT Provisioning Client topic format.  */
#define NX_AZURE_IOT_PROVISIONING_CLIENT_POLICY_NAME                  "registration"

/* Useragent e.g: DeviceClientType=c%2F1.0.0-preview.1%20%28nx%206.0%3Bazrtos%206.0%29 */
#define NX_AZURE_IOT_HUB_CLIENT_STR(C)          #C
#define NX_AZURE_IOT_HUB_CLIENT_TO_STR(x)       NX_AZURE_IOT_HUB_CLIENT_STR(x)
#define NX_AZURE_IOT_HUB_CLIENT_USER_AGENT      "DeviceClientType=c%2F" AZ_SDK_VERSION_STRING "%20%28nx%20" \
                                                NX_AZURE_IOT_HUB_CLIENT_TO_STR(NETXDUO_MAJOR_VERSION) "." \
                                                NX_AZURE_IOT_HUB_CLIENT_TO_STR(NETXDUO_MINOR_VERSION) "%3Bazrtos%20"\
                                                NX_AZURE_IOT_HUB_CLIENT_TO_STR(THREADX_MAJOR_VERSION) "." \
                                                NX_AZURE_IOT_HUB_CLIENT_TO_STR(THREADX_MINOR_VERSION) "%29"


#ifndef NBIOT_SERVICE_STACK_SIZE
#define NBIOT_SERVICE_STACK_SIZE        (4096)
#endif /* SAMPLE_HELPER_STACK_SIZE  */

#ifndef ALGO_SEND_RESULT_STACK_SIZE
#define ALGO_SEND_RESULT_STACK_SIZE     (4096)
#endif /* ALGO_SEND_RESULT_STACK_SIZE  */

#ifndef CIS_CAPTURE_IMAGE_STACK_SIZE
#define CIS_CAPTURE_IMAGE_STACK_SIZE     (4096)
#endif /* CIS_CAPTURE_IMAGE_STACK_SIZE  */

#ifndef SAMPLE_HELPER_THREAD_PRIORITY
#define SAMPLE_HELPER_THREAD_PRIORITY   (4)
#endif /* SAMPLE_HELPER_THREAD_PRIORITY  */

#ifndef NBIOT_SERVICE_THREAD_PRIORITY
#define NBIOT_SERVICE_THREAD_PRIORITY   (5)
#endif /* NBIOT_SERVICE_THREAD_PRIORITY  */

#ifndef ALGO_SEND_RESULT_THREAD_PRIORITY
#define ALGO_SEND_RESULT_THREAD_PRIORITY   (6)
#endif /* ALGO_SEND_RESULT_THREAD_PRIORITY  */

#ifndef CIS_CAPTURE_IMAGE_THREAD_PRIORITY
#define CIS_CAPTURE_IMAGE_THREAD_PRIORITY   (7)
#endif /* CIS_CAPTURE_IMAGE_THREAD_PRIORITY  */

/* Define user configurable symbols. */
#ifndef SAMPLE_IP_STACK_SIZE
#define SAMPLE_IP_STACK_SIZE            (2048)
#endif /* SAMPLE_IP_STACK_SIZE  */

#ifndef SAMPLE_PACKET_COUNT
#define SAMPLE_PACKET_COUNT             (32)
#endif /* SAMPLE_PACKET_COUNT  */

#ifndef SAMPLE_PACKET_SIZE
#define SAMPLE_PACKET_SIZE              (1536)
#endif /* SAMPLE_PACKET_SIZE  */

#define SAMPLE_POOL_SIZE                ((SAMPLE_PACKET_SIZE + sizeof(NX_PACKET)) * SAMPLE_PACKET_COUNT)

#ifndef SAMPLE_ARP_CACHE_SIZE
#define SAMPLE_ARP_CACHE_SIZE           (512)
#endif /* SAMPLE_ARP_CACHE_SIZE  */

#ifndef SAMPLE_IP_THREAD_PRIORITY
#define SAMPLE_IP_THREAD_PRIORITY       (1)
#endif /* SAMPLE_IP_THREAD_PRIORITY */

#ifdef SAMPLE_DHCP_DISABLE
#ifndef SAMPLE_IPV4_ADDRESS
#define SAMPLE_IPV4_ADDRESS           IP_ADDRESS(192, 168, 100, 33)
//#error "SYMBOL SAMPLE_IPV4_ADDRESS must be defined. This symbol specifies the IP address of device. "
#endif /* SAMPLE_IPV4_ADDRESS */

#ifndef SAMPLE_IPV4_MASK
#define SAMPLE_IPV4_MASK              0xFFFFFF00UL
//#error "SYMBOL SAMPLE_IPV4_MASK must be defined. This symbol specifies the IP address mask of device. "
#endif /* SAMPLE_IPV4_MASK */

#ifndef SAMPLE_GATEWAY_ADDRESS
#define SAMPLE_GATEWAY_ADDRESS        IP_ADDRESS(192, 168, 100, 1)
//#error "SYMBOL SAMPLE_GATEWAY_ADDRESS must be defined. This symbol specifies the gateway address for routing. "
#endif /* SAMPLE_GATEWAY_ADDRESS */

#ifndef SAMPLE_DNS_SERVER_ADDRESS
#define SAMPLE_DNS_SERVER_ADDRESS     IP_ADDRESS(192, 168, 100, 1)
//#error "SYMBOL SAMPLE_DNS_SERVER_ADDRESS must be defined. This symbol specifies the dns server address for routing. "
#endif /* SAMPLE_DNS_SERVER_ADDRESS */
#else
#define SAMPLE_IPV4_ADDRESS             IP_ADDRESS(192, 168, 52, 10)
#define SAMPLE_IPV4_MASK                IP_ADDRESS(255, 255, 255, 0)
#define SAMPLE_IPV4_GATEWAY             IP_ADDRESS(192, 168, 52, 254)
#endif /* SAMPLE_DHCP_DISABLE */

/* Define Azure RTOS TLS info.  */
static NX_SECURE_X509_CERT root_ca_cert;
static NX_AZURE_IOT_PROVISIONING_CLIENT dps_client;
static UCHAR nx_azure_iot_tls_metadata_buffer[NX_AZURE_IOT_TLS_METADATA_BUFFER_SIZE];
static NX_AZURE_IOT_HUB_CLIENT hub_client;
static UCHAR *buffer_ptr;
static UINT buffer_size = 1536;
VOID *buffer_context;

/* Define the prototypes for AZ IoT.  */
static NX_AZURE_IOT nx_azure_iot;

int get_time_flag = 0;

static TX_THREAD        nbiot_service_thread;
static TX_THREAD        algo_send_result_thread;
static TX_THREAD        cis_capture_image_thread;
static NX_PACKET_POOL   pool_0;
static NX_IP            ip_0;
static NX_DNS           dns_0;


/* Define the stack/cache for ThreadX.  */
static ULONG sample_ip_stack[SAMPLE_IP_STACK_SIZE / sizeof(ULONG)];

#ifndef SAMPLE_POOL_STACK_USER
static ULONG sample_pool_stack[SAMPLE_POOL_SIZE / sizeof(ULONG)];
static ULONG sample_pool_stack_size = sizeof(sample_pool_stack);
#else
extern ULONG sample_pool_stack[];
extern ULONG sample_pool_stack_size;
#endif

static ULONG nbiot_service_thread_stack[NBIOT_SERVICE_STACK_SIZE / sizeof(ULONG)];
static ULONG cis_capture_image_thread_stack[CIS_CAPTURE_IMAGE_STACK_SIZE / sizeof(ULONG)];
static ULONG algo_send_result_thread_stack[ALGO_SEND_RESULT_STACK_SIZE / sizeof(ULONG)];

static  char *azure_iotdps_connect_user_name;

static  char azure_iotdps_connect_password[512];

AT_STRING azure_iotdps_get_registrations_publish_topic   = AZURE_IOTDPS_GET_CLIENT_REGISTER_STATUS_PUBLISH_TOPIC;//"$dps/registrations/GET/iotdps-get-operationstatus/?$rid=1&operationId=";

char azure_iotdps_registrations_msg[256];
char azure_iotdps_get_registrations_msg[256];
char azure_registrations_msg_len[2];

/* Azure Central IoTHUB. */
static  char azure_iothub_connect_host_name[128];
static  char azure_iothub_connect_user_name[384];
static  char azure_iothub_connect_password[512];


static char azure_iothub_publish_topic[256];
char azure_iothub_publish_msg[512];

char azure_iothub_publish_msg_len[2];
char azure_iothub_publish_human[12];
char azure_iothub_publish_det_box_x[12];
char azure_iothub_publish_det_box_y[12];
char azure_iothub_publish_det_box_width[12];
char azure_iothub_publish_det_box_height[12];

/* Azure PDS Registration Info. */
char azure_iotdps_reg_time_hex[AT_MAX_LEN];
char azure_iotdps_reg_time_ascii[512];
char azure_iotdps_reg_assignhub_ascii[512];

char azure_iotdps_reg_opid[256];

/* Azure PNP DPS Event initial. */
static uint8_t azure_pnp_iotdps_event = PNP_IOTDPS_INITIAL;

/* Azure PNP IoTHub Event initial. */
static uint8_t azure_pnp_iothub_event = PNP_IOTHUB_NBIOT_CERTIFICATION;

/* Azure Algorithm Event initial. */
uint8_t azure_active_event = ALGO_EVENT_IDLE;

/* Algorithm Struct. */
struct_algoResult g_algo_res;

char azure_iothub_publish_msg_json[512];
char azure_iothub_publish_msg_json_len[2];

static int  azure_iothub_img_send_cnt = 0;
static int  azure_iothub_img_send_idx = 0;

/* UART Driver Info. */
DEV_UART_PTR dev_uart_comm;
char recv_buf[AT_MAX_LEN];
uint32_t recv_len = AT_MAX_LEN;

struct tm azure_iotdps_network_tm;
static time_t azure_iotdps_network_epoch_time;

struct tm azure_iotdps_reg_tm;
static time_t azure_iotdps_epoch_time;

/* Define the prototypes for hex to ascii. */
int hex_to_ascii(char c, char d);

/* Define the prototypes for gen dps sas key. */
INT nbiot_service_get_dps_key(ULONG expiry_time_secs, UCHAR *resource_dps_sas_token);

/* Define the prototypes for gen iothub sas key. */
INT nbiot_service_get_iothub_key(ULONG expiry_time_secs, UCHAR *resource_dps_sas_token);

/* Define the prototypes for nbiot service thread. */
static void nbiot_service_thread_entry(ULONG parameter);

/* Define the prototypes for cis capture image thread. */
static void cis_capture_image_thread_entry(ULONG parameter);

/* Define the prototypes for algo send result thread. */
static void algo_send_result_thread_entry(ULONG parameter);

/* Define the prototypes for time get. */
static UINT unix_time_get(ULONG *unix_time);

/* Include the platform IP driver. */
void _nx_ram_network_driver(struct NX_IP_DRIVER_STRUCT *driver_req);

/* Parsing String_Hex to String_ASCII. */
int hex_to_int(char c){
     int first = c/16 - 3;
     int second = c % 16;
     int result = first*10 + second;
     if(result > 9) result--;
     return result;
}

int hex_to_ascii(char c, char d){
     int high = hex_to_int(c) * 16;
     int low = hex_to_int(d);
     return high+low;
}


/* Get network time. */
static uint8_t azure_iotdps_parsing_network_time()
{

	xprintf("### Parsing Network time and Generate DPS SAS Token... ###\n");
	char azure_iotdps_netwoerk_time[48];
	char *azure_iotdps_get_network_time_loc = NULL ;

	const UINT azure_iotdps_parsing_network_time_len = 24;
	uint32_t azure_iotdps_network_year,azure_iotdps_network_month,azure_iotdps_network_day;
	uint32_t azure_iotdps_network_hour,azure_iotdps_network_minute,azure_iotdps_network_sec;
	uint32_t azure_iotdps_network_time_zone;

	ULONG current_time;
	UCHAR *resource_dps_sas_token = NULL;
	get_time_flag = 0;

	/* parsing registered Time. */
	azure_iotdps_get_network_time_loc = strstr(recv_buf, ":");
	strncpy(azure_iotdps_netwoerk_time, (recv_buf+(azure_iotdps_get_network_time_loc - recv_buf+1)),(strlen(recv_buf) - (azure_iotdps_get_network_time_loc - recv_buf)));
	memset(recv_buf, 0,AT_MAX_LEN);//clear buffer
//	xprintf("### azure_iotdps_netwoerk_time:%s ###\n",azure_iotdps_netwoerk_time);

	/* parsing network time Year. */
	azure_iotdps_network_year = atoi(azure_iotdps_netwoerk_time);
	azure_iotdps_network_tm.tm_year = (atoi(azure_iotdps_netwoerk_time) - 1900);
	//xprintf("*** azure_iotdps_reg_tm.tm_year:%d\n", (azure_iotdps_reg_tm.tm_year + 1900));

	/* parsing network time Month. */
	azure_iotdps_get_network_time_loc = strstr(azure_iotdps_netwoerk_time, "/");
	strncpy(azure_iotdps_netwoerk_time, (azure_iotdps_netwoerk_time+(azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time+1)), azure_iotdps_parsing_network_time_len- (azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time));
	azure_iotdps_network_tm.tm_mon = (atoi(azure_iotdps_netwoerk_time) - 1);
	//xprintf("*** azure_iotdps_reg_tm.tm_mon:%d ***\n", (azure_iotdps_reg_tm.tm_mon + 1));

	/* parsing network time Day. */
	azure_iotdps_get_network_time_loc = strstr(azure_iotdps_netwoerk_time, "/");
	strncpy(azure_iotdps_netwoerk_time, (azure_iotdps_netwoerk_time+(azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time+1)), azure_iotdps_parsing_network_time_len- (azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time));
	azure_iotdps_network_tm.tm_mday = atoi(azure_iotdps_netwoerk_time);
	//xprintf("*** azure_iotdps_reg_tm.tm_mday:%d ***\n", azure_iotdps_reg_tm.tm_mday);

	/* parsing network time Hour. */
	azure_iotdps_get_network_time_loc = strstr(azure_iotdps_netwoerk_time, ",");
	strncpy(azure_iotdps_netwoerk_time, (azure_iotdps_netwoerk_time+(azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time+1)), azure_iotdps_parsing_network_time_len- (azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time));
	azure_iotdps_network_tm.tm_hour = (atoi(azure_iotdps_netwoerk_time) - 1);
	//xprintf("*** azure_iotdps_reg_tm.tm_hour:%d ***\n",azure_iotdps_reg_tm.tm_hour + 1);

	/* parsing network time Minute. */
	azure_iotdps_get_network_time_loc = strstr(azure_iotdps_netwoerk_time, ":");
	strncpy(azure_iotdps_netwoerk_time, (azure_iotdps_netwoerk_time+(azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time+1)), azure_iotdps_parsing_network_time_len- (azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time));
	azure_iotdps_network_tm.tm_min = (atoi(azure_iotdps_netwoerk_time) - 1);
	//xprintf("*** azure_iotdps_reg_tm.tm_hour:%d ***\n",azure_iotdps_reg_tm.tm_hour + 1);

	/* parsing network time Second. */
	azure_iotdps_get_network_time_loc = strstr(azure_iotdps_netwoerk_time, ":");
	strncpy(azure_iotdps_netwoerk_time, (azure_iotdps_netwoerk_time+(azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time+1)), azure_iotdps_parsing_network_time_len- (azure_iotdps_get_network_time_loc - azure_iotdps_netwoerk_time));
	azure_iotdps_network_tm.tm_sec = (atoi(azure_iotdps_netwoerk_time) - 1);
	//xprintf("*** azure_iotdps_reg_tm.tm_hour:%d ***\n",azure_iotdps_reg_tm.tm_hour + 1);

	azure_iotdps_network_epoch_time = mktime(&azure_iotdps_network_tm);
	get_time_flag = 1;
	unix_time_get(&current_time);
	//xprintf(" azure_iotdps_network_epoch_time:%ld \n", azure_iotdps_network_epoch_time);
	//xprintf("*** current_time:%ld ***\n", current_time);
	nbiot_service_get_dps_key(azure_iotdps_network_epoch_time, resource_dps_sas_token);

	return 1;
}

/* Send Algorithm Metadata to Cloud. */
int8_t send_algo_result_to_cloud()
{
	int ret = 0;
#if 0
	algo_result.humanPresence				= 1;
	algo_result.ht[0].upper_body_bbox.x		= 320;
	algo_result.ht[0].upper_body_bbox.y 	= 240;
	algo_result.ht[0].upper_body_bbox.width	= 50;
	algo_result.ht[0].upper_body_bbox.height= 50;
#endif
	char azure_iothub_publish_msg_json_ascii[512];

	sprintf(azure_iothub_publish_human, "%d", algo_result.humanPresence);
	sprintf(azure_iothub_publish_det_box_x, "%d", algo_result.ht[0].upper_body_bbox.x);
	sprintf(azure_iothub_publish_det_box_y, "%d", algo_result.ht[0].upper_body_bbox.y);
	sprintf(azure_iothub_publish_det_box_width, "%d", algo_result.ht[0].upper_body_bbox.width);
	sprintf(azure_iothub_publish_det_box_height, "%d", algo_result.ht[0].upper_body_bbox.height);

	/*  {"human": */
	strcpy(azure_iothub_publish_msg_json,AZURE_IOTHUB_PUBLISH_MESSAGE_JSON_PREFIX);
	/* {"human":3 */
	strcat(azure_iothub_publish_msg_json, azure_iothub_publish_human);
	//xprintf("\n*** azure_iothub_publish_msg_json_1:%s ***\n",azure_iothub_publish_msg_json);

	/* {"human":3,"det_box_x": */
	strcat(azure_iothub_publish_msg_json, AZURE_IOTHUB_PUBLISH_MESSAGE_JSON_DET_BOX_X);
	/*{"human":3,"det_box_x":320 */
	strcat(azure_iothub_publish_msg_json, azure_iothub_publish_det_box_x);
	//xprintf("\n*** azure_iothub_publish_msg_json_2:%s ***\n",azure_iothub_publish_msg_json);

	/* {"human":3,"det_box_x":320,"det_box_y": */
	strcat(azure_iothub_publish_msg_json, AZURE_IOTHUB_PUBLISH_MESSAGE_JSON_DET_BOX_Y);
	/* {"human":3,"det_box_x":320,"det_box_y":240 */
	strcat(azure_iothub_publish_msg_json, azure_iothub_publish_det_box_y);
	//xprintf("\n*** azure_iothub_publish_msg_json_3:%s ***\n",azure_iothub_publish_msg_json);

	/* {"human":3,"det_box_x":320,"det_box_y":240,"det_box_width": */
	strcat(azure_iothub_publish_msg_json, AZURE_IOTHUB_PUBLISH_MESSAGE_JSON_DET_BOX_WIDTH);
	/* {"human":3,"det_box_x":320,"det_box_y":240,"det_box_width":50 */
	strcat(azure_iothub_publish_msg_json, azure_iothub_publish_det_box_width);
	//xprintf("\n*** azure_iothub_publish_msg_json_4:%s ***\n",azure_iothub_publish_msg_json);

	/* {"human":3,"det_box_x":320,"det_box_y":240,"det_box_width":50,"det_box_height": */
	strcat(azure_iothub_publish_msg_json, AZURE_IOTHUB_PUBLISH_MESSAGE_JSON_DET_BOX_HEIGHT);
	/* {"human":3,"det_box_x":320,"det_box_y":240,"det_box_width":50,"det_box_height":50 */
	strcat(azure_iothub_publish_msg_json, azure_iothub_publish_det_box_height);
	//xprintf("\n*** azure_iothub_publish_msg_json_5:%s ***\n",azure_iothub_publish_msg_json);

	/*{"human":3,"det_box_x":320,"det_box_y":240,"det_box_width":50,"det_box_height":50}*/
	strcat(azure_iothub_publish_msg_json, AZURE_IOTHUB_PUBLISH_MESSAGE_JSON_SUFFIX);
	//xprintf("\n*** azure_iothub_publish_msg_json:%s ***\n",azure_iothub_publish_msg_json);

	strcpy(azure_iothub_publish_msg_json_ascii,azure_iothub_publish_msg_json);
	//xprintf("\n*** azure_iothub_publish_msg_json_ascii:%s ***\n",azure_iothub_publish_msg_json_ascii);

	/* ASCII¡@convert to Hex */
	for (int j = 0; j < strlen(azure_iothub_publish_msg_json_ascii); j++){
		sprintf((azure_iothub_publish_msg_json + (j * 2)), "%02X", *(azure_iothub_publish_msg_json_ascii + j));
	}
	sprintf(azure_iothub_publish_msg_json_len, "%d", strlen(azure_iothub_publish_msg_json));
	//xprintf("**** azure_iothub_publish_msg_json_hex:%s, %s ****\n",azure_iothub_publish_msg_json_len,azure_iothub_publish_msg_json);

	ret = wnb303r_MQTT_publish_topic(dev_uart_comm, "0", azure_iothub_publish_topic, "1", "0", "0", \
			azure_iothub_publish_msg_json_len,azure_iothub_publish_msg_json,recv_buf,recv_len,PUBLISH_TOPIC_SEND_DATA);

	if(ret == UART_READ_TIMEOUT)
	{
		xprintf("### NBIOT IOTHUB Send Metadata to Cloud ing... ###\n");
			return UART_READ_TIMEOUT;
	}else if(ret == AT_ERROR){
		xprintf("### [!!!AT ERROR!!!] NBIOT IOTHUB Send Metadata to Cloud ###\n");
		return AT_ERROR;
	}
	return 1;
}

/* Send customer dats to cloud. */
int8_t send_cstm_data_to_cloud(unsigned char *databuf, int size)
{
	int ret = 0;
	static char azureIothubPublishTopic[256];
	memset(azureIothubPublishTopic, 0, 256);
	strcat(azureIothubPublishTopic,"devices/");
	strcat(azureIothubPublishTopic,AZURE_IOTHUB_DEVICE_ID);
	strcat(azureIothubPublishTopic,"/messages/events/");
	/* ascii¡@convert to hex string. */
	for (int j = 0; j < size; j++){
		sprintf((azure_iothub_publish_msg + (j * 2)), "%02X", *(databuf + j));
	}
	//xprintf("\n*** azure_iothub_publish_msg:%s ***\n",azure_iothub_publish_msg);

	sprintf(azure_iothub_publish_msg_len, "%d", (size*2));
	//xprintf("\n*** azure_iothub_publish_msg_len:%s ***\n",azure_iothub_publish_msg_len);

	/* send pkg to azure iothub. */
	//xprintf("*** azureIothubPublishTopic:%s ***\n",azureIothubPublishTopic);
	ret = wnb303r_MQTT_publish_topic(dev_uart_comm, "0", azureIothubPublishTopic, "1", "0", "0", \
			azure_iothub_publish_msg_len,azure_iothub_publish_msg,recv_buf,recv_len,PUBLISH_TOPIC_SEND_DATA);

	if(ret == UART_READ_TIMEOUT)
	{
		xprintf("### NBIOT IOTHUB Send Data to Cloud ing... ###\n");
		return UART_READ_TIMEOUT;
	}else if(ret == AT_ERROR){
		xprintf("### [!!!AT ERROR!!!] NBIOT IOTHUB Send Data to Cloud ###\n");
		return AT_ERROR;
	}

	return 1;
}

/* Parsing Azure IoT DPS Registration Operation ID. */
static uint8_t azure_pnp_iotdps_get_registration_operation_id()
{
	char azure_iotdps_reg_opid_hex[512];
	char azure_iotdps_opid_ascii[256];
	char azure_iotdps_reg_opid[256];

	char hex_msb = 0 ;
	char *azure_iotdps_reg_opid_loc;

	UINT azure_iotdps_parsing_reg_opid = 0;
	UINT azure_iotdps_reg_opid_hex_len;
	const UINT azure_iotdps_reg_opid_len = 55;

	sprintf(azure_registrations_msg_len, "%d", strlen(azure_iotdps_registrations_msg));

	if (strstr(recv_buf, "+EMQPUB:")!= NULL){
		xprintf("### Parsing DPS Registration Operation ID... ###\n");
		azure_iotdps_reg_opid_loc = strstr(recv_buf, "+EMQPUB:");
		strncpy(azure_iotdps_reg_opid_hex, (recv_buf+(azure_iotdps_reg_opid_loc - recv_buf+1)),(strlen(recv_buf) - (azure_iotdps_reg_opid_loc - recv_buf)));
		//xprintf("2**** azure_iotdps_reg_opid_hex:%d,%s*****2\n",strlen(azure_iotdps_reg_opid_hex),azure_iotdps_reg_opid_hex);
		memset(recv_buf, 0,AT_MAX_LEN);//clear buffer

		while(1)
		{
			if(strstr(azure_iotdps_reg_opid_hex, ",") !=NULL )
			{
				azure_iotdps_parsing_reg_opid++;
				azure_iotdps_reg_opid_loc = strstr(azure_iotdps_reg_opid_hex, ",");
				strncpy(azure_iotdps_reg_opid_hex, (azure_iotdps_reg_opid_hex+(azure_iotdps_reg_opid_loc - azure_iotdps_reg_opid_hex+1)),(strlen(azure_iotdps_reg_opid_hex) - (azure_iotdps_reg_opid_loc - azure_iotdps_reg_opid_hex)));
			}

			if(azure_iotdps_parsing_reg_opid == 6)
				break;
		}//while

		azure_iotdps_reg_opid_hex_len = strlen(azure_iotdps_reg_opid_hex);
		//xprintf("3**** azure_iotdps_reg_opid_hex=%d, %s ****3\r\n",azure_iotdps_reg_opid_hex_len,azure_iotdps_reg_opid_hex);
		UINT cnt = 0;
		for(int i = 0; i < azure_iotdps_reg_opid_hex_len; i++)
		{
	       if(i % 2 != 0){
	    	   azure_iotdps_opid_ascii[cnt] = hex_to_ascii(hex_msb, azure_iotdps_reg_opid_hex[i]);
	    	   //xprintf("%c",azure_iotdps_opid_ascii[cnt]);
	    	   cnt++;
	       }else{
	    	   hex_msb = azure_iotdps_reg_opid_hex[i];
	       }
	    }//for i
		//xprintf("\n");

		azure_iotdps_reg_opid_loc = strstr(azure_iotdps_opid_ascii, "operationId");
		memset(azure_iotdps_reg_opid, 0, sizeof(azure_iotdps_reg_opid));
		
		strncpy(azure_iotdps_reg_opid, (azure_iotdps_opid_ascii+(azure_iotdps_reg_opid_loc - azure_iotdps_opid_ascii+14)),azure_iotdps_reg_opid_len);
		azure_iotdps_reg_opid[55]='\0';
		//xprintf("4*****azure_iotdps_reg_opid:%d, %s *****4\n",strlen(azure_iotdps_reg_opid),azure_iotdps_reg_opid);

		/* get registration publish topic. */
		strncat(azure_iotdps_get_registrations_publish_topic, azure_iotdps_reg_opid, strlen(azure_iotdps_reg_opid));
		//xprintf("\n1.#### azure_iotdps_get_registrations_publish_topic: %s ####1.\n",azure_iotdps_get_registrations_publish_topic);

		for (int j = 0; j < strlen(azure_iotdps_get_registrations_publish_topic); j++){
			sprintf((azure_iotdps_get_registrations_msg + (j * 2)), "%02X", *(azure_iotdps_get_registrations_publish_topic + j));
		 }

		 /* assign azure_iotdps_get_registrations_msg length. */
		 sprintf(azure_registrations_msg_len, "%d", strlen(azure_iotdps_get_registrations_msg));
		 //xprintf("\n2.#### azure_iotdps_get_registrations_msg: %s ####2.\n",azure_iotdps_get_registrations_msg);

		 return 1;
	}else{
		//waiting read nbiot reply data...
		return 0;
	}
}

/* Parsing Azure IoT DPS Registration Status. */
static uint8_t azure_pnp_iotdps_get_registration_status()
{
	UINT azure_iotdps_parsing_reg_time			= 0;
	char hex_msb 								= 0;
	const UINT azure_iotdps_parsing_reg_time_len= 19;

	char azure_iotdps_reg_time_tmp[19];
	char azure_iotdps_reg_assignedhub_tmp[128];

	static char *azure_iotdps_reg_create_time_loc			= NULL ;
	static char *azure_iotdps_reg_create_assignedhub_loc	= NULL;
	static char *azure_iotdps_reg_create_deviceid_loc		= NULL;

	UINT azure_iotdps_reg_time_hex_len;
  	//for sas token
	ULONG current_time;
	UCHAR *resource_dps_sas_token = NULL;
	get_time_flag = 0;

	/*parsing registered time*/
	if (strstr(recv_buf, "+EMQPUB:")!= NULL){
		xprintf("### Parsing DPS Registration Status... ###\n");
		azure_iotdps_reg_create_time_loc = strstr(recv_buf, "+EMQPUB:");
		strncpy(azure_iotdps_reg_time_hex, (recv_buf+(azure_iotdps_reg_create_time_loc - recv_buf+1)),(strlen(recv_buf) - (azure_iotdps_reg_create_time_loc - recv_buf)));
		//xprintf("\n2**** azure_iotdps_reg_time_hex:%d,%s ****2\n",strlen(azure_iotdps_reg_time_hex),azure_iotdps_reg_time_hex);
		//xprintf("\n2**** azure_iotdps_reg_time_hex:%d,%s ****2\n",recv_len,azure_iotdps_reg_time_hex);
		memset(recv_buf, 0,AT_MAX_LEN);//clear buffer

		while(1)
		{
			if(strstr(azure_iotdps_reg_time_hex, ",") !=NULL )
			{
				azure_iotdps_parsing_reg_time++;
				azure_iotdps_reg_create_time_loc = strstr(azure_iotdps_reg_time_hex, ",");
				strncpy(azure_iotdps_reg_time_hex, (azure_iotdps_reg_time_hex+(azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_hex+1)),(strlen(azure_iotdps_reg_time_hex) - (azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_hex)));
			}
			if(azure_iotdps_parsing_reg_time == 6)
				break;
		}//while
		azure_iotdps_reg_time_hex_len = strlen(azure_iotdps_reg_time_hex);
		//xprintf("\n3**** azure_iotdps_reg_time_hex=%d, %s ****3\n",azure_iotdps_reg_time_hex_len,azure_iotdps_reg_time_hex);
		int cnt = 0;
		for(int i = 0; i < azure_iotdps_reg_time_hex_len; i++)
		{
	       if(i % 2 != 0){
	    	   azure_iotdps_reg_time_ascii[cnt] = hex_to_ascii(hex_msb, azure_iotdps_reg_time_hex[i]);
	    	   //azure_iotdps_reg_assignhub_ascii[cnt] = hex_to_ascii(hex_msb, azure_iotdps_reg_time_hex[i]);
	    	   //xprintf("%c",azure_iotdps_reg_time_ascii[cnt]);
	    	   cnt++;
	       }else{
	    	   hex_msb = azure_iotdps_reg_time_hex[i];
	       }
	    }//for
		//xprintf("\n");
		strcpy(azure_iotdps_reg_assignhub_ascii,azure_iotdps_reg_time_ascii);

		azure_iotdps_reg_create_time_loc = strstr(azure_iotdps_reg_time_ascii, "createdDateTimeUtc");
		memset(azure_iotdps_reg_time_tmp, 0, sizeof(azure_iotdps_reg_time_tmp));
		strncpy(azure_iotdps_reg_time_tmp, (azure_iotdps_reg_time_ascii+(azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_ascii+21)),azure_iotdps_parsing_reg_time_len);
		azure_iotdps_reg_time_tmp[19]='\0';
		//xprintf("\n4**** azure_iotdps_reg_time_tmp=%d, %s ****4\n",strlen(azure_iotdps_reg_time_tmp),azure_iotdps_reg_time_tmp);

		/* eX: 2021-01-30T07:55:20 */
		/* parsing registered time Year. */
		azure_iotdps_reg_tm.tm_year = (atoi(azure_iotdps_reg_time_tmp) - 1900);
		//xprintf("*** azure_iotdps_reg_tm.tm_year:%d\n", (azure_iotdps_reg_tm.tm_year + 1900));

		/* parsing registered time Month. */
		azure_iotdps_reg_create_time_loc = strstr(azure_iotdps_reg_time_tmp, "-");
		strncpy(azure_iotdps_reg_time_tmp, (azure_iotdps_reg_time_tmp+(azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp+1)), azure_iotdps_parsing_reg_time_len- (azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp));
		azure_iotdps_reg_tm.tm_mon = (atoi(azure_iotdps_reg_time_tmp) - 1);
		//xprintf("*** azure_iotdps_reg_tm.tm_mon:%d ***\n", (azure_iotdps_reg_tm.tm_mon + 1));

		/* parsing registered time Day. */
		azure_iotdps_reg_create_time_loc = strstr(azure_iotdps_reg_time_tmp, "-");
		strncpy(azure_iotdps_reg_time_tmp, (azure_iotdps_reg_time_tmp+(azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp+1)), azure_iotdps_parsing_reg_time_len- (azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp));
		azure_iotdps_reg_tm.tm_mday = atoi(azure_iotdps_reg_time_tmp);
		//xprintf("*** azure_iotdps_reg_tm.tm_mday:%d ***\n", azure_iotdps_reg_tm.tm_mday);

		/* parsing registered time Hour. */
		azure_iotdps_reg_create_time_loc = strstr(azure_iotdps_reg_time_tmp, "T");
		strncpy(azure_iotdps_reg_time_tmp, (azure_iotdps_reg_time_tmp+(azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp+1)), azure_iotdps_parsing_reg_time_len- (azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp));
		azure_iotdps_reg_tm.tm_hour = (atoi(azure_iotdps_reg_time_tmp) - 1);
		//xprintf("*** azure_iotdps_reg_tm.tm_hour:%d ***\n",azure_iotdps_reg_tm.tm_hour + 1);

		/* parsing registered time Minute. */
		azure_iotdps_reg_create_time_loc = strstr(azure_iotdps_reg_time_tmp, ":");
		strncpy(azure_iotdps_reg_time_tmp, (azure_iotdps_reg_time_tmp+(azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp+1)), azure_iotdps_parsing_reg_time_len- (azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp));
		azure_iotdps_reg_tm.tm_min = (atoi(azure_iotdps_reg_time_tmp) - 1);
		//xprintf("*** azure_iotdps_reg_tm.tm_min:%d ***\n", (azure_iotdps_reg_tm.tm_min + 1));

		/* parsing registered time Second. */
		azure_iotdps_reg_create_time_loc = strstr(azure_iotdps_reg_time_tmp, ":");
		strncpy(azure_iotdps_reg_time_tmp, (azure_iotdps_reg_time_tmp+(azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp+1)), azure_iotdps_parsing_reg_time_len- (azure_iotdps_reg_create_time_loc - azure_iotdps_reg_time_tmp));
		azure_iotdps_reg_tm.tm_sec = (atoi(azure_iotdps_reg_time_tmp) - 1);
		//xprintf("*** azure_iotdps_reg_tm.tm_sec:%d ***\n", (azure_iotdps_reg_tm.tm_sec + 1));

		azure_iotdps_epoch_time = mktime(&azure_iotdps_reg_tm);
		get_time_flag = 1;
		unix_time_get(&current_time);
		//xprintf("*** azure_iotdps_epoch_time:%ld ***\n", azure_iotdps_epoch_time);
		//xprintf("*** current_time:%ld ***\n", current_time);

		/* Get assignedHub name. */
		//(azure_iodps_reg_create_assignedhub_loc - azure_iotdps_reg_time_ascii)+14
		azure_iotdps_reg_create_assignedhub_loc = strstr(azure_iotdps_reg_assignhub_ascii, "assignedHub");
		azure_iotdps_reg_create_deviceid_loc = strstr(azure_iotdps_reg_assignhub_ascii, "deviceId");
		memset(azure_iotdps_reg_assignedhub_tmp, 0, sizeof(azure_iotdps_reg_assignedhub_tmp));
		strncpy(azure_iotdps_reg_assignedhub_tmp, (azure_iotdps_reg_assignhub_ascii+(azure_iotdps_reg_create_assignedhub_loc - azure_iotdps_reg_assignhub_ascii+14)),((azure_iotdps_reg_assignhub_ascii+(azure_iotdps_reg_create_deviceid_loc - azure_iotdps_reg_assignhub_ascii)) - (azure_iotdps_reg_assignhub_ascii+(azure_iotdps_reg_create_assignedhub_loc - azure_iotdps_reg_assignhub_ascii)) )-17);
		//xprintf("*** azure_iotdps_reg_assignedhub_tmp:%s ***\n", azure_iotdps_reg_assignedhub_tmp);

		strcpy(azure_iothub_connect_host_name,azure_iotdps_reg_assignedhub_tmp);
		xprintf("*** azure_iothub_connect_host_name:%s ***\n", azure_iothub_connect_host_name);

		nbiot_service_get_iothub_key(azure_iotdps_epoch_time, resource_dps_sas_token);

		return 1;
	}else{
		xprintf("\nWaiting azure_pnp_iotdps_get_registration_status...\n\n");
		return 0;
	}
}

/* Define Azure RTOS TLS info.  */
static NX_SECURE_X509_CERT root_ca_cert;
static UCHAR nx_azure_iot_tls_metadata_buffer[NX_AZURE_IOT_TLS_METADATA_BUFFER_SIZE];
static ULONG nx_azure_iot_thread_stack[NX_AZURE_IOT_STACK_SIZE / sizeof(ULONG)];
/* Define what the initial system looks like.  */
void    nbiot_task_define(void *first_unused_memory)
{

UINT  status;


    NX_PARAMETER_NOT_USED(first_unused_memory);

    /* Driver Initial. */
    xprintf("### NBIOT Initial... ###\n");
    dev_uart_comm = wnb303r_drv_init(DFSS_UART_0_ID, UART_BAUDRATE_115200);
    if(dev_uart_comm == NULL)
    {
    	xprintf("NBIOT Initial Fail\n");
    	return;
    }

    /* Initialize the NetX system.  */
    nx_system_initialize();
    /* Create a packet pool.  */
    status = nx_packet_pool_create(&pool_0, "NetX Main Packet Pool", SAMPLE_PACKET_SIZE,
                                   (UCHAR *)sample_pool_stack , sample_pool_stack_size);
    /* Check for pool creation error.  */
    if (status)
    {
        xprintf("nx_packet_pool_create fail: %u\r\n", status);
        return;
    }

    /* Create an IP instance.  */
    status = nx_ip_create(&ip_0, "NetX IP Instance 0",
                          SAMPLE_IPV4_ADDRESS, SAMPLE_IPV4_MASK,
                          &pool_0, NULL,
                          (UCHAR*)sample_ip_stack, sizeof(sample_ip_stack),
                          SAMPLE_IP_THREAD_PRIORITY);

    /* Check for IP create errors.  */
    if (status)
    {
        xprintf("nx_ip_create fail: %u\r\n", status);
        return;
    }

    /* Initialize TLS.  */
    nx_secure_tls_initialize();

    /* Create nbiot service thread. */
    status = tx_thread_create(&nbiot_service_thread, "nbiot service Thread",
    							nbiot_service_thread_entry, 0,
								nbiot_service_thread_stack, NBIOT_SERVICE_STACK_SIZE,
								NBIOT_SERVICE_THREAD_PRIORITY, NBIOT_SERVICE_THREAD_PRIORITY,
								TX_NO_TIME_SLICE, TX_AUTO_START);

    /* Check status.  */
    if (status)
    {
        xprintf("nbiot service thread creation fail: %u\r\n", status);
        return;
    }

    /* Create cis capture image thread. */
    status = tx_thread_create(&cis_capture_image_thread, "cis capture image Thread",
    							cis_capture_image_thread_entry, 0,
								cis_capture_image_thread_stack, CIS_CAPTURE_IMAGE_STACK_SIZE,
								CIS_CAPTURE_IMAGE_THREAD_PRIORITY, CIS_CAPTURE_IMAGE_THREAD_PRIORITY,
								TX_NO_TIME_SLICE, TX_AUTO_START);

    /* Check status.  */
    if (status)
    {
        xprintf("CIS¡@capture image thread creation fail: %u\r\n", status);
        return;
    }

    /* Create algo send result thread. */
    status = tx_thread_create(&algo_send_result_thread, "algo send result Thread",
    							algo_send_result_thread_entry, 0,
								algo_send_result_thread_stack, ALGO_SEND_RESULT_STACK_SIZE,
								ALGO_SEND_RESULT_THREAD_PRIORITY, ALGO_SEND_RESULT_THREAD_PRIORITY,
								TX_NO_TIME_SLICE, TX_AUTO_START);

    /* Check status.  */
    if (status)
    {
        xprintf("algo send result thread creation fail: %u\r\n", status);
        return;
    }
}

/* Define the prototypes for Azure RTOS IoT.  */
UINT nbiot_service_iot_provisioning_client_initialize(ULONG expiry_time_secs)
{
	UINT status;
	UINT mqtt_user_name_length;
	NXD_MQTT_CLIENT *mqtt_client_ptr;
	NX_AZURE_IOT_RESOURCE *resource_ptr;
	UCHAR *buffer_ptr;
	UINT buffer_size;
	VOID *buffer_context;
	az_span endpoint_span = az_span_create((UCHAR *)ENDPOINT, (INT)sizeof(ENDPOINT) - 1);
	az_span id_scope_span = az_span_create((UCHAR *)ID_SCOPE, (INT)sizeof(ID_SCOPE) - 1);
	az_span registration_id_span = az_span_create((UCHAR *)REGISTRATION_ID, (INT)sizeof(REGISTRATION_ID) - 1);

    memset(&dps_client, 0, sizeof(NX_AZURE_IOT_PROVISIONING_CLIENT));
    /* Set resource pointer.  */
    resource_ptr = &(dps_client.nx_azure_iot_provisioning_client_resource);
    mqtt_client_ptr = &(dps_client.nx_azure_iot_provisioning_client_resource.resource_mqtt);

    dps_client.nx_azure_iot_ptr = &nx_azure_iot;
    dps_client.nx_azure_iot_provisioning_client_endpoint = (UCHAR *)ENDPOINT;
    dps_client.nx_azure_iot_provisioning_client_endpoint_length = sizeof(ENDPOINT) - 1;
    dps_client.nx_azure_iot_provisioning_client_id_scope = (UCHAR *)ID_SCOPE;
    dps_client.nx_azure_iot_provisioning_client_id_scope_length = sizeof(ID_SCOPE) - 1;
    dps_client.nx_azure_iot_provisioning_client_registration_id = (UCHAR *)REGISTRATION_ID;
    dps_client.nx_azure_iot_provisioning_client_registration_id_length = sizeof(REGISTRATION_ID) - 1;
    dps_client.nx_azure_iot_provisioning_client_resource.resource_crypto_array = _nx_azure_iot_tls_supported_crypto;
    dps_client.nx_azure_iot_provisioning_client_resource.resource_crypto_array_size = _nx_azure_iot_tls_supported_crypto_size;
    dps_client.nx_azure_iot_provisioning_client_resource.resource_cipher_map = _nx_azure_iot_tls_ciphersuite_map;
    dps_client.nx_azure_iot_provisioning_client_resource.resource_cipher_map_size = _nx_azure_iot_tls_ciphersuite_map_size;
    dps_client.nx_azure_iot_provisioning_client_resource.resource_metadata_ptr = nx_azure_iot_tls_metadata_buffer;
    dps_client.nx_azure_iot_provisioning_client_resource.resource_metadata_size = sizeof(nx_azure_iot_tls_metadata_buffer);
#ifdef NX_SECURE_ENABLE /*YUN*/
    dps_client.nx_azure_iot_provisioning_client_resource.resource_trusted_certificate = &root_ca_cert;
#endif
    dps_client.nx_azure_iot_provisioning_client_resource.resource_hostname = (UCHAR *)ENDPOINT;
    dps_client.nx_azure_iot_provisioning_client_resource.resource_hostname_length = sizeof(ENDPOINT) - 1;
    resource_ptr->resource_mqtt_client_id_length = dps_client.nx_azure_iot_provisioning_client_registration_id_length;
    resource_ptr->resource_mqtt_client_id = (UCHAR *)dps_client.nx_azure_iot_provisioning_client_registration_id;

    //expiry_time_secs = (ULONG)time(NULL);//(ULONG)azure_iotdps_epoch_time;
    dps_client.nx_azure_iot_provisioning_client_symmetric_key = (UCHAR *)DEVICE_SYMMETRIC_KEY;
    dps_client.nx_azure_iot_provisioning_client_symmetric_key_length = sizeof(DEVICE_SYMMETRIC_KEY) - 1;
    expiry_time_secs += NX_AZURE_IOT_PROVISIONING_CLIENT_TOKEN_EXPIRY;
    //xprintf("expiry_time_secs %ld\r\n", expiry_time_secs);

    if (az_result_failed(az_iot_provisioning_client_init(&(dps_client.nx_azure_iot_provisioning_client_core),
                                                         endpoint_span, id_scope_span,
                                                         registration_id_span, NULL)))
    {
    	xprintf("IoTProvisioning client initialize fail: failed to initialize core client\n");
        return(NX_AZURE_IOT_SDK_CORE_ERROR);
    }

    status = nx_azure_iot_buffer_allocate(dps_client.nx_azure_iot_ptr,
                                          &buffer_ptr, &buffer_size, &buffer_context);
    //xprintf("buffer_size %d\n", buffer_size);
    if (status)
    {
    	xprintf("IoTProvisioning client failed initialization: BUFFER ALLOCATE FAIL\n");
        return(status);
    }

    /* Build user name.  */
    if (az_result_failed(az_iot_provisioning_client_get_user_name(&(dps_client.nx_azure_iot_provisioning_client_core),
                                                                  (CHAR *)buffer_ptr, buffer_size, &mqtt_user_name_length)))
    {
    	xprintf("IoTProvisioning client connect fail: NX_AZURE_IOT_Provisioning_CLIENT_USERNAME_SIZE is too small.\n");
        return(NX_AZURE_IOT_INSUFFICIENT_BUFFER_SPACE);
    }
    //xprintf("mqtt_user_name_length %d\n",mqtt_user_name_length);
    //xprintf("buffer_ptr %s\n",buffer_ptr);
    /* Save the resource buffer.  */
    resource_ptr -> resource_mqtt_buffer_context = buffer_context;
    resource_ptr -> resource_mqtt_buffer_size = buffer_size;
    resource_ptr -> resource_mqtt_user_name_length = mqtt_user_name_length;
    resource_ptr -> resource_mqtt_user_name = buffer_ptr;
    resource_ptr -> resource_mqtt_sas_token = buffer_ptr + mqtt_user_name_length;
    dps_client.nx_azure_iot_provisioning_client_sas_token_buff_size = buffer_size - mqtt_user_name_length;

    /* Link the resource.  */
    dps_client.nx_azure_iot_provisioning_client_resource.resource_data_ptr = &dps_client;
    dps_client.nx_azure_iot_provisioning_client_resource.resource_type = NX_AZURE_IOT_RESOURCE_IOT_PROVISIONING;
    nx_azure_iot_resource_add(&nx_azure_iot, &(dps_client.nx_azure_iot_provisioning_client_resource));
    return(NX_AZURE_IOT_SUCCESS);
}

static UINT nbiot_service_iot_hub_client_process_publish_packet(UCHAR *start_ptr,
                                                           ULONG *topic_offset_ptr,
                                                           USHORT *topic_length_ptr)
{
UCHAR *byte = start_ptr;
UINT byte_count = 0;
UINT multiplier = 1;
UINT remaining_length = 0;
UINT topic_length;

    /* Validate packet start contains fixed header.  */
    do
    {
        if (byte_count >= 4)
        {
            LogError(LogLiteralArgs("Invalid mqtt packet start position"));
            return(NX_AZURE_IOT_INVALID_PACKET);
        }

        byte++;
        remaining_length += (((*byte) & 0x7F) * multiplier);
        multiplier = multiplier << 7;
        byte_count++;
    } while ((*byte) & 0x80);

    if (remaining_length < 2)
    {
        return(NX_AZURE_IOT_INVALID_PACKET);
    }

    /* Retrieve topic length.  */
    byte++;
    topic_length = (UINT)(*(byte) << 8) | (*(byte + 1));

    if (topic_length > remaining_length - 2u)
    {
        return(NX_AZURE_IOT_INVALID_PACKET);
    }

    *topic_offset_ptr = (ULONG)((byte + 2) - start_ptr);
    *topic_length_ptr = (USHORT)topic_length;

    /* Return.  */
    return(NX_AZURE_IOT_SUCCESS);
}

static VOID nbiot_service_iot_hub_client_mqtt_receive_callback(NXD_MQTT_CLIENT* client_ptr,
                                                          UINT number_of_messages)
{
NX_AZURE_IOT_RESOURCE *resource = nx_azure_iot_resource_search(client_ptr);
NX_AZURE_IOT_HUB_CLIENT *hub_client_ptr = NX_NULL;
NX_PACKET *packet_ptr;
NX_PACKET *packet_next_ptr;
ULONG topic_offset;
USHORT topic_length;

    /* This function is protected by MQTT mutex.  */

    NX_PARAMETER_NOT_USED(number_of_messages);

    if (resource && (resource -> resource_type == NX_AZURE_IOT_RESOURCE_IOT_HUB))
    {
        hub_client_ptr = (NX_AZURE_IOT_HUB_CLIENT *)resource -> resource_data_ptr;
    }

    if (hub_client_ptr)
    {
        for (packet_ptr = client_ptr -> message_receive_queue_head;
             packet_ptr;
             packet_ptr = packet_next_ptr)
        {

            /* Store next packet in case current packet is consumed.  */
            packet_next_ptr = packet_ptr -> nx_packet_queue_next;

            /* Adjust packet to simply process logic.  */
            nx_azure_iot_mqtt_packet_adjust(packet_ptr);

            if (nbiot_service_iot_hub_client_process_publish_packet(packet_ptr -> nx_packet_prepend_ptr, &topic_offset,
                                                               &topic_length))
            {

                /* Message not supported. It will be released.  */
                nx_packet_release(packet_ptr);
                continue;
            }

            if ((topic_offset + topic_length) >
                (ULONG)(packet_ptr -> nx_packet_append_ptr - packet_ptr -> nx_packet_prepend_ptr))
            {

                /* Only process topic in the first packet since the fixed topic is short enough to fit into one packet.  */
                topic_length = (USHORT)(((ULONG)(packet_ptr -> nx_packet_append_ptr - packet_ptr -> nx_packet_prepend_ptr) -
                                         topic_offset) & 0xFFFF);
            }

            if (hub_client_ptr -> nx_azure_iot_hub_client_direct_method_message.message_process &&
                (hub_client_ptr -> nx_azure_iot_hub_client_direct_method_message.message_process(hub_client_ptr, packet_ptr,
                                                                                                 topic_offset,
                                                                                                 topic_length) == NX_AZURE_IOT_SUCCESS))
            {

                /* Direct method message is processed.  */
                continue;
            }

            if (hub_client_ptr -> nx_azure_iot_hub_client_c2d_message.message_process &&
                (hub_client_ptr -> nx_azure_iot_hub_client_c2d_message.message_process(hub_client_ptr, packet_ptr,
                                                                                       topic_offset,
                                                                                       topic_length) == NX_AZURE_IOT_SUCCESS))
            {

                /* Could to Device message is processed.  */
                continue;
            }

            if ((hub_client_ptr -> nx_azure_iot_hub_client_device_twin_message.message_process) &&
                (hub_client_ptr -> nx_azure_iot_hub_client_device_twin_message.message_process(hub_client_ptr,
                                                                                               packet_ptr, topic_offset,
                                                                                               topic_length) == NX_AZURE_IOT_SUCCESS))
            {

                /* Device Twin message is processed.  */
                continue;
            }

            /* Message not supported. It will be released.  */
            nx_packet_release(packet_ptr);
        }

        /* Clear all message from MQTT receive queue.  */
        client_ptr -> message_receive_queue_head = NX_NULL;
        client_ptr -> message_receive_queue_tail = NX_NULL;
        client_ptr -> message_receive_queue_depth = 0;
    }
}

UINT nbiot_service_iot_hub_client_initialize(NX_AZURE_IOT_HUB_CLIENT* hub_client_ptr,
                                        NX_AZURE_IOT *nx_azure_iot_ptr,
                                        const UCHAR *host_name, UINT host_name_length,
                                        const UCHAR *device_id, UINT device_id_length,
                                        const UCHAR *module_id, UINT module_id_length,
                                        const NX_CRYPTO_METHOD **crypto_array, UINT crypto_array_size,
                                        const NX_CRYPTO_CIPHERSUITE **cipher_map, UINT cipher_map_size,
                                        UCHAR * metadata_memory, UINT memory_size
#ifdef NX_SECURE_ENABLE /*YUN*/
										,NX_SECURE_X509_CERT *trusted_certificate
#endif
										)
{
UINT status;
NX_AZURE_IOT_RESOURCE *resource_ptr;
az_span hostname_span = az_span_create((UCHAR *)host_name, (INT)host_name_length);
az_span device_id_span = az_span_create((UCHAR *)device_id, (INT)device_id_length);
az_iot_hub_client_options options = az_iot_hub_client_options_default();
az_result core_result;

    if ((nx_azure_iot_ptr == NX_NULL) || (hub_client_ptr == NX_NULL) || (host_name == NX_NULL) ||
        (device_id == NX_NULL) || (host_name_length == 0) || (device_id_length == 0))
    {
        LogError(LogLiteralArgs("IoTHub client initialization fail: INVALID POINTER"));
        return(NX_AZURE_IOT_INVALID_PARAMETER);
    }

    memset(hub_client_ptr, 0, sizeof(NX_AZURE_IOT_HUB_CLIENT));

    hub_client_ptr -> nx_azure_iot_ptr = nx_azure_iot_ptr;
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_crypto_array = crypto_array;
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_crypto_array_size = crypto_array_size;
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_cipher_map = cipher_map;
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_cipher_map_size = cipher_map_size;
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_metadata_ptr = metadata_memory;
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_metadata_size = memory_size;
#ifdef NX_SECURE_ENABLE /*YUN*/
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_trusted_certificate = trusted_certificate;
#endif
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_hostname = host_name;
    hub_client_ptr -> nx_azure_iot_hub_client_resource.resource_hostname_length = host_name_length;
    options.module_id = az_span_create((UCHAR *)module_id, (INT)module_id_length);
    options.user_agent = AZ_SPAN_FROM_STR(NX_AZURE_IOT_HUB_CLIENT_USER_AGENT);

    core_result = az_iot_hub_client_init(&hub_client_ptr -> iot_hub_client_core,
                                         hostname_span, device_id_span, &options);
    if (az_result_failed(core_result))
    {
        LogError(LogLiteralArgs("IoTHub client failed initialization with error status: %d"), core_result);
        return(NX_AZURE_IOT_SDK_CORE_ERROR);
    }

    /* Set resource pointer.  */
    resource_ptr = &(hub_client_ptr -> nx_azure_iot_hub_client_resource);

    /* Link the resource.  */
    resource_ptr -> resource_data_ptr = (VOID *)hub_client_ptr;
    resource_ptr -> resource_type = NX_AZURE_IOT_RESOURCE_IOT_HUB;
    nx_azure_iot_resource_add(nx_azure_iot_ptr, resource_ptr);


    return(NX_AZURE_IOT_SUCCESS);
}

static UINT nbiot_service_dps_entry(void)
{
	UINT status;
	ULONG expiry_time_secs;

#ifdef ENABLE_AZURE_PORTAL_DPS_HUB_DEVICE
#ifndef AZURE_PNP_CERTIFICATION_EN //20210330 jaosn+
	UCHAR *iothub_hostname = (UCHAR *)HOST_NAME;
	UINT iothub_hostname_length = sizeof(HOST_NAME) - 1;
#else
	UCHAR *iothub_hostname = (UCHAR *)ENDPOINT;
	UINT iothub_hostname_length = sizeof(ENDPOINT) - 1;
#endif
#else
	UCHAR *iothub_hostname = (UCHAR *)ENDPOINT;
	UINT iothub_hostname_length = sizeof(ENDPOINT) - 1;
#endif

	UCHAR *iothub_device_id = (UCHAR *)AZURE_IOTHUB_DEVICE_ID;
	UINT iothub_device_id_length = sizeof(AZURE_IOTHUB_DEVICE_ID) - 1;
	expiry_time_secs = (ULONG)time(NULL);

	//xprintf("Create Azure IoT handler...\r\n");
	/* Create Azure IoT handler.  */
	if ((status = nx_azure_iot_create(&nx_azure_iot, (UCHAR *)"Azure IoT", &ip_0, &pool_0, &dns_0,
									  nx_azure_iot_thread_stack, sizeof(nx_azure_iot_thread_stack),
									  NX_AZURE_IOT_THREAD_PRIORITY, unix_time_get)))
	{
		xprintf("Failed on nx_azure_iot_create!: error code = 0x%08x\r\n", status);
		return(NX_AZURE_IOT_SDK_CORE_ERROR);
	}

    //xprintf("Start Provisioning Client...\r\n");

    /* Initialize IoT provisioning client.  */
    if (status = nbiot_service_iot_provisioning_client_initialize(expiry_time_secs))
    {
        xprintf("Failed on nx_azure_iot_provisioning_client_initialize!: error code = 0x%08x\r\n", status);
        return(status);
    }

    /* Initialize IoTHub client.  */
    if ((status = nbiot_service_iot_hub_client_initialize(&hub_client, &nx_azure_iot,
                                                     iothub_hostname, iothub_hostname_length,
                                                     iothub_device_id, iothub_device_id_length,
                                                     (UCHAR *)MODEL_ID, sizeof(MODEL_ID) - 1,
                                                     _nx_azure_iot_tls_supported_crypto,
                                                     _nx_azure_iot_tls_supported_crypto_size,
                                                     _nx_azure_iot_tls_ciphersuite_map,
                                                     _nx_azure_iot_tls_ciphersuite_map_size,
                                                     nx_azure_iot_tls_metadata_buffer,
                                                     sizeof(nx_azure_iot_tls_metadata_buffer),
                                                     &root_ca_cert)))
    {
        xprintf("Failed on nx_azure_iot_hub_client_initialize!: error code = 0x%08x\r\n", status);
        return(status);
    }

    return(status);
}

INT nbiot_service_get_dps_key(ULONG expiry_time_secs, UCHAR *resource_dps_sas_token)
{
		UINT status;
		NX_AZURE_IOT_RESOURCE *resource_ptr;
		UCHAR *output_ptr;
		UINT output_len;
		az_span span;
		az_result core_result;
		az_span buffer_span;
		az_span policy_name = AZ_SPAN_LITERAL_FROM_STR(NX_AZURE_IOT_PROVISIONING_CLIENT_POLICY_NAME);

	    /* Set resource pointer.  */
	    resource_ptr = &(dps_client.nx_azure_iot_provisioning_client_resource);

	    span = az_span_create(resource_ptr->resource_mqtt_sas_token,
	                          (INT)dps_client.nx_azure_iot_provisioning_client_sas_token_buff_size);

	    status = nx_azure_iot_buffer_allocate(dps_client.nx_azure_iot_ptr,
	                                          &buffer_ptr, &buffer_size,
	                                          &buffer_context);
	    //xprintf("nx_azure_iot_buffer_allocate: BUFFER size %d\r\n", buffer_size);
	    if (status)
	    {
	        xprintf("IoTProvisioning client sas token fail: BUFFER ALLOCATE FAI\r\n");
	        return(status);
	    }

	    //xprintf("expiry_time_secs %ld\r\n", expiry_time_secs);

	    core_result = az_iot_provisioning_client_sas_get_signature(&(dps_client.nx_azure_iot_provisioning_client_core),
	                                                               expiry_time_secs, span, &span);
	    //xprintf("az_iot_provisioning_client_sas_get_signature\r\n");
	    if (az_result_failed(core_result))
	    {
	        xprintf("IoTProvisioning failed failed to get signature with error status: %d\r\n", core_result);
	        return(NX_AZURE_IOT_SDK_CORE_ERROR);
	    }
	    //xprintf("prov_client.nx_azure_iot_provisioning_client_symmetric_key %s\r\n", dps_client.nx_azure_iot_provisioning_client_symmetric_key);
	    //xprintf("prov_client.nx_azure_iot_provisioning_client_symmetric_key_length %d\r\n", dps_client.nx_azure_iot_provisioning_client_symmetric_key_length);

	    status = nx_azure_iot_base64_hmac_sha256_calculate(resource_ptr,
	    												   dps_client.nx_azure_iot_provisioning_client_symmetric_key,
														   dps_client.nx_azure_iot_provisioning_client_symmetric_key_length,
	                                                       az_span_ptr(span), (UINT)az_span_size(span), buffer_ptr, buffer_size,
	                                                       &output_ptr, &output_len);
	    //xprintf("output_ptr %s\r\n", output_ptr);
	    if (status)
	    {
	        xprintf("IoTProvisioning failed to encoded hash\r\n");
	        return(status);
	    }

	    buffer_span = az_span_create(output_ptr, (INT)output_len);

	    //xprintf("11expiry_time_secs %ld\r\n", expiry_time_secs);

	    core_result = az_iot_provisioning_client_sas_get_password(&(dps_client.nx_azure_iot_provisioning_client_core),
	                                                              buffer_span, expiry_time_secs, policy_name,
	                                                              (CHAR *)resource_ptr -> resource_mqtt_sas_token,
																  dps_client.nx_azure_iot_provisioning_client_sas_token_buff_size,
	                                                              &(resource_ptr -> resource_mqtt_sas_token_length));
	    if (az_result_failed(core_result))
	    {
	        xprintf("IoTProvisioning failed to generate token with error : %d\r\n", core_result);
	        return(NX_AZURE_IOT_SDK_CORE_ERROR);
	    }
	    //xprintf("resource_mqtt_sas_token %s\n",resource_ptr -> resource_mqtt_sas_token);
	    strcpy(azure_iotdps_connect_password, resource_ptr -> resource_mqtt_sas_token);
	   	//xprintf("\n*** azure_iotdps_connect_password:%s ***\n", azure_iotdps_connect_password);

	   	resource_dps_sas_token = resource_ptr -> resource_mqtt_sas_token;
	    return(NX_AZURE_IOT_SUCCESS);
}

static UINT nbiot_service_iot_hub_client_sas_token_get(NX_AZURE_IOT_HUB_CLIENT *hub_client_ptr,
                                                  ULONG expiry_time_secs, const UCHAR *key, UINT key_len,
                                                  UCHAR *sas_buffer, UINT sas_buffer_len, UINT *sas_length)
{
UCHAR *buffer_ptr;
UINT buffer_size;
VOID *buffer_context;
az_span span = az_span_create(sas_buffer, (INT)sas_buffer_len);
az_span buffer_span;
UINT status;
UCHAR *output_ptr;
UINT output_len;
az_result core_result;

    status = nx_azure_iot_buffer_allocate(hub_client_ptr -> nx_azure_iot_ptr, &buffer_ptr, &buffer_size, &buffer_context);
    if (status)
    {
        LogError(LogLiteralArgs("IoTHub client sas token fail: BUFFER ALLOCATE FAIL"));
        return(status);
    }

    core_result = az_iot_hub_client_sas_get_signature(&(hub_client_ptr -> iot_hub_client_core),
                                                      expiry_time_secs, span, &span);
    if (az_result_failed(core_result))
    {
        LogError(LogLiteralArgs("IoTHub failed failed to get signature with error status: %d"), core_result);
        nx_azure_iot_buffer_free(buffer_context);
        return(NX_AZURE_IOT_SDK_CORE_ERROR);
    }

    status = nx_azure_iot_base64_hmac_sha256_calculate(&(hub_client_ptr -> nx_azure_iot_hub_client_resource),
                                                       key, key_len, az_span_ptr(span), (UINT)az_span_size(span),
                                                       buffer_ptr, buffer_size, &output_ptr, &output_len);
    if (status)
    {
        LogError(LogLiteralArgs("IoTHub failed to encoded hash"));
        nx_azure_iot_buffer_free(buffer_context);
        return(status);
    }

    buffer_span = az_span_create(output_ptr, (INT)output_len);
    core_result= az_iot_hub_client_sas_get_password(&(hub_client_ptr -> iot_hub_client_core),
                                                    expiry_time_secs, buffer_span, AZ_SPAN_EMPTY,
                                                    (CHAR *)sas_buffer, sas_buffer_len, &sas_buffer_len);
    if (az_result_failed(core_result))
    {
        LogError(LogLiteralArgs("IoTHub failed to generate token with error status: %d"), core_result);
        nx_azure_iot_buffer_free(buffer_context);
        return(NX_AZURE_IOT_SDK_CORE_ERROR);
    }

    *sas_length = sas_buffer_len;
    nx_azure_iot_buffer_free(buffer_context);

    return(NX_AZURE_IOT_SUCCESS);
}

INT nbiot_service_get_iothub_key(ULONG expiry_time_secs, UCHAR *resource_dps_sas_token)
//INT nbiot_service_get_iothub_key(ULONG expiry_time_secs, UCHAR *resource_dps_sas_token,UCHAR *host_name )
{
	UINT            status;
	NXD_ADDRESS     server_address;
	NX_AZURE_IOT_RESOURCE *resource_ptr;
	NXD_MQTT_CLIENT *mqtt_client_ptr;
	UCHAR           *buffer_ptr;
	UINT            buffer_size;
	VOID            *buffer_context;
	UINT            buffer_length;
	az_result       core_result;

    /* Allocate buffer for client id, username and sas token.  */
    status = nx_azure_iot_buffer_allocate(hub_client.nx_azure_iot_ptr,
                                          &buffer_ptr, &buffer_size, &buffer_context);
    if (status)
    {
    	xprintf("IoTHub client failed initialization: BUFFER ALLOCATE FAIL\n");
        return(status);
    }

    /* Set resource pointer and buffer context.  */
    resource_ptr = &(hub_client.nx_azure_iot_hub_client_resource);

    /* Build client id.  */
    buffer_length = buffer_size;
    core_result = az_iot_hub_client_get_client_id(&(hub_client.iot_hub_client_core),
                                                  (CHAR *)buffer_ptr, buffer_length, &buffer_length);
    if (az_result_failed(core_result))
    {
        nx_azure_iot_buffer_free(buffer_context);
        xprintf("IoTHub client failed to get clientId with error status: \n", core_result);
        return(NX_AZURE_IOT_SDK_CORE_ERROR);
    }
    resource_ptr -> resource_mqtt_client_id = buffer_ptr;
    resource_ptr -> resource_mqtt_client_id_length = buffer_length;

    /* Update buffer for user name.  */
    buffer_ptr += resource_ptr -> resource_mqtt_client_id_length;
    buffer_size -= resource_ptr -> resource_mqtt_client_id_length;

    /* Build user name.  */
    buffer_length = buffer_size;
    core_result = az_iot_hub_client_get_user_name(&hub_client.iot_hub_client_core,
                                                  (CHAR *)buffer_ptr, buffer_length, &buffer_length);
    if (az_result_failed(core_result))
    {
        nx_azure_iot_buffer_free(buffer_context);
        xprintf("IoTHub client connect fail, with error status: %d\n", core_result);
        return(NX_AZURE_IOT_SDK_CORE_ERROR);
    }
    resource_ptr -> resource_mqtt_user_name = buffer_ptr;
    resource_ptr -> resource_mqtt_user_name_length = buffer_length;

    /* Build sas token.  */
    resource_ptr -> resource_mqtt_sas_token = buffer_ptr + buffer_length;
    resource_ptr -> resource_mqtt_sas_token_length = buffer_size - buffer_length;

    hub_client.nx_azure_iot_hub_client_symmetric_key = (UCHAR *)DEVICE_SYMMETRIC_KEY;
    hub_client.nx_azure_iot_hub_client_symmetric_key_length = sizeof(DEVICE_SYMMETRIC_KEY) - 1;

    /* Host Name. 20210309 jason+*/
    hub_client.iot_hub_client_core._internal.iot_hub_hostname._internal.ptr = (UCHAR *)azure_iothub_connect_host_name;
    hub_client.iot_hub_client_core._internal.iot_hub_hostname._internal.size = strlen(azure_iothub_connect_host_name) - 1;

    //xprintf("\n*** hub_client_iothub_host_name:%s ***\n", hub_client.iot_hub_client_core._internal.iot_hub_hostname._internal.ptr);

    expiry_time_secs += NX_AZURE_IOT_HUB_CLIENT_TOKEN_EXPIRY;
    status = nbiot_service_iot_hub_client_sas_token_get(&hub_client,
                                                       expiry_time_secs,
                                                       hub_client.nx_azure_iot_hub_client_symmetric_key,
                                                       hub_client.nx_azure_iot_hub_client_symmetric_key_length,
                                                       resource_ptr -> resource_mqtt_sas_token,
                                                       resource_ptr -> resource_mqtt_sas_token_length,
                                                       &(resource_ptr -> resource_mqtt_sas_token_length));
    if (status)
    {
        nx_azure_iot_buffer_free(buffer_context);
        xprintf("IoTHub client connect fail: Token generation failed status: %d", status);
        return(status);
    }
    resource_dps_sas_token = resource_ptr -> resource_mqtt_sas_token;
    //xprintf("resource_mqtt_sas_token %s\n",resource_ptr -> resource_mqtt_sas_token);

    strcpy(azure_iothub_connect_password, resource_ptr -> resource_mqtt_sas_token);
    //xprintf("\n*** azure_iothub_connect_password:%s ***\n", azure_iothub_connect_password);

    return(NX_AZURE_IOT_SUCCESS);
}

/* Define nbiot service thread entry.  */
void nbiot_service_thread_entry(ULONG parameter)
{
	xprintf("### Start nbiot_service_thread ###\n");

	ULONG expiry_time_secs = 0;
	UCHAR *resource_dps_sas_token = NULL;
	//TX_INTERRUPT_SAVE_AREA
	unsigned int nbiot_atcmd_retry_cnt = 0;

	nbiot_service_dps_entry();
	expiry_time_secs = (ULONG)time(NULL);
	nbiot_service_get_dps_key(expiry_time_secs, resource_dps_sas_token);
	//nbiot_service_get_iothub_key(expiry_time_secs, resource_dps_sas_token);

	TX_INTERRUPT_SAVE_AREA

	xprintf("### NBIOT Power ON... ###\n");
	/* NBIOT power on. */
	hx_drv_iomux_set_pmux(IOMUX_PGPIO12, 3);
	hx_drv_iomux_set_outvalue(IOMUX_PGPIO12, 1);
	tx_thread_sleep(3500); //200ms
	hx_drv_iomux_set_outvalue(IOMUX_PGPIO12, 0);
	tx_thread_sleep(6500);

	/* DSP ...*/
	/* Azure PNP DPS¡@Event initial. */
	azure_pnp_iotdps_event = PNP_IOTDPS_INITIAL;
	/* Azure PNP IoTHub¡@Event initial. */
	azure_pnp_iothub_event = PNP_IOTHUB_NBIOT_CERTIFICATION;

	/*"ID_SCOPE/registrations/REGISTRATION_ID/api-version=2019-03-31"*/
	azure_iotdps_connect_user_name ="\"" ID_SCOPE "/registrations/" REGISTRATION_ID
		                                           "/api-version=" AZURE_IOTDPS_SERVICE_VERSION "\"";
	//xprintf("**** azure_iotdps_connect_user_name: %s ****\n",azure_iotdps_connect_user_name);

	/* {"registrationId":REGISTRATION_ID,"payload":{"modelId":MODEL_ID}}*/
	char azure_iotdps_registrations_msg_ascii[] = "{\"registrationId\":\"" REGISTRATION_ID "\""
			",\"payload\":{\"modelId\":\"" MODEL_ID "\"}}";
	//xprintf("**** azure_iotdps_registrations_msg_ascii: %s ****\n",azure_iotdps_registrations_msg_ascii);


	for (int i = 0; i < strlen(azure_iotdps_registrations_msg_ascii); i++){
		sprintf((azure_iotdps_registrations_msg + (i* 2)), "%02X", *(azure_iotdps_registrations_msg_ascii + i));
	 }
	//xprintf("**** azure_iotdps_registrations_msg: %s ****\n",azure_iotdps_registrations_msg);
	sprintf(azure_registrations_msg_len, "%d", strlen(azure_iotdps_registrations_msg));

	xprintf("\n#############################################################################\n");
	xprintf("**** Enter Azure DPS Connect... ****\n");
	xprintf("#############################################################################\n");
	while(1){
		memset(recv_buf,0,AT_MAX_LEN);//clear buffer
		switch(azure_pnp_iotdps_event)
		{
		/* 0:Search IP...*/
		case PNP_IOTDPS_INITIAL:
			if (wnb303r_query_ip(dev_uart_comm,recv_buf, recv_len)==AT_OK){
				xprintf("Search +IP OK!!\n");
				azure_pnp_iotdps_event = PNP_IOTDPS_GET_NETWORK_TIME;
			}else{
				xprintf("### Search +IP... ###\n");
			}
		break;
		/* 1:Get Network time. */
		case PNP_IOTDPS_GET_NETWORK_TIME:
			if(wnb303r_query_time(dev_uart_comm,recv_buf,recv_len) == AT_OK){
				xprintf("Get Network Time OK!!\n");
				azure_iotdps_parsing_network_time();
				azure_pnp_iotdps_event	= PNP_IOTDPS_NBIOT_CERTIFICATION;
				nbiot_atcmd_retry_cnt	= 0;
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### Get Network Time ing... ###\n");
			}
		break;
		/* 2:NBIOT MQTT¡@Certification. */
		case PNP_IOTDPS_NBIOT_CERTIFICATION:
			if(wnb303r_MQTT_certification(dev_uart_comm, "3", "0", "0", "0,",recv_buf,recv_len) == AT_OK){
				xprintf("NBIOT Certification OK!!\n");
				azure_pnp_iotdps_event	= PNP_IOTDPS_CREATE_CONNECTION;
				nbiot_atcmd_retry_cnt	= 0;
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT Certification ing... ###\n");
			}
		break;
		/* 3:Create MQTT Connect to Server. */
		case PNP_IOTDPS_CREATE_CONNECTION:
			if(wnb303r_MQTT_connect_server(dev_uart_comm, "\"" ENDPOINT "\"", "\""NBIOT_MQTT_TLS_PORT"\"", "1200", "1536",recv_buf,recv_len) == AT_OK)
			{
				xprintf("NBIOT Connect to Server OK!!\n");
				azure_pnp_iotdps_event	= PNP_IOTDPS_CONNECT_TO_DPS;
				nbiot_atcmd_retry_cnt	= 0;
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT Connect to Server ing... ###\n");
			}
		break;
		/* 4:NBIOT Connect to DPS. */
		case PNP_IOTDPS_CONNECT_TO_DPS:
			if(wnb303r_MQTT_connect_dps_iothub(dev_uart_comm, "0", "4", "\"" REGISTRATION_ID"\"" ,"240","1", "0", \
					azure_iotdps_connect_user_name, azure_iotdps_connect_password,recv_buf,recv_len) == AT_OK)
			{
				xprintf("NBIOT Connect to DPS OK!!\n");
				azure_pnp_iotdps_event	= PNP_IOTDPS_REGISTRATION_SUBSCRIBE;
				nbiot_atcmd_retry_cnt	= 0;
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT Connect to DPS ing... ###\n");
			}
		break;
		/* 5:MQTT Subscribe Topic Packet. */
		case PNP_IOTDPS_REGISTRATION_SUBSCRIBE:
			if(wnb303r_MQTT_send_subscribe_topic(dev_uart_comm, "0", "\"" AZURE_IOTDPS_CLIENT_REGISTER_SUBSCRIBE_TOPIC "\"", "1", \
					recv_buf,recv_len) == AT_OK)
			{
				xprintf("NBIOT DPS Subscribe Topic OK!!\n");
				azure_pnp_iotdps_event	= PNP_IOTDPS_REGISTRATION_PUBLISH;
				nbiot_atcmd_retry_cnt	= 0;

			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT DPS Subscribe Topic Packet ing... ###\n");
			}
		break;
		 /* 6:MQTT Publish Topic Packet. Get DPS Registration Operation ID. */
		case PNP_IOTDPS_REGISTRATION_PUBLISH:
			if(wnb303r_MQTT_publish_topic(dev_uart_comm, "0", AZURE_IOTDPS_CLIENT_REGISTER_PUBLISH_TOPIC, "1", "0", "0", \
					azure_registrations_msg_len,azure_iotdps_registrations_msg,recv_buf,recv_len,PUBLISH_TOPIC_DPS_IOTHUB)== AT_OK)
			{
				if(azure_pnp_iotdps_get_registration_operation_id())
				{
					xprintf("Parsing DPS Registration Operation ID OK!!\n");
					azure_pnp_iotdps_event	= PNP_IOTDPS_GET_REGISTRATION_STATUS;
					nbiot_atcmd_retry_cnt	= 0;
				}
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT DPS Publish Topic Get Reg OP_ID ing... ###\n");
			}
		break;
		 /* 7:MQTT publish topic packet. Get DPS registration status.*/
		case PNP_IOTDPS_GET_REGISTRATION_STATUS:
			if(0 > wnb303r_MQTT_publish_topic(dev_uart_comm, "0", azure_iotdps_get_registrations_publish_topic, "1", "0", "0", \
					azure_registrations_msg_len,azure_iotdps_get_registrations_msg,recv_buf,recv_len,PUBLISH_TOPIC_DPS_IOTHUB) == AT_OK)
			{
				if(azure_pnp_iotdps_get_registration_status())
				{
					xprintf("Parsing DPS Registration Status OK!!\n");
					azure_pnp_iotdps_event	= PNP_IOTDPS_REGISTRATION_DONE;
					nbiot_atcmd_retry_cnt	= 0;
				}
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT DPS Publish Topic Get Reg Status ing... ###\n");
			}
		break;
		/* DPS Registration Done. */
		case PNP_IOTDPS_REGISTRATION_DONE:
			xprintf("NBIOT DPS Connect OK!!\n");
			nbiot_atcmd_retry_cnt = 0;
		break;
		/* DPS Re-Connect. */
		case PNP_IOTDPS_RECONNECT:
			xprintf("### NBIOT DPS MQTT Re-Connect... ###\n");
			/* MQTT Disconnect DPS. */
			if( wnb303r_MQTT_disconnect(dev_uart_comm,"0",recv_buf,recv_len) == AT_OK){
				xprintf("NBIOT DPS MQTT Disconnect OK!!\n");
				azure_pnp_iotdps_event 	= PNP_IOTDPS_GET_NETWORK_TIME;
				azure_pnp_iothub_event	= PNP_IOTHUB_NBIOT_CERTIFICATION;
				azure_active_event 		= ALGO_EVENT_IDLE;
			}else{
				xprintf("### NBIOT DPS MQTT Disconnect ing... ###\n");
			}
		break;
		}//switch case

		/* MQTT DPS Re-Connect. */
		if(nbiot_atcmd_retry_cnt == NBIOT_ATCMD_RETRY_MAX_TIMES){
			nbiot_atcmd_retry_cnt = 0;
			azure_pnp_iotdps_event = PNP_IOTDPS_RECONNECT;
		}

		 /* MQTT Connect DPS Success. */
		if(azure_pnp_iotdps_event == PNP_IOTDPS_REGISTRATION_DONE)
		{
			/* MQTT Disconnect DPS*/
			if( wnb303r_MQTT_disconnect(dev_uart_comm,"0",recv_buf,recv_len) == AT_OK)
			{
				xprintf("NBIOT DPS MQTT Disconnect OK!!\n");
			}else{
				xprintf("### NBIOT DPS MQTT Disconnect ing... ###\n");
			}
			break;
		}
	}//while

	xprintf("\n#############################################################################\n");
	xprintf("**** Enter Azure IoTHUB Connect... ****\n");
	xprintf("#############################################################################\n");
	/* IOTHUB ...*/
	strcat(azure_iothub_connect_user_name,"\"");
	strcat(azure_iothub_connect_user_name,azure_iothub_connect_host_name);
	strcat(azure_iothub_connect_user_name,"/");
	char azure_iothub_connect_user_name_tmp[] = AZURE_IOTHUB_DEVICE_ID "/?api-version=" AZURE_IOTHUB_SERVICE_VERSION "&model-id=" MODEL_ID "\"";
	strcat(azure_iothub_connect_user_name,azure_iothub_connect_user_name_tmp);
	//xprintf("**** azure_iothub_connect_user_name: %s ****\n",azure_iothub_connect_user_name);

	/* azure iothub publish topic. */
	/* devices/" AZURE_IOTHUB_DEVICE_ID "/messages/events/" */;
	strcat(azure_iothub_publish_topic,"devices/");
	strcat(azure_iothub_publish_topic,AZURE_IOTHUB_DEVICE_ID);
	strcat(azure_iothub_publish_topic,"/messages/events/");
	/* devices/device_id/messages/events/ */
	//xprintf("**** azure_iothub_publish_topic: %s ****\n",azure_iothub_publish_topic);

	memset(recv_buf,0,AT_MAX_LEN);//clear buffer
	azure_pnp_iothub_event = PNP_IOTHUB_NBIOT_CERTIFICATION;
	nbiot_atcmd_retry_cnt = 0;
	while(1){
		if(azure_pnp_iothub_event != PNP_IOTHUB_CONNECTIING){
		}

		switch(azure_pnp_iothub_event)
		{
		/* 0:Search IP...*/
		case PNP_IOTHUB_INITIAL:
			if (wnb303r_query_ip(dev_uart_comm,recv_buf, recv_len)==AT_OK){
				xprintf("Search +IP OK!!\n");
				azure_pnp_iothub_event = PNP_IOTHUB_NBIOT_CERTIFICATION;
			}else{
				xprintf("### Search +IP... ###\n");
			}
		break;
		/* 1: NBIOT MQTT Certification. */
		case PNP_IOTHUB_NBIOT_CERTIFICATION:
			if(wnb303r_MQTT_certification(dev_uart_comm, "3", "0", "0", "0,",recv_buf,recv_len) == AT_OK){
				xprintf("NBIOT Certification OK!!\n");
				azure_pnp_iothub_event	= PNP_IOTHUB_CREATE_CONNECTION;
				nbiot_atcmd_retry_cnt	= 0;
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT Certification ing... ###\n");
			}
		break;
		/* 2:Create MQTT Connect to Server. */
		case PNP_IOTHUB_CREATE_CONNECTION:
			if(wnb303r_MQTT_connect_server(dev_uart_comm, azure_iothub_connect_host_name, "\""NBIOT_MQTT_TLS_PORT"\"", "1200", "1536",recv_buf,recv_len) == AT_OK)
			{
				xprintf("NBIOT Connect to Server OK!!\n");
				azure_pnp_iothub_event	= PNP_IOTHUB_CONNECT_TO_DEVICE;
				nbiot_atcmd_retry_cnt	= 0;
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT Connect to Server ing... ###\n");
			}
		break;
		/* 3:NBIOT Connect to IOTHUB Device. */
		case PNP_IOTHUB_CONNECT_TO_DEVICE:
			if(wnb303r_MQTT_connect_dps_iothub(dev_uart_comm, "0", "4", "\"" AZURE_IOTHUB_DEVICE_ID"\"" ,"240","1", "0", \
					azure_iothub_connect_user_name, azure_iothub_connect_password,recv_buf,recv_len) == AT_OK)
			{
				xprintf("NBIOT Connect to IOTHUB Device OK!!\n");
				azure_pnp_iothub_event	= PNP_IOTHUB_MESSAGE_PUBLISH;
				nbiot_atcmd_retry_cnt	= 0;
			}else{
				++nbiot_atcmd_retry_cnt;
				xprintf("### NBIOT Connect to IOTHUB Device ing... ###\n");
			}
		break;
		/* 4:MQTT Publish Topic Packet. Publish Message to Device/Cloud. */
		case PNP_IOTHUB_MESSAGE_PUBLISH:
			azure_pnp_iothub_event = PNP_IOTHUB_CONNECT_TO_DEVICE_DONE;
		break;
		/* MQTT Connect IOTHUB Device Done. */
		case PNP_IOTHUB_CONNECT_TO_DEVICE_DONE:
			azure_pnp_iothub_event	= PNP_IOTHUB_CONNECTIING;
			azure_active_event		= ALGO_EVENT_IDLE;
		break;
		case PNP_IOTHUB_RECONNECT:
			xprintf("### NBIOT IOTHUB MQTT Re-Connect... ###\n");
			/* MQTT Disconnect IOTHUB. */
			if( wnb303r_MQTT_disconnect(dev_uart_comm,"0",recv_buf,recv_len) == AT_OK)
			{
				xprintf("NBIOT IOTHUB MQTT Disconnect OK!!\n");
				azure_pnp_iothub_event 	= PNP_IOTHUB_NBIOT_CERTIFICATION;
				azure_active_event 		= ALGO_EVENT_IDLE;
			}else{
				xprintf("### NBIOT IOTHUB MQTT Disconnect ing... ###\n");
			}
		break;
		}//switch case

		/* MQTT IOTHUB Re-Connect. */
		if(nbiot_atcmd_retry_cnt == NBIOT_ATCMD_RETRY_MAX_TIMES){
			nbiot_atcmd_retry_cnt	= 0;
			azure_pnp_iothub_event = PNP_IOTHUB_RECONNECT;
		}

		tx_thread_sleep(7000);
	}//while
}


/* Define cis capture image thread entry.  */
void cis_capture_image_thread_entry(ULONG parameter)
{
	xprintf("### Start cis_capture_image_thread_entry ###\n");

	while(1){
		switch(azure_active_event)
		{
		/* Algorithm event idle*/
		case ALGO_EVENT_IDLE:
			if( azure_pnp_iotdps_event	== PNP_IOTDPS_REGISTRATION_DONE && \
				azure_pnp_iothub_event	== PNP_IOTHUB_CONNECTIING)
			{
				tflitemicro_start(); //GetImage -> tflitemicro_algo_run
			}
		break;
		}//switch case
		tx_thread_sleep(7000);
	}//while
}

/* Define algo send result thread entry.  */
void algo_send_result_thread_entry(ULONG parameter)
{
	xprintf("### Start algo_send_result_thread ###\n");
	int ret											= 0;
	unsigned int nbiot_atcmd_retry_cnt				= 0;
	unsigned int azure_img_idx_cnt					= 0;
	unsigned int azure_algo_snd_res_img_event_tmp	= ALGO_EVENT_IDLE;

	memset(recv_buf,0,AT_MAX_LEN);//clear buffer
	while(1){
		switch (azure_active_event)
		{
		/* 0:Not do thing. */
		case ALGO_EVENT_IDLE:
			/* Capture image in cis_capture_image_thread_entry. */
		break;
		/* 1:Send Metadata to Cloud. */
		case ALGO_EVENT_SEND_RESULT_TO_CLOUD:
			ret = send_algo_result_to_cloud();
			if(ret == 1)
			{
				xprintf("NBIOT IOTHUB Send MetaData to Cloud OK!!\n");

				nbiot_atcmd_retry_cnt	= 0;
				/* clear buffer. */
				memset(recv_buf,0,AT_MAX_LEN);
				memset(azure_iothub_publish_msg,0,(SEND_PKG_MAX_SIZE*2));

				if(azure_algo_snd_res_img_event_tmp == ALGO_EVENT_SEND_RESULT_AND_IMAGE){
					azure_active_event = ALGO_EVENT_SEND_IMAGE_TO_CLOUD;
				}else{
#ifdef ENABLE_PMU
					azure_active_event = ENTER_PMU_MODE;
#else
					azure_active_event = ALGO_EVENT_IDLE;
#endif
				}
			}else if(ret == AT_ERROR){
				xprintf("!!!!nbiot_atcmd_metadata_retry_cnt:%d!!!!\n",nbiot_atcmd_retry_cnt);
				++nbiot_atcmd_retry_cnt;
			}
		break;
		/* 2:Send Image to Cloud. */
		case ALGO_EVENT_SEND_IMAGE_TO_CLOUD:
#if 1
			if(g_imgsize > SEND_PKG_MAX_SIZE){
				ret = send_cstm_data_to_cloud((unsigned char *)g_img_cur_addr_pos, SEND_PKG_MAX_SIZE);
				if(ret == 1)
				{
					++azure_img_idx_cnt;
					xprintf("### Send Data:%d/%d Bytes... ###\n",(azure_img_idx_cnt*SEND_PKG_MAX_SIZE),g_pimg_config.jpeg_size);

					g_img_cur_addr_pos += SEND_PKG_MAX_SIZE;
					g_imgsize		   -= SEND_PKG_MAX_SIZE;

					nbiot_atcmd_retry_cnt = 0;
					/* clear buffer. */
					memset(recv_buf,0,AT_MAX_LEN);
					memset(azure_iothub_publish_msg,0,(SEND_PKG_MAX_SIZE*2));
				}else if(ret == AT_ERROR){
					xprintf("!!!!nbiot_atcmd_snd_img_retry_cnt:%d!!!!\n",nbiot_atcmd_retry_cnt);
					++nbiot_atcmd_retry_cnt;
				}
			}else{
				/* g_pimg_config.jpeg_size < SEND_PKG_MAX_SIZE. */
				g_img_cur_addr_pos += g_imgsize;
				ret = send_cstm_data_to_cloud((unsigned char *)g_img_cur_addr_pos, g_imgsize);
				if(ret == 1)
				{
					xprintf("### Send Data:%d/%d Bytes... ###\n",(azure_img_idx_cnt*SEND_PKG_MAX_SIZE) + g_imgsize
							,g_pimg_config.jpeg_size);

					xprintf("NBIOT IOTHUB Send Image to Cloud OK!!\n");
					azure_algo_snd_res_img_event_tmp = ALGO_EVENT_IDLE;
					azure_img_idx_cnt		= 0;
					nbiot_atcmd_retry_cnt	= 0;
					/* clear buffer. */
					memset(recv_buf,0,AT_MAX_LEN);
					memset(azure_iothub_publish_msg,0,(SEND_PKG_MAX_SIZE*2));
#ifdef ENABLE_PMU
					azure_active_event = ENTER_PMU_MODE;
#else
					azure_active_event = ALGO_EVENT_IDLE;
#endif
				}else if(ret == AT_ERROR){
					xprintf("!!!!nbiot_atcmd_snd_img_retry_cnt:%d!!!!\n",nbiot_atcmd_retry_cnt);
					++nbiot_atcmd_retry_cnt;
				}
			}
#endif
		break;
		/* Send Metadata and Imag. */
		case ALGO_EVENT_SEND_RESULT_AND_IMAGE:
			azure_algo_snd_res_img_event_tmp = ALGO_EVENT_SEND_RESULT_AND_IMAGE;
			azure_active_event				 = ALGO_EVENT_SEND_RESULT_TO_CLOUD;
		break;
		/* Azure DPS_IOTHUB Re-Connect... */
		case ALGO_EVENT_IOTHUB_RECONNECT:
			xprintf("\n###  Azure DPS_IOTHUB Re-Connect... ###\n");
			azure_active_event 		= ALGO_EVENT_IDLE;
			azure_pnp_iothub_event  = PNP_IOTHUB_RECONNECT;
		break;
		/*Enter PMU Mode. */
		case ENTER_PMU_MODE:
			/* MQTT Disconnect IOTHUB. */
			if( wnb303r_MQTT_disconnect(dev_uart_comm,"0",recv_buf,recv_len) == AT_OK){
				xprintf("NBIOT IOTHUB MQTT Disconnect OK!!\n");
			}else{
				xprintf("### NBIOT IOTHUB MQTT Disconnect ing... ###\n");
			}

			xprintf("### NBIOT Power OFF ing... ###\n");
			hx_drv_iomux_set_pmux(IOMUX_PGPIO12, 3);
			hx_drv_iomux_set_outvalue(IOMUX_PGPIO12, 1);

			tx_thread_sleep(40250);
			hx_drv_iomux_set_outvalue(IOMUX_PGPIO12, 0);
			xprintf("NBIOT Power OFF OK!!\r\n");

			xprintf("### EnterToPMU() ###\n");
			EnterToPMU(PMU_SLEEP_MS);
		break;
		}//switch case

		if(nbiot_atcmd_retry_cnt == NBIOT_ATCMD_RETRY_MAX_TIMES){
			nbiot_atcmd_retry_cnt = 0;
			azure_active_event = ALGO_EVENT_IOTHUB_RECONNECT;
		}
		tx_thread_sleep(7000);
	}//while
}

static UINT unix_time_get(ULONG *unix_time)
{

    /* Using time() to get unix time on x86.
       Note: User needs to implement own time function to get the real time on device, such as: SNTP.  */
	*unix_time = (ULONG)time(NULL);

    return(NX_SUCCESS);
}
#endif
