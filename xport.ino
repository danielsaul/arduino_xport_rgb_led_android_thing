#include <LiquidCrystal.h>
#include <string.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <AF_XPort.h>
#include <SoftwareSerial.h>

LiquidCrystal lcd(7, 8, 5, 6, 9, 12);

// Strings stored in flash of the HTML we will be xmitting
const prog_char http_404header[] PROGMEM = "HTTP/1.1 404 Not Found\nServer: arduino\nContent-Type: text/html\n\n<html><head><title>404</title></head><body><h1>404: Sorry, that page cannot be found!</h1></body>";
const prog_char http_header[] PROGMEM = "HTTP/1.0 200 OK\nServer: arduino\nContent-Type: text/html\n\n";

const prog_char la_html1[] PROGMEM = "<!DOCTYPE html><html><link rel=\"stylesheet\" href=\"http://dev.dansaul.co.uk/colourpicker/css/colorpicker.css\" type=\"text/css\" /><link rel=\"stylesheet\" type=\"text/css\" href=\"http://dev.dansaul.co.uk/colourpicker/css/layout.css\" /><title>ColorPicker</title><script type=\"text/javascript\" src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.7.2/jquery.min.js\"></script><script type=\"text/javascript\" src=\"http://dev.dansaul.co.uk/colourpicker/js/colorpicker.js\"></script><script type=\"text/javascript\" src=\"http://dev.dansaul.co.uk/colourpicker/js/eye.js\"></script><script type=\"text/javascript\" src=\"http://dev.dansaul.co.uk/colourpicker/js/utils.js\"></script><meta name=\"viewport\" content=\"width=device-width\"/><script type=\"text/javascript\">var hexcolor;  $(document).ready(function(){$('#colorpickerHolder').ColorPicker({color: \"";

const prog_char la_html2[] PROGMEM = "\", flat: true, onChange: function(hsb, hex, rgb){hexcolor = hex; console.debug(hex);}});});</script><center><h1>'Really tall antenna with RGB LED on top'</h1><p id=\"colorpickerHolder\" ><br/><div id=\"submitbtn\"> <button onclick=\"$.get('/?color=%23' + hexcolor.toUpperCase())\">Set to this colour.</button></div><br/><button class=\"blink\" onclick=\"$.get('/?blink')\">I want to find my tent.</button><br/><button class=\"flash\" onclick=\"$.get('/?flash')\">Toggle Flash, Flash, Flash.</button><br/><br/><button class=\"turnoff\" onclick=\"$.get('/?off')\">Shut down & Explode.</button></p></center></html>";

// EEprom locations for the data (for permanent storage)
#define RED_EADDR 1
#define GREEN_EADDR 2
#define BLUE_EADDR 3

char linebuffer[128];    // a large buffer to store our data

// keep track of how many connections we've got
int requestNumber = 0;

// the xport!
#define XPORT_RX        A1
#define XPORT_TX        A2
#define XPORT_RESET     A0
#define XPORT_CTS       A3
#define XPORT_RTS       0 // not used
#define XPORT_DTR       0 // not used
AF_XPort xport = AF_XPort(XPORT_RX, XPORT_TX, XPORT_RESET, XPORT_DTR, XPORT_RTS, XPORT_CTS);

void setup() {
  lcd.begin(16, 2);
  lcd.print("Initialising...");
    
  Serial.begin(115200);
  Serial.println("Setup");
    
  xport.begin(9600);
  xport.reset();
  lcd.setCursor(0, 0);
  lcd.print("XPort ready ");
  
}

bool flash = false;

void loop() {
uint16_t ret;
  ret = requested();
  if (ret == 404) {
     xport.flush(250);
     // first the stuff for the web client
     xport.ROM_print(http_404header);    
     xport.disconnect();
  } else if (ret == 200) {
    changeLED();
    respond();
    
    pinMode(XPORT_RESET, HIGH);
    delay(50); 
    pinMode(XPORT_RESET, LOW);
    
  }

    if(flash){
        delay(250);
        analogWrite(10, 0);
        analogWrite(3, 0);
        analogWrite(11, 0);
        delay(1000);
        analogWrite(10, EEPROM.read(RED_EADDR));
        analogWrite(3, EEPROM.read(GREEN_EADDR));
        analogWrite(11, EEPROM.read(BLUE_EADDR));
    }

}

