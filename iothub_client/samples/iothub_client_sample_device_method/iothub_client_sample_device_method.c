// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>

#include "iothub_client.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "iothubtransportmqtt.h"
#include "iothubtransportamqp.h"

#include <vld.h>

#ifdef MBED_BUILD_TIMESTAMP
#include "certs.h"
#endif // MBED_BUILD_TIMESTAMP

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
static const char* connectionString = "";

static char msgText[1024];
static char propText[1024];
static bool g_continueRunning;
#define MESSAGE_COUNT       5
#define DOWORK_LOOP_NUM     3

typedef struct EVENT_INSTANCE_TAG
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId;  // For tracking the messages within the user callback.
} EVENT_INSTANCE;

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    IOTHUB_MESSAGE_HANDLE messageHandle = (IOTHUB_MESSAGE_HANDLE)userContextCallback;

    (void)printf("Confirmation received for message %s\r\n", ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));

    IoTHubMessage_Destroy(messageHandle);
}

static int ll_incoming_method_callback(const char* method_name, const unsigned char* payload, size_t size, METHOD_ID method_id, void* userContextCallback)
{
    IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle = (IOTHUB_CLIENT_LL_HANDLE)userContextCallback;

    int result;

    printf("\r\nDevice Method called\r\n");
    printf("Device Method name:    %s\r\n", method_name);
    printf("Device Method payload: %.*s\r\n", (int)size, (const char*)payload);

    int status = 200;
    char* RESPONSE_STRING = "{ \"Response\": \"This is an actual async response from the device\" }";

    IOTHUB_MESSAGE_HANDLE messageHandle;
    if ((messageHandle = IoTHubMessage_CreateFromString("Test Async Message")) == NULL)
    {
        (void)printf("ERROR: iotHubMessageHandle is NULL!\r\n");
        RESPONSE_STRING = "{ \"Response\": \"Failed IoTHubMessage_CreateFromString\" }";
        status = 304;
    }
    else
    {
        if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, SendConfirmationCallback, messageHandle) != IOTHUB_CLIENT_OK)
        {
            RESPONSE_STRING = "{ \"Response\": \"Failed IoTHubClient_LL_SendEventAsync\" }";
            (void)printf("ERROR: IoTHubClient_LL_SendEventAsync Failed!\r\n");
            status = 304;
            IoTHubMessage_Destroy(messageHandle);
        }
    }

    size_t resp_length = strlen(RESPONSE_STRING);

    IoTHubClient_LL_DeviceMethodResponse(iotHubClientHandle, method_id, (unsigned char*)RESPONSE_STRING, resp_length, status);

    result = 0;

    g_continueRunning = false;
    return result;
}

static int incoming_method_callback(const char* method_name, const unsigned char* payload, size_t size, METHOD_ID method_id, void* userContextCallback)
{
    IOTHUB_CLIENT_HANDLE iotHubClientHandle = (IOTHUB_CLIENT_HANDLE)userContextCallback;

    int result;

    printf("\r\nDevice Method called\r\n");
    printf("Device Method name:    %s\r\n", method_name);
    printf("Device Method payload: %.*s\r\n", (int)size, (const char*)payload);

    int status = 200;
    char* RESPONSE_STRING = "{ \"Response\": \"This is an actual async response from the device\" }";

    IOTHUB_MESSAGE_HANDLE messageHandle;
    if ((messageHandle = IoTHubMessage_CreateFromString("Test Async Message")) == NULL)
    {
        (void)printf("ERROR: iotHubMessageHandle is NULL!\r\n");
        RESPONSE_STRING = "{ \"Response\": \"Failed IoTHubMessage_CreateFromString\" }";
        status = 304;
    }
    else
    {
        if (IoTHubClient_SendEventAsync(iotHubClientHandle, messageHandle, SendConfirmationCallback, messageHandle) != IOTHUB_CLIENT_OK)
        {
            RESPONSE_STRING = "{ \"Response\": \"Failed IoTHubClient_SendEventAsync\" }";
            (void)printf("ERROR: IoTHubClient_LL_SendEventAsync Failed!\r\n");
            status = 304;
        }
    }

    size_t resp_length = strlen(RESPONSE_STRING);
    IoTHubClient_DeviceMethodResponse(iotHubClientHandle, method_id, (unsigned char*)RESPONSE_STRING, resp_length, status);

    result = 0;

    g_continueRunning = false;
    return result;
}

