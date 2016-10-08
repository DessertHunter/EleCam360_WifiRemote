/*
 * Based on C-PTPIP.h created by Andrew Haining on 22/04/2015.
 * @see https://github.com/TheSombreroKid/cptpip/blob/master/cptpip.h
 * 
 * Modified by @DessertHunter 
 *  - to be usable on Arduino based ESP8266
 *  - replaced socket by WiFiClient https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/client-class.md
 *  - Made C++ Class from C-Code
 *  
 */
#ifndef ESP_PTP_IP_H
#define ESP_PTP_IP_H

#if !defined(ESP8266)
  #error("ESP8266 board needed!")
#endif

#include <Arduino.h>
// TODO: #include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ESP8266WiFi.h> // https://github.com/esp8266/Arduino

/*
 * Picture Transfer Protocol over IP
 */
class ESP_PTP_IP
{
  public:
    ESP_PTP_IP(void);
    
    bool Init(const char* szHost);

    uint32_t GetMode(void);
    void GetAllProperties(uint8_t** raw_property_list, uint16_t* raw_property_list_length);

    enum Code
    {
      C_InitiateCapture = 0x100E,  
      C_GetPropDesc = 0x1014,       // GetDevicePropDesc
      C_GetProp = 0x1015,           // GetDevicePropValue
      C_SetProp = 0x1016,           // SetDevicePropValue
      C_GetAllPropsDesc = 0x9614,
      C_Open = 0x1002,              // OpenSession
      C_Close = 0x1003,             // CloseSession
      C_Mode = 0xD604
    };
    
    void GetProperty(enum Code code, uint8_t const* raw_property_list, uint16_t raw_property_list_length, uint8_t const** values, uint8_t* value_size, uint8_t* value_count);
    
    bool OpInitiateCapture(void);
    
    void Uninit(void);
  
  private:
    WiFiClient mtClient;
    WiFiClient mtEventClient;
    uint32_t mu32SessionId;
    uint32_t mu32TransactionId;

    #pragma mark - prototypes
    // init
    bool initSession(void);
    bool initEventSocket(void);
    
    // session
    bool openSession(void);
    bool closeSession(void);
    
    // cmds
    static bool cmdResponse(WiFiClient& rtSocket, uint8_t **data, uint16_t *length);
    static bool cmdResponseData(WiFiClient& rtSocket, uint8_t* response, uint8_t** data, uint16_t* length);
    
    // utils
    uint32_t getNextTransactionId();
    static uint8_t* getResponse(WiFiClient& rtSocket);

};

#endif // ESP_PTP_IP_H



