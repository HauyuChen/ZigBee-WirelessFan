
#include <ioCC2530.h>
#include "OnBoard.h"

#define uint unsigned int
#define uchar unsigned char

#define wenshi P1_0

/*******函数声明*********/
void Delay_us(void); //1 us延时
void Delay_10us(void); //10 us延时
void Delay_ms(uint Time);//n ms延时
void COM(void);	// 温湿写入
void DHT11_TEST(void) ;  //温湿传感启动


//温湿度定义
uchar ucharFLAG,uchartemp;
uchar shidu_shi,shidu_ge,wendu_shi,wendu_ge=4;
uchar ucharT_data_H,ucharT_data_L,ucharRH_data_H,ucharRH_data_L,ucharcheckdata;
uchar ucharT_data_H_temp,ucharT_data_L_temp,ucharRH_data_H_temp,ucharRH_data_L_temp,ucharcheckdata_temp;
uchar ucharcomdata;

uchar temp[2]={0,0}; 
uchar temp1[5]="temp=";
uchar humidity[2]={0,0};
uchar humidity1[9]="humidity=";

/****************************
//延时函数
*****************************/
void Delay_us(void) //1 us延时

{
    MicroWait(1);   
}

void Delay_10us(void) //10 us延时
{
   MicroWait(10);
}

void Delay_ms(uint Time)//n ms延时
{
  unsigned char i;
  while(Time--)
  {
    for(i=0;i<100;i++)
     Delay_10us();
  }
}


/***********************
   温湿度传感
***********************/
void COM()	// 温湿写入
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
     if(wenshi)uchartemp=1;
     ucharFLAG=2;
     while((wenshi)&&ucharFLAG++);   
     if(ucharFLAG==1)break;    
     ucharcomdata<<=1;
     ucharcomdata|=uchartemp; 
     }    
}

void DHT11_TEST()   //温湿传感启动
{
    wenshi=0;
    Delay_ms(19);  //>18MS
    wenshi=1; 
    P1DIR &= ~0x01; //重新配置IO口方向
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
      uchartemp=(ucharT_data_H_temp+ucharT_data_L_temp+ucharRH_data_H_temp+ucharRH_data_L_temp);
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
    else //没用成功读取，返回0
    {
         wendu_shi=0; 
         wendu_ge=0;
	 
         shidu_shi=0; 
         shidu_ge=0;  
    } 
    
     P1DIR |= 0x01; //IO口需要重新配置 
}
