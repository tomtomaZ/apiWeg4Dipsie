/////////////////////////////////
//
//	TITLE:		WegIoT2AzureSolution.c
//
//	VERSION:	1.0
//
//	DATE:		13-Ago-2019
//
//	AUTHOR:		Nicolas Goulart
//				nicolas.goulart@dipsie.com.br
//
//	COMMENTS:	
// 	This code works with the X-Machine Remote Monitoring and Control Solution for:
// 	- Report device capabilities.
// 	- Send telemetry.
// 	- Respond to methods, including a long-running firmware update method.
//
//               This file best viewed with tabs set to four.
//__________________________________________________________________
//  Copyright and Proprietary Information
//
//   NOTE: CONTAINS DIPSIE Solutions PROPRIETARY INFORMATION.  DO NOT COPY, STORE IN A
//   RETRIEVAL SYSTEM, TRANSMIT OR DISCLOSE TO ANY THIRD PARTY WITHOUT THE PRIOR
//   WRITTEN PERMISSION FROM AN OFFICER OF DIPSIE Solutions S/A
//__________________________________________________________________
//  Copyright ©2015-2019 Dipsie SOLUTIONS. All rights are reserved.
//
////////////////////////////////
//
// REVISION HISTORY:
//
//
//
/////////////////////////////////

#include <stdio.h>
#include <stdlib.h>

#include "iothub.h"
#include "iothub_device_client.h"
#include "iothub_client_options.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothubtransportmqtt.h"
#include "parson.h"

#define MESSAGERESPONSE(code, message) const char deviceMethodResponse[] = message; \
	*response_size = sizeof(deviceMethodResponse) - 1;                              \
	*response = malloc(*response_size);                                             \
	(void)memcpy(*response, deviceMethodResponse, *response_size);                  \
	result = code;                                                                  \

#define FIRMWARE_UPDATE_STATUS_VALUES \
    DOWNLOADING,                      \
    APPLYING,                         \
    REBOOTING,                        \
    IDLE                              \

/*Enumeration specifying firmware update status */
MU_DEFINE_ENUM(FIRMWARE_UPDATE_STATUS, FIRMWARE_UPDATE_STATUS_VALUES);
MU_DEFINE_ENUM_STRINGS(FIRMWARE_UPDATE_STATUS, FIRMWARE_UPDATE_STATUS_VALUES);

/* Paste in your device connection string  */
static const char* connectionString = "<connectionstring>";
static char msgText[1024];
static size_t g_message_count_send_confirmations = 0;
static const char* initialFirmwareVersion = "1.0.0";

IOTHUB_DEVICE_CLIENT_HANDLE device_handle;

// <datadefinition>
typedef struct MESSAGESCHEMA_TAG
{
	char* name;
	char* format;
	char* fields;
} MessageSchema;

typedef struct TELEMETRYSCHEMA_TAG
{
	MessageSchema messageSchema;
} TelemetrySchema;

typedef struct TELEMETRYPROPERTIES_TAG
{
	TelemetrySchema telemetryTag1Schema;
	TelemetrySchema telemetryTag2Schema;
	TelemetrySchema telemetryTag3Schema;
	TelemetrySchema telemetryTag4Schema;
	TelemetrySchema telemetryTag5Schema;
} TelemetryProperties;

typedef struct DEVICE_TAG
{
	// Reported properties
	char* protocol;
	char* supportedMethods;
	char* type;
	char* firmware;
	FIRMWARE_UPDATE_STATUS firmwareUpdateStatus;
	char* location;
	double latitude;
	double longitude;
	TelemetryProperties telemetry;

	// Manage firmware update process
	char* new_firmware_version;
	char* new_firmware_URI;
} Device;
// </datadefinition>







