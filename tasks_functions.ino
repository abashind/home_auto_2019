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
		current_time = String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
	}
	xSemaphoreGive(wifi_mutex);
		
	Serial.println(current_time);
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

#pragma endregion

#pragma region Tasks

void get_temps(void *pvParameters)
{
	while (true)
	{
		temp_inside_sensor.requestTemperatures();
		temp_outside_sensor.requestTemperatures();
		temp_water_sensor.requestTemperatures();
			
		vTaskDelay(2000 / portTICK_RATE_MS);
		
		float _temp_inside = temp_inside_sensor.getTempCByIndex(0);
		if (int(_temp_inside) != -127)
			temp_inside = _temp_inside;
    
		float _temp_outside = temp_outside_sensor.getTempCByIndex(0);
		if (int(_temp_outside) != -127)
			temp_outside = _temp_outside;

		float _water_temp = temp_water_sensor.getTempCByIndex(0);
		if (int(_water_temp) != -127)  
			temp_water = _water_temp;
		
		vTaskDelay(28000 / portTICK_RATE_MS);
	}
}

void get_time_task(void *pvParameters)
{
	while (true)
	{
		get_time();
		
		vTaskDelay(1000 / portTICK_RATE_MS);
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
		
		
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void detect_pir_move(void *pvParameters)
{
	while (true)
	{
		if (digitalRead(pir_pin))
			pir_move = true;
		else
			pir_move = false;
		
		vTaskDelay(1000 / portTICK_RATE_MS);
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
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	
}

void out_lamp_control(void *pvParameters)
{
	while (true)
	{
		//If panic, outside lamp is managed from panic control()
		if(panic_mode != 1) {}
		//Outside lamp OFF
		else if(out_lamp_mode == 1)
		{
			digitalWrite(out_lamp_pin, LOW);
			out_lamp_enabled = false;
		}
		//Outside lamp ON
		else if(out_lamp_mode == 2)
		{
			digitalWrite(out_lamp_pin, HIGH);
			out_lamp_enabled = true;
		}
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void outside_lamp_blinks(void *pvParameters)
{
	int interval = (int) pvParameters;
	
	while (true)
	{
		if (out_lamp_enabled)
		{
			digitalWrite(out_lamp_pin, LOW);
			out_lamp_enabled = false;
		}
		else
		{
			digitalWrite(out_lamp_pin, HIGH);
			out_lamp_enabled = true;
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
		siren_enabled = true;
		vTaskDelay(beep);
		digitalWrite(siren_pin, HIGH);
		siren_enabled = false;
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
			siren_enabled = false; 
		}
		//Outside lamp blinks
		if(panic_mode == 2)
		{
			if ((slow_blink_handle) == NULL)
				xTaskCreate(outside_lamp_blinks, "outside_lamp_blynk", 10000, (void *)1000, 1, &slow_blink_handle);
			digitalWrite(siren_pin, HIGH);
			siren_enabled = false; 
		}
		else if(slow_blink_handle != NULL)
		{
			vTaskDelete(slow_blink_handle);
			slow_blink_handle = NULL;
		}
		//Siren beeps, outside lamp works like a strobe
		if(panic_mode == 3)
		{
			if (fast_blink_handle_1 == NULL)
				xTaskCreate(outside_lamp_blinks, "outside_lamp_blynk", 10000, (void *)166, 1, &fast_blink_handle_1); 
		
			if (beep_handle == NULL)
				xTaskCreate(siren_beeps, "siren_beeps", 10000, NULL, 1, &beep_handle);
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
		//Full panic
		if(panic_mode == 4)
		{
			if (fast_blink_handle_2 == NULL)
				xTaskCreate(outside_lamp_blinks, "outside_lamp_blynk", 10000, (void *)166, 1, &fast_blink_handle_2); 
			digitalWrite(siren_pin, LOW);
			siren_enabled = true; 
		}
		else if(fast_blink_handle_2 != NULL)
		{
			vTaskDelete(fast_blink_handle_2);
			fast_blink_handle_2 = NULL;
		}
		vTaskDelay(300 / portTICK_RATE_MS);
	}
}

void guard_control(void *pvParameters)
{
	while (true)
	{
		//Guard OFF, manual panic control
		if(guard_mode == 1){}

		//Guard FULL
		if(guard_mode == 2)
		{
			if (pir_move)
				panic_mode = 4;
			else
				panic_mode = 1;
		}

		//Guard semi silent
		if(guard_mode == 3)
		{
			if (pir_move)
				panic_mode = 3;
			else
				panic_mode = 1;
		}
		
		vTaskDelay(1000 / portTICK_RATE_MS);
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
			if (out_lamp_enabled)
				Blynk.virtualWrite(pin_out_lamp_mode, 2);
			else
				Blynk.virtualWrite(pin_out_lamp_mode, 1);
		}
		Blynk.virtualWrite(pin_current_time, current_time);
		Blynk.virtualWrite(pin_temp_inside, temp_inside);
		Blynk.virtualWrite(pin_temp_water, temp_water);
		Blynk.virtualWrite(pin_temp_outside, temp_outside);
		if (pir_move)
			Blynk.virtualWrite(pin_pir_move, 1);
		else
			Blynk.virtualWrite(pin_pir_move, 0);
		
		xSemaphoreGive(wifi_mutex);
		
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void run_blynk(void *pvParameters)
{
	while (true)
	{
		xSemaphoreTake(wifi_mutex, portMAX_DELAY);
		Blynk.run();
		xSemaphoreGive(wifi_mutex);
		vTaskDelay(500 / portTICK_RATE_MS);
	}
}

#pragma endregion