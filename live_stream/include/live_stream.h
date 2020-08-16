#ifndef __LIVE_STREAM_H__
#define __LIVE_STREAM_H__

#include <iostream>
#include <chrono>


#include <list>
#include <sys/un.h>
#include <atomic>

using namespace std::chrono;



class live_stream
{
  public:
  	    
		virtual int init_live_stream() = 0;     
		
		virtual int create_cloud(void *args, int size) = 0; 
		virtual int destroy_cloud() = 0; 
		
		virtual int start_cloud() = 0;  
		virtual int stop_cloud() = 0; 
		
		virtual int put_video_stream(void *data,  unsigned int	len,  const nanoseconds pts,  const nanoseconds dts, int flags) = 0; 
		virtual int put_audio_stream() = 0;
  	
};

#endif

