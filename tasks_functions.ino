#pragma region Functions

void get_time()
{
	struct tm timeinfo;
	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
	if (!getLocalTime(&timeinfo)) 
		current_time  = "TimeFail";
	else
	{
		current_hour = timeinfo.tm_hour;
		current_day = timeinfo.tm_mday;
		current_month = timeinfo.tm_mon;
		current_year = timeinfo.tm_year;
		current_time = to_str<int>(timeinfo.tm_hour) + ":" + to_str<int>(timeinfo.tm_min) + ":" + to_str<int>(timeinfo.tm_sec);
	}
	xSemaphoreGive(wifi_mutex);
}

void warm_cool(float setPoint)
{
	bool too_cold = temp_inside <= setPoint - half_inside_dzone;
	bool water_not_max = temp_water < max_water_temp - half_water_dzone;
	bool water_too_cold = temp_water < min_water_temp - half_water_dzone;
	
	bool too_warm = temp_inside >= setPoint + half_inside_dzone;
	bool water_too_hoot = temp_water >= max_water_temp + half_water_dzone;
	
	if ((too_cold && water_not_max) || water_too_cold)
	{
		digitalWrite(heater_pin, HIGH);
		heater_enabled = true; 
	}
	else if (too_warm || water_too_hoot)
	{
		digitalWrite(heater_pin, LOW);
		heater_enabled = false;
	}   
}

void read_settings_from_pref()
{
	xSemaphoreTake(pref_mutex, portMAX_DELAY);
	man_mode_set_p = pref.getFloat("man_mode_set_p");
	day_set_p = pref.getFloat("day_set_p");
	night_set_p = pref.getFloat("night_set_p");
	max_water_temp = pref.getInt("max_water_temp");    
	min_water_temp = pref.getInt("min_water_temp");    
	guard_mode = pref.getInt("guard_mode");    
	panic_mode = pref.getInt("panic_mode");    
	heating_mode = pref.getInt("heating_mode");    
	porch_lamps_mode = pref.getInt("porch_lamps_mode");
	invasion_detected = pref.getBool("invasion_detected");
	mp3_number = pref.getInt("mp3_number");
	xSemaphoreGive(pref_mutex);	
}

void set_led_red(WidgetLED &led)
{
	led.setColor("#D3435C");
	led.on();
}

void set_led_green(WidgetLED &led)
{
	led.setColor("#23C48E");
	led.on();
}

void reset_all_the_alarm_leds()
{
		set_led_green(porch_led_alarm);
		porch_led_alarm_is_red = false;

		set_led_green(front_side_led_alarm);
		front_side_led_alarm_is_red = false;
	
		set_led_green(back_side_led_alarm);
		back_side_led_alarm_is_red = false;
	
		set_led_green(left_side_led_alarm);
		left_side_led_alarm_is_red = false;
	
		set_led_green(right_side_led_alarm);
		right_side_led_alarm_is_red = false;
	
		set_led_green(inside_led_alarm);
		inside_led_alarm_is_red = false;
}

void send_panic_to_outdoor_esp32()
{
	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
	bridge1.virtualWrite(V2, panic_mode);
	xSemaphoreGive(wifi_mutex);
}