// <main>
int main(void)
{
	srand((unsigned int)time(NULL));
	double minTelemetryTag1 = 50.0;
	double minTelemetryTag2 = 55.0;
	double minTelemetryTag3 = 30.0;
	double telemetryTag1 = 0;
	double telemetryTag2 = 0;
	double telemetryTag3 = 0;
	double telemetryTag4 = 0;
	double telemetryTag5 = 0;

	(void)printf("This sample simulates a Device device connected to the X-MachineRMC solution\r\n\r\n");

	// Used to initialize sdk subsystem
	(void)IoTHub_Init();

	(void)printf("Creating IoTHub handle\r\n");
	// Create the iothub handle here
	device_handle = IoTHubDeviceClient_CreateFromConnectionString(connectionString, MQTT_Protocol);
	if (device_handle == NULL)
	{
		(void)printf("Failure creating Iothub device.  Hint: Check you connection string.\r\n");
	}
	else
	{
		// Setting connection status callback to get indication of connection to iothub
		(void)IoTHubDeviceClient_SetConnectionStatusCallback(device_handle, connection_status_callback, NULL);

		Device device;
		memset(&device, 0, sizeof(Device));
		device.protocol = "MQTT";
		device.supportedMethods = "Reboot,FirmwareUpdate,Method#1,Method#2";
		device.type = "<DeviceType>";
		size_t size = strlen(initialFirmwareVersion) + 1;
		device.firmware = malloc(size);
		memcpy(device.firmware, initialFirmwareVersion, size);
		device.firmwareUpdateStatus = IDLE;
		device.location = "<DeviceLocation>";
		device.latitude = 47.638928;
		device.longitude = -122.13476;
		//TelemetryTag1
		device.telemetry.telemetryTag1Schema.messageSchema.name = "device-telemetryTag1;v1";
		device.telemetry.telemetryTag1Schema.messageSchema.format = "JSON";
		device.telemetry.telemetryTag1Schema.messageSchema.fields = "{\"telemetryTag1\":\"Double\",\"telemetryTag1_unit\":\"Text\"}";
		//TelemetryTag2
		device.telemetry.telemetryTag2Schema.messageSchema.name = "device-telemetryTag2;v1";
		device.telemetry.telemetryTag2Schema.messageSchema.format = "JSON";
		device.telemetry.telemetryTag2Schema.messageSchema.fields = "{\"telemetryTag2\":\"Double\",\"telemetryTag2_unit\":\"Text\"}";
		//TelemetryTag3
		device.telemetry.telemetryTag3Schema.messageSchema.name = "device-telemetryTag3;v1";
		device.telemetry.telemetryTag3Schema.messageSchema.format = "JSON";
		device.telemetry.telemetryTag3Schema.messageSchema.fields = "{\"telemetryTag3\":\"Double\",\"telemetryTag3_unit\":\"Text\"}";
		//TelemetryTag4
		device.telemetry.telemetryTag4Schema.messageSchema.name = "device-telemetryTag4;v1";
		device.telemetry.telemetryTag4Schema.messageSchema.format = "JSON";
		device.telemetry.telemetryTag4Schema.messageSchema.fields = "{\"telemetryTag4\":\"Double\",\"telemetryTag4_unit\":\"Text\"}";
		//TelemetryTag5
		device.telemetry.telemetryTag5Schema.messageSchema.name = "device-telemetryTag5;v1";
		device.telemetry.telemetryTag5Schema.messageSchema.format = "JSON";
		device.telemetry.telemetryTag5Schema.messageSchema.fields = "{\"telemetryTag5\":\"Double\",\"telemetryTag5_unit\":\"Text\"}";



		sendDeviceReportedProperties(&device);

		(void)IoTHubDeviceClient_SetDeviceMethodCallback(device_handle, device_method_callback, &device);

		while (1)
		{
			telemetryTag1 = minTelemetryTag1 + ((rand() % 10) + 5);
			telemetryTag3 = minTelemetryTag2 + ((rand() % 10) + 5);
			telemetryTag2 = minTelemetryTag3 + ((rand() % 20) + 5);

			if (device.firmwareUpdateStatus == IDLE)
			{
				//TelemetryTag1
				(void)printf("Sending sensor value TelemetryTag1 = %f %s,\r\n", telemetryTag1, "<TeleTag1Unit>");
				(void)sprintf_s(msgText, sizeof(msgText), "{\"telemetryTag1\":%.2f,\"telemetryTag1_unit\":\"<TeleTag1Unit>\"}", telemetryTag1);
				send_message(device_handle, msgText, device.telemetry.telemetryTag1Schema.messageSchema.name);

				//TelemetryTag2
				(void)printf("Sending sensor value TelemetryTag2 = %f %s,\r\n", telemetryTag2, "<TeleTag2Unit>");
				(void)sprintf_s(msgText, sizeof(msgText), "{\"telemetryTag2\":%.2f,\"telemetryTag2_unit\":\"<TeleTag2Unit>\"}", telemetryTag2);
				send_message(device_handle, msgText, device.telemetry.telemetryTag2Schema.messageSchema.name);

				//TelemetryTag3
				(void)printf("Sending sensor value TelemetryTag3 = %f %s,\r\n", telemetryTag3, "<TeleTag3Unit>");
				(void)sprintf_s(msgText, sizeof(msgText), "{\"telemetryTag3\":%.2f,\"telemetryTag3_unit\":\"<TeleTag3Unit>\"}", telemetryTag3);
				send_message(device_handle, msgText, device.telemetry.telemetryTag3Schema.messageSchema.name);

				//TelemetryTag4
				(void)printf("Sending sensor value TelemetryTag4 = %f %s,\r\n", telemetryTag4, "<TeleTag4Unit>");
				(void)sprintf_s(msgText, sizeof(msgText), "{\"telemetryTag4\":%.2f,\"telemetryTag4_unit\":\"<TeleTag4Unit>\"}", telemetryTag4);
				send_message(device_handle, msgText, device.telemetry.telemetryTag4Schema.messageSchema.name);

				//TelemetryTag5
				(void)printf("Sending sensor value TelemetryTag3 = %f %s,\r\n", telemetryTag5, "<TeleTag5Unit>");
				(void)sprintf_s(msgText, sizeof(msgText), "{\"telemetryTag5\":%.2f,\"telemetryTag5_unit\":\"<TeleTag5Unit>\"}", telemetryTag5);
				send_message(device_handle, msgText, device.telemetry.telemetryTag5Schema.messageSchema.name);
			}

			ThreadAPI_Sleep(5000);
		}

		(void)printf("\r\nShutting down\r\n");

		// Clean up the iothub sdk handle and free resources
		IoTHubDeviceClient_Destroy(device_handle);
		free(device.firmware);
		free(device.new_firmware_URI);
		free(device.new_firmware_version);
	}
	// Shutdown the sdk subsystem
	IoTHub_Deinit();

	return 0;
}
// </main>


