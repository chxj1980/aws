#include"aws_live.h"

LOGGER_TAG("com.amazonaws.kinesis.video.gstreamer");

aws_live::aws_live()
{

}

aws_live::~aws_live()
{

}

int aws_live::init_live_stream()
{
	char *key_id_env = "xxx"; 	   
	char *secret_key_env = "fff";

	setenv(key_id_env, "ddddddd", 1);
	setenv(secret_key_env, "dddddd", 1);	
	
	log4cplus::PropertyConfigurator::doConfigure("ddddd");
	
  return 0;
}

int aws_live::create_cloud(void *args, int size)
{
    if(args == NULL)
    {
         printf("the init_live_stream fun args  is null !\n");
       return -1;
	}

	int ret = 0;
	char stream_name[MAX_STREAM_NAME_LEN + 1];
	char *tmp_name = (char*)args;	
	
   	STRNCPY(stream_name, tmp_name, size);
	stream_name[MAX_STREAM_NAME_LEN] = '\0';
	
	M_CusData.stream_name = stream_name;	
	
    /* init and cteate Kinesis Video  producer*/
    ret =kinesis_video_init(&M_CusData);
	if(ret < 0)
    {
        printf("creat kinesis_video_stream  produce  fail !\n");
	  return -1;
    }
	 
	/* init and cteate Kinesis Video  stream*/
    ret = kinesis_video_stream_init(&M_CusData);
    if(ret < 0)
    {
        printf("creat kinesis_video_stream fail !\n");
	  return -1;
    }

    return 0;
}

int aws_live::destroy_cloud()
{
    M_CusData.kinesis_video_stream->stopSync();
    M_CusData.kinesis_video_producer->freeStream(M_CusData.kinesis_video_stream);
}

int aws_live::start_cloud()
{
	return Start_Kinesis_video_stream(&M_CusData);
}

int aws_live::stop_cloud()
{
    return Stop_Kinesis_video_stream(&M_CusData);
}

int aws_live::put_audio_stream()
{

}

int aws_live::put_video_stream(void *data,  unsigned int len,  const nanoseconds pts,  const nanoseconds dts, int flags )
{
    if(len < 0 || len > 1024 * 1024 *600)
    {
         printf("error: video stream lenth < 0  or  > 600k !\n");
        return -1;
	}
	
	if ( put_frame( M_CusData.kinesis_video_stream, data, len, pts,   dts,  (FRAME_FLAGS)flags) < 0)
	{
	    printf("put video stream  failed !\n");
      return -1;
	} 
	
   return 0;
}

int aws_live::kinesis_video_stream_init(CustomData *CusData) 
{
    /* create a test stream */
    map<string, string> tags;
    char tag_name[MAX_TAG_NAME_LEN];
    char tag_val[MAX_TAG_VALUE_LEN];
    SPRINTF(tag_name, "piTag");
    SPRINTF(tag_val, "piValue");

    STREAMING_TYPE streaming_type = DEFAULT_STREAMING_TYPE;
    bool use_absolute_fragment_times = DEFAULT_ABSOLUTE_FRAGMENT_TIMES;

    if (CusData->streamSource == FILE_SOURCE) 
	{
        streaming_type = STREAMING_TYPE_OFFLINE;
        use_absolute_fragment_times = true;
    }

    auto stream_definition = make_unique<StreamDefinition>(CusData->stream_name,
                                                           hours(DEFAULT_RETENTION_PERIOD_HOURS),
                                                           &tags,
                                                           DEFAULT_KMS_KEY_ID,
                                                           streaming_type,
                                                           DEFAULT_CONTENT_TYPE,
                                                           duration_cast<milliseconds> (seconds(DEFAULT_MAX_LATENCY_SECONDS)),
                                                           milliseconds(DEFAULT_FRAGMENT_DURATION_MILLISECONDS),
                                                           milliseconds(DEFAULT_TIMECODE_SCALE_MILLISECONDS),
                                                           DEFAULT_KEY_FRAME_FRAGMENTATION,
                                                           DEFAULT_FRAME_TIMECODES,
                                                           use_absolute_fragment_times,
                                                           DEFAULT_FRAGMENT_ACKS,
                                                           DEFAULT_RESTART_ON_ERROR,
                                                           DEFAULT_RECALCULATE_METRICS,
                                                            NAL_ADAPTATION_ANNEXB_NALS | NAL_ADAPTATION_ANNEXB_CPD_NALS,   /*TNT*/
                                                           DEFAULT_STREAM_FRAMERATE,
                                                           DEFAULT_AVG_BANDWIDTH_BPS,
                                                           seconds(DEFAULT_BUFFER_DURATION_SECONDS),
                                                           seconds(DEFAULT_REPLAY_DURATION_SECONDS),
                                                           seconds(DEFAULT_CONNECTION_STALENESS_SECONDS),
                                                           DEFAULT_CODEC_ID,
                                                           DEFAULT_TRACKNAME,
                                                           nullptr,
                                                           0);
	
     CusData->kinesis_video_stream = CusData->kinesis_video_producer->createStreamSync(move(stream_definition));

    if(CusData->kinesis_video_stream == nullptr )
    {
           LOG_DEBUG("Stream is not  ready");
       return -1;
	}
	
    // reset state
    CusData->stream_status = STATUS_SUCCESS;
    CusData->stream_started = false;

    // since we are starting new putMedia, timestamp need not be padded.
    if (CusData->streamSource == FILE_SOURCE) {
        CusData->base_pts = 0;
        CusData->max_frame_pts = 0;
    }

    LOG_DEBUG("Stream is ready");
	return 0;
}

