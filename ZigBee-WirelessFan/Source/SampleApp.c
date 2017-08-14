/******************************************************************************
** 概述：基于SampleApp修改Zigbee工程，无线风扇项目程序
** 功能：1.串口通讯；接收和发送
         2.采集温度：通过DHT11温湿度传感器，读取温湿度，通过串口发送给上位机
         3.风扇控制：根据串口指令，控制继电器，实现风扇的打开和关闭
         4.状态反馈：反馈继电器状态，风扇是否打开
** 日期：2014.12.8
******************************************************************************/

/*********************************************************************
** 预处理
*********************************************************************/
#include "OSAL.h"
#include "ZGlobals.h"
#include "AF.h"
#include "aps_groups.h"
#include "ZDApp.h"

#include "SampleApp.h"
#include "SampleAppHw.h"

#include "OnBoard.h"

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"

/* DHT11温湿度传感器*/
#include "DHT11.h"

/* 风扇状态的定义 */
int flag;
  
/* 风扇继电器引脚的定义 */
#define FAN P1_3


/*********************************************************************
** 变量定义
*********************************************************************/
const cId_t SampleApp_ClusterList[SAMPLEAPP_MAX_CLUSTERS] =
{
  SAMPLEAPP_FAN_ON_CLUSTERID,
  SAMPLEAPP_FAN_OFF_CLUSTERID,
  SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID
};

const SimpleDescriptionFormat_t SampleApp_SimpleDesc =
{
  SAMPLEAPP_ENDPOINT,              //  int Endpoint;
  SAMPLEAPP_PROFID,                //  uint16 AppProfId[2];
  SAMPLEAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  SAMPLEAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  SAMPLEAPP_FLAGS,                 //  int   AppFlags:4;
  SAMPLEAPP_MAX_CLUSTERS,          //  uint8  AppNumInClusters;
  (cId_t *)SampleApp_ClusterList,  //  uint8 *pAppInClusterList;
  SAMPLEAPP_MAX_CLUSTERS,          //  uint8  AppNumInClusters;
  (cId_t *)SampleApp_ClusterList   //  uint8 *pAppInClusterList;
};

endPointDesc_t SampleApp_epDesc;

uint8 SampleApp_TaskID;   // Task ID for internal task/event processing
                          // This variable will be received when
                          // SampleApp_Init() is called.
devStates_t SampleApp_NwkState;

uint8 SampleApp_TransID;  // This is the unique message ID (counter)

afAddrType_t SampleApp_Periodic_DstAddr;
afAddrType_t SampleApp_Flash_DstAddr;
afAddrType_t Point_To_Point_DstAddr;//网蜂点对点通信定义

aps_Group_t SampleApp_Group;

uint8 SampleAppFlashCounter = 0;


/*********************************************************************
** 函数原型
*********************************************************************/
void SampleApp_HandleKeys( uint8 shift, uint8 keys );
void SampleApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
void SampleApp_OpenFan( void );
void SampleApp_CloseFan(void);
static void UART_CallBack (uint8 port,uint8 event);
void SampleApp_SendPointToPointMessage(void); 


