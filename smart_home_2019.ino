#pragma region Include
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <time.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <map>
#pragma endregion

#pragma region Pins
#define out_temp_pin 19
#define in_temp_pin 18
#define water_temp_pin 5
#define out_lamp_pin 17
#define siren_pin 16
#define heater_pin 4
#define pir_pin 23
#pragma endregion

#pragma region ForBlynk
char auth[] = "64eb1e89df674887b797183a7d3150a5";
char ssid[] = "7SkyHome";
char pass[] = "89191532537";
#pragma endregion

#pragma region Sensors
OneWire sensor_1(in_temp_pin);
DallasTemperature temp_inside_sensor(&sensor_1);
OneWire sensor_2(out_temp_pin);
DallasTemperature temp_outside_sensor(&sensor_2);
OneWire sensor_3(water_temp_pin);
DallasTemperature temp_water_sensor(&sensor_3);		  
#pragma endregion

#pragma region Temperatures
float temp_inside;
float temp_outside;
float temp_water;		  
#pragma endregion

#pragma region Setpoints
int man_mode_set_p = 23;               
int day_set_p = 20;                     
int night_set_p = 23;                   
int max_water_temp = 60;
int min_water_temp = 35;			  
#pragma endregion

#pragma region DayTimeZone
#define zone_one_start 9
#define zone_one_finish 20		  
#pragma endregion

#pragma region DeadZones
#define inside_dzone 0.2
float half_inside_dzone = inside_dzone / 2;
#define water_dzone 4
int half_water_dzone = water_dzone / 2;			  
#pragma endregion

#pragma region Security
int panic_mode = 1;
bool siren_enabled = false;
bool pir_move = false;
int guard_mode = 1; 		  
#pragma endregion

#pragma region NTP
const char *ntpServer = "pool.ntp.org";
#define gmtOffset_sec 18000
#define daylightOffset_sec 0
#pragma endregion

#pragma region Blynk virt pins

#define pin_manual_mode_set_point 0
#define pin_current_time 1
#define pin_heater_enabled 2
#define pin_temp_inside 3
#define pin_min_water_temp 4
#define pin_heating_mode 5
#define pin_day_set_point 6
#define pin_night_set_point 7
#define pin_temp_water 9
#define pin_temp_outside 8
#define pin_max_water_temp 10
#define pin_panic_mode 11
#define pin_out_lamp_mode 12
#define pin_for_dmy_heated_hours 13
#define pin_for_months_heated_hours 14
#define pin_pir_move 24
#define pin_guard_mode 25

#pragma endregion

Preferences pref;

int heating_mode = 1;
bool heater_enabled;

int out_lamp_mode = 1;                           
bool out_lamp_enabled;

String current_time;
int current_hour;
int current_day;
int current_month;
int current_year;

TaskHandle_t slow_blink_handle;
TaskHandle_t fast_blink_handle_1;
TaskHandle_t fast_blink_handle_2;
TaskHandle_t beep_handle;

SemaphoreHandle_t wifi_mutex;
SemaphoreHandle_t pref_mutex;

const char* heated_hours_dmy_keys[] = 
{ 
	"h_h_per_day",
	"h_h_per_month",
	"h_h_per_year"
};

const char* current_dmy_keys[] = 
{ 
	"current_day",
	"current_month",
	"current_may"
};

std::map<const char*, uint8_t> heated_hours_months_keys_numbers =
{ 
	{ "h_h_per_sep", 8 },
	{ "h_h_per_oct", 9 },
	{ "h_h_per_nov", 10 },
	{ "h_h_per_dec", 11 },
	{ "h_h_per_jan", 12 },
	{ "h_h_per_feb", 1 },
	{ "h_h_per_mar", 2 },
	{ "h_h_per_apr", 3 },
	{ "h_h_per_may", 4 }
};

void setup()
{
	wifi_mutex = xSemaphoreCreateMutex();
	pref_mutex = xSemaphoreCreateMutex();
	
	Serial.begin(115200);
	
	pref.begin("pref_1", false);
	
	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
	Blynk.begin(auth, ssid, pass);
	xSemaphoreGive(wifi_mutex);
	
	Wire.begin();
	
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	
	get_time();
	
	read_settings_from_pref();
	
	#pragma region TempsBegin
	temp_inside_sensor.begin();
	temp_outside_sensor.begin();
	temp_water_sensor.begin(); 
	#pragma endregion

	#pragma region PinInit
	pinMode(heater_pin, OUTPUT);
	pinMode(out_lamp_pin, OUTPUT);
	pinMode(out_lamp_pin, OUTPUT);
	pinMode(siren_pin, OUTPUT);
	pinMode(pir_pin, INPUT_PULLDOWN);
	#pragma endregion

	#pragma region TaskCreate
	xTaskCreate(get_temps, "get_temps", 2048, NULL, 1, NULL);
	xTaskCreate(get_time_task, "get_time_task", 10240, NULL, 1, NULL);
	xTaskCreate(calculate_water_temp, "calculate_water_temp", 2048, NULL, 1, NULL);
	xTaskCreate(detect_pir_move, "detect_pir_move", 1024, NULL, 1, NULL);
	xTaskCreate(heating_control, "heating_control", 1024, NULL, 1, NULL);
	xTaskCreate(out_lamp_control, "light_control", 1024, NULL, 1, NULL);
	xTaskCreate(panic_control, "panic_control", 1024, NULL, 1, NULL);
	xTaskCreate(guard_control, "guard_control", 1024, NULL, 1, NULL);
	xTaskCreate(send_data_to_blynk, "send_data_to_blynk", 10240, NULL, 1, NULL);
	xTaskCreate(run_blynk, "run_blynk", 2048, NULL, 1, NULL);
	xTaskCreate(write_setting_to_pref, "write_setting_to_pref", 2048, NULL, 1, NULL);
	xTaskCreate(count_heated_hours, "count_heated_hours", 2048, NULL, 1, NULL);
	xTaskCreate(send_heated_hours_to_app, "send_heated_hours_to_app", 4096, NULL, 1, NULL);
	#pragma endregion
	
}

#pragma region BLYNK_WRITE

BLYNK_WRITE(pin_manual_mode_set_point)
{
	man_mode_set_p = param.asFloat();
}

BLYNK_WRITE(pin_heater_enabled)
{
	if (param.asInt())
		heater_enabled = true;
	else
		heater_enabled = false;
}

BLYNK_WRITE(pin_min_water_temp)
{
	min_water_temp = param.asInt();
}

BLYNK_WRITE(pin_heating_mode)
{
	heating_mode = param.asInt();
}

BLYNK_WRITE(pin_day_set_point)
{
	day_set_p = param.asFloat();
}

BLYNK_WRITE(pin_night_set_point)
{
	night_set_p = param.asFloat();
}

BLYNK_WRITE(pin_max_water_temp)
{
	max_water_temp = param.asInt();
}

BLYNK_WRITE(pin_panic_mode)
{
	panic_mode = param.asInt();
}

BLYNK_WRITE(pin_out_lamp_mode)
{
	out_lamp_mode = param.asInt();
}

BLYNK_WRITE(pin_guard_mode)
{
	guard_mode = param.asInt();
}

BLYNK_CONNECTED()
{
	Blynk.syncAll();
}

#pragma endregion

void loop(){}