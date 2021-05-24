# Himax-AIoT-NB-G2 Platform EVB USER GUIDE

Himax-AIoT-NB-G2 Platform EVB includes Himax WE-I Plus MCU, image sensor and rich peripheral supports. The details are given in the following paragraph. 


## Table of contents

- [Himax-AIoT-NB-G2 Platform EVB](#himax-aiot-nb-g2-platform-evb)
- [System Requirement](#system-requirement)
  - [Hardware Environment Setup](#hardware-environment-setup)
  - [Software Tools](#software-tools)
- [Himax-AIoT-NB-G2 Platform EVB Startup](#himax-aiot-nb-g2-platform-evb-startup)
  - [Startup Flowchart](#startup-flowchart) 
  - [Power ON EVB](#power-on-evb) 
  - [Flash Image Update](#flash-image-update)
  - [After Flash Image Update](#after-flash-image-update)
  - [Check UART Message Output](#check-uart-message-output)
- [Firmware Read Back Backup Flow](#firmware-read-back-backup-flow)
- [Connect to Azure IoT Hub](#connect-to-azure-iot-hub)
  - [Connect Azure Device Provisioning Service and IoTHub Device](#connect-azure-device-provisioning-service-and-iothub-device)
  - [Send Algorithm Metadata](#send-algorithm-metadata)
  - [Send Image](#send-image)
  - [Send Algorithm Metadata and Image](#send-algorithm-metadata-and-image)
  - [Send Custom JSON Format Data](#send-custom-json-format-data)
  - [Enter Power Saving Mode](#enter-power-saving-mode)
  - [Azure RTOS Startup](#azure-rtos-startup)
- [TensorFlow Lite for Microcontroller Example](#tensorflow-lite-for-microcontroller-example)
  - [TFLM Model Path](#tflm-model-path)
  - [TFLM Example Person Detection INT8](#tflm-example-person-detection-int8)
- [Model Deployment Tutorial](#model-deployment-tutorial)


## Himax-AIoT-NB-G2 Platform EVB

![alt text](images/himax_nbiot_evb.png)

  1.	Himax WE-I Plus chip
  2.  Himax Debug Board
  3.	HM0360 AoS<sup>TM</sup> VGA camera
  4.  Microphones (L/R) at back side 
  5.	3-Axis Accelerometer
  6.	3-Axis Gyro
  7.	3-Axis Magnetometer
  8.	Reset Button
  9.	GREEN LED x2 and RED LEDx1 and BLUE LEDx1  
  10.	Micro USB cable: Debug Board (I2C/SPI/Flash Download)

  <a href="docs/H010_HX6539_NB-IoT_WNB303R_V10.pdf" target="_blank">Board Schematic PDF</a>

## System Requirement
  - Hardware Environment Setup
    - Prepare Two Micro USB Cable: connect to EVB (as Power/UART)

  - Software Tools
    - HMX-AIOT-NB-G2_GUI (I2C/CLK/SPI/Flash Image Download)
    - Serial Terminal Emulation Application
      - In the following description, [TeraTerm](https://ttssh2.osdn.jp/index.html.en) and [Minicom](https://linux.die.net/man/1/minicom) 
        will be used.
     
## Himax-AIoT-NB-G2 Platform EVB Startup
  - Use the following procedure to startup the Himax-AIoT-NB-G2 platform EVB.

### Startup Flowchart
    
  ![alt text](images/Himax-AIoT-NB-G2_platform_startup_flowchart.png)

### Power ON EVB
    
![alt text](images/himax_nbiot_evb_debug.png) 

### Flash Image Update
  - Before Power ON EVB
    - Step 1: NB-IoT board SW pin switch to ON
    - Step 2: Debug board SW pin 1 switch to OFF, pin 2 keep ON
    - NB-IoT Board
    
    ![alt text](images/Himax_NB-IoT_board_sw.png)
    
    - Debug Board
    
    ![alt text](images/Himax_Debug_board_sw.png)

  - After Power ON EVB
    - Use HMX-AIOT-NB-G2_GUI Tool update EVB flash image
  
  - Use HMX-AIOT-NB-G2_GUI Tool to download EVB flash image
    - Step 1: Open HMX-AIOT-NB-G2_GUI.exe and switch to Flash download page
    - Step 2: Read ID to check HW ready (ID info Show in blue box)
    - Step 3: Select correct image file
    - Step 4: Erase flash (optional)
    - Step 5: Programming data
      
    ![alt text](images/Himax_Gui_tool_dowload.png) 
    
### After Flash Image Update
  - NB-IoT board SW pin switch to OFF
  - Debug board SW pin 1 switch to ON, pin 2 keep ON
    - NB-IoT Board
    
    ![alt text](images/Himax_NB-IOT_board_sw2.png)
    
    - Debug Board
    
    ![alt text](images/Himax_Debug_Board_sw2.png)

### Check UART Message Output    
  - Serial Terminal Emulation Application Setting 

|   |  |
| :------------ |:---------------:|
| Baud Rate  | 115200 bps |
| Data | 8 bit |
| Parity  | none  |
| Stop  | 1 bit  |
| Flow control | none |   

  The system will output the following message to the UART console. Please setup UART terminal tool setting as (115200/8/N/1).  

  - Display Log Message
  In the example, we use TeraTerm to see the output message.
   
    - TeraTerm New Connection
    
    ![alt text](images/Himax-AIoT-NB-G2_TeraTerm_NewConnect.png)

    - TeraTerm Select COM Port
    
    ![alt text](images/Himax-AIoT-NB-G2_TeraTerm_SelectCOMPort.png)
    
    - Connect DPS Log Message
    
    ![alt text](images/Himax-AIoT-NB-G2_TeraTerm_ConnectDPS_log.png)
        
    - Connect IoTHUB Log Message
    
    ![alt text](images/Himax-AIoT-NB-G2_TeraTerm_ConnectIoTHUB_log.png)
       
    - Run Person Detect Algorithm Log Message
    
    ![alt text](images/Himax-AIoT-NB-G2_TeraTerm_tflitemicro_run_log.png)

  You can use the [Azure IoTHub Explorer](https://docs.microsoft.com/en-us/azure/iot-pnp/howto-use-iot-explorer) tool to see your metadata. 
  As following image is simple demonstrate. 
  Note: model id:`dtmi:himax:himax_aiot_nb_g2;2`
  
  ![alt text](images/Himax-AIoT-NB-G2_Azure_IoT_Explorer_RecvData.png)


## Firmware Read Back Backup Flow
In this chapter, you can know how to firmware image read back backup flow.
- Before Power ON EVB
  - Step 1: NB-IoT board SW pin switch to ON
  - Step 2: Debug board SW pin 1 switch to OFF, pin 2 keep ON
- After Power ON EVB
  - Step 1: Open HMX-AIOT-NB-G2_GUI.exe and switch to Flash download page
  - Step 2: Read ID to check HW ready (ID info Show in blue box)
  - Step 3: Enter Start Addr `0x00000000` and Size `0x00200000`
  - Step 4: Read Back 

   ![alt text](images/Himax-AIoT-NB-G2_fw_readback_image_1.png) 

- Backup firmware image Path
Himax-AIoT-NB-G2-SDK-Azure-RTOS-main\HMX-AIOT-NB-G2_GUI_v2.1.7\read_0000.bin

you can reference [`Flash Image Update`](https://github.com/HimaxWiseEyePlus/Himax-AIoT-NB-G2-SDK-Azure-RTOS/tree/main/Himax-AIoT-NB-G2_user_guide#flash-image-update) re-download `read_0000.bin` to EVB.

##  Connect to Azure IoT Hub
### Connect Azure Device Provisioning Service and IoTHub Device
    - Define as follows value in Himax-AIoT-NB-G2-SDK-Azure-RTOS-main\himax_aiot_nb-master\app\scenario_app\hx_aiot_nb\inc\azure_iothub.h 
      - #define AZURE_DPS_IOTHUB_STANDALONE_TEST 1
      - #define ENDPOINT                        "global.azure-devices-provisioning.net"
      - #define HOST_NAME                       "Key-in your HOST_NAME" 
      - #define REGISTRATION_ID                 "Key-in your REGISTRATION_ID" 
      - #define ID_SCOPE                        "Key-in your ID_SCOPE"
      - #define DEVICE_SYMMETRIC_KEY            "Key-in DEVICE_SYMMETRIC_KEY"
      
more information please reference the file:  
<a href="docs/himax_WEI_Azure_RTOS_Device_getStartedDoc.pdf" target="_blank">himax_WEI_Azure_RTOS_Device_getStartedDoc</a>
      
## Send Algorithm Metadata
    - Algorithm Metadata structure(sample)
     - humanPresence : 1(detect human) , 0(no human)
     - upper_body_bbox.x : bounding box x-axis for detected human.
     - upper_body_bbox.y : bounding box y-axis for detected human. 
     - upper_body_bbox.width : bounding box width for detected human.
     - upper_body_bbox.height : bounding box height for detected human.
    - Change [azure_active_event]  value  in void tflitemicro_start() at Himax-AIoT-NB-G2-SDK-Azure-RTOS-main\himax_tflm-master\app\scenario_app\hx_aiot_nb\hx_aiot_nb.c  
     - azure_active_event = ALGO_EVENT_SEND_RESULT_TO_CLOUD;
     
## Send Image
    - Change [azure_active_event] value in void tflitemicro_start() at Himax-AIoT-NB-G2-SDK-Azure-RTOS-main\himax_tflm-master\app\scenario_app\hx_aiot_nb\hx_aiot_nb.c  
     - azure_active_event = ALGO_EVENT_SEND_IMAGE_TO_CLOUD;

## Send Algorithm Metadata and Image
    - Change [azure_active_event] value in void tflitemicro_start() at Himax-AIoT-NB-G2-SDK-Azure-RTOS-main\himax_tflm-master\app\scenario_app\hx_aiot_nb\hx_aiot_nb.c  
     - azure_active_event = ALGO_EVENT_SEND_RESULT_AND_IMAGE;

## Send Custom JSON Format Data
    - You can send custom json format data to the cloud through send_cstm_data_to_cloud(unsigned char *databuf, int size)
    - unsigned char *databuf: data buffer, data must be json format.
    - int size: data size.

## Enter Power Saving Mode
    - #define ENABLE_PMU in Himax-AIoT-NB-G2-SDK-Azure-RTOS-main\himax_tflm-master\app\scenario_app\hx_aiot_nb\azure_iothub.c 

## Azure RTOS Startup
  - More detail information please reference [here](https://github.com/azure-rtos)

## TensorFlow Lite for Microcontroller Example 

### TFLM Model Path
  - Put your training model in Himax-AIoT-NB-G2-SDK-Azure-RTOS-main\himax_aiot_nb-master\app\scenario_app\hx_aiot_nb folder
 
### TFLM Example Person Detection INT8

  To generate person detection example flash binary for Himax AIoT Platform EVB:
  1. Based on the flow of [person detection example](https://github.com/tensorflow/tensorflow/tree/master/tensorflow/lite/micro/examples/person_detection_experimental#person-detection-example) to generate flash image. 
  2. Download image binary to Himax-AIoT-NB-G2 EVB, detail steps can be found at [flash image update](#flash-image-update).
  3. Person detection example message will be shown on the terminal application. 

### Model Deployment Tutorial

<a href="docs/Model deployment tutorial of Himax_AIOT_NB_G2.pdf" target="_blank">Model Deployment Tutorial PDF</a>
 
