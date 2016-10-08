/*
 * Changes by @DessertHunter:
 * 
 * Uses Arduino Client Class https://www.arduino.cc/en/Reference/ClientConstructor
 * 
 */
#include "ESP_PTP_IP.h"
// https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/client-class.md
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#pragma mark - Data Structures

#define InitRequestEnd 1
#define InitResponseEnd 0x10000

enum FailReason
{
  FR_None,
  FR_Rejected,
  FR_Busy,
  FR_Unspecified
};

enum Type
{
  T_None,
  T_InitRequest,
  T_InitResponse,
  T_EventRequest,
  T_EventResponse,
  T_InitFail,
  T_CmdRequest,
  T_CmdResponse,
  T_Event,
  T_DataStart,
  T_Data,
  T_CancelTransaction,
  T_DataEnd,
};

enum DataTypes
{
  DT_None,
  DT_int8,
  DT_uint8,
  DT_int16,
  DT_uint16,
  DT_int32,
  DT_uint32,
  DT_int64,
  DT_uint64,
  DT_int128,
  DT_uint128,
  
  DT_uint8_array = 0x4002,
  
  DT_string = 0xFFFF
};

struct  __attribute__((packed)) ptpip_header
{
  uint32_t length;
  uint32_t type;
};

struct  __attribute__((packed)) init_request
{
  struct ptpip_header header;
  uint8_t guid[16];
  uint8_t name[16]; // fixed size name because cba
  uint32_t end;
  
};

struct  __attribute__((packed)) init_response
{
  struct ptpip_header header;
  uint32_t session;
  uint8_t camera[16];
  uint32_t zerobytes;
  uint32_t end;
};

struct  __attribute__((packed)) event_request
{
  struct ptpip_header header;
  uint32_t code;
};

struct  __attribute__((packed)) event_response
{
  struct ptpip_header header;
};

struct  __attribute__((packed)) cmd_request
{
  struct ptpip_header header;
  uint32_t flag;
  uint16_t code;
  uint32_t transactionid;
};

struct  __attribute__((packed)) cmd_with1param_request
{
  struct cmd_request cmd;
  uint32_t param;
};

struct  __attribute__((packed)) cmd_with2param_request
{
  struct cmd_request cmd;
  uint32_t param[2];
};


typedef struct cmd_request cmd_close_request;
typedef struct cmd_with1param_request cmd_getprop_request;
typedef struct cmd_with1param_request cmd_open_request;
typedef struct cmd_with2param_request cmd_initiatecapture_request;


struct  __attribute__((packed)) cmd_response
{
  struct ptpip_header header;
  uint16_t code;
  uint32_t transactionid;
  uint32_t zerobytes[5];
};

struct  __attribute__((packed)) data_header
{
  struct ptpip_header header;
  uint32_t transactionid;
};

struct  __attribute__((packed)) data_start
{
  struct data_header dataheader;
  uint32_t datalength;
  uint32_t zerobytes;
};

struct  __attribute__((packed)) property_header
{
  uint32_t length;
  uint16_t code;
  uint16_t dataType;
  bool readonly;
};

uint32_t swap(uint32_t littleEndian)
{
  //return littleEndian;
  return ((littleEndian>>24)&0xFF) | ((littleEndian>>8)&0xFF00) | ((littleEndian<<8)&0xFF0000) | ((littleEndian<<24)&0xFF000000);
}


ESP_PTP_IP::ESP_PTP_IP(void)
  : mu32SessionId(0)
  , mu32TransactionId(0)
{

}

#pragma mark - main init
bool ESP_PTP_IP::Init(const char* szHost)
{
  const int ciPTP_IP_Port = 15740;
  
  // Use WiFiClient class to create TCP connections
  if (!this->mtClient.connect(szHost, ciPTP_IP_Port))
  {
    Serial.println("connection 1/2 failed");
    this->Uninit();
    return false;
  }

  if (!this->mtEventClient.connect(szHost, ciPTP_IP_Port))
  {
    Serial.println("connection 2/2 failed");
    this->Uninit();
    return false;
  }
  
  if(!this->initSession())
  {
    this->Uninit();
    return false;
  }
    
  if(!this->openSession())
  {
    this->Uninit();
    return false;
  }

  return (this->mtClient.connected() && this->mtEventClient.connected());
}

void ESP_PTP_IP::Uninit(void)
{
  this->closeSession();
  Serial.println("PTP/IP: disconnecting from server");
  this->mtClient.stop();
  this->mtEventClient.stop();
}