// <FUNCTIONS>
/*  Converts the Device object into a JSON blob with reported properties ready to be sent across the wire as a twin. */
static char* serializeToJson(Device* device)
{
	char* result;

	JSON_Value* root_value = json_value_init_object();
	JSON_Object* root_object = json_value_get_object(root_value);

	// Only reported properties:
	(void)json_object_set_string(root_object, "Protocol", device->protocol);
	(void)json_object_set_string(root_object, "SupportedMethods", device->supportedMethods);
	(void)json_object_set_string(root_object, "Type", device->type);
	(void)json_object_set_string(root_object, "Firmware", device->firmware);
	(void)json_object_set_string(root_object, "FirmwareUpdateStatus", MU_ENUM_TO_STRING(FIRMWARE_UPDATE_STATUS, device->firmwareUpdateStatus));
	(void)json_object_set_string(root_object, "Location", device->location);
	(void)json_object_set_number(root_object, "Latitude", device->latitude);
	(void)json_object_set_number(root_object, "Longitude", device->longitude);

	//TelemetryTag1
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag1Schema.MessageSchema.Name", device->telemetry.telemetryTag1Schema.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag1Schema.MessageSchema.Format", device->telemetry.telemetryTag1Schema.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag1Schema.MessageSchema.Fields", device->telemetry.telemetryTag1Schema.messageSchema.fields);

	//TelemetryTag2
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag2Schema.MessageSchema.Name", device->telemetry.telemetryTag2Schema.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag2Schema.MessageSchema.Format", device->telemetry.telemetryTag2Schema.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag2Schema.MessageSchema.Fields", device->telemetry.telemetryTag2Schema.messageSchema.fields);

	//TelemetryTag3
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Name", device->telemetry.telemetryTag3Schema.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Format", device->telemetry.telemetryTag3Schema.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Fields", device->telemetry.telemetryTag3Schema.messageSchema.fields);

	//TelemetryTag4
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Name", device->telemetry.telemetryTag4Schema.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Format", device->telemetry.telemetryTag4Schema.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Fields", device->telemetry.telemetryTag4Schema.messageSchema.fields);

	//TelemetryTag5
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Name", device->telemetry.telemetryTag5Schema.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Format", device->telemetry.telemetryTag5Schema.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.TelemetryTag3Schema.MessageSchema.Fields", device->telemetry.telemetryTag5Schema.messageSchema.fields);

	result = json_serialize_to_string(root_value);

	json_value_free(root_value);

	return result;
}

