#ifndef __AWS_LIVE_H__
#define __AWS_LIVE_H__

#include"live_stream.h"
#include "KinesisVideoProducer.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <iostream>
#include <thread>
#include <string.h>
#include <chrono>
#include <Logger.h>
#include <vector>
#include <stdlib.h>
#include <mutex>

#include <atomic>
#include <stdio.h>
#include <math.h>


using namespace std;
using namespace com::amazonaws::kinesis::video;
using std::unique_ptr;
using std::shared_ptr;
using std::move;


#define ACCESS_KEY_ENV_VAR "AWS_ACCESS_KEY_ID"
#define SECRET_KEY_ENV_VAR "AWS_SECRET_ACCESS_KEY"
#define SESSION_TOKEN_ENV_VAR "AWS_SESSION_TOKEN"
#define DEFAULT_REGION_ENV_VAR "AWS_DEFAULT_REGION"

#define DEFAULT_RETENTION_PERIOD_HOURS 2
#define DEFAULT_KMS_KEY_ID ""
#define DEFAULT_STREAMING_TYPE STREAMING_TYPE_REALTIME
#define DEFAULT_CONTENT_TYPE "video/h264"
#define DEFAULT_MAX_LATENCY_SECONDS 30
#define DEFAULT_FRAGMENT_DURATION_MILLISECONDS 10000
#define DEFAULT_TIMECODE_SCALE_MILLISECONDS 1
#define DEFAULT_KEY_FRAME_FRAGMENTATION TRUE
#define DEFAULT_FRAME_TIMECODES TRUE
#define DEFAULT_ABSOLUTE_FRAGMENT_TIMES FALSE
#define DEFAULT_FRAGMENT_ACKS TRUE
#define DEFAULT_RESTART_ON_ERROR TRUE
#define DEFAULT_RECALCULATE_METRICS TRUE
#define DEFAULT_STREAM_FRAMERATE 25
#define DEFAULT_AVG_BANDWIDTH_BPS (2* 1024 * 1024)
#define DEFAULT_BUFFER_DURATION_SECONDS 120
#define DEFAULT_REPLAY_DURATION_SECONDS 40
#define DEFAULT_CONNECTION_STALENESS_SECONDS 60
#define DEFAULT_CODEC_ID "V_MPEG4/ISO/AVC"
#define DEFAULT_TRACKNAME "kinesis_video"
#define DEFAULT_FRAME_DURATION_MS 1
#define DEFAULT_FRAME_DATA_SIZE_BYTE 5000
#define FRAME_DATA_SIZE_MULTIPLIER 1.5
#define DEFAULT_CREDENTIAL_ROTATION_SECONDS 2400
#define DEFAULT_CREDENTIAL_EXPIRATION_SECONDS 180


#define INFO_BUFF_SIZE	12000
#define STREAM_BUF_SIZE 0x100000


typedef enum _StreamSource {
    FILE_SOURCE,
    LIVE_SOURCE,
    RTSP_SOURCE
} StreamSource;

typedef struct _FileInfo {
    _FileInfo():
            path(""),
            last_fragment_ts(0) {}
    string path;
    uint64_t last_fragment_ts;
} FileInfo;

typedef struct _CustomData {
    _CustomData():
            streamSource(LIVE_SOURCE),
            synthetic_dts(0),
            stream_status(STATUS_SUCCESS),
            base_pts(0),
            max_frame_pts(0),
            key_frame_pts(0),
            frame_data_size(DEFAULT_FRAME_DATA_SIZE_BYTE)
         {
             producer_start_time = chrono::duration_cast<nanoseconds>(systemCurrentTime().time_since_epoch()).count();
         }

    unique_ptr<KinesisVideoProducer> kinesis_video_producer;
    shared_ptr<KinesisVideoStream> kinesis_video_stream;
    bool stream_started;
    char *stream_name;

    atomic_uint stream_status;

    uint64_t base_pts;
    uint64_t max_frame_pts;
    uint64_t key_frame_pts;
    uint64_t producer_start_time;

    volatile StreamSource streamSource;
    unique_ptr<Credentials> credential;

    uint64_t synthetic_dts;

    uint8_t *frame_data;
    uint32_t frame_data_size;
} CustomData;


class SampleStreamCallbackProvider : public StreamCallbackProvider \
{
        UINT64 custom_data_;
    public:
	    SampleStreamCallbackProvider(UINT64 custom_data) : custom_data_(custom_data) {}

