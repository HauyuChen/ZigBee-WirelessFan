/******************************************************************************
** ����������SampleApp�޸�Zigbee���̣����߷�����Ŀ����
** ���ܣ�1.����ͨѶ�����պͷ���
         2.�ɼ��¶ȣ�ͨ��DHT11��ʪ�ȴ���������ȡ��ʪ�ȣ�ͨ�����ڷ��͸���λ��
         3.���ȿ��ƣ����ݴ���ָ����Ƽ̵�����ʵ�ַ��ȵĴ򿪺͹ر�
         4.״̬�����������̵���״̬�������Ƿ��
** ���ڣ�2014.12.8
******************************************************************************/

/*********************************************************************
** Ԥ����
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

/* DHT11��ʪ�ȴ�����*/
#include "DHT11.h"

/* ����״̬�Ķ��� */
int flag;
  
/* ���ȼ̵������ŵĶ��� */
#define FAN P1_3


/*********************************************************************
** ��������
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
afAddrType_t Point_To_Point_DstAddr;//�����Ե�ͨ�Ŷ���

aps_Group_t SampleApp_Group;

uint8 SampleAppFlashCounter = 0;


/*********************************************************************
** ����ԭ��
*********************************************************************/
void SampleApp_HandleKeys( uint8 shift, uint8 keys );
void SampleApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
void SampleApp_OpenFan( void );
void SampleApp_CloseFan(void);
static void UART_CallBack (uint8 port,uint8 event);
void SampleApp_SendPointToPointMessage(void); 


/*********************************************************************
** ��������
*********************************************************************/
void SampleApp_Init( uint8 task_id )
{
  SampleApp_TaskID = task_id;
  SampleApp_NwkState = DEV_INIT;
  SampleApp_TransID = 0;

  /* �������ýṹ�� */
  halUARTCfg_t uartConfig;
  /* ���ڳ�ʼ�� */
  uartConfig.configured = TRUE;
  uartConfig.baudRate   = HAL_UART_BR_38400;  //����������Ϊ38400
  uartConfig.flowControl  = FALSE;
  uartConfig.flowControlThreshold = 64;
  uartConfig.rx.maxBufSize        = 128;
  uartConfig.tx.maxBufSize        = 128;
  uartConfig.idleTimeout          = 6;
  uartConfig.intEnable            = TRUE;
  uartConfig.callBackFunc = UART_CallBack;    //UART_CallBack���Լ�д�Ĵ��ڻص�����
  HalUARTOpen(0,&uartConfig);                 //��������
  
  /* �̵���IO�ڳ�ʼ�� */
  P1DIR |= 0x08;
  FAN = 1;  //��ʼ��Ϊ�ߵ�ƽ���̵���Ϊ�͵�ƽ��Ч

#if defined ( BUILD_ALL_DEVICES )
  if ( readCoordinatorJumper() )
    zgDeviceLogicalType = ZG_DEVICETYPE_COORDINATOR;
  else
    zgDeviceLogicalType = ZG_DEVICETYPE_ROUTER;
#endif // BUILD_ALL_DEVICES

#if defined ( HOLD_AUTO_START )
  ZDOInitDevice(0);
#endif

  /* �鲥ͨѶ���� */
  SampleApp_Flash_DstAddr.addrMode = (afAddrMode_t)afAddrGroup;
  SampleApp_Flash_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_Flash_DstAddr.addr.shortAddr = SAMPLEAPP_FLASH_GROUP;

  /* ��Ե�ͨѶ���� */
  Point_To_Point_DstAddr.addrMode = (afAddrMode_t)Addr16Bit; //�㲥
  Point_To_Point_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
  Point_To_Point_DstAddr.addr.shortAddr = 0x0000;           //����Э����
  
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
          if ( //(SampleApp_NwkState == DEV_ZB_COORD)   //�����·�������ն�
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
    /* �¶ȼ�� */
    DHT11_TEST();
    
    //��Ե�ͨѶ�ĳ���
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
    /* �򿪷��� */
    SampleApp_OpenFan( );
  }
  
  if ( keys & HAL_KEY_SW_3 )
  {
    /* �رշ��� */
    SampleApp_CloseFan();
  }
  
  /* ��ӻ��Ƴ��� */
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
      FAN = 0;    //�̵����͵�ƽ��Ч
      HalLedSet( HAL_LED_2, HAL_LED_MODE_ON );  //ָʾ����
      flag = 1;
    break;
      
    case SAMPLEAPP_FAN_OFF_CLUSTERID:
      FAN = 1;    //�̵����͵�ƽ��Ч
      HalLedSet( HAL_LED_2, HAL_LED_MODE_OFF );   //ָʾ����
      flag = 0;
    break;
    
    case SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID:
      /***********�¶ȴ�ӡ***************/
      HalUARTWrite(0,"Temperature:",12);        //��ʾ���յ�����
      HalUARTWrite(0,&pkt->cmd.Data[0],2);      //�¶�
      HalUARTWrite(0," ",1);                    //�ո�
      
     /***************ʪ�ȴ�ӡ****************/
      HalUARTWrite(0,"Humidity:",9);            //��ʾ���յ�����
      HalUARTWrite(0,&pkt->cmd.Data[2],2);      //ʪ��
      HalUARTWrite(0," ",1);                    //�ո� 
      
      /***************����״̬��ӡ****************/
      HalUARTWrite(0,"State:",6);               //��ʾ���յ�����
      HalUARTWrite(0,&pkt->cmd.Data[4],1);      //����״̬
      HalUARTWrite(0,"\n",1);                   // �س����� 
      
      /***************�����¶��Զ�����****************/
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
** �û��Լ�����ĺ���
******************************************************/
/******************************************************
** �򿪷��ȣ�SAMPLEAPP_FAN_ON_CLUSTERID
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
** �رշ��ȣ�SAMPLEAPP_FAN_OFF_CLUSTERID
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
** �ն˷������ݣ�SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID
*/
void SampleApp_SendPointToPointMessage( void )
{
  uint8 T_H[5];     //�¶�+ʪ��+״̬
  
  /* �¶� */
  T_H[0]=wendu_shi+48;      //����ת�����ַ���
  T_H[1]=wendu_ge%10+48;    //����ת�����ַ���
  
  /* ʪ�� */
  T_H[2]=shidu_shi+48;      //����ת�����ַ���
  T_H[3]=shidu_ge%10+48;    //����ת�����ַ���
  
  /* ����״̬ */
  T_H[4]=flag+48;           //����ת�����ַ���
  
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
** ���ڻص�����
*/
static void UART_CallBack (uint8 port,uint8 event)
{
  uint8 usartbuf[5] = " ";
  HalUARTRead(0,usartbuf,5);            //�Ӵ����ж�ȡ���ݣ��������buf��
  if(osal_memcmp(usartbuf,"FAN_Y",5))   //�����ȡ��������FAN_Y
  {
    /* �򿪷��� */
    SampleApp_OpenFan( );
  }
  if(osal_memcmp(usartbuf,"FAN_N",5))   //�����ȡ��������FAN_N
  {
    /* �رշ��� */
    SampleApp_CloseFan( );
  }
}
