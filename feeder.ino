#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "webpage.h"
#include <time.h> // for time() ctime()

// Configuration of NTP
#define MY_NTP_SERVER "asia.pool.ntp.org"
#define MY_TZ "<+08>-8"


// Global vars
// Motor vars
int motorPin = 2;
int motorState = 0;
bool timerOn = false;
short timerIndex = -1;

// Timer vars
time_t timeNow;         // this are the seconds since Epoch (1970) - UTC
tm tmTimeNow;
short prevSec = 0;

time_t feederStartTime;
unsigned int feedDuration = 0;

short feedHour[10] = {8, 10, 12, 14, 16, 17, 23, 99, 99, 99};
short feedMin[10] = {30, 0, 0, 0, 0, 0, 59, 0, 0, 0};
short feedLength[10] = {60, 60, 60, 60, 60, 60, 80, 0, 0, 0};

ESP8266WebServer server(80);        // Init and start webserver


// Constants
String html = MAIN_page;
String feedString = "Time Feeded : ";

const char *ssid = "Multi Modal @unifi";
const char *password = "\"(mPE!fPE!fV7V7LGP";


void setup(void)
{
    // Init Serial communication
    Serial.begin(115200);
    delay(1000);


    // Init and connect to WiFi
    WiFi.begin(ssid, password);

    Serial.println("Attempting to connect to WiFi...");

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("----Connecting----");
        delay(250);
    }

    Serial.println("WiFi Connected !");
    Serial.print("SSID : ");
    Serial.println(ssid);
    Serial.print("IPV4 : ");
    Serial.println(WiFi.localIP());


    // Init and setup webpage
    server.on("/", echoRootPage);

    server.on("/toggleMotor", serverToggleMotor);

    server.on("/update", update); // to be updated to display instead of console.log

    server.begin();

    Serial.println("Webserver started");


    // Init pin connection
    pinMode(motorPin, OUTPUT);
    digitalWrite(motorPin, motorState);


    // Init NTP server connection
    configTime(MY_TZ, MY_NTP_SERVER);
    delay(2000);
}

// Echoes back the root page to client
void echoRootPage()
{
    server.send(200, "text/html", html);
}

// Runs when button is toggled and server.on is triggered
// Updates state of motor and updates the pin out
void serverToggleMotor()
{
    // If motor is not on, then only process the trigger, else ignore
    // In the future will add blocking of clicking the toggle button
    if (motorState == 0)
    {
        // Record time of turning on motor
        // Will set start time even if server toggle it off, but is fine as not frequent call
        time(&feederStartTime);

        // Set motor state
        motorState = server.arg("motorState").toInt();
        digitalWrite(motorPin, motorState);

        // Add duration of feeding to the feed time if server toggled the motor off
        if (motorState == 0)
            feedDuration += int(difftime(time(nullptr), feederStartTime));

        // Send success message to server and print state to serial
        server.send(200, "text/plain", "Motor State Successfully Updated");
        Serial.println(motorState);
    }
}

// Return to webpage time feeded when triggered
void update()
{
    server.send(200, "text/plain", feedString + String(feedDuration) + " Seconds.");
}

// Check whether its time to feed or not
void timeCheck()
{
    // Get time from internal clock
    time(&timeNow);     // read the current time
    localtime_r(&timeNow, &tmTimeNow);     // update the structure tm with the current time

    // Print current time
    if (tmTimeNow.tm_sec != prevSec)
    {
        Serial.print(tmTimeNow.tm_hour);
        Serial.print("  :  ");
        Serial.print(tmTimeNow.tm_min);
        Serial.print("  :  ");
        Serial.println(tmTimeNow.tm_sec);
        prevSec = tmTimeNow.tm_sec;
    }

    // Check time for whether motor should be on
    if (!timerOn)
    {
        for (size_t i = 0; i < 10; i++)
        {
            // If time and minute of array index i is current time, then turn it on
            if ((tmTimeNow.tm_hour == feedHour[i] && tmTimeNow.tm_min == feedMin[i]) && feedHour[i] != 99)
            {
                // Record starting time of turning on motor if motor was not on
                if (motorState == 0)
                    time(&feederStartTime);

                // Toggle timer flag, set timerIndex and turn motor on
                timerOn = true;
                timerIndex = i;
                motorState = 1;
                digitalWrite(motorPin, motorState);
            }
        }
    }


    // While motor is running due to timer 
    if (motorState == 1 && timerOn)
    {
        // Check if already feed for x seconds
        if (difftime(time(nullptr), feederStartTime) > feedLength[timerIndex])
        {
            // Turn motor off
            motorState = 0;
            digitalWrite(motorPin, motorState);

            // Add duration of feeding to the feed time
            feedDuration += difftime(time(nullptr), feederStartTime);

            // Toggle timer flag off
            timerOn = false;
        }
    }
}

// Constantly check if wifi is connected
void wifiCheck()
{
    // Check if wifi is connected, if not then connect
    if (WiFi.status() != WL_CONNECTED)
    {
        // Connect to WiFi
        WiFi.begin(ssid, password);

        // Wait till wifi is fully connected
        while (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("----Connecting----");
            delay(250);
        }
    }
}

// Changes ntp startup delay to 1 second
uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000()
{
  randomSeed(A0);
  return random(1000);
}

// Runs endlessly after start up
void loop(void)
{
    // Handle server request
    server.handleClient();

    // Check whether its time to feed or not
    timeCheck();

    // Runs every 5 minutes
    if ((tmTimeNow.tm_min % 5) == 0)
    {
        // Check if the wifi is still connected
        wifiCheck();
    }
}