static void connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
	(void)reason;
	(void)user_context;
	// This sample DOES NOT take into consideration network outages.
	if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
	{
		(void)printf("The device client is connected to iothub\r\n");
	}
	else
	{
		(void)printf("The device client has been disconnected\r\n");
	}
}

static void send_confirm_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
	(void)userContextCallback;
	g_message_count_send_confirmations++;
	(void)printf("Confirmation callback received for message %zu with result %s\r\n", g_message_count_send_confirmations, MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}

static void reported_state_callback(int status_code, void* userContextCallback)
{
	(void)userContextCallback;
	(void)printf("Device Twin reported properties update completed with result: %d\r\n", status_code);
}

static void sendDeviceReportedProperties(Device* device)
{
	if (device_handle != NULL)
	{
		char* reportedProperties = serializeToJson(device);
		(void)IoTHubDeviceClient_SendReportedState(device_handle, (const unsigned char*)reportedProperties, strlen(reportedProperties), reported_state_callback, NULL);
		free(reportedProperties);
	}
}

// <firmwareupdate>
/*
 This is a thread allocated to process a long-running device method call.
 It uses device twin reported properties to communicate status values
 to the Remote Monitoring solution accelerator.
*/
static int do_firmware_update(void *param)
{
	Device *device = (Device *)param;
	printf("Running simulated firmware update: URI: %s, Version: %s\r\n", device->new_firmware_URI, device->new_firmware_version);

	printf("Simulating download phase...\r\n");
	device->firmwareUpdateStatus = DOWNLOADING;
	sendDeviceReportedProperties(device);

	ThreadAPI_Sleep(5000);

	printf("Simulating apply phase...\r\n");
	device->firmwareUpdateStatus = APPLYING;
	sendDeviceReportedProperties(device);

	ThreadAPI_Sleep(5000);

	printf("Simulating reboot phase...\r\n");
	device->firmwareUpdateStatus = REBOOTING;
	sendDeviceReportedProperties(device);

	ThreadAPI_Sleep(5000);

	size_t size = strlen(device->new_firmware_version) + 1;
	(void)memcpy(device->firmware, device->new_firmware_version, size);

	device->firmwareUpdateStatus = IDLE;
	sendDeviceReportedProperties(device);

	return 0;
}
// </firmwareupdate>

void getFirmwareUpdateValues(Device* device, const unsigned char* payload)
{
	free(device->new_firmware_version);
	free(device->new_firmware_URI);
	device->new_firmware_URI = NULL;
	device->new_firmware_version = NULL;

	JSON_Value* root_value = json_parse_string((char *)payload);
	JSON_Object* root_object = json_value_get_object(root_value);

	JSON_Value* newFirmwareVersion = json_object_get_value(root_object, "Firmware");

	if (newFirmwareVersion != NULL)
	{
		const char* data = json_value_get_string(newFirmwareVersion);
		if (data != NULL)
		{
			size_t size = strlen(data) + 1;
			device->new_firmware_version = malloc(size);
			(void)memcpy(device->new_firmware_version, data, size);
		}
	}

	JSON_Value* newFirmwareURI = json_object_get_value(root_object, "FirmwareUri");

	if (newFirmwareURI != NULL)
	{
		const char* data = json_value_get_string(newFirmwareURI);
		if (data != NULL)
		{
			size_t size = strlen(data) + 1;
			device->new_firmware_URI = malloc(size);
			(void)memcpy(device->new_firmware_URI, data, size);
		}
	}

	// Free resources
	json_value_free(root_value);

}

