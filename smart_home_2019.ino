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
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#pragma endregion

#pragma region Pins
#define reset_pin 2
#define heater_pin 4 
#define water_temp_pin 5
#define backside_lamps_pin 15 
#define siren_pin 16
#define porch_lamps_pin 17 
#define in_temp_pin 27
#define out_temp_pin 32
#define outdoor_control_pin 12
#define gate_control_pin 13

#define porch_alarm_pin 23
#define front_side_alarm_pin 21
#define back_side_alarm_pin 22
#define left_side_alarm_pin 14
#define right_side_alarm_pin 18
#define inside_alarm_pin 19

#define mp3_serial_rx_pin 25
#define mp3_serial_tx_pin 26
#pragma endregion

#pragma region ForBlynk
char auth[] = "enipoEUgTxuGbxVaon3jMxTEnDYDOcsL";
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
float man_mode_set_p = 23;               
float day_set_p = 20;                     
float night_set_p = 23;                   
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

int guard_mode = 1;
bool gate_signal = false;
bool outdoor_signal = false;

bool invasion_detected = false;
bool protect_front_side = true;
bool protect_back_side = true;
bool protect_left_side = true;
bool protect_right_side = true;
bool protect_porch = true;
bool protect_inside = true;

bool front_side_alarm = false;
bool back_side_alarm = false;
bool left_side_alarm = false;
bool right_side_alarm = false;
bool porch_alarm = false;
bool inside_alarm = false;

unsigned long reset_panic_timer_starts_with = 0;

unsigned long how_long_panic_lasts = 400000;

int mp3_number = 1;

bool mp3_player_works = false;

#pragma endregion

#pragma region Lamps
int porch_lamps_mode = 1;                           
bool porch_lamps_enabled;

int backside_lamps_mode = 1;
bool backside_lamps_enabled;
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
#define pin_porch_lamps_mode 12
#define pin_for_dmy_heated_hours 13
#define pin_for_months_heated_hours 14
WidgetBridge bridge1(V15);
#define pin_backside_lamps_mode 16
#define pin_outdoor_signal 17
#define pin_gate_signal 18

#define vpin_protect_front_side 19
#define vpin_protect_back_side 20
#define vpin_protect_left_side 21
#define vpin_protect_right_side 22
#define vpin_protect_porch 23

#define vpin_porch_alarm 24
#define pin_guard_mode 25
#define pin_restart 26

#define vpin_front_side_alarm 27
#define vpin_back_side_alarm 28
#define vpin_left_side_alarm 29
#define vpin_right_side_alarm 30
#define vpin_reset_all_the_alarm_leds 31

#define vpin_protect_inside 32
#define vpin_inside_alarm 33

#define vpin_how_long_panic_lasts 34
#define vpin_mp3_number 35
#pragma endregion

#pragma region Virtaul leds

WidgetLED porch_led_alarm(vpin_porch_alarm);
WidgetLED front_side_led_alarm(vpin_front_side_alarm);
WidgetLED back_side_led_alarm(vpin_back_side_alarm);
WidgetLED left_side_led_alarm(vpin_left_side_alarm);
WidgetLED right_side_led_alarm(vpin_right_side_alarm);
WidgetLED inside_led_alarm(vpin_inside_alarm);
#pragma endregion

Preferences pref;
HardwareSerial mp3_serial(2);
DFRobotDFPlayerMini mp3_player;

int heating_mode = 1;
bool heater_enabled;

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

#pragma region For watchdog

#define wdtTimeout 5000
hw_timer_t *timer = NULL;

