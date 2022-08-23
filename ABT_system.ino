#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Firebase_Arduino_WiFiNINA.h>

//Wireless and Firebase INFO
#define DATABASE_URL "nameExample.location.firebasedatabase.app" 
#define DATABASE_SECRET "insertSecretKey" 
#define WIFI_SSID "insertWifiName"
#define WIFI_PASSWORD "insertWifiPassword"

FirebaseData fbdo;

#define RFTAG_MAX_LEN       4 
#define MAX_DATA_SIZE       16
#define MAX_AMT_SIZE        MAX_DATA_SIZE
#define FARE_PER_STOP       10 //You can change the fare per stop here
#define LAST_STOP           50
#define MIN_AMT_REQ         FARE_PER_STOP * LAST_STOP
#define MAX_AMT_PER_BYTE    255
#define INIT_AMOUNT         {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}

//Define PINS
#define LEDG 4//You can change the value
#define LEDR 5//You can change the value
#define RST_PIN 6//You can change the value           
#define SS_PIN 11//You can change the value          

typedef struct passenger {
    char rfTag[RFTAG_MAX_LEN];
    int startPoint;
    int endPoint;
    struct passenger *next;
} passenger_t;

struct rf_details {
    byte rfTag[4]; //Number of STOPS
    byte stop_no[1];
};

//STOP UIDs VALUE
const struct rf_details stop_rf[] = {
    {{0XE7,0X4F,0XE7,0X11},{1}},
    {{0XB3,0X26,0X42,0X1C},{2}},
    {{0X0C,0X2A,0XE9,0X2F},{3}},
    {{0XCC,0XA7,0X0D,0X30},{4}}
};

MFRC522 mfrc522(SS_PIN, RST_PIN);   
MFRC522::MIFARE_Key key;  

void print_balance(byte, byte);
unsigned char data_array[16] = {0};
unsigned int curr_output = 0;
unsigned int prev_output = 0;
unsigned int updated_balance = 0;

int cur_stop = 0;
static passenger_t *head = NULL;
static passenger_t *tail = NULL;
volatile byte bstop_card_ret = '\0';
byte b_busstop = '\0';

//The RFID TAG sector where you will read and write the value
byte sector         = 1; 
byte blockAddr      = 4;
byte dataBlock[]    = {
    0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff  
};
byte trailerBlock   = 7;

MFRC522::StatusCode status;
byte buffer[18];
byte size = sizeof(buffer);

int Cal_fare(int amt, int num_stop)
{
    return (amt - (num_stop * FARE_PER_STOP));
    // return num_stop;
}

int min_req_amt(int cur_stop)
{
    return (FARE_PER_STOP * (LAST_STOP - cur_stop));
}

void int_to_byte(int data, byte buffer[MAX_DATA_SIZE])
{
    int x = data / MAX_AMT_PER_BYTE;
    int y = data % MAX_AMT_PER_BYTE;
    int i = 0;
    memset(buffer, 0, MAX_DATA_SIZE);

    if (x == 0) {
        buffer[0] = y;
    } else {
        for (i = 0; i < x; i++) {  
            buffer[i] = 0xff;
        }
        buffer[x] = y;
    }
}

int byte_to_int(byte buffer[MAX_AMT_SIZE])
{
    int curr_output = 0;
    for(int i = 0; i < MAX_AMT_SIZE; i++) {
        curr_output = curr_output + (int) buffer[i];
    }
    return curr_output;
}

void display_list()
{
    passenger_t *list = head;
    while (list) {
        Serial.print("List");
        Serial.print(list->rfTag[0],HEX);
        Serial.print("\t");
        Serial.print(list->startPoint);
        Serial.println();
        list = list->next;
    }
}

struct passenger *search_tag(byte *rfTag)
{
    passenger_t *list = head;
    if (!rfTag)
        return NULL;
        
    while (list) {
        if (!memcmp(list->rfTag, rfTag, RFTAG_MAX_LEN))
            return list;
        list = list->next;
    }
    return NULL;
}

void delete_entry(byte *rfTag)
{
    passenger_t *list = head;
    passenger_t *prev = NULL;
    if (!rfTag)
        return;

    if (list && !memcmp(list->rfTag, rfTag, RFTAG_MAX_LEN)) {
        head = list->next;
        free(list);
        return;
    }

    while (list && memcmp(list->rfTag, rfTag, RFTAG_MAX_LEN)) {
        prev = list;
        list = list->next;
    }

    if (!list)
        return;
    prev->next = list->next;

    if (list == tail)
        tail = prev;
    free(list);
    Serial.println("Entry Deleted");
}