// <devicemethodcallback>
static int device_method_callback(const char* method_name, const unsigned char* payload, size_t size, unsigned char** response, size_t* response_size, void* userContextCallback)
{
	Device *device = (Device *)userContextCallback;

	int result;

	(void)printf("Direct method name:    %s\r\n", method_name);

	(void)printf("Direct method payload: %.*s\r\n", (int)size, (const char*)payload);

	if (strcmp("Reboot", method_name) == 0)
	{
		MESSAGERESPONSE(201, "{ \"Response\": \"Rebooting\" }")
	}
	else if (strcmp("EmergencyValveRelease", method_name) == 0)
	{
		MESSAGERESPONSE(201, "{ \"Response\": \"Method#1\" }")
	}
	else if (strcmp("IncreaseTelemetryTag2", method_name) == 0)
	{
		MESSAGERESPONSE(201, "{ \"Response\": \"Method#2\" }")
	}
	else if (strcmp("FirmwareUpdate", method_name) == 0)
	{
		if (device->firmwareUpdateStatus != IDLE)
		{
			(void)printf("Attempt to invoke firmware update out of order\r\n");
			MESSAGERESPONSE(400, "{ \"Response\": \"Attempting to initiate a firmware update out of order\" }")
		}
		else
		{
			getFirmwareUpdateValues(device, payload);

			if (device->new_firmware_version != NULL && device->new_firmware_URI != NULL)
			{
				// Create a thread for the long-running firmware update process.
				THREAD_HANDLE thread_apply;
				THREADAPI_RESULT t_result = ThreadAPI_Create(&thread_apply, do_firmware_update, device);
				if (t_result == THREADAPI_OK)
				{
					(void)printf("Starting firmware update thread\r\n");
					MESSAGERESPONSE(201, "{ \"Response\": \"Starting firmware update thread\" }")
				}
				else
				{
					(void)printf("Failed to start firmware update thread\r\n");
					MESSAGERESPONSE(500, "{ \"Response\": \"Failed to start firmware update thread\" }")
				}
			}
			else
			{
				(void)printf("Invalid method payload\r\n");
				MESSAGERESPONSE(400, "{ \"Response\": \"Invalid payload\" }")
			}
		}
	}
	else
	{
		// All other entries are ignored.
		(void)printf("Method not recognized\r\n");
		MESSAGERESPONSE(400, "{ \"Response\": \"Method not recognized\" }")
	}

	return result;
}
// </devicemethodcallback>

// <sendmessage>
static void send_message(IOTHUB_DEVICE_CLIENT_HANDLE handle, char* message, char* schema)
{
	IOTHUB_MESSAGE_HANDLE message_handle = IoTHubMessage_CreateFromString(message);

	// Set system properties
	(void)IoTHubMessage_SetMessageId(message_handle, "MSG_ID");
	(void)IoTHubMessage_SetCorrelationId(message_handle, "CORE_ID");
	(void)IoTHubMessage_SetContentTypeSystemProperty(message_handle, "application%2fjson");
	(void)IoTHubMessage_SetContentEncodingSystemProperty(message_handle, "utf-8");

	// Set application properties
	MAP_HANDLE propMap = IoTHubMessage_Properties(message_handle);
	(void)Map_AddOrUpdate(propMap, "$$MessageSchema", schema);
	(void)Map_AddOrUpdate(propMap, "$$ContentType", "JSON");

	time_t now = time(0);
	struct tm* timeinfo;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996) /* Suppress warning about possible unsafe function in Visual Studio */
#endif
	timeinfo = gmtime(&now);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
	char timebuff[50];
	strftime(timebuff, 50, "%Y-%m-%dT%H:%M:%SZ", timeinfo);
	(void)Map_AddOrUpdate(propMap, "$$CreationTimeUtc", timebuff);

	IoTHubDeviceClient_SendEventAsync(handle, message_handle, send_confirm_callback, NULL);

	IoTHubMessage_Destroy(message_handle);
}
// </sendmessage>

// </functions>




/*
NOTAS



EmergencyValveRelease -> Method#1
IncreaseTelemetryTag2 -> Method#2





temperatureSchema -> telemetryTag1Schema;
axialVibrationSchema -> telemetryTag2Schema;
radialVibrationSchema -> telemetryTag3Schema;
verticalVibrationSchema -> telemetryTag4Schema;
stateSchema -> telemetryTag5Schema;


<TeleTag1Unit> -> psi
<TeleTag2Unit> -> %% (% is scape to %)
<TeleTag3Unit>
<TeleTag4Unit>
<TeleTag5Unit>


<connectionstring>
<DeviceType>
<DeviceLocation>

## exemplos de Subs

DEVICE -> MOTOR
Device -> Motor
device -> motor

TelemetryTag1 -> Temperature
TelemetryTag2 -> AxialVibration
TelemetryTag3 -> RadialVibration
TelemetryTag4 -> VerticalVibration
TelemetryTag5 -> State [on,off]







*/