void IRAM_ATTR restart() 
{
	pinMode(reset_pin, OUTPUT);
}
#pragma endregion

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
	Serial.begin(9600);
	
	mp3_serial.begin(9600, SERIAL_8N1, mp3_serial_rx_pin, mp3_serial_tx_pin);
	Serial.print("Mp3 player started with result: ");
	Serial.print(mp3_player.begin(mp3_serial));
	mp3_player.setTimeOut(500);
	mp3_player.volume(26);
	mp3_player.EQ(DFPLAYER_EQ_BASS);
	mp3_player.outputDevice(DFPLAYER_DEVICE_SD);
	
	Serial.println("Setup start...");
	
	wifi_mutex = xSemaphoreCreateMutex();
	pref_mutex = xSemaphoreCreateMutex();
	
	pref.begin("pref_1", false);
	
	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
	Blynk.begin(auth, ssid, pass);
	xSemaphoreGive(wifi_mutex);
	
	Wire.begin();
	
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	
	delay(1000);
	get_time();
	
	read_settings_from_pref();
	
	#pragma region TempsBegin
	temp_inside_sensor.begin();
	temp_outside_sensor.begin();
	temp_water_sensor.begin(); 
	#pragma endregion

	#pragma region PinInit
	pinMode(heater_pin, OUTPUT);
	pinMode(porch_lamps_pin, OUTPUT);
	pinMode(siren_pin, OUTPUT);
	pinMode(backside_lamps_pin, OUTPUT);
	
	pinMode(outdoor_control_pin, OUTPUT);
	digitalWrite(outdoor_control_pin, HIGH);
	pinMode(gate_control_pin, OUTPUT);
	digitalWrite(gate_control_pin, HIGH);
	
	pinMode(porch_alarm_pin, INPUT_PULLUP);
	pinMode(front_side_alarm_pin, INPUT_PULLUP);
	pinMode(back_side_alarm_pin, INPUT_PULLUP);
	pinMode(left_side_alarm_pin, INPUT_PULLUP);
	pinMode(right_side_alarm_pin, INPUT_PULLUP);
	pinMode(inside_alarm_pin, INPUT_PULLUP);
	#pragma endregion

	#pragma region Watchdog init
	timer = timerBegin(0, 80, true);                   
	timerAttachInterrupt(timer, &restart, true);    
	timerAlarmWrite(timer, wdtTimeout * 1000, false);  
	timerAlarmEnable(timer);                           
	#pragma endregion
	
	#pragma region TaskCreate
	xTaskCreate(get_temps, "get_temps", 2048, NULL, 1, NULL);
	xTaskCreate(get_time_task, "get_time_task", 10240, NULL, 1, NULL);
	xTaskCreate(calculate_water_temp, "calculate_water_temp", 2048, NULL, 1, NULL);
	xTaskCreate(heating_control, "heating_control", 1024, NULL, 1, NULL);
	xTaskCreate(porch_lamps_control, "porch_lamps_control", 1024, NULL, 1, NULL);
	xTaskCreate(backside_lamps_control, "backside_lamps_control", 1024, NULL, 1, NULL);
	xTaskCreate(panic_control, "panic_control", 1024, NULL, 1, NULL);
	xTaskCreate(guard_control, "guard_control", 18432, NULL, 1, NULL);
	xTaskCreate(send_data_to_blynk, "send_data_to_blynk", 10240, NULL, 1, NULL);
	xTaskCreate(run_blynk, "run_blynk", 10240, NULL, 1, NULL);
	xTaskCreate(write_setting_to_pref, "write_setting_to_pref", 2048, NULL, 1, NULL);
	xTaskCreate(count_heated_hours, "count_heated_hours", 2048, NULL, 1, NULL);
	xTaskCreate(send_heated_hours_to_app, "send_heated_hours_to_app", 4096, NULL, 1, NULL);
	xTaskCreate(feed_watchdog, "feed_watchdog", 1024, NULL, 1, NULL);
	xTaskCreate(heart_beat, "heart_beat", 1024, NULL, 1, NULL);
	xTaskCreate(restart_if_temp_sensors_have_frozen, "restart_if_temp_sensors_have_frozen", 2048, NULL, 1, NULL);
	xTaskCreate(open_outdoor, "open_outdoor", 8192, NULL, 1, NULL);
	xTaskCreate(send_signal_to_gate, "send_signal_to_gate", 8192, NULL, 1, NULL);
	#pragma endregion

}

#pragma region BLYNK_WRITE

BLYNK_WRITE(pin_manual_mode_set_point)
{
	man_mode_set_p = param.asFloat();
}

BLYNK_WRITE(pin_heater_enabled)
{
	heater_enabled = param.asInt();
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
	bridge1.virtualWrite(V2, panic_mode);
}

BLYNK_WRITE(pin_porch_lamps_mode)
{
	porch_lamps_mode = param.asInt();
}

BLYNK_WRITE(pin_backside_lamps_mode)
{
	backside_lamps_mode = param.asInt();
}

BLYNK_WRITE(pin_guard_mode)
{
	guard_mode = param.asInt();
}

BLYNK_WRITE(pin_restart)
{
	int restart_signal = param.asInt();
	if (restart_signal == 1) restart();
}

BLYNK_WRITE(pin_outdoor_signal)
{
	outdoor_signal = param.asInt();
}

BLYNK_WRITE(pin_gate_signal)
{
	gate_signal = param.asInt();
}

BLYNK_WRITE(vpin_protect_front_side)
{
	protect_front_side = param.asInt();
}

BLYNK_WRITE(vpin_protect_back_side)
{
	protect_back_side = param.asInt();
}

BLYNK_WRITE(vpin_protect_left_side)
{
	protect_left_side = param.asInt();
}

BLYNK_WRITE(vpin_protect_right_side)
{
	protect_right_side = param.asInt();
}

BLYNK_WRITE(vpin_protect_porch)
{
	protect_porch = param.asInt();
}

BLYNK_WRITE(vpin_protect_inside)
{
	protect_inside = param.asInt();
}

BLYNK_WRITE(vpin_reset_all_the_alarm_leds)
{
	if (param.asInt())
		reset_all_the_alarm_leds();
}

BLYNK_WRITE(vpin_how_long_panic_lasts)
{
	how_long_panic_lasts = param.asInt() * 60000;
}

BLYNK_WRITE(vpin_mp3_number)
{
	mp3_number = param.asInt();
}

BLYNK_CONNECTED()
{
	Blynk.syncAll();
	bridge1.setAuthToken("vxFa-4f0xhSnfX87x2mrsKtjtaLECdBW");
}

#pragma endregion

void loop(){}