void detect_invasion()
{
	//Get states of move sensors.
	porch_alarm = !digitalRead(porch_alarm_pin);
	front_side_alarm = !digitalRead(front_side_alarm_pin);
	back_side_alarm = !digitalRead(back_side_alarm_pin);
	left_side_alarm = !digitalRead(left_side_alarm_pin);
	right_side_alarm = !digitalRead(right_side_alarm_pin);
	inside_alarm = !digitalRead(inside_alarm_pin);
	
	//Set leds to red color.
	if(porch_alarm && !porch_led_alarm_is_red)
	{
		set_led_red(porch_led_alarm);
		porch_led_alarm_is_red = true;
	}
		
	if (front_side_alarm && !front_side_led_alarm_is_red)
	{
		set_led_red(front_side_led_alarm);
		front_side_led_alarm_is_red = true;
	}
		
	if (back_side_alarm && !back_side_led_alarm_is_red)
	{
		set_led_red(back_side_led_alarm);
		back_side_led_alarm_is_red = true;
	}
		
	if (left_side_alarm && !left_side_led_alarm_is_red)
	{
		set_led_red(left_side_led_alarm);
		left_side_led_alarm_is_red = true;
	}
		
	if (right_side_alarm && !right_side_led_alarm_is_red)
	{
		set_led_red(right_side_led_alarm);
		right_side_led_alarm_is_red = true;
	}
	
	if (inside_alarm && !inside_led_alarm_is_red)
	{
		set_led_red(inside_led_alarm);
		inside_led_alarm_is_red = true;
	}
		
	//Determine invasion, if it's needed.
	if((porch_alarm && protect_porch) || 
		(front_side_alarm && protect_front_side) || 
		(back_side_alarm && protect_back_side) ||
		(left_side_alarm && protect_left_side) ||
		(right_side_alarm && protect_right_side) ||
		(inside_alarm && protect_inside))
		invasion_detected = true;
	else
		invasion_detected = false;
}

void write_to_pref_invasion_detected()
{
	xSemaphoreTake(pref_mutex, portMAX_DELAY);
	pref.putBool("invasion_detected", invasion_detected);
	xSemaphoreGive(pref_mutex);
}

void send_to_app_invasion_notification()
{
	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
	Blynk.notify("Invasion has been detected! Go to watch the cams!!!");
	xSemaphoreGive(wifi_mutex);
}

void run_mp3_player()
{
	if (!mp3_player_works)
	{
		mp3_player.loop(mp3_number);
		mp3_player_works = true;
	}
}

void stop_mp3_player()
{
	if (mp3_player_works)
	{
		mp3_player.stop();
		mp3_player_works = false;
	}
}

#pragma endregion

#pragma region Tasks

void get_temps(void *pvParameters)
{
	while (true)
	{
		temp_inside_sensor.requestTemperatures();
		temp_outside_sensor.requestTemperatures();
		temp_water_sensor.requestTemperatures();
			
		temp_inside = temp_inside_sensor.getTempCByIndex(0);
		temp_outside = temp_outside_sensor.getTempCByIndex(0);
		temp_water = temp_water_sensor.getTempCByIndex(0);
		
		vTaskDelay(15000 / portTICK_RATE_MS);
	}
}

void get_time_task(void *pvParameters)
{
	while (true)
	{
		get_time();
		
		vTaskDelay(2000 / portTICK_RATE_MS);
	}
}

void calculate_water_temp(void *pvParameters)
{
	while (true)
	{
		if (heating_mode == 3) {}
		else
		{
			if (temp_outside > -20)
				max_water_temp = 60;
			if (temp_outside <= -20 && temp_outside > -25)
				max_water_temp = 65;
			if (temp_outside <= -25 && temp_outside > -30)
				max_water_temp = 70;
			if (temp_outside <= -30)
				max_water_temp = 85;
		}
		
		vTaskDelay(30000 / portTICK_RATE_MS);
	}
}

void heating_control(void *pvParameters)
{
	while (true)
	{
		switch (heating_mode)
		{
			//Manual mode
			case 1 :
			{
				warm_cool(man_mode_set_p);    
				break;
			}

			//Day/night mode
			case 2 :
			{
				//Day
				if(current_hour >= zone_one_start && current_hour < zone_one_finish)
					warm_cool(day_set_p); 
				//Night
				else
					warm_cool(night_set_p);
				break;
			}

			//Automatic off
			case 3 :
			{
				if (heater_enabled)
					digitalWrite(heater_pin, HIGH);
				else
					digitalWrite(heater_pin, LOW);
				break;
			}
		}
		vTaskDelay(2000 / portTICK_RATE_MS);
	}
}

void porch_lamps_control(void *pvParameters)
{
	while (true)
	{
		//If panic, outside lamp is managed from panic control()
		if(panic_mode != 1) {}
		//Outside lamp OFF
		else if(porch_lamps_mode == 1)
		{
			digitalWrite(porch_lamps_pin, LOW);
			porch_lamps_enabled = false;
		}
		//Outside lamp ON
		else if(porch_lamps_mode == 2)
		{
			digitalWrite(porch_lamps_pin, HIGH);
			porch_lamps_enabled = true;
		}
		vTaskDelay(1200 / portTICK_RATE_MS);
	}
}

