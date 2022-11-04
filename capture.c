#include <stdio.h>
#include <wiringPi.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#define MAX_DATA 1024
const int dataPin = 1;
static const char* const windDirections[] = {"N","NE","E","SE","S","SW","W","NW"};

//Global variables
bool dataBuff[MAX_DATA]; //received bits buffer
int p=0; //received bits buffer pointer
bool endOfMessage=false; //indicates end of message
static int dur; //bit changes duration

int crc8(bool *BitString,int nBits)
{
        char CRC=0;
        int  i;
        bool DoInvert;
        for (i=0; i<nBits; ++i)
        {
                DoInvert = (BitString[i] ^ (CRC&0x80)>7);         // XOR required?
                if(DoInvert){
                        CRC=CRC^0x18;
                }
                CRC=CRC<<1;
                if(DoInvert){
                        CRC=CRC|0x01;
                }
        }
        return(CRC);
}

void decode(unsigned char byteArray[8]){
        int id;
        long aux;
        float temp,viento,rafaga,rAcum;
        int hum,wSpeed,wGust,unk,status,dir,crc;
        time_t rawtime,curTime;
        struct tm * timeinfo;
        unsigned char bcdByte;
        FILE *fp;

        int type=(byteArray[0]&0xF0)>>4;
        switch (type){
                case 0x0A: //Weather message
                        //Getting the data
                        id=((byteArray[0]&0x0F)<<4)+ ((byteArray[2]&0xF0)>>4);
                        aux=((byteArray[1]&0x0F)<<8)+ ((byteArray[2]&0xFF));
                        temp=(aux*0.1)-40;
                        hum=byteArray[3];
                        wSpeed=byteArray[4];
                        wGust=byteArray[5];
                        unk=(byteArray[6]&0xF0)>>4;
                        aux=((byteArray[6]&0x0F)<<8)+ ((byteArray[7]&0xFF));
                        rAcum=aux*0.3;
                        status=(byteArray[8]&0xF0)>>4;
                        dir=(byteArray[8]&0x0F)>>1;

                        //Data to CSV file
                        if(unk==0 && status==0){
                                fp = fopen ("/home/pi/estacion/data/data.csv", "a");
                                time ( &rawtime );
                                timeinfo = localtime ( &rawtime );
                                fprintf(fp,"%02d/%02d/%04d %02d:%02d:%02d;0x%02X;%d;%0.1f;%d;%d;%d;0x%02X;%0.1f;0x%02X;%d;%s\n",
                                        timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
                                        type,id,temp,hum,wSpeed,wGust,unk,rAcum,status,dir*45,windDirections[dir]);
                                fclose(fp);
                                fp = fopen ("/home/pi/estacion/data/data2.csv", "a");
                                time ( &rawtime );
                                timeinfo = localtime ( &rawtime );
                                fprintf(fp,"%02d/%02d/%04d %02d:%02d:%02d,%0.1f,%d,%d,%d,%0.1f,%d\n",
                                        timeinfo->tm_mon + 1,timeinfo->tm_mday, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
                                        temp,hum,wSpeed,wGust,rAcum,dir*45);
                                fclose(fp);

                        }

                        //Debug
                        fprintf(stderr,"Message: 0x%02X\n",type);
                        fprintf(stderr,"ID: %d\n",id);
                        fprintf(stderr,"Temperature: %0.1f ºC\n",temp);
                        fprintf(stderr,"Humidity: %d %\n",hum);
                        fprintf(stderr,"Wind speed: %d\n",wSpeed);
                        fprintf(stderr,"Wind gust: %d\n",wGust);
                        fprintf(stderr,"Unknown: 0x%02X\n",unk);
                        fprintf(stderr,"Rainfall acum: %0.1f mm\n",rAcum);
                        fprintf(stderr,"Status bits: 0x%02X\n",status);
                        fprintf(stderr,"Wind direction: %d %s\n",dir,windDirections[dir]);
                        break;
                case 0x0B: //DCF77
                        //Getting the data
                        bcdByte=byteArray[2]&0x3F;
                        timeinfo->tm_hour=(((bcdByte & 0xF0) >> 4) * 10) + (bcdByte & 0x0F);//hour BCD
                        fprintf(stderr,"Hour: %02X -> %d\n", byteArray[2],timeinfo->tm_hour);

                        bcdByte=byteArray[3];
                        timeinfo->tm_min=(((bcdByte & 0xF0) >> 4) * 10) + (bcdByte & 0x0F); //minute BCD
                        fprintf(stderr,"Min: %02X -> %d\n", byteArray[3],timeinfo->tm_min);

                        bcdByte=byteArray[4];
                        timeinfo->tm_sec=(((bcdByte & 0xF0) >> 4) * 10) + (bcdByte & 0x0F); //second BCD
                        fprintf(stderr,"Sec: %02X -> %d\n", byteArray[4],timeinfo->tm_sec);

                        bcdByte=byteArray[5];
                        timeinfo->tm_year=(((bcdByte & 0xF0) >> 4) * 10) + (bcdByte & 0x0F) + 100; //year BCD from 2000
                        fprintf(stderr,"Year: %02X -> %d\n", byteArray[5],timeinfo->tm_year);

                        bcdByte=byteArray[6]&0x3F;
                        timeinfo->tm_mon=(((bcdByte & 0xF0) >> 4) * 10) + (bcdByte & 0x0F) -1; //month BCD starting at 1
                        fprintf(stderr,"Month: %02X -> %d\n", byteArray[6],timeinfo->tm_mon);

                        bcdByte=byteArray[7];
                        timeinfo->tm_mday=(((bcdByte & 0xF0) >> 4) * 10) + (bcdByte & 0x0F); //second BCD
                        fprintf(stderr,"Day: %02X -> %d\n", byteArray[7],timeinfo->tm_mday);

                        //Time data to system
                        time(&curTime);
                        rawtime=mktime(timeinfo);
                        if(abs(difftime(curTime,rawtime))<7200){ //2 hours
                                stime(&rawtime);
                        }

                        //Debug
                        fprintf(stderr,"New time: %02d/%02d/%04d %02d:%02d:%02d",timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
                        break;
                default:
                        fprintf(stderr,"Unknown message: 0x%02X\n",type);
                        break;
        }
}


void inputInterrupt(void){
static int old;
int duration;
        int buttonState = digitalRead(dataPin);
        if (buttonState != old) {
                duration= (micros() - dur);
                if((old==1) && duration<800){  //High state for short time means '1'
                        dataBuff[p++]=1;
                        fprintf(stderr,"1");
                }else if((old==1) && duration>=800){ //High state for long time means '0'
                        dataBuff[p++]=0;
                        fprintf(stderr,"0");
                }
                if(p>=MAX_DATA-1) p=0;
                old=buttonState;
                dur=micros();
        }
}

int main (void)
{
        FILE *fp;
        unsigned char byteArray[15];
        bool ldataBuff[MAX_DATA]= { 0 };; //received bits buffer
        int lp;
        int b=0,i=0,j=0;

        time_t rawtime;
        struct tm * timeinfo;

        if (wiringPiSetup () == -1)
                return 1 ;

        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        fprintf(stderr,"%02d/%02d/%04d %02d:%02d:%02d: Capture starts.",timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

        pinMode (dataPin, INPUT);
        wiringPiISR(dataPin, INT_EDGE_BOTH, inputInterrupt); //attach interrupt function to pin change

        for (;;)//infinite loop
        {
                sleep(1); //freeing cpu usage
                if((micros() - dur)>50000){
                        if(p>=80){ //received an acceptable amount of bits
                                lp=p;
                                memcpy(ldataBuff,dataBuff,sizeof(ldataBuff));
                                b=0;
                                time ( &rawtime );
                                timeinfo = localtime ( &rawtime );
                                fprintf(stderr,"\n %02d/%02d/%04d %02d:%02d:%02d",timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
                                fprintf(stderr," %d bits: ",lp);
                                for(i=(p-80);i<lp;i++){ //group bits into bytes
                                        fprintf(stderr,"%d",ldataBuff[i]);
                                        byteArray[b]=(byteArray[b]<<1) | (ldataBuff[i]==1);
                                        if(((i-lp-80)+1)%8==0) {
                                                b++;
                                                byteArray[b]=0;
                                                fprintf(stderr," ");
                                        }
                                }
                                fprintf(stderr,"\n%d bytes: ",b);
                                for(j=0;j<b;j++){
                                        fprintf(stderr,"0X%02X ",byteArray[j]);
                                }
                                fprintf(stderr,"\n");

                                if(crc8(&dataBuff[p-80],80)==0){ //CRC OK
                                        decode(byteArray);
                                }else{
                                        fprintf(stderr,"CRC Error\n");
                                }
                        p=0;
                        endOfMessage=false;
                        }
                }
        }
        return 0 ;
}