#pragma mark - init
bool ESP_PTP_IP::initSession()
{
  bool result = false;
  
  struct init_response response;
  memset(&response, 0, sizeof(response));
  struct init_request request;
  memset(&request, 0, sizeof(request));
  request.header.length = sizeof(request);
  request.header.type = T_InitRequest;
  strcpy((char*)request.guid, "iOSDeviceGUID111"); // TODO
  
  for (uint8_t i = 0; i < 7; ++i)
  {
    request.name[i*2] = 'i';
  }
  request.end = InitRequestEnd;
  if (this->mtClient.write((const uint8_t *)&request, request.header.length) == request.header.length)
  {
    uint8_t* response = getResponse(this->mtClient);
    if (response)
    {
      struct ptpip_header* header = (struct ptpip_header *)response;
      if (header->type == T_InitResponse && header->length == sizeof(struct init_response))
      {
        struct init_response* initresponse = (struct init_response *)response;
        if (initresponse->end == InitResponseEnd)
        {
          this->mu32SessionId = initresponse->session;
          result = true;
        }
      }
      free(response);
    }
  }
  return result;
}

#pragma mark - event socket
bool ESP_PTP_IP::initEventSocket(void)
{
  bool result = false;
  
  struct event_request request;
  request.header.length = sizeof(request);
  request.header.type = T_EventRequest;
  request.code = this->mu32SessionId;
  if (this->mtEventClient.write((uint8_t*)&request, request.header.length) == request.header.length)
  {
    uint8_t* response = getResponse(this->mtEventClient);
    if (response)
    {
      struct ptpip_header* header = (struct ptpip_header*)response;
      if (header->type == T_EventResponse && header->length == sizeof(struct event_response))
      {
        result = true;
      }
      free(response);
    }
  }
  return result;
}

#pragma mark - session
bool ESP_PTP_IP::openSession(void)
{
  bool result = false;
  
  cmd_open_request request;
  request.cmd.header.length = sizeof(request);
  request.cmd.header.type = T_CmdRequest;
  request.cmd.flag = 1;
  request.cmd.code = C_Open;
  request.cmd.transactionid = this->mu32SessionId;
  request.param = 1;
  if (this->mtClient.write((const uint8_t*)&request, request.cmd.header.length) == request.cmd.header.length)
  {
    uint8_t* response = getResponse(this->mtClient);
    if (response)
    {
      struct ptpip_header* header = (struct ptpip_header*)response;
      if (header->type == T_CmdResponse)
      {
        result = true;
      }
      free(response);
    }
  }
  return result;
}

bool ESP_PTP_IP::closeSession(void)
{
  bool result = false;
  
  cmd_close_request request;
  request.header.length = sizeof(request);
  request.header.type = T_CmdRequest;
  request.flag = 1;
  request.code = C_Close;
  request.transactionid = this->mu32SessionId;
  if(this->mtClient.write((const uint8_t*)&request, request.header.length) == request.header.length)
  {
    uint8_t* response = getResponse(this->mtClient);
    if (response)
    {
      struct ptpip_header* header = (struct ptpip_header*)response;
      if (header->type == T_CmdResponse)
      {
        result = true;
      }
      free(response);
    }
  }
  return result;
}

bool ESP_PTP_IP::OpInitiateCapture(void)
{
  bool result = false;
  
  cmd_initiatecapture_request request;
  request.cmd.header.length = sizeof(request);
  request.cmd.header.type = T_CmdRequest;
  request.cmd.flag = 1;
  request.cmd.code = C_InitiateCapture;
  request.cmd.transactionid = this->getNextTransactionId();
  request.param[1] = 0x00000000; // StorageID (unused)
  request.param[2] = 0x00000000; // ObjectFormatCode (unused)
  
  if (this->mtClient.write((const uint8_t*)&request, request.cmd.header.length) == request.cmd.header.length)
  {
    uint8_t *data = NULL;
    uint16_t length = 0;
    cmdResponse(this->mtClient, &data, &length);
    struct cmd_response* response = (struct cmd_response*)data;
    result = response->code;
    free(data);

    result = result;
  }
  
  return false;
}

#pragma mark - properties
uint32_t ESP_PTP_IP::GetMode(void)
{
  uint32_t result = 0;
  
  cmd_getprop_request request;
  request.cmd.header.length = sizeof(request);
  request.cmd.header.type = T_CmdRequest;
  request.cmd.flag = 1;
  request.cmd.code = C_GetProp;
  request.cmd.transactionid = this->getNextTransactionId();
  request.param = C_Mode;
  
  if (this->mtClient.write((const uint8_t*)&request, request.cmd.header.length) == request.cmd.header.length)
  {
    uint8_t *data = NULL;
    uint16_t length = 0;
    cmdResponse(this->mtClient, &data, &length);
    struct cmd_response* response = (struct cmd_response*)data;
    result = response->code;
    free(data);
  }
  
  return result;
}

void ESP_PTP_IP::GetAllProperties(uint8_t** raw_property_list, uint16_t* raw_property_list_length)
{
  cmd_getprop_request request;
  request.cmd.header.length = sizeof(request);
  request.cmd.header.type = T_CmdRequest;
  request.cmd.flag = 1;
  request.cmd.code = C_GetAllPropsDesc;
  request.cmd.transactionid = this->getNextTransactionId();
  if(this->mtClient.write((uint8_t*)&request, request.cmd.header.length) == request.cmd.header.length)
  {
    *raw_property_list_length = 0;
    cmdResponse(this->mtClient, raw_property_list, raw_property_list_length);
  }
}

