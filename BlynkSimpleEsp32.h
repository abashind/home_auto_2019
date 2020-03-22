 /* 
 I remade these methods because of wrong (for me) Blynk code behavior.
 It was made thus that Blynk.begin() blocks rest code, in other words if WiFi connection and blynk connection during Blynk.begin() don't be established rest code won't be executed. 
 I wasn't happy with how it worked, so I decided to rewrite this code.
 Now, it's no matters whether or not these connections are established, code waits 5 seconds for WiFi connection and make
 3 attemps to get through to Blynk server. Then it goes forward.
 It means you don't need WiFi when ESP32 starts. For example, assume your WiFi router brake down, and then they turn off electricity. After electricity appearing your ESP32 board with Blynk code doesn't start because of it needs WiFi to start.
 But using this code your ESP32 with Blynk code starts ordinary and proceeds do its tasks, just without communication with
 Internet and Blynk servers.
 You have to replace original methods by these methods or put these methods near original and invoke them.
 But you may need to restore WiFi connection when WiFi will be ok. It turned out that Blynk.run() doesn't do it but you can
 see how I solved this issue in file tasks_functions.ino of this project. Find run_blynk method. 
 */

void begin(const char* auth,
               const char* ssid,
               const char* pass,
               const char* domain = BLYNK_DEFAULT_DOMAIN,
               uint16_t    port   = BLYNK_DEFAULT_PORT)
    {
        connectWiFi(ssid, pass);
        config(auth, domain, port);
	    if (WiFi.status() == WL_CONNECTED)
	    {
		    for (int i = 0; i < 3; i++)
		    {
			    Serial.println("Attempt connect to server number " + String(i) + ".");
			    if (this->connect() == true) break;
		    }
	    }
	    else
	    {
		    Serial.println("WiFi connection wasn't established, so there is no sense connect to Blynk server...");
	    }
    }
	
	    void connectWiFi(const char* ssid, const char* pass)
    {
        BLYNK_LOG2(BLYNK_F("Connecting to "), ssid);
        WiFi.mode(WIFI_STA);
        if (pass && strlen(pass)) {
            WiFi.begin(ssid, pass);
        } else {
            WiFi.begin(ssid);
        }
	    for (int i = 0; i < 5; i++)
	    {
		    Serial.println("Waiting connection to WiFi, seconds: " + String(i) + ".");
		    if (WiFi.status() == WL_CONNECTED) break; 
		    BlynkDelay(1000);	       
	    }
	    if (WiFi.status() == WL_CONNECTED)
	    {
		    BLYNK_LOG1(BLYNK_F("Connected to WiFi"));
		    IPAddress myip = WiFi.localIP();
		    BLYNK_LOG_IP("IP: ", myip);
		    return;
	    }
	     Serial.println("Connection to WiFi failed...");
    }
