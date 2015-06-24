// Default sensor sketch for Sensebender Micro module
// Act as a temperature / humidity sensor by default.
//
// If A0 is held low while powering on, it will enter testmode, which verifies all on-board peripherals
// 
// Battery voltage is repported as child sensorId 199, as well as battery percentage (Internal message)


#include <MySensor.h>
#include <Wire.h>
#include <SPI.h> 
#include <Bounce2.h>


// Define a static node address, remove if you want auto address assignment
//#define NODE_ADDRESS   3

#define RELEASE "1.0"

// Child sensor ID's
//#define CHILD_ID_TEMP  1
//#define CHILD_ID_HUM   2
//#define CHILD_ID_BATT  199

// How many milli seconds between each measurement
#define MEASURE_INTERVAL 60000

// FORCE_TRANSMIT_INTERVAL, this number of times of wakeup, the sensor is forced to report all values to the controller
#define FORCE_TRANSMIT_INTERVAL 30 

// When MEASURE_INTERVAL is 60000 and FORCE_TRANSMIT_INTERVAL is 30, we force a transmission every 30 minutes.
// Between the forced transmissions a tranmission will only occur if the measured value differs from the previous measurement

//Pin definitions
#define TEST_PIN       A0
#define LED_PIN        A2
#define BUTTON_PIN  3  // Arduino Digital I/O pin number for button 
#define RELAY_1  3  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define NUMBER_OF_RELAYS 1 // Total number of attached relays
#define RELAY_ON 1  // GPIO value to write to turn on attached relay
#define RELAY_OFF 0 // GPIO value to write to turn off attached relay

MySensor gw;

// Sensor messages
//MyMessage msgHum(CHILD_ID_HUM, V_HUM);
//MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
//MyMessage msgBattery(CHILD_ID_BATT, V_VOLTAGE);

// Global settings
int measureCount = 0;
//boolean isMetric = true;

// Storage of old measurements
//float lastTemperature = -100;
//int lastHumidity = -100;
//long lastBattery = -100;

//bool highfreq = true;

void setup() {

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  Serial.print(F("MYS-Relay "));
  Serial.print(RELEASE);
  Serial.flush();

  // First check if we should boot into test mode

  pinMode(TEST_PIN,INPUT);
  digitalWrite(TEST_PIN, HIGH); // Enable pullup
  if (!digitalRead(TEST_PIN)) testMode();

  digitalWrite(TEST_PIN,LOW);
  digitalWrite(LED_PIN, HIGH); 

#ifdef NODE_ADDRESS
  gw.begin(incomingMessage, NODE_ADDRESS, false);
#else
  gw.begin(incomingMessage,AUTO,false);
#endif

  digitalWrite(LED_PIN, LOW);

  Serial.flush();
  Serial.println(F(" - Online!"));
  gw.sendSketchInfo("MYS-Relay", RELEASE);

  for (int CHILD_ID=1, pin=RELAY_1; CHILD_ID<=NUMBER_OF_RELAYS;CHILD_ID++, pin++) {
    // Register all sensors to gw (they will be created as child devices)
    gw.present(CHILD_ID, S_LIGHT);
    // Then set relay pins in output mode
    pinMode(pin, OUTPUT);   
    // Set relay to last known state (using eeprom storage) 
    digitalWrite(pin, gw.loadState(CHILD_ID)?RELAY_ON:RELAY_OFF);
    gw.send(msgHum.set(humidity));
  }
}


// Main loop function
void loop() {
    
  gw.process();
  debouncer.update();
  // Get the update value
  int value = debouncer.read();
  if (value != oldValue && value==0) {
      gw.send(msg.set(state?false:true), true); // Send new state and request ack back
  }
  oldValue = value;
  
}

void incomingMessage(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type==V_LIGHT) {
     // Change relay state
     digitalWrite(message.sensor-1+RELAY_1, message.getBool()?RELAY_ON:RELAY_OFF);
     // Store state in eeprom
     gw.saveState(message.sensor, message.getBool());
     // Write some debug info
     Serial.print("Incoming change for sensor:");
     Serial.print(message.sensor);
     Serial.print(", New status: ");
     Serial.println(message.getBool());
   } 
}

void sendRelayStatus() {
  for (int CHILD_ID=1, pin=RELAY_1; CHILD_ID<=NUMBER_OF_RELAYS;CHILD_ID++, pin++) {
    MyMessage msg(CHILD_ID,V_LIGHT);
    gw.send(msg.set(digitalRead(pin)));
  }
}



// Verify all peripherals, and signal via the LED if any problems.
void testMode()
{
  uint8_t rx_buffer[SHA204_RSP_SIZE_MAX];
  uint8_t ret_code;
  byte tests = 0;
  
  digitalWrite(LED_PIN, HIGH); // Turn on LED.
  Serial.println(F(" - TestMode"));
  Serial.println(F("Testing peripherals!"));
  Serial.flush();
  Serial.print(F("-> SI7021 : ")); 
  Serial.flush();
  
  if (humiditySensor.begin()) 
  {
    Serial.println(F("ok!"));
    tests ++;
  }
  else
  {
    Serial.println(F("failed!"));
  }
  Serial.flush();

  Serial.print(F("-> Flash : "));
  Serial.flush();
  if (flash.initialize())
  {
    Serial.println(F("ok!"));
    tests ++;
  }
  else
  {
    Serial.println(F("failed!"));
  }
  Serial.flush();

  
  Serial.print(F("-> SHA204 : "));
  ret_code = sha204.sha204c_wakeup(rx_buffer);
  Serial.flush();
  if (ret_code != SHA204_SUCCESS)
  {
    Serial.print(F("Failed to wake device. Response: ")); Serial.println(ret_code, HEX);
  }
  Serial.flush();
  if (ret_code == SHA204_SUCCESS)
  {
    ret_code = sha204.getSerialNumber(rx_buffer);
    if (ret_code != SHA204_SUCCESS)
    {
      Serial.print(F("Failed to obtain device serial number. Response: ")); Serial.println(ret_code, HEX);
    }
    else
    {
      Serial.print(F("Ok (serial : "));
      for (int i=0; i<9; i++)
      {
        if (rx_buffer[i] < 0x10)
        {
          Serial.print('0'); // Because Serial.print does not 0-pad HEX
        }
        Serial.print(rx_buffer[i], HEX);
      }
      Serial.println(")");
      tests ++;
    }

  }
  Serial.flush();

  Serial.println(F("Test finished"));
  
  if (tests == 3) 
  {
    Serial.println(F("Selftest ok!"));
    while (1) // Blink OK pattern!
    {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
  }
  else 
  {
    Serial.println(F("----> Selftest failed!"));
    while (1) // Blink FAILED pattern! Rappidly blinking..
    {
    }
  }  
}


