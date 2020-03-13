/////////////////////////////////
//
// TITLE:	WegIoT2AzureSolution.c
//
//	VERSION:	1.0
//
//	DATE:		13-Ago-2019
//
//	AUTHOR:		Nicolas Goulart
//				nicolas.goulart@dipsie.com.br
//
//	COMMENTS:	
// This code works with the X-Machine Remote Monitoring and Control Solution for:
// - Report device capabilities.
// - Send telemetry.
// - Respond to methods, including a long-running firmware update method.
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



// <Includes Headers>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
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
#include "windows.h"
#include "Winhttp.h"
#include "WegIoT2AzureSolution.h"
//#include "http.h" <- do not use


// <Azure Conection datadefinition>
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
static const char* connectionString = "HostName=kkap7-hub.azure-devices.net;DeviceId=166-MotorPulper2;SharedAccessKey=kVKbyQyY2b5oIxYD63Fi7S5ZSj/dsr/9+e35zpc80lo=";
static char msgText[1024];
static size_t g_message_count_send_confirmations = 0;
static const char* initialFirmwareVersion = "1.0.0";

IOTHUB_DEVICE_CLIENT_HANDLE device_handle;
typedef struct CONSULTAHTTP {
	char* id_motor;
	int* min;
	int* hora;
	int* dia;
	int* mes;
	int* ano;
	char* data_inicial;
	char* data_final;
	char* tempo_inicial;
	char* tempo_final;
	
 }Consulta_http;

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
	TelemetrySchema axialvibration;
	TelemetrySchema radialvibration;
	TelemetrySchema verticalvibration;
	TelemetrySchema motortemperature;
	TelemetrySchema environmenttemperature;
} TelemetryProperties;

typedef struct MOTOR_TAG
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
} Motor;
// </Azure Conection datadefinition>





// <WinHttp datadefinition>

DWORD dwDownloaded = 0;
LPSTR pszOutBufferVibra;
LPSTR pszOutBufferTemp;
BOOL  bResults = FALSE;
HINTERNET  hSession = NULL,
hConnect = NULL,
hRequest = NULL;

static char* Token, Secret, Planta;
char* HOST, DEVICEID, Variaveis, Variaveis2, Data_From, Data_to, Consulta, Consulta2, data1, data2;
LPCWSTR Header;
char* Server;


typedef struct MOTOR_TELEMETRY
{
	char* deviceid;
	double axialvibration, verticalvibration, radialvibration, motortemperature, environmenttemperature;
} Motor_telemetry;
Consulta_http consulta;
Motor_telemetry motor_telemetry;
// </WinHttp datadefinition>





//<Functions to Azure Conection>