void backside_lamps_control(void *pvParameters)
{
	while (true)
	{
		//If panic, outside lamp is managed from panic control()
		if(panic_mode != 1) {}
		//Outside lamp OFF
		else if(backside_lamps_mode == 1)
		{
			digitalWrite(backside_lamps_pin, LOW);
			backside_lamps_enabled = false;
		}
		//Outside lamp ON
		else if(porch_lamps_mode == 2)
		{
			digitalWrite(backside_lamps_pin, HIGH);
			backside_lamps_enabled = true;
		}
		vTaskDelay(1200 / portTICK_RATE_MS);
	}
}

void lamps_blink(void *pvParameters)
{
	int interval = (int) pvParameters;
	
	while (true)
	{
		if (porch_lamps_enabled)
		{
			digitalWrite(porch_lamps_pin, LOW);
			porch_lamps_enabled = false;
		}
		else
		{
			digitalWrite(porch_lamps_pin, HIGH);
			porch_lamps_enabled = true;
		}
		
		if (backside_lamps_enabled)
		{
			digitalWrite(backside_lamps_pin, LOW);
			backside_lamps_enabled = false;
		}
		else
		{
			digitalWrite(backside_lamps_pin, HIGH);
			backside_lamps_enabled = true;
		}
		
		vTaskDelay(interval / portTICK_RATE_MS);
	}
}

void siren_beeps(void *pvParameters)
{
	TickType_t beep = 200 / portTICK_RATE_MS;
	TickType_t silent = 800 / portTICK_RATE_MS;
	
	while (true)
	{
		digitalWrite(siren_pin, LOW);
		vTaskDelay(beep);
		digitalWrite(siren_pin, HIGH);
		vTaskDelay(silent);
	}
}

