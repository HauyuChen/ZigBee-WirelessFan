# 0 写在前面 #

无线风扇是本人在大二上学期在实验室期间实现的项目，实现了电风扇的无线控制。这也是我学习无线传感器网络实现的第一个项目，看着实验室的电风扇真的被代码操控着，感觉真的很奇妙。

无线风扇项目完整源码：[https://github.com/HauyuChen/ZigBee-WirelessFan](https://github.com/HauyuChen/ZigBee-WirelessFan)

如果对您有帮助，欢迎您在GitHub上给我 Follow 或 Stars ：）

博客原文链接：[http://chenhy.com/post/zb-zigbee-wirelessfan/](http://chenhy.com/post/zb-zigbee-wirelessfan/)

<br/>

# 1 简介 #

无线风扇项目实现了风扇的手动、自动两种模式的控制。

- 手动模式：根据指令控制风扇的开关；
- 自动模式：根据周围环境的温度自动控制风扇，比如当周围温度大于等于 26 摄氏度时自动打开风扇。

大体的思路是这样的： 通过两个 ZigBee 模块组成一个 ZigBee 网络；ZigBee终端外接温湿度传感器、继电器，继电器与风扇连接；ZigBee 协调器通过串口（RS232） 与电脑连接（也可以通过 ZigBee 协调器与 GPRS 模块连接，实现手机控制的功能）。

连接示意图如下：

![](https://raw.githubusercontent.com/HauyuChen/PicsBox/master/fan01.PNG)

虽说是无线风扇功能，实质就是实现传感器数据采集和模块 IO 口控制。风扇控制主要通过 IO 口导通继电器来实现。所以，如果继电器连接电灯、空调等，那就可以实现不同功能了。

<br/>

# 2 功能需求 #
## 2.1 串口通信 ##
- ZigBee 协调器接收温湿度、风扇状态等数据，并通过串口传送给电脑，在串口调试助手上显示相关数据。
- 电脑通过串口向 ZigBee 协调器发送指令，控制风扇的开关。

## 2.2 温湿度数据采集 ##
- ZigBee 终端外接 DHT11 温湿度传感器，通过传感器采集周围环境的温湿度信息；
- ZigBee 终端采集到的温湿度数据通过 ZigBee 网络发送给 ZigBee 协调器；

## 2.3 风扇控制 ##
- 手动模式：ZigBee 协调器接收到电脑发送的指令后根据指令控制电风扇。如，当收到“FAN_Y”，电风扇打开；当受到“FAN_N”，电风扇关闭。
- 自动模式：风扇状态根据周围环境的温度自动调节。如，当周围环境温度大于等于 26 摄氏度时，电风扇自动打开；当周围环境小于 26 摄氏度时，电风扇自动关闭。

## 2.4 状态反馈 ##
- 通过串口调试助手实时显示电风扇状态。

<br/>

# 3 功能实现 #

本项目通过修改 Z-Stack 中的例程 SampleApp 实现。

注：[Z-Stack安装文件链接](https://github.com/HauyuChen/Z-Stack)

## 3.1 ZigBee协议栈中实现串口通信 ##
在第一篇中，简单提了一下串口的初始化，这里说一下如何在 ZigBee 协议栈中实现串口通信。

**（1）在 SampleApp_Init 函数里面添加串口初始化代码**

```
void SampleApp_Init( uint8 task_id )
{
	···

	/* 增加串口初始化代码 */
	halUARTCfg_t uartConfig;		//串口配置结构体
	uartConfig.configured = TRUE;
	uartConfig.baudRate = HAL_UART_BR_38400;		//波特率设置为38400
	uartConfig.flowControl = FALSE;
	uartConfig.flowControlThreshold = 64;
	uartConfig.rx.maxBufSize = 128;
	uartConfig.tx.maxBufSize = 128;
	uartConfig.idleTimeout = 6;
	uartConfig.intEnable = TRUE;
	uartConfig.callBackFunc = UART_CallBack;	//UART_CallBack 是自己写的串口回调函数
	HalUARTOpen(0,&uartConfig); 		//启动串口

	···
}
```

**（2）编写串口回调函数**

还记得在串口初始化中的 uartConfig.callBackFunc = UART_CallBack; 吗？

UART_CallBack 就是我们自己写的串口回调函数，在这个函数中可以定义当串口收到数据时我们要执行什么操作。只要串口接收到数据，程序就会自动调用 UART_CallBack 函数，所以我们只需知道串口接收到了什么数据，并根据不同的数据执行不同的操作。

UART_CallBack 函数代码如下：

```
//当串口接收到数据会自动调用串口回调函数 UART_CallBack
static void UART_CallBack (uint8 port,uint8 event)
{
	uint8 usartbuf[5] = " ";
	HalUARTRead(0,usartbuf,5); 	//从串口中读取数据，并存放在数组 usartbuf 中，这里读取5个字符。
	if(osal_memcmp(usartbuf,"FAN_Y",5)) //如果读取的数据是 FAN_Y
	{
		//执行操作
		SampleApp_OpenFan( ); 	//打开风扇，自己定义的，后面会提到
	}
	if(osal_memcmp(usartbuf,"FAN_N",5)) //如果读取的数据是 FAN_N
	{
		//执行操作
		SampleApp_CloseFan( ); 	//关闭风扇，自己定义的，后面会提到
	}
}
```

通过这样的一段代码我们就可以实现串口接收到特定数据执行特定功能的效果了，只需稍作修改就可以实现其他的功能，可作模板使用。

**（3）ZigBee协议栈中通过串口发送数据**

前面我们知道程序通过 HalUARTRead 函数接收串口数据，其实发送数据也可调用系统自带 API ，这些都是 ZigBee 协议栈里面封装好的，我们暂时只需知道它怎么用就好了。

串口发送数据调用函数 HalUARTWrite ，比 如 通 过 串 口 发 送 数 据 “Temperature:” ，代码如下：

```
HalUARTWrite(0,"Temperature:",12);	//向串口写入内容
```

至此，我们已经知道如何在 ZigBee 程序中接收、发送串口数据了。

## 3.2 ZigBee协议栈中实现传感器数据采集 ##
### 3.2.1 传感器采集数据 ###
在本项目中，采用温湿度传感器 DHT11 采集周围环境的温湿度。拿到 DHT11 传感器后，不用先想着怎么在 ZigBee 协议栈里面用它。

首先，在单片机上用它来采集数据，通过串口调试助手看看采集的数据是否正确。确认在单片机上采集正确后，我们只需将代码加入 ZigBee 工程，然后在 ZigBee 工程中调用即可。比如，下面的两个函数就是采集数据的主要函数，最终在ZigBee程序中调用 DHT11_TEST() 函数即可获取传感器数据。

这里主要讲述在 ZigBee 中的应用，至于如何采集传感器的数据，可以自己在开发板上试试通过 IO 口采集传感器的数据，或者直接使用源代码文件的“DHT11.C”和“DHT11.H”文件。

DHT11.c 内部实现了传感器的数据采集，最后通过函数 DHT11_TEST 封装起来，所以在主文件中调用 DHT11_TEST() 就能获取传感器数据了。

温湿度采集核心代码如下：

```
void COM()  	//温湿写入
{
	uchar i;
	for(i=0;i<8;i++)
	{
		ucharFLAG=2;
		while((!wenshi)&&ucharFLAG++);
		Delay_10us();
		Delay_10us();
		Delay_10us();
		uchartemp=0;
		if(wenshi){
			uchartemp=1;	
		}
		ucharFLAG=2;
		while((wenshi)&&ucharFLAG++);
		if(ucharFLAG==1){
			break;
		}
		ucharcomdata<<=1;
		ucharcomdata|=uchartemp;
	}
}

void DHT11_TEST() 	//温湿传感启动
{
	wenshi=0;
	Delay_ms(19); 	//>18MS
	wenshi=1;
	P1DIR &= ~0x01; //重新配置 IO 口方向
	Delay_10us();
	Delay_10us();
	Delay_10us();
	Delay_10us();
	if(!wenshi)
	{
		ucharFLAG=2;
		while((!wenshi)&&ucharFLAG++);
		ucharFLAG=2;
		while((wenshi)&&ucharFLAG++);
		COM();
		ucharRH_data_H_temp=ucharcomdata;
		COM();
		ucharRH_data_L_temp=ucharcomdata;
		COM();
		ucharT_data_H_temp=ucharcomdata;
		COM();
		ucharT_data_L_temp=ucharcomdata;
		COM();
		ucharcheckdata_temp=ucharcomdata;
		wenshi=1;
		uchartemp=(ucharT_data_H_temp+ucharT_data_L_temp+ucharRH_data_
		H_temp+ucharRH_data_L_temp);
		if(uchartemp==ucharcheckdata_temp)
		{
			ucharRH_data_H=ucharRH_data_H_temp;
			ucharRH_data_L=ucharRH_data_L_temp;
			ucharT_data_H=ucharT_data_H_temp;
			ucharT_data_L=ucharT_data_L_temp;
			ucharcheckdata=ucharcheckdata_temp;
		}
		wendu_shi=ucharT_data_H/10;
		wendu_ge=ucharT_data_H%10;
		shidu_shi=ucharRH_data_H/10;
		shidu_ge=ucharRH_data_H%10;
	}
	else //没成功读取，返回 0
	{
		wendu_shi=0;
		wendu_ge=0;
		shidu_shi=0;
		shidu_ge=0;
	}
	P1DIR |= 0x01; //IO 口需要重新配置
}
```

### 3.2.2 在ZigBee工程添加传感器代码 ###
我们已经知道如何采集传感器数据，可是怎么把它应用在 ZigBee 工程中呢？

很简单，我们知道ZigBee工程中所有的功能都是围绕 SampleApp_ProcessEvent 函数实现的，所以我们应该在这个函数内部作相应的修改。我们可以在自定义事件中调用传感器数据采集的函数，将采集的数据存放在变量中，在ZigBee工程中通过变量来获取温湿度数据。

 SampleApp_ProcessEvent 的大体结构是这样的：

```
uint16 SampleApp_ProcessEvent( uint8 task_id, uint16 events )
{
	/* 系统事件 */
	if ( events & SYS_EVENT_MSG )  
	{
		while ( MSGpkt )
		{
			switch ( MSGpkt->hdr.event )
			{
				case KEY_CHANGE: //按键事件
					//处理代码
				break;

				case AF_INCOMING_MSG_CMD:  //接收到空中消息
					//处理代码
				break;

				case ZDO_STATE_CHANGE: //设备状态改变
					//处理代码
				break;

				default:
					//处理代码
				break;
			}
		}
		return (events ^ SYS_EVENT_MSG);
	}

	/* 自定义事件 */
	if ( events & SAMPLEAPP_SEND_PERIODIC_MSG_EVT )
	{
		//温湿度检测。调用它获取传感器采集到的数据，这个函数就是我们在 DHT11.c 文件定义的
		DHT11_TEST(); 

		//点对点通讯。将 ZigBee 终端采集到的数据发送给 ZigBee 协调器
		SampleApp_SendPointToPointMessage(); 

		//定时触发用户事件，也就是这个函数每个规定的时间就执行一次，所以本项目将每隔一段时间采集周围环境的温湿度数据
		osal_start_timerEx(SampleApp_TaskID,SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
		(SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT + (osal_rand() &
		0x00FF)) );	

		// return unprocessed events
		return (events ^ SAMPLEAPP_SEND_PERIODIC_MSG_EVT);
	}
	return 0;
}
```

### 3.2.3 ZigBee终端发送温湿度数据 ###
前面我们已经获取了传感器采集到的数据，并将这些数据存放在了变量 wendu_shi 、 wendu_ge 、 shidu_shi 、 shidu_ge 中，所以在发送数据函数中将这些数据发送给 ZigBee 协调器就可以了。

ZigBee终端发送空中消息代码如下：

```
void SampleApp_SendPointToPointMessage( void )
{
	uint8 T_H[5]; //温度+湿度+状态

	/* 温度 */
	T_H[0]=wendu_shi+48; 	//整型转换成字符串
	T_H[1]=wendu_ge%10+48; 	//整型转换成字符串

	/* 湿度 */
	T_H[2]=shidu_shi+48; 	//整型转换成字符串
	T_H[3]=shidu_ge%10+48; 	//整型转换成字符串

	/* 风扇状态 */
	T_H[4]=flag+48; 		//整型转换成字符串

	/* 调用AF_DataRequest发送空中消息 */
	if ( AF_DataRequest( &Point_To_Point_DstAddr,
			&SampleApp_epDesc,
			SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID,	//簇 ID
			5,  			//数据长度
			T_H, 			//数据存放位置
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
```

### 3.2.4 ZigBee协调器接收温湿度数据 ###
ZigBee 协调器接收到数据后将数据通过串口发送到电脑，所以在电脑通过串口调试助手就可以知道传感器采集到什么数据了。

还记得在ZigBee终端通过调用 AF_DataRequest 发送空中消息吗？AF_DataRequest 内包含了一个簇ID： SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID ，ZigBee协调器接收到数据后会根据这个簇ID来执行不同的操作。

在 SampleApp_MessageMSGCB 函数内增加以下代码，来实现对簇ID：SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID 的处理。

```
void SampleApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
  	switch ( pkt->clusterId )
  	{

		······

		case SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID: //簇ID
			/* 温度串口输出 */
			HalUARTWrite(0,"Temperature:",12); 		//输出“Temperature:”
			HalUARTWrite(0,&pkt->cmd.Data[0],2); 	//输出温度数据
			HalUARTWrite(0," ",1); 					//输出空格
			/* 湿度串口输出 */
			HalUARTWrite(0,"Humidity:",9); 			//输出“Humidity:”
			HalUARTWrite(0,&pkt->cmd.Data[2],2); 	//输出温度数据
			HalUARTWrite(0," ",1); 					//输出空格
			/* 风扇状态打印 */
			HalUARTWrite(0,"State:",6); 			//输出“State:”
			HalUARTWrite(0,&pkt->cmd.Data[4],1); 	//输出风扇状态
			HalUARTWrite(0,"\n",1); 				//回车换行

		······

	}
}
```

 ZigBee 协调器通过串口连接电脑，打开串口调试助手工具，将输出温湿度、风扇状态数据。

至此，我们已经实现周围环境温湿度数据、风扇状态的获取，并通过电脑显示出来。

## 3.3 如何实现风扇控制 ##
实现风扇控制，实际上就是实现继电器的控制，将电风扇的地线和火线分别接到继电器的两个口（注意安全，先关闭电闸！），然后将继电器连接在 ZigBee 终端的 IO 口，通过控制这个 IO 的状态，就可以使继电器导通或者不导通，从而控制电风扇的转动。

```
/* 风扇继电器引脚的定义 */
#define FAN P1_3
```

使用IO 口 P1_3，那么将继电器的控制线接到 P1_3 引脚就可以了。也就是说，P1_3 口的状态决定风扇的状态。

### 3.3.1 手动模式 ###
**（1）串口接收控制指令**

前面我们提到可以通过串口回调函数来根据接收的数据执行不同的操作。当串口接收到数据时，判断数据是否是控制指令。若是，执行相应的功能。

串口回调函数代码如下：

```
static void UART_CallBack (uint8 port,uint8 event)
{
	uint8 usartbuf[5] = " ";
	HalUARTRead(0,usartbuf,5); 		//从串口读取数据，存放在 usartbuf
	if(osal_memcmp(usartbuf,"FAN_Y",5)) 	//如果读取的数据是FAN_Y
	{
		SampleApp_OpenFan( ); 	//打开风扇（自己定义的函数，后面提到）
	}
	if(osal_memcmp(usartbuf,"FAN_N",5)) 	//如果读取的数据是FAN_N
	{
		SampleApp_CloseFan( ); 	//关闭风扇（自己定义的函数，后面提到）
	}
}
```
**（2）打开风扇：SampleApp_OpenFan()**

打开风扇，其实就是 ZigBee 协调器向 ZigBee 终端发送簇 ID 为
SAMPLEAPP_FAN_ON_CLUSTERID 的数据，发送空中消息我们还是通过调用 AF_DataReques 来实现。

SampleApp_OpenFan() 实现代码如下：

```
void SampleApp_OpenFan(void)
{
	HalLedBlink(HAL_LED_2,3,50,100);	//闪个灯先
	uint8 data[1];
	data[0] = 1; 		//任意数据均可，这里通过1代表打开风扇
	if(AF_DataReques(&SampleApp_Flash_DstAddr,&SampleApp_epDesc,
			SAMPLEAPP_FAN_ON_CLUSTERID,		//簇ID
			1,
			data,
			&SampleApp_TransID,
			AF_DISCV_ROUTE,
			AF_DEFAULT_RADIUS  )  ==afStatus_SUCCESS )
	{
	}
	else
	{
		// Error occurred in request to send.
	}
}
```

这样， ZigBee 协调器就向 ZigBee 终端发送了一个簇 ID 为 SAMPLEAPP_FAN_ON_CLUSTERID ，内容为 1 的消息了， ZigBee 终端将对接收的消息进行处理。

**（3）关闭风扇：SampleApp_CloseFan()**

同样，关闭风扇就是 ZigBee 协调器向 ZigBee 终端发送簇 ID 为 SAMPLEAPP_FAN_OFF_CLUSTERID 的数据。

SampleApp_CloseFan() 实现代码如下：

```
void SampleApp_CloseFan(void)
{
	HalLedBlink(HAL_LED_2,3,50,100);
	uint8 data[1];
	data[1] = 0; 		//任意数据均可，这里通过0代表关闭风扇
	if(AF_DataRequest(&SampleApp_Flash_DstAddr,&SampleApp_epDesc,
			SAMPLEAPP_FAN_OFF_CLUSTERID,	//簇ID
			1,
			data,
			&SampleApp_TransID,
			AF_DISCV_ROUTE,
			AF_DEFAULT_RADIUS  )  ==afStatus_SUCCESS )
	{
	}
	else
	{
		// Error occurred in request to send.
	}
}
```

这样， ZigBee 协调器就向 ZigBee 终端发送了一个簇 ID 为 SAMPLEAPP_FAN_OFF_CLUSTERID ，内容为 0 的消息了， ZigBee 终端将对接收的消息进行处理。

（4）ZigBee终端控制 IO 口状态

当 ZigBee 终端探测到有空中消息时，程序将走到 SampleApp_ProcessEvent 函数中的 case AF_INCOMING_MSG_CMD ，我们只需在这里调用消息处理函数即可。

```
case AF_INCOMING_MSG_CMD:
	SampleApp_MessageMSGCB( MSGpkt );
break;
```

然后，在消息处理函数 SampleApp_MessageMSGCB 中判断接收的是什么数据，从而执行不同的操作。

```
void SampleApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
	switch ( pkt->clusterId )
	{
		······

		case SAMPLEAPP_FAN_ON_CLUSTERID:  //簇ID
			FAN = 0; 	//FAN是我们前面定义的控制继电器的IO端口，因为继电器低电平有效，为 0 时导通，所以风扇导通了电源，将开始转动
			HalLedSet( HAL_LED_2, HAL_LED_MODE_ON ); //指示灯亮
			flag = 1;	//风扇状态，1表示风扇已打开
		break;
	
		case SAMPLEAPP_FAN_OFF_CLUSTERID:  //簇ID
			FAN = 1; 		//FAN是我们前面定义的控制继电器的IO端口，因为继电器低电平有效，为 1 时不导通，所以风扇将停止转动
			HalLedSet( HAL_LED_2, HAL_LED_MODE_OFF ); //指示灯灭
			flag = 0;		//风扇状态，0表示风扇已关闭
		break;

		······
	}
}
```

至此，我们实现了手动控制电风扇的功能。 ZigBee 协调器向 ZigBee 终端发送不同的数据， ZigBee 终端根据不同数据执行不同的操作，控制连接继电器的 IO 口，从而控制电风扇的状态。

### 3.3.2 自动模式 ###
前面提到 ZigBee 终端采集到传感器的数据后，会将这些数据通过 ZigBee 网络发送给 ZigBee 协调器。自动模式要实现温度大于设定值后自动打开，也就是说，要判断接收到的数据是多少，然后根据温度数据来决定风扇是否打开。

我们只需在 ZigBee 协调器接收数据时进行判断，添加我们的执行代码即可，实现代码如下：

```
void SampleApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{
	switch ( pkt->clusterId )
	{
		······

		case SAMPLEAPP_SEND_DATA_PTOP_CLUSTERID: //接收到传感器数据

			······		

			int t = (pkt->cmd.Data[0]-48)*10 + (pkt->cmd.Data[1]-48);	//数据转换，t为温度值
			int flag;
			if(t>=26) 		//温度值大于等于 26 摄氏度
			{
				SampleApp_OpenFan();  	//打开风扇
				flag = 1;				//更新风扇状态
			}
			else if(t<26) 	//温度值小于 26 摄氏度
			{
				SampleApp_CloseFan();  	//关闭风扇
				flag = 0;				//更新风扇状态
			}
		break;

		······
	}
}

```

至此，我们已经实现了所有的功能，当温度大于等于 26 摄氏度或发送“FAN_Y”指令，电风扇将开始转动；当温度小于 26 摄氏度或发送“FAN_N”指令，电风扇将停止转动。

<br/>

# 3 总结 #
无线风扇这个项目其实不难，只需有 ZigBee 组网、IO 口控制和传感器数据采集的知识就可以实现这样的功能。刚学习 ZigBee 时可以试试实现这样的小功能，比较容易找到学习的乐趣。

本文讲了整个小项目的大体思路和部分实现细节，具体的实现方式可参考本文开头给出的项目源代码。

<br/>