byte is_stop_card(byte tag[RFTAG_MAX_LEN])
{
    int i = 0;
    for (i = 0; i < 4; i++) {
        if (!memcmp(tag, stop_rf[i].rfTag, RFTAG_MAX_LEN))
        {
            return stop_rf[i].stop_no[0];
        }
    }
    return false;
}

int update_pass_list(byte *rfTag, int point, int amount)
{
#if 0
    byte sector         = 1;
    byte blockAddr      = 4;
    byte trailerBlock   = 7;
    MFRC522::StatusCode status;
    byte buffer[18];
    byte size = sizeof(buffer);
#endif
    passenger_t *pass = NULL;

#if 0
    if(!head)
    {
        Serial.print("Working");
        Serial.println();
    }
    else
    {
        Serial.print("Not Working");
        Serial.println();
    }
#endif

    if (!head) {
        if (amount < min_req_amt(point)) {
            Serial.print("Passenger does not have the required amount in the account!");
            Serial.println();
            return -1;
        }
        pass = (passenger_t *) malloc(sizeof(passenger_t));
        if (!pass) {
            printf("Error\n");
            exit (-EXIT_FAILURE);
        }
        head = tail = pass;
        memcpy(pass->rfTag, rfTag, RFTAG_MAX_LEN);
        pass->startPoint = point;
        pass->next = NULL;
        fprintf(stdout, "Passenger [%s] boarded at station [%d]\n", rfTag, point);
        Serial.println();
        Serial.print("Passenger boarded at station ");
        Serial.print(point);
        Serial.println();
        Serial.print("Balance: ");
        Serial.print(curr_output);
        Serial.println();
        Serial.println();
        Serial.print("------------------------------------------");

    } else {
        pass = search_tag(rfTag);
        if (pass) {
            byte data[MAX_DATA_SIZE] = { };
            byte* out = 0;
            int_to_byte(Cal_fare(amount, (point - pass->startPoint)), data);
            updated_balance = byte_to_int(data); 
#if 1
            status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
            if (status != MFRC522::STATUS_OK) {
                return -1;
            }

            status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
            if (status != MFRC522::STATUS_OK) {
                return -1;
            }
            dump_byte_array(buffer, 16); 

            status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(mfrc522.uid));
            if (status != MFRC522::STATUS_OK) {
                return -1;
            }

            dump_byte_array(data, 16); 
            status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockAddr, data, 16);
            if (status != MFRC522::STATUS_OK) {
                Serial.print(F("Authenticatation Failed: "));
                Serial.println(mfrc522.GetStatusCodeName(status));
            }
           
            status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
            if (status != MFRC522::STATUS_OK) {
                Serial.print(F("Authenticatation Failed: "));
                Serial.println(mfrc522.GetStatusCodeName(status));
            }
            dump_byte_array(buffer, 16);
        #endif

            updated_balance = byte_to_int(data_array);
            fprintf(stdout, "Passenger [%s] got off at station [%d]\n", rfTag, point);
            Serial.println();
            Serial.print("Passenger got off at station ");
            Serial.print(point);
            Serial.println();
            Serial.print("Updated Balance: ");
            Serial.print(updated_balance);
            Serial.println();
            Serial.println();
            Serial.print("Thank you for using our system!");
            Serial.println();
            delete_entry(rfTag);
        } else {
            if (amount < min_req_amt(point)) {
                Serial.print("Passenger does not have the required amount in the account!");
                Serial.println();
                return -1;
            }

            pass = (passenger_t *) malloc(sizeof(passenger_t));
            if (!pass) {
                printf("Error\n");
                exit (-EXIT_FAILURE);
            }
            tail->next = pass;

            memcpy(pass->rfTag, rfTag, RFTAG_MAX_LEN);
            pass->startPoint = point;
            pass->next = NULL;
            tail = tail->next; 
            fprintf(stdout, "Passenger [%s] boarded at station [%d]\n", rfTag, point);
            Serial.print("Passenger : ");
            Serial.print(rfTag[0]);
            Serial.println();
            Serial.print("Stop : ");
            Serial.print(point);
            Serial.println();
        }
    }
}

void setup() {
  pinMode(LEDG,OUTPUT);
  pinMode(LEDR,OUTPUT);
  Serial.begin(9600);
  delay(100);

  Serial.println();
  Serial.print("Connecting to Wi-fi");

  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED)
  {
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print(".");
    delay(100);
  }

  Serial.println();
  Serial.print("Succesfully Connected, IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.begin(DATABASE_URL, DATABASE_SECRET, WIFI_SSID, WIFI_PASSWORD);
  Firebase.reconnectWiFi(true);
  
  while (!Serial);    
  SPI.begin();        
  mfrc522.PCD_Init(); 

    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }
    dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);

    Serial.println(F("Account Based Ticketing System"));
    Serial.println(F("-----------------------------"));
    Serial.println();
}