/*********************************************************************
** 函数定义
*********************************************************************/
void SampleApp_Init( uint8 task_id )
{
  SampleApp_TaskID = task_id;
  SampleApp_NwkState = DEV_INIT;
  SampleApp_TransID = 0;

  /* 串口配置结构体 */
  halUARTCfg_t uartConfig;
  /* 串口初始化 */
  uartConfig.configured = TRUE;
  uartConfig.baudRate   = HAL_UART_BR_38400;  //波特率设置为38400
  uartConfig.flowControl  = FALSE;
  uartConfig.flowControlThreshold = 64;
  uartConfig.rx.maxBufSize        = 128;
  uartConfig.tx.maxBufSize        = 128;
  uartConfig.idleTimeout          = 6;
  uartConfig.intEnable            = TRUE;
  uartConfig.callBackFunc = UART_CallBack;    //UART_CallBack是自己写的串口回调函数
  HalUARTOpen(0,&uartConfig);                 //启动串口
  
  /* 继电器IO口初始化 */
  P1DIR |= 0x08;
  FAN = 1;  //初始化为高电平，继电器为低电平有效

#if defined ( BUILD_ALL_DEVICES )
  if ( readCoordinatorJumper() )
    zgDeviceLogicalType = ZG_DEVICETYPE_COORDINATOR;
  else
    zgDeviceLogicalType = ZG_DEVICETYPE_ROUTER;
#endif // BUILD_ALL_DEVICES

#if defined ( HOLD_AUTO_START )
  ZDOInitDevice(0);
#endif

  /* 组播通讯定义 */
  SampleApp_Flash_DstAddr.addrMode = (afAddrMode_t)afAddrGroup;
  SampleApp_Flash_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_Flash_DstAddr.addr.shortAddr = SAMPLEAPP_FLASH_GROUP;

  /* 点对点通讯定义 */
  Point_To_Point_DstAddr.addrMode = (afAddrMode_t)Addr16Bit; //点播
  Point_To_Point_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
  Point_To_Point_DstAddr.addr.shortAddr = 0x0000;           //发给协调器
  
  SampleApp_epDesc.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_epDesc.task_id = &SampleApp_TaskID;
  SampleApp_epDesc.simpleDesc
            = (SimpleDescriptionFormat_t *)&SampleApp_SimpleDesc;
  SampleApp_epDesc.latencyReq = noLatencyReqs;

  afRegister( &SampleApp_epDesc );

  RegisterForKeys( SampleApp_TaskID );

  SampleApp_Group.ID = 0x0001;
  osal_memcpy( SampleApp_Group.name, "Group 1", 7  );
  aps_AddGroup( SAMPLEAPP_ENDPOINT, &SampleApp_Group );

#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "SampleApp", HAL_LCD_LINE_1 );
#endif
}

uint16 SampleApp_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( SampleApp_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        case KEY_CHANGE: 
          SampleApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case AF_INCOMING_MSG_CMD:
          SampleApp_MessageMSGCB( MSGpkt );
          break;

        case ZDO_STATE_CHANGE:
          SampleApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
          if ( //(SampleApp_NwkState == DEV_ZB_COORD)   //如果是路由器或终端
                 (SampleApp_NwkState == DEV_ROUTER)
              || (SampleApp_NwkState == DEV_END_DEVICE) )
          {
            osal_start_timerEx( SampleApp_TaskID,
                              SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
                              SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT );
          }
          else
          {
            // Device is no longer in the network
          }
          break;

        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt );

      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( SampleApp_TaskID );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }
  
  if ( events & SAMPLEAPP_SEND_PERIODIC_MSG_EVT )
  {  
    /* 温度检测 */
    DHT11_TEST();
    
    //点对点通讯的程序
    SampleApp_SendPointToPointMessage();
    
    osal_start_timerEx( SampleApp_TaskID, SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
        (SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT + (osal_rand() & 0x00FF)) );
    // return unprocessed events
    return (events ^ SAMPLEAPP_SEND_PERIODIC_MSG_EVT);
  }

  // Discard unknown events
  return 0;
}

void SampleApp_HandleKeys( uint8 shift, uint8 keys )
{
  (void)shift;  // Intentionally unreferenced parameter
  
  if ( keys & HAL_KEY_SW_1 )
  {
    /* 打开风扇 */
    SampleApp_OpenFan( );
  }
  
  if ( keys & HAL_KEY_SW_3 )
  {
    /* 关闭风扇 */
    SampleApp_CloseFan();
  }
  
  /* 添加或移除组 */
  if ( keys & HAL_KEY_SW_5 )
  {
    aps_Group_t *grp;
    grp = aps_FindGroup( SAMPLEAPP_ENDPOINT, SAMPLEAPP_FLASH_GROUP );
    if ( grp )
    {
      // Remove from the group
      aps_RemoveGroup( SAMPLEAPP_ENDPOINT, SAMPLEAPP_FLASH_GROUP );
    }
    else
    {
      // Add to the flash group
      aps_AddGroup( SAMPLEAPP_ENDPOINT, &SampleApp_Group );
    }
  }
}

void SampleApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
  switch ( pkt->clusterId )
  {
    case SAMPLEAPP_FAN_ON_CLUSTERID:
      FAN = 0;    //继电器低电平有效
      HalLedSet( HAL_LED_2, HAL_LED_MODE_ON );  //指示灯亮
      flag = 1;
    break;
      
    case SAMPLEAPP_FAN_OFF_CLUSTERID:
      FAN = 1;    //继电器低电平有效
      HalLedSet( HAL_LED_2, HAL_LED_MODE_OFF );   //指示灯灭
      flag = 0;
    break;
    
    case SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID:
      /***********温度打印***************/
      HalUARTWrite(0,"Temperature:",12);        //提示接收到数据
      HalUARTWrite(0,&pkt->cmd.Data[0],2);      //温度
      HalUARTWrite(0," ",1);                    //空格
      
     /***************湿度打印****************/
      HalUARTWrite(0,"Humidity:",9);            //提示接收到数据
      HalUARTWrite(0,&pkt->cmd.Data[2],2);      //湿度
      HalUARTWrite(0," ",1);                    //空格 
      
      /***************风扇状态打印****************/
      HalUARTWrite(0,"State:",6);               //提示接收到数据
      HalUARTWrite(0,&pkt->cmd.Data[4],1);      //风扇状态
      HalUARTWrite(0,"\n",1);                   // 回车换行 
      
      /***************根据温度自动调节****************/
      /*
      int t = (pkt->cmd.Data[0]-48)*10 + (pkt->cmd.Data[1]-48);
      int flag;
      if(t>=26)
      {
        SampleApp_OpenFan();
        flag = 1;
      }
      else if(t<26) 
      {
        SampleApp_CloseFan();
        flag = 0;
      }
      break;
      */
  }
}


/******************************************************
** 用户自己定义的函数
******************************************************/
/******************************************************
** 打开风扇：SAMPLEAPP_FAN_ON_CLUSTERID
*/
void SampleApp_OpenFan(void)
{
  HalLedBlink(HAL_LED_2,3,50,100);
  uint8 data[1];
  data[0] = 1;
  if ( AF_DataRequest( &SampleApp_Flash_DstAddr, &SampleApp_epDesc,
                       SAMPLEAPP_FAN_ON_CLUSTERID,
                       1,
                       data,
                       &SampleApp_TransID,
                       AF_DISCV_ROUTE,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
}

/******************************************************
** 关闭风扇：SAMPLEAPP_FAN_OFF_CLUSTERID
*/
void SampleApp_CloseFan(void)
{
  HalLedBlink(HAL_LED_2,3,50,100);
  uint8 data[1];
  data[1] = 2;
  if ( AF_DataRequest( &SampleApp_Flash_DstAddr, &SampleApp_epDesc,
                       SAMPLEAPP_FAN_OFF_CLUSTERID,
                       1,
                       data,
                       &SampleApp_TransID,
                       AF_DISCV_ROUTE,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
}

/******************************************************
** 终端发送数据：SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID
*/
void SampleApp_SendPointToPointMessage( void )
{
  uint8 T_H[5];     //温度+湿度+状态
  
  /* 温度 */
  T_H[0]=wendu_shi+48;      //整型转换成字符串
  T_H[1]=wendu_ge%10+48;    //整型转换成字符串
  
  /* 湿度 */
  T_H[2]=shidu_shi+48;      //整型转换成字符串
  T_H[3]=shidu_ge%10+48;    //整型转换成字符串
  
  /* 风扇状态 */
  T_H[4]=flag+48;           //整型转换成字符串
  
  if ( AF_DataRequest( &Point_To_Point_DstAddr,
                       &SampleApp_epDesc,
                       SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID,
                       5,
                       T_H,
                       &SampleApp_TransID,
                       AF_DISCV_ROUTE,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
}

/******************************************************
** 串口回调函数
*/
static void UART_CallBack (uint8 port,uint8 event)
{
  uint8 usartbuf[5] = " ";
  HalUARTRead(0,usartbuf,5);            //从串口中读取数据，并存放在buf中
  if(osal_memcmp(usartbuf,"FAN_Y",5))   //如果读取的数据是FAN_Y
  {
    /* 打开风扇 */
    SampleApp_OpenFan( );
  }
  if(osal_memcmp(usartbuf,"FAN_N",5))   //如果读取的数据是FAN_N
  {
    /* 关闭风扇 */
    SampleApp_CloseFan( );
  }
}
