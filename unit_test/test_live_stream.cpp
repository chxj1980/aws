
#include <iostream>
#include <chrono>

#include<time.h>
#include"live_stream.h"
#include"aws_live.h"


#define MAX_LEN 10000

live_stream *live_s = nullptr;

int SendAvcStream(FILE *fp, unsigned *arr, unsigned int len, CustomData *data)
{
 	int ret = 0;
	int type = 0;
	int flags = 0;
	int i = 1;
    int res = 0;
	bool delta ;
	
    unsigned int  frameSize = 0;
	uint64_t  synthetic_dts = 0;
	int data_lenth = 0;
	bool IsKey = FALSE;
	
    unsigned long long pts = 1598037687L;
    static unsigned char *bufI = NULL;
    static unsigned char *buf2 = NULL;
    unsigned char * buf1_end = NULL;
	
    if(!bufI)
    {   
        bufI = (unsigned char *)malloc(1920 * 1080* 8 *sizeof(unsigned char));
        if(!bufI) return -1;

		buf1_end = bufI + 1920 * 1080* 8 *sizeof(unsigned char);
    }
	
    if(!buf2)
    {   
        buf2 = (unsigned char *)malloc(1920 * 500 *10);
        if(!buf2) return -1;
    }
 
    while(1)
    {   
    	if(i > len)
		{
			printf("~~~~~~~~~~~~ !\n");
			fseek(fp, 0L, SEEK_SET);
			i = 1;
			data_lenth = 0;
			IsKey = FALSE;
		}
		
        unsigned int size = arr[i] - arr[i-1];     
        res = fread(buf2, 1, size, fp);
        if(res < 0 ) 
		{
			printf("++++++++++ !\n");
		    perror("fread faile: ");
		   continue;
		}

		if(buf2[0]==0x00 && buf2[1]==0x00 && buf2[2]==0x00 && buf2[3]==0x01)	
        { 
           type = buf2[4] & 0x1f;
		}
		else if(buf2[0]==0x00 && buf2[1]==0x00 && buf2[2]==0x01)
		{
           type = buf2[3] & 0x1f;
		}
		
       // printf("frame type is %d\n", type);
        if((type == 8) || (type == 7) || (type == 1) || (type == 5) )// 7 8 1 5
        {
		    if((type == 8) || (type == 7) )
		    {
			   memcpy(bufI + data_lenth, buf2, size);
               data_lenth += size;
			   IsKey = TRUE;
			   i++;
			   continue ;
		    }
			
		    if( (type == 1)|| (type == 5) )
		    {
                if( (bufI + data_lenth) > (buf1_end - 500))
                {
                    printf("[TWT]: ----->$$$$$$$$$$.\n");
                    data_lenth = buf1_end - bufI; 
				}

				if(size > 200*1000)
				{  
					 data_lenth = 0;
					 if(IsKey == TRUE )
					 	    printf("[TWT]: ----->data size error.\n");
					 
				     IsKey = FALSE;
					 i++;
			     	 continue;
				}

				if(type == 5)
				{
					//printf("[TWT]: !!!!!!!!!!!!!!!!!!\n");
					memcpy(bufI + data_lenth, buf2, size);
					data_lenth += size;
				}
			    else
			    {
					memcpy(bufI , buf2, size);
					data_lenth = size;
					//printf("[TWT]: ----------->>>\n");
				}
				
		        if(IsKey == TRUE )
		            flags  = 1;//key frame
				else
	                flags  = 0;// not key frame
				
				
				printf("[P:]%02x, %02x, %02x, %02x, %02x, %02x, %02x \n", bufI[0], bufI[1], bufI[2], bufI[3], bufI[4], bufI[5], bufI[6]);
				ret = live_s->put_video_stream(bufI , data_lenth , std::chrono::nanoseconds(pts),  std::chrono::nanoseconds(synthetic_dts), flags);
				if(ret != true)
				{
				    printf("put frame to AWS kinesis fail !\n");
				}
				
		  	    synthetic_dts += DEFAULT_FRAME_DURATION_MS * HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_TIME_UNIT_IN_NANOS * 10;
			    pts += 40000000; //ST_Sys_GetPts(25);

				data_lenth = 0;
				IsKey = FALSE;
			}
        }
 
        usleep(1000 * 45);
        i++;
			
    }
	
	printf("put frame to AWS kinesis fail !\n");
	printf("put frame to AWS kinesis fail !\n");

    return 0;
}