int aws_live::kinesis_video_init(CustomData *CusData)
{
    unique_ptr<DeviceInfoProvider> device_info_provider = make_unique<SampleDeviceInfoProvider>();
    unique_ptr<ClientCallbackProvider> client_callback_provider = make_unique<SampleClientCallbackProvider>();
    unique_ptr<StreamCallbackProvider> stream_callback_provider = make_unique<SampleStreamCallbackProvider>(reinterpret_cast<UINT64>(CusData));

    char const *accessKey;
    char const *secretKey;
    char const *sessionToken;
    char const *defaultRegion;
    string defaultRegionStr;
    string sessionTokenStr;

    char const *iot_get_credential_endpoint;
    char const *cert_path;
    char const *private_key_path;
    char const *role_alias;
    char const *ca_cert_path;

    unique_ptr<CredentialProvider> credential_provider;

    if (nullptr == (defaultRegion = getenv(DEFAULT_REGION_ENV_VAR))) {
        defaultRegionStr = DEFAULT_AWS_REGION;
    } else {
        defaultRegionStr = string(defaultRegion);
    }
	
    cout<<"Using region:" << defaultRegionStr<<endl;

    if (nullptr != (accessKey = getenv(ACCESS_KEY_ENV_VAR)) &&
        nullptr != (secretKey = getenv(SECRET_KEY_ENV_VAR))) 
    {

        cout<<"Using aws credentials for Kinesis Video Streams"<<endl;
        if (nullptr != (sessionToken = getenv(SESSION_TOKEN_ENV_VAR))) 
		{
            cout<<"Session token detected."<<endl;;
            sessionTokenStr = string(sessionToken);
        } else {
            cout<<"No session token was detected."<<endl;
            sessionTokenStr = "";
        }

        CusData->credential = make_unique<Credentials>(string(accessKey),
                                                    string(secretKey),
                                                    sessionTokenStr,
                                                    std::chrono::seconds(DEFAULT_CREDENTIAL_EXPIRATION_SECONDS));
        credential_provider = make_unique<SampleCredentialProvider>(*CusData->credential.get());

    }
	else {
        LOG_AND_THROW("No valid credential method was found");
		return -1;
    }

    CusData->kinesis_video_producer = KinesisVideoProducer::createSync(move(device_info_provider),
                                                                    move(client_callback_provider),
                                                                    move(stream_callback_provider),
                                                                    move(credential_provider),
                                                                    defaultRegionStr);
	
    LOG_DEBUG("Client is ready");
	return 0;
}

int aws_live::Start_Kinesis_video_stream(CustomData *CusData)
{
	BYTE cpd2[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x34,
	0xAC, 0x2B, 0x40, 0x1E, 0x00, 0x78, 0xD8, 0x08,
	0x80, 0x00, 0x01, 0xF4, 0x00, 0x00, 0xEA, 0x60,
	0x47, 0xA5, 0x50, 0x00, 0x00, 0x00, 0x01, 0x68,
	0xEE, 0x3C, 0xB0};
	UINT32 cpdSize = SIZEOF(cpd2);
	
	bool ret = CusData->kinesis_video_stream->start(cpd2, cpdSize, DEFAULT_TRACK_ID);
	if(ret == false)
	{
        printf("Kinesis_video_stream start fail !\n");
		return -1;
	}

	return 0;
}

int aws_live::Stop_Kinesis_video_stream(CustomData *CusData)
{
   if (false == CusData->kinesis_video_stream->stop())
   {
        printf("Kinesis_video_stream stop  fail !\n");
       return -1;
   }
   
   return 0;
}

void aws_live::create_kinesis_video_frame(Frame *frame, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags, void *m_data, size_t len) 
{
    frame->flags = flags;
    frame->decodingTs = static_cast<UINT64>(dts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
    frame->presentationTs = static_cast<UINT64>(pts.count()) / DEFAULT_TIME_UNIT_IN_NANOS;
    frame->duration = DEFAULT_FRAME_DURATION_MS * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    frame->size = static_cast<UINT32>(len);
    frame->frameData = reinterpret_cast<PBYTE>(m_data);
    frame->trackId = DEFAULT_TRACK_ID;

	char * data = (char *)m_data;
	/*printf("\n===>%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n=-->%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",  \
	       data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10], \
	       data[11], data[12], data[13], data[14], data[15], data[16], data[17], data[18], data[19], data[20], data[21],\
	       data[22], data[23], data[24], data[25], data[26], data[27], data[28], data[29], data[30], data[31], data[32]);*/
	 printf("\n=====[len:%d, pts:%d, dts:%d, flags:%d, duration:%d, trackId:%d ]==\n", len, pts, dts, flags,  frame->duration, frame->trackId);
}

bool aws_live::put_frame(shared_ptr<KinesisVideoStream> kinesis_video_stream, void *data, unsigned int  len, const nanoseconds &pts, const nanoseconds &dts, FRAME_FLAGS flags) 
{
    Frame frame;
    create_kinesis_video_frame(&frame, pts, dts, flags, data, len);
	
    return kinesis_video_stream->putFrame(frame);
}