void ESP_PTP_IP::GetProperty(enum Code code, uint8_t const* raw_property_list, uint16_t raw_property_list_length, uint8_t const** values, uint8_t* value_size, uint8_t* value_count)
{
  uint16_t read_head = 0;
  *value_size = 0;
  *value_count = 0;
  
  while (read_head < raw_property_list_length)
  {
    struct property_header* propertyheader = (struct property_header*)(raw_property_list + read_head);
    
    if (propertyheader->code == code)
    {
      size_t header_size = sizeof(*propertyheader);
      //uint16_t value = *(uint16_t*)(self.raw_property_list+read_head+header_size+2);
      *values = raw_property_list + read_head + header_size;
      switch (propertyheader->dataType)
      {
        case DT_int8:
        case DT_uint8:
          *value_size = 1;
          break;
        case DT_int16:
        case DT_uint16:
          *value_size = 2;
          break;
        case DT_int32:
        case DT_uint32:
          *value_size = 4;
          break;
        case DT_int64:
        case DT_uint64:
          *value_size = 8;
          break;
        case DT_int128:
        case DT_uint128:
          *value_size = 16;
          break;
      }
      *value_count = (propertyheader->length - sizeof(*propertyheader)) / (*value_size);
    }
    
    read_head += propertyheader->length;
  }
}

#pragma mark - commands

/*static*/ bool ESP_PTP_IP::cmdResponse(WiFiClient& rtSocket, uint8_t** data, uint16_t* length)
{
  bool result = false;
  
  uint8_t* response = getResponse(rtSocket);
  if (response)
  {
    struct ptpip_header* header = (struct ptpip_header*)response;
    if (header->type == T_CmdResponse)
    {
      struct cmd_response* cmdresponse = (struct cmd_response*)response;
    }
    else if (header->type == T_DataStart)
    {
      result = cmdResponseData(rtSocket, response, data, length);
    }
    
    free(response);
  }
  return result;
}

/*static*/ bool ESP_PTP_IP::cmdResponseData(WiFiClient& rtSocket, uint8_t* response, uint8_t** data, uint16_t* length)
{
  struct ptpip_header* header = (struct ptpip_header*)response;
  if (header->type == T_DataStart && header->length == sizeof(struct data_start))
  {
    struct data_start* datastart = (struct data_start*)response;
    *length = datastart->datalength;
    *data = (uint8_t*)malloc(*length);
    memset(*data, 0, *length);
    uint8_t* write_head = *data;
    
    enum Type type = T_None;
    uint32_t size = 0;
    while (type != T_DataEnd)
    {
      uint8_t* response2 = getResponse(rtSocket);
      if (response2)
      {
        header = (struct ptpip_header *)response2;
        size += header->length - sizeof(struct data_header);
        assert(header->type == T_Data || header->type == T_DataEnd);
        if (header->type == T_DataEnd)
        {
          struct data_header *dataheader = (struct data_header*)response2;
          //uint16_t camMode = *(response2 + sizeof(struct data_header));
          memcpy(write_head, (response2 + sizeof(struct data_header)), dataheader->header.length - sizeof(struct data_header));
          write_head += dataheader->header.length - sizeof(struct data_header);
        }
        type = (enum Type)header->type;
        free(response2);
      }
    }
    
    assert(size == *length);
    
    uint8_t *response3 = getResponse(rtSocket);
    if (response3)
    {
      header = (struct ptpip_header *)response3;
      assert(header->type == T_CmdResponse);
      free(response3);
    }
    
    return true;
  }
  
  return false;
}

#pragma mark - utils

uint32_t ESP_PTP_IP::getNextTransactionId()
{
  return this->mu32TransactionId++;
}

/*static*/ uint8_t* ESP_PTP_IP::getResponse(WiFiClient& rtSocket)
{
  uint8_t* response = NULL;
  struct ptpip_header header;

  unsigned long timeout = millis();
  while (rtSocket.available() == 0)
  {
    if (millis() - timeout > 5000)
    {
      Serial.println("PTP/IP: Error receive timeout!");
      return NULL; // Abort
    }
  }
  
  int bytes = rtSocket.read((uint8_t*)&header, sizeof(header));
  if (bytes >= 0)
  {
    if (bytes != sizeof(header))
      Serial.printf("PTP/IP: Error received wrong size %d (expected)\n", bytes, sizeof(header));
    
    assert(bytes >= sizeof(header)); // TODO bytes == sizeof(header)
    
    response = (uint8_t*)malloc(header.length);
    memcpy(response, &header, sizeof(header));
    memset(response + sizeof(header), 0, header.length - sizeof(header));
    if (header.length > sizeof(header))
    {
      bytes = 0;
      while (bytes < header.length - sizeof(header))
      {
        int currentBytes = rtSocket.read(response + sizeof(header) + bytes, header.length - sizeof(header) - bytes);
        if (currentBytes >= 0)
          bytes += currentBytes;
      }
    }
  }
  return response;
}