int getAllContent(FILE *fp, unsigned char *str, int strLen, unsigned int *arr, unsigned int *len)
{
    if(!fp || !arr || !len) return -1;

	char idr_arr[] ={0x00, 0x00 ,0x01};
	
    unsigned int arrLen = *len;
    long pos = 0;
    long posEnd = 0;
    char *buf = (char *)malloc(sizeof(char)*strLen);
    if(!buf) return -2;
 
    fseek(fp, 0L, SEEK_END);
    posEnd = ftell(fp) - strLen;
 
    *len = 0;
    int res = 0;
    while(pos <= posEnd && *len < arrLen)
    {
        fseek(fp, pos, SEEK_SET);
        res = fread(buf, sizeof(char), strLen, fp);
        if(res != strLen) break;
		
        if((memcmp(str, buf, strLen*sizeof(char)) == 0 ) )
        {
           printf("\n==+++=>%02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
            arr[*len] = pos;
            (*len)++;
			pos += 4;
        }
		else if(memcmp(idr_arr, buf, 3) == 0 )
		{
			printf("\n==***=>%02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
			 arr[*len] = pos;
			 (*len)++;
			 pos += 3;
		}
		else
		{
		     pos++;
		}
       
    }
 
    fseek(fp, 0L, SEEK_SET);
    free(buf);
    return 0;
}

unsigned long long  ST_Sys_GetPts(unsigned    int  u32FrameRate)
{
    if (0 == u32FrameRate)
    {
         return (unsigned long long)(-1);
     }
 
     return (unsigned long long)(1000 / u32FrameRate);
}


void* PutStream(void * DaTa)
{	
	char *path = "test2.h264";
	FILE *fd = fopen(path, "rb"); //ES
	if (!fd)
	{
		perror("Open file failed!\n");
		return NULL;
	}

	unsigned int remoteArr[MAX_LEN] = {0};
	unsigned int remoteArrLen = MAX_LEN;
	unsigned  char nal[4] = {0x00, 0x00, 0x00, 0x01};
	
	getAllContent(fd , nal,4, remoteArr, &remoteArrLen);
	printf("remoteArrLen: [%d] \n", remoteArrLen);
	SendAvcStream(fd, remoteArr, remoteArrLen, NULL);
  return NULL;
}

int Test_FileStreamProc( )
{

     pthread_t tid;
	 int ret = pthread_create(&tid, NULL,PutStream,  NULL);
	 if(ret != 0){
		 fprintf(stderr,"Fail to pthread_create : %s\n",strerror(ret));
		 return -1;
	 }

  return -1;
}

int main()
{
    int ret  = 0;
    char * ctream_name = "my-stream-name";
    live_s =  new aws_live();

	live_s->init_live_stream();
	
	ret = live_s->create_cloud(ctream_name,  strlen(ctream_name));
    if(ret < 0)
    {
        printf("create cloud  faile ! \n");
		return -1;
	}
	
	ret = live_s->start_cloud();
    if(ret < 0)
    {
        printf("start cloud  faile ! \n");
		live_s->destroy_cloud();
		return -1;
	}

	Test_FileStreamProc(); 

   /* check stream status*/
	while(1)
	{
   
	   usleep(1000 * 600);
	}
	
    live_s->stop_cloud();
    live_s->destroy_cloud();

	delete live_s;
  return 0;
}