uint16_t requested(void) {
  uint8_t read, x;
  char *found;
  //Serial.println("Waiting for connection...");
  while (1) {
    read = xport.readline_timeout(linebuffer, 128, 200);
    //Serial.println(read, DEC);   // debugging output
    if (read == 0)     // nothing read (we timed out)
      return 0;
    if (read)          // we got something! 

   if (strstr(linebuffer, "GET / ")) {
      return 200;   // a valid request!
    }
   if (strstr(linebuffer, "GET /?")) { 
      return 200;   // a valid CGI request!
   }
   if(strstr(linebuffer, "GET ")) {
      return 404;    // some other file, which we dont have
   }
  }
}

// shortcut procedure just prints out the #xxxxxx values stored in EEPROM
void printLEDcolorHex(void) {
  uint8_t temp;
  
  xport.print('#');
  temp = EEPROM.read(RED_EADDR);
  if (temp <= 16)  
     xport.print('0'); // print a leading zero
  xport.print(temp , HEX);
  temp = EEPROM.read(GREEN_EADDR);
  if (temp <= 16)   
     xport.print('0'); // print a leading zero
  xport.print(temp , HEX);   
    temp = EEPROM.read(BLUE_EADDR);
  if (temp <= 16)   
     xport.print('0'); // print a leading zero
  xport.print(temp , HEX);
}

// read a Hex value and return the decimal equivalent
uint8_t parseHex(char c) {
    if (c < '0')
      return 0;
    if (c <= '9')
      return c - '0';
    if (c < 'A')
       return 0;
    if (c <= 'F')
       return (c - 'A')+10;
}


bool setrequest;

// check to see if we got a colorchange request
void changeLED(void) {
    char *found=0;
    uint8_t red, green, blue;

    setrequest = false;

    // Look for colour 
    found = strstr(linebuffer, "?color=%23"); // "?color=#" GET request
    Serial.println(linebuffer);
    if (found) {
      setrequest = true;
      found += 10;
      red = parseHex(found[0]) * 16 + parseHex(found[1]);
      found += 2;
      green = parseHex(found[0]) * 16 + parseHex(found[1]);
      found += 2;
      blue = parseHex(found[0]) * 16 + parseHex(found[1]);
      
      lcd.clear();
      lcd.print("Colour Received.");
      lcd.setCursor(0, 1);
      lcd.print("R");
      lcd.print(red);
      lcd.print(" G");
      lcd.print(green);
      lcd.print(" B");
      lcd.print(blue);
      
      EEPROM.write(RED_EADDR, red);  
      EEPROM.write(GREEN_EADDR, green);  
      EEPROM.write(BLUE_EADDR, blue);  
       
      analogWrite(10, red);   // Write current values to LED pins
      analogWrite(3, green); 
      analogWrite(11, blue);  
  }

    // Turn off
    found = strstr(linebuffer, "?off");
    if (found) {
      setrequest = true;

      analogWrite(10, 0);
      analogWrite(3, 0);
      analogWrite(11, 0);

      lcd.setCursor(0,0);
      lcd.print("Turned off.     ");

    }

    //Look for blink
    found = strstr(linebuffer, "?blink");
    if (found) {
        setrequest = true;
        lcd.setCursor(0,0);
        lcd.print("Blinking.       ");

        for(int i = 0; i < 20; i++){
        delay(100);
        lcd.setCursor(0,0);
        lcd.print("                ");
        analogWrite(10, 0);
        analogWrite(3, 0);
        analogWrite(11, 0);
        delay(100);
        lcd.setCursor(0,0);
        lcd.print("Blinking.       ");
        analogWrite(10, EEPROM.read(RED_EADDR));
        analogWrite(3, EEPROM.read(GREEN_EADDR));
        analogWrite(11, EEPROM.read(BLUE_EADDR));
        }
        lcd.setCursor(0,0);
        lcd.print("                ");

    }

    found = strstr(linebuffer, "?flash");
    if (found){
        setrequest = true;
        if(flash){
            flash = false;
            lcd.setCursor(0,0);
            lcd.print("Stop Flashing.  ");
        }else{
            flash = true;
            lcd.setCursor(0,0);
            lcd.print("Flashing.       ");
        }

    }

}


// Return the HTML place
void respond(){

    if(setrequest){
        return;
    }

  Serial.println("respond");
  
  uint8_t temp;
  xport.flush(50);
  // first the stuff for the web client
  xport.ROM_print(http_header);
  // next the start of the html header
  xport.ROM_print(la_html1);
  
  // the CSS code that will change the box to the right color when we first start
  printLEDcolorHex();

  xport.ROM_print(la_html2);

  // get rid of any other data left
  xport.flush(255);
  // disconnecting doesnt work on the xport direct by default? we will just use the timeout which works fine.
  xport.disconnect();
   
}