void panic_control(void *pvParameters)
{
	while (true)
	{
		//Not panic
		if(panic_mode == 1)
		{
			digitalWrite(siren_pin, HIGH);
			stop_mp3_player();
		}
		
		//Outside lamps blink, 
		if(panic_mode == 2)
		{
			if ((slow_blink_handle) == NULL) xTaskCreate(lamps_blink, "lamps_blink", 4096, (void *)1000, 1, &slow_blink_handle);
			digitalWrite(siren_pin, HIGH);
			run_mp3_player();
		}
		else if(slow_blink_handle != NULL)
		{
			vTaskDelete(slow_blink_handle);
			slow_blink_handle = NULL;
		}
		
		//Siren beeps, outside lamps work like a strobe
		if(panic_mode == 3)
		{
			if (fast_blink_handle_1 == NULL) xTaskCreate(lamps_blink, "lamps_blink", 4096, (void *)166, 1, &fast_blink_handle_1); 
			if (beep_handle == NULL) xTaskCreate(siren_beeps, "siren_beeps", 4096, NULL, 1, &beep_handle);
			run_mp3_player();
		}
		else 
		{
			if (fast_blink_handle_1 != NULL)
			{
				vTaskDelete(fast_blink_handle_1);
				fast_blink_handle_1 = NULL;
			}
			if (beep_handle != NULL)
			{
				vTaskDelete(beep_handle);
				beep_handle = NULL;
			}
		}
		
		//Full panic.
		if(panic_mode == 4)
		{
			if (fast_blink_handle_2 == NULL) xTaskCreate(lamps_blink, "lamps_blink", 4096, (void *)166, 1, &fast_blink_handle_2); 
			digitalWrite(siren_pin, LOW);
			run_mp3_player();
		}
		else if(fast_blink_handle_2 != NULL)
		{
			vTaskDelete(fast_blink_handle_2);
			fast_blink_handle_2 = NULL;
		}
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void guard_control(void *pvParameters)
{
	while (true)
	{
		detect_invasion();
		
		if (panic_mode != 1 && !invasion_detected)
		{
			if (reset_panic_timer_starts_with == 0) reset_panic_timer_starts_with = millis();
			if (millis() - reset_panic_timer_starts_with > how_long_panic_lasts)
			{
				panic_mode = 1;
				write_to_pref_invasion_detected();
				send_panic_to_outdoor_esp32();
				reset_panic_timer_starts_with = 0;
				xSemaphoreTake(wifi_mutex, portMAX_DELAY);
				Blynk.virtualWrite(pin_panic_mode, panic_mode);
				xSemaphoreGive(wifi_mutex);
			}
		}
		
		if (guard_mode != 1 && invasion_detected)
		{
			panic_mode = guard_mode;
			send_panic_to_outdoor_esp32();
			send_to_app_invasion_notification();
			write_to_pref_invasion_detected();
			reset_panic_timer_starts_with = millis();
			vTaskDelay(2000 / portTICK_RATE_MS);
		}
		
		vTaskDelay(100 / portTICK_RATE_MS);
	}
}

void send_data_to_blynk(void *pvParameters)
{
	while (true)
	{
		xSemaphoreTake(wifi_mutex, portMAX_DELAY);
		
		if (heating_mode != 3)
		{
			Blynk.virtualWrite(pin_heater_enabled, heater_enabled);
			Blynk.virtualWrite(pin_max_water_temp, max_water_temp);
		}
		if (guard_mode != 1)
			Blynk.virtualWrite(pin_panic_mode, panic_mode);
		if (panic_mode != 1)
		{	
			if (porch_lamps_enabled)
				Blynk.virtualWrite(pin_porch_lamps_mode, 2);
			else
				Blynk.virtualWrite(pin_porch_lamps_mode, 1);
			
			if (backside_lamps_enabled)
				Blynk.virtualWrite(pin_backside_lamps_mode, 2);
			else
				Blynk.virtualWrite(pin_backside_lamps_mode, 1);
		}
		Blynk.virtualWrite(pin_current_time, current_time.c_str());
		Blynk.virtualWrite(pin_temp_inside, temp_inside);
		Blynk.virtualWrite(pin_temp_water, temp_water);
		Blynk.virtualWrite(pin_temp_outside, temp_outside);
		
		xSemaphoreGive(wifi_mutex);
		
		vTaskDelay(2000 / portTICK_RATE_MS);
	}
}

void run_blynk(void *pvParameters)
{
	while (true)
	{
		xSemaphoreTake(wifi_mutex, portMAX_DELAY);
		if (WiFi.status() != WL_CONNECTED) 
		{
			Blynk.connectWiFi(ssid, pass);
		}
		Blynk.run();
		xSemaphoreGive(wifi_mutex);
		vTaskDelay(750 / portTICK_RATE_MS);
	}
}

void write_setting_to_pref(void *pvParameters)
{
	while (true)
	{
		xSemaphoreTake(pref_mutex, portMAX_DELAY);
		pref.putFloat("man_mode_set_p", man_mode_set_p);
		pref.putFloat("day_set_p", day_set_p);    
		pref.putFloat("night_set_p", night_set_p);    
		pref.putInt("max_water_temp", max_water_temp);    
		pref.putInt("min_water_temp", min_water_temp);
		pref.putInt("guard_mode", guard_mode); 
		pref.putInt("panic_mode", panic_mode);    
		pref.putInt("heating_mode", heating_mode);    
		pref.putInt("porch_lamps_mode", porch_lamps_mode);
		pref.putInt("mp3_number", mp3_number);
		xSemaphoreGive(pref_mutex);
		vTaskDelay(30000 / portTICK_RATE_MS);
	}
}

void count_heated_hours(void *pvParameters)
{
	while (true)
	{
		#pragma region Reset heated hours
		
		xSemaphoreTake(pref_mutex, portMAX_DELAY);
		
		auto pref_current_day = pref.getInt(current_dmy_keys[0]);
		if (current_time != "TimeFail" && current_day != pref_current_day)
		{
			pref.putInt(current_dmy_keys[0], current_day);
			pref.putInt(heated_hours_dmy_keys[0], 0);
		}
		
		auto pref_current_month= pref.getInt(current_dmy_keys[1]);
		if (current_time != "TimeFail" && current_month != pref_current_month)
		{
			pref.putInt(current_dmy_keys[1], current_month);
			pref.putInt(heated_hours_dmy_keys[1], 0);
		}
		
		auto pref_current_year = pref.getInt(current_dmy_keys[2]);
		if (current_time != "TimeFail" && current_year != pref_current_year)
		{
			pref.putInt(current_dmy_keys[2], current_year);
			pref.putInt(heated_hours_dmy_keys[2], 0);
			for (auto &key_number : heated_hours_months_keys_numbers)
			{
				pref.putInt(key_number.first, 0);
			}
		}
		
		#pragma endregion
		
		if (heater_enabled)
		{
			for (auto &key : heated_hours_dmy_keys)
			{
				auto value = pref.getInt(key);
				value++;
				pref.putInt(key, value);
			}
			
			for (auto &key_number : heated_hours_months_keys_numbers)
			{
				if (current_month == key_number.second)
				{
					auto value = pref.getInt(key_number.first);
					value++;
					pref.putInt(key_number.first, value);
				}
			}
		}
		
		xSemaphoreGive(pref_mutex);
		vTaskDelay(30000 / portTICK_RATE_MS);
	}
}

void send_heated_hours_to_app(void *pvParameters)
{
	while (true)
	{
		xSemaphoreTake(wifi_mutex, portMAX_DELAY);
		xSemaphoreTake(pref_mutex, portMAX_DELAY);
		
		std::string dmy_heated_hours;
		for (auto &key : heated_hours_dmy_keys)
		{	
			auto half_minutes = pref.getInt(key);
			auto hours = half_minutes * 0.00833;
			dmy_heated_hours = dmy_heated_hours + to_str<float>(hours) + "/";
		}
		dmy_heated_hours.pop_back();
		Blynk.virtualWrite(pin_for_dmy_heated_hours, dmy_heated_hours.c_str());
		
		std::string months_heated_hours;
		for (auto &key_number : heated_hours_months_keys_numbers)
		{
			auto half_minutes = pref.getInt(key_number.first);
			auto hours = half_minutes * 0.00833;
			months_heated_hours = months_heated_hours + to_str<float>(hours) + "/";
		}
		months_heated_hours.pop_back();
		Blynk.virtualWrite(pin_for_months_heated_hours, months_heated_hours.c_str());
		
		xSemaphoreGive(pref_mutex);
		xSemaphoreGive(wifi_mutex);
		vTaskDelay(30000 / portTICK_RATE_MS);
	}
}

void heart_beat_feed_watchdog(void *pvParameters)
{
	while (true)
	{	
		timerWrite(timer, 0);
		Serial.println("Looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong.");
		Serial.println(xPortGetFreeHeapSize());
		Serial.println(xPortGetMinimumEverFreeHeapSize());
		vTaskDelay(2000 / portTICK_RATE_MS);
	}
}

void open_outdoor(void *pvParameters)
{
	while (true)
	{
		if (outdoor_signal)
		{
			digitalWrite(outdoor_control_pin, LOW);
			vTaskDelay(750 / portTICK_RATE_MS);
			digitalWrite(outdoor_control_pin, HIGH);
			outdoor_signal = false;
			
			xSemaphoreTake(wifi_mutex, portMAX_DELAY);
			Blynk.virtualWrite(pin_outdoor_signal, outdoor_signal);
			xSemaphoreGive(wifi_mutex);
		}
			
		vTaskDelay(1500 / portTICK_RATE_MS);
	}
}

void send_signal_to_gate(void *pvParameters)
{
	while (true)
	{
		if (gate_signal)
		{
			digitalWrite(gate_control_pin, LOW);
			vTaskDelay(250 / portTICK_RATE_MS);
			digitalWrite(gate_control_pin, HIGH);
			gate_signal = false;
			
			xSemaphoreTake(wifi_mutex, portMAX_DELAY);
			Blynk.virtualWrite(pin_gate_signal, gate_signal);
			xSemaphoreGive(wifi_mutex);
		}
		
		vTaskDelay(1500 / portTICK_RATE_MS);
	}
}

#pragma endregion