#include "CDRCWebServer.hpp"
#include "CustomDRChtml.h"
#include "CustomDRCcss.h"
#include "CustomDRCjs.h"
#include "Audison_AC_Link_Bus.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Update.h>

/* Put your SSID & Password */
const char *ssid = "Custom-DRC";   // Enter SSID here
const char *password = "12345678"; // Enter Password here

/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Create the web server
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket web_socket_handle("/ws");

void handle_json_key_value(JsonPair key_value)
{
    if (strcmp(key_value.key().c_str(), "getRemoteSettings") == 0)
    {
        Serial.println("*WS* Webpage loaded. Get settings");
    }
    else if (strcmp(key_value.key().c_str(), "password") == 0)
    {
        String password = key_value.value();
        Serial.printf("*WS* password: %s\n", password.c_str());
    }
    else if (strcmp(key_value.key().c_str(), "dspMemory") == 0)
    {
        uint8_t dspMemoryValue = key_value.value();
        Serial.printf("*WS* dspMemory: %d\n", dspMemoryValue);
    }
    else if (strcmp(key_value.key().c_str(), "inputSelect") == 0)
    {
        uint8_t inputSelectValue = key_value.value();
        Serial.printf("*WS* inputSelect: %d\n", inputSelectValue);
    }
    else if (strcmp(key_value.key().c_str(), "mute") == 0)
    {
        uint8_t muteValue = key_value.value();
        Serial.printf("*WS* mute: %d\n", muteValue);
    }
    else if (strcmp(key_value.key().c_str(), "masterVolume") == 0)
    {
        uint8_t master_volume_value = key_value.value();
        Serial.printf("*WS* masterVolume: %d\n", master_volume_value);
        Audison_AC_Link.set_volume(master_volume_value);
    }
    else if (strcmp(key_value.key().c_str(), "subVolume") == 0)
    {
        uint8_t sub_volume_value = key_value.value();
        Audison_AC_Link.set_sub_volume(sub_volume_value);
        Serial.printf("*WS* subVolume: %d\n", sub_volume_value);
    }
    else if (strcmp(key_value.key().c_str(), "balance") == 0)
    {
        uint8_t balance_value = key_value.value();
        Audison_AC_Link.set_balance(balance_value);
        Serial.printf("*WS* balance: %d\n", balance_value);
    }
    else if (strcmp(key_value.key().c_str(), "fader") == 0)
    {
        uint8_t fader_value = key_value.value();
        Audison_AC_Link.set_fader(fader_value);
        Serial.printf("*WS* fader: %d\n", fader_value);
    }
    else
    {
        Serial.println("Unknown JSON format key value pair");
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        data[len] = '\0';
        // Serial.printf("WS Message Rec: %s\n", data);
        JsonDocument doc;                                        // Allocate the JSON document
        DeserializationError error = deserializeJson(doc, data); // Parse JSON object
        if (error == DeserializationError::Ok)
        {
            for (JsonPair kv : doc.as<JsonObject>())
            {
                handle_json_key_value(kv);
            }
        }
        else
        {
            Serial.println("Error parsing JSON");
        }
    }
}

// Websocket on event callback
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

void initWebSocket()
{
    web_socket_handle.onEvent(onEvent);
    server.addHandler(&web_socket_handle);
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

// handles uploads
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);

    if (!index)
    {
        logmessage = "Upload Start: " + String(filename);
        // open the file on first call and store the file handle in the request object
        // request->_tempFile = SPIFFS.open("/" + filename, "w");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            Update.printError(Serial);
        }
        Serial.println(logmessage);
    }

    if (len)
    {
        // stream the incoming chunk to the opened file
        // request->_tempFile.write(data, len);
        logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
        Serial.println(logmessage);
        if (Update.write(data, len) != len)
        {
            Update.printError(Serial);
            Serial.printf("Progress: %d%%\n", (Update.progress() * 100) / Update.size());
        }
    }

    if (final)
    {
        logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
        // close the file handle as the upload is now done
        // request->_tempFile.close();
        Serial.println(logmessage);
        if (!Update.end(true))
        {
            Update.printError(Serial);
        }
        else
        {
            Serial.println("Update complete - rebooting in 5 seconds");
            vTaskDelay(pdMS_TO_TICKS(5000));
            Serial.flush();
            ESP.restart();
        }
    }
}

void web_server_init()
{
    WiFi.softAP(ssid, password, 1, 0, 1, false);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    delay(100);

    initWebSocket();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", (const uint8_t *)custom_html, sizeof(custom_html), nullptr); });

    server.on("/pico.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/css", (const uint8_t *)custom_css, sizeof(custom_css), nullptr); });

    server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/js", (const uint8_t *)custom_js, sizeof(custom_js), nullptr); });

    // run handleUpload function when any file is uploaded
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
              { request->send(200); }, handleUpload);

    server.begin();
    Serial.println("HTTP server started");
    server.onNotFound(notFound);
    server.begin();
}

void update_web_server_master_volume_value(uint8_t value)
{
    web_socket_handle.printfAll("{\"masterVolume\": %d}", value);
}
void update_web_server_fader_value(uint8_t value)
{
    web_socket_handle.printfAll("{\"fader\": %d}", value);
}
void update_web_server_balance_value(uint8_t value)
{
    web_socket_handle.printfAll("{\"balance\": %d}", value);
}
void update_web_server_sub_volume_value(uint8_t value)
{
    web_socket_handle.printfAll("{\"subVolume\": %d}", value);
}