/*  Converts the Motor object into a JSON blob with reported properties ready to be sent across the wire as a twin. */
static char* serializeToJson(Motor* motor)
{
	char* result;

	JSON_Value* root_value = json_value_init_object();
	JSON_Object* root_object = json_value_get_object(root_value);

	// Only reported properties:
	(void)json_object_set_string(root_object, "Protocol", motor->protocol);
	(void)json_object_set_string(root_object, "SupportedMethods", motor->supportedMethods);
	(void)json_object_set_string(root_object, "Type", motor->type);
	(void)json_object_set_string(root_object, "Firmware", motor->firmware);
	(void)json_object_set_string(root_object, "FirmwareUpdateStatus", MU_ENUM_TO_STRING(FIRMWARE_UPDATE_STATUS, motor->firmwareUpdateStatus));
	(void)json_object_set_string(root_object, "Location", motor->location);
	(void)json_object_set_number(root_object, "Latitude", motor->latitude);
	(void)json_object_set_number(root_object, "Longitude", motor->longitude);
	(void)json_object_dotset_string(root_object, "Telemetry.Axialvibration.MessageSchema.Name", motor->telemetry.axialvibration.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.Axialvibration.MessageSchema.Format", motor->telemetry.axialvibration.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.Axialvibration.MessageSchema.Fields", motor->telemetry.axialvibration.messageSchema.fields);
	(void)json_object_dotset_string(root_object, "Telemetry.Radialvibration.MessageSchema.Name", motor->telemetry.radialvibration.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.Radialvibration.MessageSchema.Format", motor->telemetry.radialvibration.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.Radialvibration.MessageSchema.Fields", motor->telemetry.radialvibration.messageSchema.fields);
	(void)json_object_dotset_string(root_object, "Telemetry.Verticalvibration.MessageSchema.Name", motor->telemetry.verticalvibration.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.Verticalvibration.MessageSchema.Format", motor->telemetry.verticalvibration.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.Verticalvibration.MessageSchema.Fields", motor->telemetry.verticalvibration.messageSchema.fields);
	(void)json_object_dotset_string(root_object, "Telemetry.Motortemperature.MessageSchema.Name", motor->telemetry.motortemperature.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.Motortemperature.MessageSchema.Format", motor->telemetry.motortemperature.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.Motortemperature.MessageSchema.Fields", motor->telemetry.motortemperature.messageSchema.fields);
	(void)json_object_dotset_string(root_object, "Telemetry.Environmenttemperature.MessageSchema.Name", motor->telemetry.environmenttemperature.messageSchema.name);
	(void)json_object_dotset_string(root_object, "Telemetry.Environmenttemperature.MessageSchema.Format", motor->telemetry.environmenttemperature.messageSchema.format);
	(void)json_object_dotset_string(root_object, "Telemetry.Environmenttemperature.MessageSchema.Fields", motor->telemetry.environmenttemperature.messageSchema.fields);

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
		system("cls");
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

static void sendMotorReportedProperties(Motor* motor)
{
	if (device_handle != NULL)
	{
		char* reportedProperties = serializeToJson(motor);
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
	Motor *motor = (Motor *)param;
	printf("Running simulated firmware update: URI: %s, Version: %s\r\n", motor->new_firmware_URI, motor->new_firmware_version);

	printf("Simulating download phase...\r\n");
	motor->firmwareUpdateStatus = DOWNLOADING;
	sendMotorReportedProperties(motor);

	ThreadAPI_Sleep(5000);

	printf("Simulating apply phase...\r\n");
	motor->firmwareUpdateStatus = APPLYING;
	sendMotorReportedProperties(motor);

	ThreadAPI_Sleep(5000);

	printf("Simulating reboot phase...\r\n");
	motor->firmwareUpdateStatus = REBOOTING;
	sendMotorReportedProperties(motor);

	ThreadAPI_Sleep(5000);

	size_t size = strlen(motor->new_firmware_version) + 1;
	(void)memcpy(motor->firmware, motor->new_firmware_version, size);

	motor->firmwareUpdateStatus = IDLE;
	sendMotorReportedProperties(motor);

	return 0;
}
void getFirmwareUpdateValues(Motor* motor, const unsigned char* payload)
{
	free(motor->new_firmware_version);
	free(motor->new_firmware_URI);
	motor->new_firmware_URI = NULL;
	motor->new_firmware_version = NULL;

	JSON_Value* root_value = json_parse_string((char *)payload);
	JSON_Object* root_object = json_value_get_object(root_value);

	JSON_Value* newFirmwareVersion = json_object_get_value(root_object, "Firmware");

	if (newFirmwareVersion != NULL)
	{
		const char* data = json_value_get_string(newFirmwareVersion);
		if (data != NULL)
		{
			size_t size = strlen(data) + 1;
			motor->new_firmware_version = malloc(size);
			(void)memcpy(motor->new_firmware_version, data, size);
		}
	}

	JSON_Value* newFirmwareURI = json_object_get_value(root_object, "FirmwareUri");

	if (newFirmwareURI != NULL)
	{
		const char* data = json_value_get_string(newFirmwareURI);
		if (data != NULL)
		{
			size_t size = strlen(data) + 1;
			motor->new_firmware_URI = malloc(size);
			(void)memcpy(motor->new_firmware_URI, data, size);
		}
	}

	// Free resources
	json_value_free(root_value);

}
// </firmwareupdate>
// <devicemethodcallback>
static int device_method_callback(const char* method_name, const unsigned char* payload, size_t size, unsigned char** response, size_t* response_size, void* userContextCallback)
{
	Motor *motor = (Motor *)userContextCallback;

	int result;

	(void)printf("Direct method name:    %s\r\n", method_name);

	(void)printf("Direct method payload: %.*s\r\n", (int)size, (const char*)payload);

	if (strcmp("Reboot", method_name) == 0)
	{
		MESSAGERESPONSE(201, "{ \"Response\": \"Rebooting\" }")
	}
	else if (strcmp("FirmwareUpdate", method_name) == 0)
	{
		if (motor->firmwareUpdateStatus != IDLE)
		{
			(void)printf("Attempt to invoke firmware update out of order\r\n");
			MESSAGERESPONSE(400, "{ \"Response\": \"Attempting to initiate a firmware update out of order\" }")
		}
		else
		{
			getFirmwareUpdateValues(motor, payload);

			if (motor->new_firmware_version != NULL && motor->new_firmware_URI != NULL)
			{
				// Create a thread for the long-running firmware update process.
				THREAD_HANDLE thread_apply;
				THREADAPI_RESULT t_result = ThreadAPI_Create(&thread_apply, do_firmware_update, motor);
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

// <sendmessage for Azure>
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
// </sendmessage
//</Functions to Azure Conection>



//<Functions to Winhttp>
Motor_telemetry ParsingJsonMotorTelemetryVibra(LPSTR* buffer)
{
	JSON_Value *root_value;
	JSON_Array *root_objects;
	JSON_Object *root_object;
	size_t i;

	char cleanup_command[256]="cls";

	/*Motor_telemetry motor_telemetry;
	memset(&motor_telemetry, 0, sizeof(Motor_telemetry));
	motor_telemetry.deviceid;
	motor_telemetry.axialvibration;
	motor_telemetry.radialvibration;
	motor_telemetry.verticalvibration;
	motor_telemetry.motortemperature;
	motor_telemetry.environmenttemperature;*/

	//parsing json and validating output
	root_value = json_parse_string(pszOutBufferVibra);
	if (json_value_get_type(root_value) != JSONArray) {//se 
		//system(cleanup_command);
		printf("Error in parsing json and validating output.\n");
		return;
	}
	root_objects = json_value_get_array(root_value);
	for (i = 0; i < json_array_get_count(root_objects); i++) {

		root_object = json_array_get_object(root_objects, i);

		motor_telemetry.deviceid = json_object_dotget_string(root_object, "DEVICEID");
		motor_telemetry.axialvibration = json_object_get_number(root_object, "AXIALVIBRATION");
		motor_telemetry.verticalvibration = json_object_get_number(root_object, "VERTICALVIBRATION");
		motor_telemetry.radialvibration = json_object_dotget_number(root_object, "RADIALVIBRATION");


	/*Getting and printing array from root value root_object info
	root_objects = json_value_get_array(root_value);
	// printf("%-10.10s %-10.10s %-15.10s %-15.10s\n", "DeviceID", "AxialVib", "VerticalVib", "RadialVib");
	for (i = 0; i < json_array_get_count(root_objects); i++) {
		root_object = json_array_get_object(root_objects, i);
		
		//printf("%-10.10s %-10.10f %-10.10f %-10.10f\n",
			motor_telemetry.deviceid = json_object_dotget_string(root_object, "DEVICEID");
			motor_telemetry.axialvibration = json_object_get_number(root_object, "AXIALVIBRATION");
			motor_telemetry.verticalvibration = json_object_get_number(root_object, "VERTICALVIBRATION");
			motor_telemetry.radialvibration = json_object_dotget_number(root_object, "RADIALVIBRATION");*/
	}
	


	return motor_telemetry;

	//cleanup code
	
	json_value_free(root_value);
	//system(cleanup_command);
	
}








Motor_telemetry ParsingJsonMotorTelemetryTemp(LPSTR* buffer, char id_motor)
{
	JSON_Value *root_value;
	JSON_Array *root_objects;
	JSON_Object *root_object;
	size_t i;

	char cleanup_command[256] = "cls";

	// Motor_telemetry motor_telemetry;
	//memset(&motor_telemetry, 0, sizeof(Motor_telemetry));


	//parsing json and validating output

	root_value = json_parse_string(pszOutBufferTemp);
	if (json_value_get_type(root_value) != JSONArray) {
		//system(cleanup_command);
		printf("Error in parsing json and validating output.\n");
		return;
	}
	//if (motor_telemetry.deviceid != id_motor) {
	//	printf("Erro na confimação de id do motor.\n");
		//return;
	//}
	root_objects = json_value_get_array(root_value);
	for (i = 0; i < json_array_get_count(root_objects); i++) {

		root_object = json_array_get_object(root_objects, i);
		
		motor_telemetry.deviceid = json_object_dotget_string(root_object, "DEVICEID");
		motor_telemetry.motortemperature = json_object_get_number(root_object, "MOTORTEMPERATURE");
		motor_telemetry.environmenttemperature = json_object_get_number(root_object, "ENVIRONMENTTEMPERATURE");

	}
	return motor_telemetry;

	//cleanup code
	
	json_value_free(root_value);
	//system(cleanup_command);

}
char* atualizaConsulta(char uri){
	
	if (uri == "" )
		printf("cadeia de caracteres vazia!");

}
static void atualizaVariavel() {
	;
	int* min;
	int* hora;
	int* dia;
	int* mes;
	int* ano;


}







static void winhttpVibra(void)
{
	// <code to WinHttp>

		// Use WinHttpOpen to obtain a session handle.
	hSession = WinHttpOpen(L"WinHTTP Example/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);

	// Specify an HTTP server.
	if (hSession)
		hConnect = WinHttpConnect(hSession, L"iot-connect.weg.net",
			INTERNET_DEFAULT_HTTPS_PORT, 0);

	// Create an HTTP request handle.
	if (hConnect)
	{
		atualizaVariavel();
		///v1/customer/measurement/plant/a290495e74b24266881947ae5bcdce80/devices/df08892000c0/variables/AXIALVIBRATION,RADIALVIBRATION,VERTICALVIBRATION/2019-08-16T03:00:00.000Z/2019-08-22T02:59:59.999Z?varSet=motor-fft-measurement&aggregateFunction=AVG&groupby=SECOND
		hRequest = WinHttpOpenRequest(hConnect, L"GET", L" v1/customer/measurement/plant/a290495e74b24266881947ae5bcdce80/devices/"+ consulta.id_motor +"/variables/AXIALVIBRATION,RADIALVIBRATION,VERTICALVIBRATION/2019-08-16T"+tempoInicial+"Z/2019-08-22T+"+tempoFinal+"Z?varSet=motor-fft-measurement&aggregateFunction=AVG&groupby=SECOND",
			NULL, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE);
		
		bResults = WinHttpAddRequestHeaders(hRequest,
			L"X-Api-Key:67f39b57-f8c6-49e8-985e-6a845cdb2836",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD);

		bResults = WinHttpAddRequestHeaders(hRequest,
			L"X-Api-Secret:ck$nxx.CDEy54-duWaf4hV#aE5cdfpM5kV-Lv$GdsTxozW6Zgphnq#M+PNF)au#q",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD);
		bResults = WinHttpAddRequestHeaders(hRequest,
			L"cache - control:no-cache",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD);
		bResults = WinHttpAddRequestHeaders(hRequest,
			L"Content-type:application/json",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD);

	}
	// Send a request.
	if (hRequest)
		bResults = WinHttpSendRequest(hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS,
			0, WINHTTP_NO_REQUEST_DATA, 0,
			0, 0);


	// End the request.
	if (bResults)
		bResults = WinHttpReceiveResponse(hRequest, NULL);

	// Keep checking for data until there is nothing left.
	if (bResults)
	{
		DWORD dwSize = 0;
		do
		{
			// Check for available data.
		
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
			{
				printf("Error %u in WinHttpQueryDataAvailable.\n",
					GetLastError());
				break;
			}

			// No more available data.
			if (!dwSize)
				break;

			// Allocate space for the buffer.
		
			pszOutBufferVibra =(char*) malloc(sizeof(char)*(dwSize));
			if (!pszOutBufferVibra)
			{
				printf("Out of memory\n");
				break;
			}

			// Read the Data.
			ZeroMemory(pszOutBufferVibra, dwSize);

			if (!WinHttpReadData(hRequest, (LPVOID)pszOutBufferVibra,
				dwSize, &dwDownloaded))
			{
				printf("Error %u in WinHttpReadData.\n", GetLastError());
			}
			//Print on screen
			else
			{
				__try {
					//Printing buffer Read

					//printf("%s\n", pszOutBuffer);
					motor_telemetry = ParsingJsonMotorTelemetryVibra(pszOutBufferVibra);
				}
				__except (EXCEPTION_EXECUTE_HANDLER ) {
					printf("fudeuVibra");
				}
			}

			// Free the memory allocated to the buffer.
			//free(pszOutBufferVibra);
			ZeroMemory(pszOutBufferVibra, dwSize);
		} while (dwSize > 0);
	}
	else
	{
		// Report any errors.
		printf("Error %d has occurred.\n", GetLastError());
	}

	// Close any open handles.
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);
	// </code to WinHttp>
}









static void winhttpTemp(void)
{	//vendo se id do motor atual é igual ao motor dado como parametro
	
	// <code to WinHttp>
	
		// Use WinHttpOpen to obtain a session handle.
	hSession = WinHttpOpen(L"WinHTTP Example/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);

	// Specify an HTTP server.
	if (hSession)
		hConnect = WinHttpConnect(hSession, L"iot-connect.weg.net",
			INTERNET_DEFAULT_HTTPS_PORT, 0);

	// Create an HTTP request handle.
	if (hConnect)
	{
		///v1/customer/measurement/plant/a290495e74b24266881947ae5bcdce80/devices/df08892000c0/variables/AXIALVIBRATION,RADIALVIBRATION,VERTICALVIBRATION/2019-08-16T03:00:00.000Z/2019-08-22T02:59:59.999Z?varSet=motor-fft-measurement&aggregateFunction=AVG&groupby=SECOND
		hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/v1/customer/measurement/plant/a290495e74b24266881947ae5bcdce80/devices//" + consultaidDevice + "/variables/STATUS,MOTORTEMPERATURE,ENVIRONMENTTEMPERATURE/2020-03-11T/" + consulta.tempo_inicial + "Z/2020-03-11T/" + consulta.tempo_final + "Z?varSet=motor-short-measurement&aggregateFunction=AVG&groupby=SECOND",
			NULL, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE);
	
		bResults = WinHttpAddRequestHeaders(hRequest,
			L"X-Api-Key:67f39b57-f8c6-49e8-985e-6a845cdb2836",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD);

		bResults = WinHttpAddRequestHeaders(hRequest,
			L"X-Api-Secret:ck$nxx.CDEy54-duWaf4hV#aE5cdfpM5kV-Lv$GdsTxozW6Zgphnq#M+PNF)au#q",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD);
		bResults = WinHttpAddRequestHeaders(hRequest,
			L"cache - control:no-cache",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD);
		bResults = WinHttpAddRequestHeaders(hRequest,
			L"Content-type:application/json",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD);

	}
	// Send a request.
	if (hRequest)
		bResults = WinHttpSendRequest(hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS,
			0, WINHTTP_NO_REQUEST_DATA, 0,
			0, 0);


	// End the request.
	if (bResults)
		bResults = WinHttpReceiveResponse(hRequest, NULL);

	// Keep checking for data until there is nothing left.
	if (bResults)
	{
		DWORD dwSize2 = 0;
		do
		{
			// Check for available data.
			
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize2))
			{
				printf("Error %u in WinHttpQueryDataAvailable.\n",
					GetLastError());
				break;
			}

			// No more available data.
			if (!dwSize2)
				break;

			// Allocate space for the buffer.
			pszOutBufferTemp = (char*)malloc(sizeof(char) * (dwSize2));
			//pszOutBufferTemp = malloc(dwSize);
			if (!pszOutBufferTemp)
			{
				printf("Out of memory\n");
				break;
			}

			// Read the Data.
			ZeroMemory(pszOutBufferTemp, dwSize2);

			if (!WinHttpReadData(hRequest, (LPVOID)pszOutBufferTemp,
				dwSize2, &dwDownloaded))
			{
				printf("Error %u in WinHttpReadData.\n", GetLastError());
			}
			//Print on screen
			else
			{
				__try {
					//Printing buffer Read

					//printf("%s\n", pszOutBufferTemp);
				    motor_telemetry = ParsingJsonMotorTelemetryTemp(pszOutBufferTemp, motor_telemetry.deviceid);
				}
				__except (EXCEPTION_EXECUTE_HANDLER) {
					printf("fudeuTemp");
				}
			}

			// Free the memory allocated to the buffer.
			//free(pszOutBufferTemp);
			ZeroMemory(pszOutBufferTemp, dwSize2);
		} while (dwSize2 > 0);
	}
	else
	{
		// Report any errors.
		printf("Error %d has occurred.\n", GetLastError());
	}

	// Close any open handles.
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);
	// </code to WinHttp>
}
//</Functions to Winhttp>




// <main>
int main(void)
{
//<code to Azure conection>

	srand((unsigned int)time(NULL));

	(void)printf("This sample simulates a Motor device connected to the X-Machine Remote Monitoring solution\r\n\r\n");

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

		//Serialize to JSON DEVICE properties and Send it to Cloud Solution
		Motor motor;
		memset(&motor, 0, sizeof(Motor));
		motor.protocol = "MQTT";
		motor.supportedMethods = "Reboot,FirmwareUpdate";
		motor.type = "Motor";
		size_t size = strlen(initialFirmwareVersion) + 1;
		motor.firmware = malloc(size);
		memcpy(motor.firmware, initialFirmwareVersion, size);
		motor.firmwareUpdateStatus = IDLE;
		motor.location = "Planta Carvalheira - BJP";
		motor.latitude = -23.150109;
		motor.longitude = -46.460882;

		motor.telemetry.axialvibration.messageSchema.name = "motor-axialvibration;v1";
		motor.telemetry.axialvibration.messageSchema.format = "JSON";
		motor.telemetry.axialvibration.messageSchema.fields = "{\"axialvibration\":\"Double\",\"axialvibration_unit\":\"Text\"}";

		motor.telemetry.radialvibration.messageSchema.name = "motor-radialvibration;v1";
		motor.telemetry.radialvibration.messageSchema.format = "JSON";
		motor.telemetry.radialvibration.messageSchema.fields = "{\"radialvibration\":\"Double\",\"radialvibration_unit\":\"Text\"}";

		motor.telemetry.verticalvibration.messageSchema.name = "motor-verticalvibration;v1";
		motor.telemetry.verticalvibration.messageSchema.format = "JSON";
		motor.telemetry.verticalvibration.messageSchema.fields = "{\"verticalvibration\":\"Double\",\"verticalvibration_unit\":\"Text\"}";

		motor.telemetry.motortemperature.messageSchema.name = "motor-motortemperature;v1";
		motor.telemetry.motortemperature.messageSchema.format = "JSON";
		motor.telemetry.motortemperature.messageSchema.fields = "{\"motortemperature\":\"Double\",\"motortemperature_unit\":\"Text\"}";

		motor.telemetry.environmenttemperature.messageSchema.name = "motor-environmenttemperature;v1";
		motor.telemetry.environmenttemperature.messageSchema.format = "JSON";
		motor.telemetry.environmenttemperature.messageSchema.fields = "{\"environmenttemperature\":\"Double\",\"environmenttemperature_unit\":\"Text\"}";
		sendMotorReportedProperties(&motor);

		(void)IoTHubDeviceClient_SetDeviceMethodCallback(device_handle, device_method_callback, &motor);
// </code to Azure conection>


		while (1)
		{
		winhttpVibra();
		winhttpTemp();
			if (motor.firmwareUpdateStatus == IDLE)
			{
				(void)printf("DEVICE: %s\r\n", motor_telemetry.deviceid);
				
				if (motor_telemetry.axialvibration < 100) {
					(void)printf("Sending sensor value Axialvibration = %f %s,\r\n", motor_telemetry.axialvibration, "mm/s");
					(void)sprintf_s(msgText, sizeof(msgText), "{\"axialvibration\":%.2f,\"axialvibration_unit\":\"mm/s\"}", motor_telemetry.axialvibration);
					//send_message(device_handle, msgText, motor.telemetry.axialvibration.messageSchema.name);
				}
				if (motor_telemetry.verticalvibration < 100) {
					(void)printf("Sending sensor value Verticalvibration = %f %s,\r\n", motor_telemetry.verticalvibration, "mm/s");
					(void)sprintf_s(msgText, sizeof(msgText), "{\"verticalvibration\":%.2f,\"verticalvibration_unit\":\"mm/s\"}", motor_telemetry.verticalvibration);
					//send_message(device_handle, msgText, motor.telemetry.verticalvibration.messageSchema.name);
				}
				if (motor_telemetry.radialvibration < 100) {
					(void)printf("Sending sensor value Radialvibration = %f %s,\r\n", motor_telemetry.radialvibration, "mm/s");
					(void)sprintf_s(msgText, sizeof(msgText), "{\"radialvibration\":%.2f,\"radialvibration_unit\":\"mm/s\"}", motor_telemetry.radialvibration);
					//send_message(device_handle, msgText, motor.telemetry.radialvibration.messageSchema.name);
				}

				if (motor_telemetry.motortemperature < 100) {
					(void)printf("Sending sensor value MotorTemperature = %f %s,\r\n", motor_telemetry.motortemperature, "C");
					(void)sprintf_s(msgText, sizeof(msgText), "{\"motorTemperature\":%.2f,\"motorTemperature_unit\":\"C\"}", motor_telemetry.motortemperature);
					//send_message(device_handle, msgText, motor.telemetry.motortemperature.messageSchema.name);
				}

				if (motor_telemetry.environmenttemperature < 100) {
					(void)printf("Sending sensor value EnvironmentTemperature = %f %s,\r\n", motor_telemetry.environmenttemperature, "C");
					(void)sprintf_s(msgText, sizeof(msgText), "{\"environmenttemperature\":%.2f,\"environmenttemperature_unit\":\"C\"}", motor_telemetry.environmenttemperature);
					//send_message(device_handle, msgText, motor.telemetry.environmenttemperature.messageSchema.name);
				}
			}

			 ThreadAPI_Sleep(1500);
		}

		(void)printf("\r\nShutting down\r\n");

		// Clean up the iothub sdk handle and free resources
		IoTHubDeviceClient_Destroy(device_handle);
		free(motor.firmware);
		free(motor.new_firmware_URI);
		free(motor.new_firmware_version);
	}

	// Shutdown the sdk subsystem
	IoTHub_Deinit();
	
	return 0;
}
// </main>