	    UINT64 getCallbackCustomData() override
	    {
	        return custom_data_;
	    }

	    StreamConnectionStaleFunc getStreamConnectionStaleCallback() override 
	    {
	        return streamConnectionStaleHandler;
	    };

	    StreamErrorReportFunc getStreamErrorReportCallback() override 
		{
	        return streamErrorReportHandler;
	    };

	    DroppedFrameReportFunc getDroppedFrameReportCallback() override
		{
	        return droppedFrameReportHandler;
	    };

	    FragmentAckReceivedFunc getFragmentAckReceivedCallback() override 
		{
	        return fragmentAckReceivedHandler;
	    };

private:
	    static STATUS streamConnectionStaleHandler(UINT64 custom_data, STREAM_HANDLE stream_handle,  UINT64 last_buffering_ack){   return STATUS_SUCCESS;  }
	    static STATUS streamErrorReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle, UPLOAD_HANDLE upload_handle, UINT64 errored_timecode,  STATUS status_code){   return STATUS_SUCCESS;  }
	    static STATUS droppedFrameReportHandler(UINT64 custom_data, STREAM_HANDLE stream_handle, UINT64 dropped_frame_timecode) {    return STATUS_SUCCESS; }
	    static STATUS fragmentAckReceivedHandler( UINT64 custom_data, STREAM_HANDLE stream_handle,   UPLOAD_HANDLE upload_handle, PFragmentAck pFragmentAck) {   return STATUS_SUCCESS;}
};


class SampleClientCallbackProvider : public ClientCallbackProvider 
{
   public:

	    UINT64 getCallbackCustomData() override 
		{
	        return reinterpret_cast<UINT64> (this);
	    }

	    StorageOverflowPressureFunc getStorageOverflowPressureCallback() override 
	    {
	        return storageOverflowPressure;
	    }

	    static STATUS storageOverflowPressure(UINT64 custom_handle, UINT64 remaining_bytes){  return STATUS_SUCCESS;  }
};


class SampleCredentialProvider : public StaticCredentialProvider 
{
	    // Test rotation period is 40 second for the grace period.
	    const std::chrono::duration<uint64_t> ROTATION_PERIOD = std::chrono::seconds(DEFAULT_CREDENTIAL_ROTATION_SECONDS);
  public:
	    SampleCredentialProvider(const Credentials &credentials) : StaticCredentialProvider(credentials) {}

	    void updateCredentials(Credentials &credentials) override 
	    {
	        // Copy the stored creds forward
	        credentials = credentials_;

	        // Update only the expiration
	        auto now_time = std::chrono::duration_cast<std::chrono::seconds>( systemCurrentTime().time_since_epoch());
	        auto expiration_seconds = now_time + ROTATION_PERIOD;
	        credentials.setExpiration(std::chrono::seconds(expiration_seconds.count()));
	        printf("New credentials expiration is: \n",  credentials.getExpiration().count());
	    }
};

class SampleDeviceInfoProvider : public DefaultDeviceInfoProvider 
{
	public:
	    device_info_t getDeviceInfo() override 
		{
	        auto device_info = DefaultDeviceInfoProvider::getDeviceInfo();
	        // Set the storage size to 128mb
	        device_info.storageInfo.storageSize = 12 * 1024 * 1024;
	        return device_info;
        }
};



class aws_live:public live_stream
{
   public:
		aws_live();
		~aws_live();

		virtual int init_live_stream(); 

		virtual int create_cloud(void *args, int size); 
		virtual int destroy_cloud(); 

		virtual int start_cloud();  
		virtual int stop_cloud(); 

		virtual int put_video_stream(void *data,  unsigned int len,  const nanoseconds  pts,  const nanoseconds  dts, int flags); 
		virtual int put_audio_stream(); 
 
   	
   private:
	     CustomData M_CusData;  

	     int kinesis_video_stream_init(CustomData *CusData);
		 
	     int kinesis_video_init(CustomData *CusData);
		 
		 int Start_Kinesis_video_stream(CustomData *CusData);
		 int Stop_Kinesis_video_stream(CustomData *CusData);
		 
		 bool put_frame(shared_ptr<KinesisVideoStream> kinesis_video_stream, void *data, unsigned int	len, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags);
		 void create_kinesis_video_frame(Frame *frame, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags, void *m_data, size_t len); 
};

#endif