void loop() {
    if ( ! mfrc522.PICC_IsNewCardPresent())
        return;

    if ( ! mfrc522.PICC_ReadCardSerial())
        return;

    bstop_card_ret = is_stop_card(mfrc522.uid.uidByte);

    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);

    if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
            &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
            &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        // Serial.println(F("The RFID module only works with MIFARE Classic tags."));
        return;
    }

    switch(bstop_card_ret)
    {
        case 1:
            b_busstop = bstop_card_ret;
            Serial.print("The bus is at the station 1");
            Serial.println();
            digitalWrite(LEDR,HIGH);
            delay(1500);
            digitalWrite(LEDR,LOW);   
            break;

        case 2:
            b_busstop = bstop_card_ret;
            Serial.print("The bus is at the station 2");
            Serial.println();
            digitalWrite(LEDR,HIGH);
            delay(1500);
            digitalWrite(LEDR,LOW);
            break;

        case 3:
            b_busstop = bstop_card_ret;
            Serial.print("The bus is at the station 3");
            Serial.println();
            digitalWrite(LEDR,HIGH);
            delay(1500);
            digitalWrite(LEDR,LOW);
            break;

        case 4:
            b_busstop = bstop_card_ret;
            Serial.print("The bus is at the station 4");
            Serial.println();
            digitalWrite(LEDR,HIGH);
            delay(1500);
            digitalWrite(LEDR,LOW);
            break;

        default:
            Serial.print("No stop tag detected");
            Serial.println();
            break;
    }

    if(bstop_card_ret==0)
    {   Serial.println();
        Serial.print("Used Card Tapped");
        Serial.println();

        status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
        if (status != MFRC522::STATUS_OK) {
            return;
        }

        status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
        if (status != MFRC522::STATUS_OK) {
        }
        
            
        dump_byte_array(buffer, 16); 
        print_balance(mfrc522.uid.uidByte, mfrc522.uid.size);
        update_pass_list(mfrc522.uid.uidByte,b_busstop,curr_output);

        Serial.println();
        Serial.print("Sending data to fireBase: ");
        digitalWrite(LEDG,HIGH);
        delay(1500);
        digitalWrite(LEDG,LOW);
        int FirebaseBalance = 0;
        if(updated_balance == 0) {
            FirebaseBalance = curr_output;
        } else {
            FirebaseBalance = updated_balance;
        }

        if (Firebase.setInt(fbdo, "/locationExample/Example/", FirebaseBalance))
        {
            Serial.println("SUCCES!");
            Serial.println();
            if (fbdo.dataType() == "int")
            fbdo.intData();
            if (fbdo.dataType() == "int64")
            fbdo.int64Data();
            if (fbdo.dataType() == "uint64")
            fbdo.uint64Data();
            else if (fbdo.dataType() == "double")
            fbdo.doubleData();
            else if (fbdo.dataType() == "float")
            fbdo.floatData();
            else if (fbdo.dataType() == "boolean")
            fbdo.boolData() == 1 ? "true" : "false";
            else if (fbdo.dataType() == "string")
            fbdo.stringData();
            else if (fbdo.dataType() == "json")
            fbdo.jsonData();
            else if (fbdo.dataType() == "array")
            fbdo.arrayData();
        }
        else
        {
            Serial.println("fireBase Error, " + fbdo.errorReason());
        }
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}

void dump_byte_array(byte *buffer, byte bufferSize) {

    unsigned long long b = 0;
    unsigned short int k, l = 0;

    for (byte i = 0; i < bufferSize; i++) {
        data_array[i] = buffer[i];    
    }
} 
void dump_byte_array_dec(byte *buffer, byte bufferSize) {

    for (byte j = 0; j < bufferSize; j++) {
        Serial.print(buffer[j] < 0x10 ? " 0" : " ");
        Serial.print(buffer[j], DEC);
    }
    Serial.print("\n");
} 

void print_balance(byte *buffer, byte bufferSize)
{
    unsigned long int person_name = 0;
    curr_output = 0;

    Serial.print("***********User Information:***********");
    Serial.println();
    Serial.print("UID :");
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
#if 1
    for (byte j = 0; j < 16; j++) {
        curr_output += data_array[j];
    }
#endif
    Serial.println();
}