static int DeviceMethodCallback(const char* method_name, const unsigned char* payload, size_t size, unsigned char** response, size_t* resp_size, void* userContextCallback)
{
    (void)userContextCallback;

    printf("\r\nDevice Method called\r\n");
    printf("Device Method name:    %s\r\n", method_name);
    printf("Device Method payload: %.*s\r\n", (int)size, (const char*)payload);

    int status = 200;
    char* RESPONSE_STRING = "{ \"Response\": \"This is the response from the device\" }";
    printf("\r\nResponse status: %d\r\n", status);
    printf("Response payload: %s\r\n\r\n", RESPONSE_STRING);

    *resp_size = strlen(RESPONSE_STRING);
    if ((*response = malloc(*resp_size)) == NULL)
    {
        status = -1;
    }
    else
    {
        memcpy(*response, RESPONSE_STRING, *resp_size);
    }
    g_continueRunning = false;
    return status;
}

void iothub_client_sample_device_method_sync(IOTHUB_CLIENT_TRANSPORT_PROVIDER transport_type)
{
    IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

    if ((iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, transport_type)) == NULL)
    {
        (void)printf("ERROR: iotHubClientHandle is NULL!\r\n");
    }
    else
    {
        bool traceOn = true;
        IoTHubClient_LL_SetOption(iotHubClientHandle, "logtrace", &traceOn);

#ifdef MBED_BUILD_TIMESTAMP
        // For mbed add the certificate information
        if (IoTHubClient_LL_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
        {
            printf("failure to set option \"TrustedCerts\"\r\n");
        }
#endif // MBED_BUILD_TIMESTAMP

        //if (IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, DeviceMethodCallback, (void*)iotHubClientHandle) != IOTHUB_CLIENT_OK)
        if (IoTHubClient_LL_SetIncomingDeviceMethodCallback(iotHubClientHandle, ll_incoming_method_callback, (void*)iotHubClientHandle) != IOTHUB_CLIENT_OK)
        {
            (void)printf("ERROR: IoTHubClient_LL_SetDeviceMethodCallback..........FAILED!\r\n");
        }
        else
        {
            (void)printf("IoTHubClient_LL_SetDeviceMethodCallback...successful.\r\n");

            do
            {
                IoTHubClient_LL_DoWork(iotHubClientHandle);
                ThreadAPI_Sleep(1);
            } while (g_continueRunning);

            (void)printf("iothub_client_sample_device_method exited, call DoWork %d more time to complete final sending...\r\n", DOWORK_LOOP_NUM);
            for (size_t index = 0; index < DOWORK_LOOP_NUM; index++)
            {
                IoTHubClient_LL_DoWork(iotHubClientHandle);
                ThreadAPI_Sleep(1);
            }
        }

        printf("Press Any Key to continue\r\n");
        (void)getchar();
        IoTHubClient_LL_Destroy(iotHubClientHandle);
    }
}

void iothub_client_sample_device_method_threaded(IOTHUB_CLIENT_TRANSPORT_PROVIDER transport_type)
{
    IOTHUB_CLIENT_HANDLE iotHubClientHandle;

    if ((iotHubClientHandle = IoTHubClient_CreateFromConnectionString(connectionString, transport_type)) == NULL)
    {
        (void)printf("ERROR: iotHubClientHandle is NULL!\r\n");
    }
    else
    {
        bool traceOn = true;
        IoTHubClient_SetOption(iotHubClientHandle, "logtrace", &traceOn);

#ifdef MBED_BUILD_TIMESTAMP
        // For mbed add the certificate information
        if (IoTHubClient_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
        {
            printf("failure to set option \"TrustedCerts\"\r\n");
        }
#endif // MBED_BUILD_TIMESTAMP

        if (IoTHubClient_SetIncomingDeviceMethodCallback(iotHubClientHandle, incoming_method_callback, (void*)iotHubClientHandle) != IOTHUB_CLIENT_OK)
        {
            (void)printf("ERROR: IoTHubClient_SetDeviceMethodCallback..........FAILED!\r\n");
        }
        else
        {
            (void)printf("IoTHubClient_SetDeviceMethodCallback...successful.\r\n");

            do
            {
                ThreadAPI_Sleep(1);
            } while (g_continueRunning);
        }

        printf("Press Any Key to continue");
        (void)getchar();

        IoTHubClient_Destroy(iotHubClientHandle);
    }
}

int main(void)
{
    if (platform_init() != 0)
    {
        (void)printf("Failed to initialize the platform.\r\n");
    }
    else
    {
        g_continueRunning = true;

        //IOTHUB_CLIENT_TRANSPORT_PROVIDER transport_type = MQTT_Protocol;
        IOTHUB_CLIENT_TRANSPORT_PROVIDER transport_type = AMQP_Protocol;

        iothub_client_sample_device_method_threaded(transport_type);
        //iothub_client_sample_device_method(transport_type);
        platform_deinit();
    }
    return 0;
}
