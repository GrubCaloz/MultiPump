#include <Arduino.h>

#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_MCP23017.h>
#include <ezOutput.h>
#include <ssd1306.h>
#include <Ramp.h> 

#define  linespace (uint8_t)10

//NeoPixel
#define PIN 5 // Pin where NeoPixels are connected
#define NUMPIXELS 42
// Declare our NeoPixel strip object:
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
unsigned long PreviusMillisIN=0;
unsigned long PreviusMillisOUT=0;
uint8_t TimeStamp=10;

//Couleur Flow
#define PixelOff (uint32_t)pixels.Color(0,0,0)
#define PixelVert (uint32_t)pixels.Color(0,255,0)
#define PixelRouge (uint32_t)pixels.Color(255,0,0)
#define PixelStB (uint32_t)pixels.Color(167,0,139)
#define PixelEau (uint32_t)pixels.Color(0,0,255)
#define PixelIOAct (uint32_t)pixels.Color(250,200,17)
int progressBar=0;

//MCP
Adafruit_MCP23017 mcpTankIO; // S'occupe de boutons sur la partie Tank U11
Adafruit_MCP23017 mcpSUBD; // s'occupe des IO pour la Sub-d IN U10
Adafruit_MCP23017 mcpMCU; // s'occupe de la partie erreur et choix du programme IO second Sub-D U12

// output on arduino to Sub-D 25 with ezOutput
ezOutput OutQ1(23);  //Q1
ezOutput OutQ2(22);  //Q2
ezOutput OutQ3(21);  //Q3
ezOutput OutB10(20); //B10
ezOutput OutB20(18);  //B20
ezOutput OutB30(19); //B30
ezOutput OutB40(7); //B40
ezOutput OutB6(8);   //B6
ezOutput OutB7(9);   //B7
ezOutput OutB50(10); //B50
ezOutput OutB51(11); //B51
ezOutput OutStatu(12); //Systeme status  off système OFF, On système OK, Blink Défaut

ezOutput LEDInt(LED_BUILTIN);

// Gestion des réservoires
struct Tank{
  int TankID=1;
  uint8_t BPMax; // BP de test capteur Max
  uint8_t BPMin; // BPde test capteur Min
  uint8_t BPNivBas; // BP Niveau bas
  uint8_t BPNivHaut; // BP Niveau haut
  #define LEDMax (unit16_t)10 // LED capteur max
  #define LEDMin (uint16_t)13 // Led capteur min
  #define LEDNivBas (uint16_t)12// Led capteur niveau bas
  #define LEDNivHaut (uint16_t)11//Led capteur niveau haut
  const uint8_t ArrLedLevel[10]={0,1,2,3,4,5,6,7,8,9}; // Led de niveau
  int Level=0; // Niveau actuel
  const int MaxLevel=10000; // Niveau max
};

Tank Cuve;
bool Cuvevidable;

//structure pour le moteur
struct Motor{
  int MotorID;
  int heure;
  int regime;
  int debit;
  uint8_t Hplus;
  uint8_t State=0; //0 off, 1 starting, 2 On, 3 stopping
  uint8_t LedID;
  rampUnsignedInt RampeM;
  const int TimeStart=2500;
  uint8_t MotorPin;
  bool MotorPinStatu=false;
  bool Error=false;
  bool MotVer=false;
};

const uint8_t MBPCaptTestPin[4]={14,12,10,0};
const uint8_t MLedID[4]={33,29,25,17};
const uint8_t MRegimLEDID[4]={34,30,26,16};
const uint8_t MMotorPin[4]={8,9,10,11};
ezOutput MBregime[4]={OutB10,OutB20,OutB30,OutB40};
ezOutput OutMotVer[3]={OutQ1,OutQ2,OutQ3};
const uint8_t MotVerPin[3]={15,13,11};

Motor MPompe[4]{};
int QinTot;
int QoutTot;
#define MRegimeMax 1485

// Structur pour les vannes
struct Vanne{
  int VanneID;
  uint8_t State; //0 close, 1 openning, 2 open, 3 closeing
  uint8_t LEDNumber;
  uint8_t LedID;
  rampUnsignedInt RampeY;
  const int TimeStart=700;
  uint8_t VannePin;
  bool VannePinStatu=false;
};

Vanne Yx[10]{};
const uint8_t YLedID[10]={31,32,27,28,24,23,35,36,37,38};
const uint8_t YVannePin[10]={12,13,14,15,0,1};



//Capteur B0-3 BP IN
#define B0PinIN 1
#define B1PinIN 0
#define B2PinIN 6
#define B3PinIN 4

//Capteur B0-3 Out sur J2
#define B0PinOUT 7
bool B0Active;
#define B1PinOUT 6
bool B1Active;
#define B2PinOUT 5
bool B2Active;
#define B3PinOUT 4
bool B3Active;

//Capteur B6, 7, 50 ,51
#define B6PBPin 9
#define B7BPPin 8
#define B50BPPin 4
#define B51BPPin 5

const uint8_t LEDFlowIn[3]={20,21,22};
const uint8_t LEDFlowOut[3]={41,40,39};
byte FlowCunterIn;
byte FlowCunterOut;



//Ecriture du niveau du réservoir
void printlevel(){
  float levelpercent=((float)Cuve.Level/(float)Cuve.MaxLevel)*100;
  String StringPecent=String(levelpercent,1);
  char str[8];
  StringPecent.concat(" %    ");
  StringPecent.toCharArray(str,8);
  ssd1306_printFixed(75,  4*10, str, STYLE_NORMAL);
}
//régime moteur
void printMrpm(int motrpm, int line){
  String StringMoth=String(motrpm);
  char str[9];
  StringMoth.concat(" rpm   ");
  StringMoth.toCharArray(str,9);
  ssd1306_printFixed(30,  line, str, STYLE_NORMAL);
}

// heures moteur
void printMh(int moth, int line){
  String StringMoth=String(moth);
  char str[7];
  StringMoth.concat(" h   ");
  StringMoth.toCharArray(str,7);
  ssd1306_printFixed(90,  line, str, STYLE_NORMAL);
}

// heures moteur
void printQ(int Qinst, int line){
  float Qinstf=(float)Qinst/1000;
  String StringMoth=String(Qinstf,3);
  char str[12];
  StringMoth.concat(" m3/h    ");
  StringMoth.toCharArray(str,12);
  ssd1306_printFixed(45,  line, str, STYLE_NORMAL);
}

// Affichage de LED TOR
bool PixelONBool(bool Cond, uint8_t PixLed, uint32_t ActiveColor){
  bool PixelActive;
  if(Cond){
    pixels.setPixelColor(PixLed, ActiveColor);
  }else{
    pixels.setPixelColor(PixLed, PixelOff);
  }
  PixelActive=!pixels.getPixelColor(PixLed)==0;
  return PixelActive;
}

//======================================================================

void setup() {
  Serial.begin(115200);
//MCPs
    ssd1306_setFixedFont(ssd1306xled_font6x8);
    ssd1306_128x64_i2c_init();
    ssd1306_clearScreen();
   
    mcpTankIO.begin(); //U11
    mcpSUBD.begin(4); //U10
    mcpMCU.begin(6); //U12
    delay(10);

  for (byte i = 0; i < 16; i++)//mcpSubD et mcpMCU toutes en I
  {
    mcpSUBD.pinMode(i,INPUT);
    mcpMCU.pinMode(i,INPUT);
    mcpTankIO.pinMode(i,INPUT);
    mcpTankIO.pullUp(i,HIGH);
  }

  mcpMCU.pinMode(B0PinOUT,OUTPUT); //B0
  mcpMCU.pinMode(B1PinOUT,OUTPUT); //B1
  mcpMCU.pinMode(B2PinOUT,OUTPUT); //B2
  mcpMCU.pinMode(B3PinOUT,OUTPUT); //B3


  // cpteurs B0-3
  pinMode(B0PinIN,INPUT_PULLUP);
  pinMode(B1PinIN,INPUT_PULLUP);
  pinMode(B2PinIN,INPUT_PULLUP);
  pinMode(B3PinIN,INPUT_PULLUP);
  
//Start NeoPixels
  pixels.begin();
  pixels.setBrightness(11);
  for (byte i = 0; i < NUMPIXELS; i++)
  {
    pixels.setPixelColor(i, pixels.Color(167, 12, 69));
    pixels.show();
  }
  delay(100);
  pixels.clear();
  
//Vannes externes

for (byte i = 0; i < 10; i++)
{
  Yx[i].LedID=YLedID[i];
   if (i<6){
     Yx[i].VannePin=YVannePin[i];
   }
}

//Moteurs

  for (byte i = 0; i < 4; i++)
  {
    MPompe[i].LedID=MLedID[i];
    MPompe[i].MotorPin=MMotorPin[i];
  }

  LEDInt.blink(100,400);  

  ssd1306_printFixed(0,  0, "-M1:", STYLE_NORMAL);
  ssd1306_printFixed(0,  linespace, "-M2:", STYLE_NORMAL);
  ssd1306_printFixed(0,  2*linespace, "-M3:", STYLE_NORMAL);
  ssd1306_printFixed(0,  3*linespace, "-M4:", STYLE_NORMAL);
  ssd1306_printFixed(0,  4*linespace, "Niveau Tank:", STYLE_NORMAL);
  ssd1306_printFixed(0,  5*linespace, "Q In:", STYLE_NORMAL);
  ssd1306_printFixed(0,  6*linespace, "Q Out:", STYLE_NORMAL);  
  
}

//======================================================================
//======================================================================



void loop(){
  
 // Serial.println(FlowCunterIn);
  //EzOutput
  for (byte i = 0; i < 4; i++)
  {
    MBregime[i].loop();
  }
  
  LEDInt.loop();
  OutB50.loop();
  OutB51.loop();
  OutB6.loop();
  OutB7.loop();
  OutQ1.loop();
  OutQ2.loop();
  OutQ3.loop();
  OutStatu.loop();
  // Neopixel
  pixels.show();



  //Mise à jour du niveau tank
  for (byte i = 0; i < 10; i++)
  {
    PixelONBool((Cuve.Level/(Cuve.MaxLevel/10))>i,i,PixelEau);
  }
  printlevel();

// gestion et pilotage des pompes
//======================================================================================================================

  for (byte i = 0; i < 3; i++)
  {
    
    
  }
  

  for (byte i = 0; i < 4; i++){ 
  //Lecture du signal de démarrage
   if (i<3 && !mcpTankIO.digitalRead(MotVerPin[i])){
      MPompe[i].MotVer=true;
      OutMotVer[i].high();
    }else{
      OutMotVer[i].low();
      MPompe[i].MotVer=false;
    }
  
  MPompe[i].MotorPinStatu=mcpSUBD.digitalRead(MPompe[i].MotorPin) && !MPompe[i].MotVer;

  MPompe[i].Error=false;
    switch (MPompe[i].State)//Switch dans les modes
    {
    case 0: //Pompe Off
      if (MPompe[i].MotVer && i<3){
        PixelONBool(LEDInt.getState(),MPompe[i].LedID,PixelStB);
      }else{
        pixels.setPixelColor(MPompe[i].LedID,PixelRouge);
      }
      MPompe[i].regime=0;
      MPompe[i].RampeM.update();
      if (MPompe[i].MotorPinStatu)
      {
        MPompe[i].State=1;
        MPompe[i].RampeM.go(MRegimeMax,MPompe[i].TimeStart);
      }     
      break;
    case 1: // Pompe starting
      PixelONBool(LEDInt.getState(),MPompe[i].LedID,PixelIOAct);
      MPompe[i].regime=MPompe[i].RampeM.update();


      if (MPompe[i].RampeM.isFinished()){
        MPompe[i].State=2;
      }

      if (!MPompe[i].MotorPinStatu)
      {
        MPompe[i].State=3;
        MPompe[i].RampeM.go(0,MPompe[i].TimeStart);
      }
      break;
    case 2:// Pompe en service
      pixels.setPixelColor(MPompe[i].LedID,PixelVert);
      MPompe[i].RampeM.update();
      MPompe[i].regime=MRegimeMax +random(-2,2);
      MPompe[i].Hplus++;
      if (MPompe[i].heure<9999 && MPompe[i].Hplus>15)
      {
        MPompe[i].heure++; 
        MPompe[i].Hplus=0;
      }   
      if (!MPompe[i].MotorPinStatu)
      {
        MPompe[i].State=3;
        MPompe[i].RampeM.go(0,MPompe[i].TimeStart);
      }
      break;
    case 3: // Pompe stoping
      PixelONBool(LEDInt.getState(),MPompe[i].LedID,PixelIOAct);
      MPompe[i].regime=MPompe[i].RampeM.update();

      if (MPompe[i].RampeM.isFinished()){
        MPompe[i].State=0;
      }

      if (MPompe[i].MotorPinStatu)// motor stop
      {
        MPompe[i].State=2;
        MPompe[i].RampeM.go(MRegimeMax,MPompe[i].TimeStart);
      }
      break;
    default: // erreur
      PixelONBool(LEDInt.getState(),MPompe[i].LedID,PixelRouge);
      break;
    }
    // gestion des débits de pompe 1 à 3
    if (i<=2)
    {  
      if (Yx[2*i].State!=0 && Yx[(2*i)+1].State!=0){ //verification de l'ouverture des vannes
        MPompe[i].debit=MPompe[i].regime*4;  
      }else {
        MPompe[i].debit=0;
        if (MPompe[0].regime>0){
          MPompe[i].Error=true;
        }
    }

    if (Cuve.Level<=Cuve.MaxLevel && MPompe[i].debit>500)
    {
      Cuve.Level+=2;
    }
    }

    printMh(MPompe[i].heure, i*10);
    printMrpm(MPompe[i].regime,i*10);

    //Gestion capteur de régime
    if (!mcpTankIO.digitalRead(MBPCaptTestPin[i])){
      MBregime[i].high();
    }else{
      if (MPompe[i].regime>10)//Pompe 1
      {
        MBregime[i].blink(1500-MPompe[i].regime,60);
      } else{
        MBregime[i].low();
      }
    }

    PixelONBool(MBregime[i].getState(),MRegimLEDID[i],PixelIOAct);
  }

  //=========================================================
  //=========================================================

  for (byte i = 0; i < 10; i++){ // gestion et pilotage des vannes
    if (i<=5){
      Yx[i].VannePinStatu=mcpSUBD.digitalRead(Yx[i].VannePin); //Lecture du signal de démarrage
    }
    if (i==6){
      Yx[i].VannePinStatu=(mcpMCU.digitalRead(0)) || !mcpTankIO.digitalRead(3); // entrée sur J2 
    }
     if (i==7){
      Yx[i].VannePinStatu=(mcpMCU.digitalRead(1)) || !mcpTankIO.digitalRead(2); // entrée sur J2
    }
     if (i==8){
      Yx[i].VannePinStatu=(mcpMCU.digitalRead(2))|| !mcpTankIO.digitalRead(1); // entrée sur J2
    }
     if (i==9){
      Yx[i].VannePinStatu=(mcpMCU.digitalRead(3)) || !mcpTankIO.digitalRead(7); // entrée sur J2
    }
    

    switch (Yx[i].State)//Switch dans les modes
    {
    case 0: //vanne Off
      pixels.setPixelColor(Yx[i].LedID,PixelRouge);
      if (Yx[i].VannePinStatu)
      {
        Yx[i].State=1;
        Yx[i].RampeY.go(100,Yx[i].TimeStart);
      }     
      break;
    case 1: // vanne opening
      PixelONBool(LEDInt.getState(),Yx[i].LedID,PixelIOAct);
      Yx[i].RampeY.update();

      if (Yx[i].RampeY.isFinished()){
        Yx[i].State=2;
      }

      if (!Yx[i].VannePinStatu)
      {
        Yx[i].State=3;
        Yx[i].RampeY.go(0,Yx[i].TimeStart);
      }
      break;
    case 2:// Vane ouverte
      pixels.setPixelColor(Yx[i].LedID,PixelVert);
      if (!Yx[i].VannePinStatu)
      {
        Yx[i].State=3;
        Yx[i].RampeY.go(0,Yx[i].TimeStart);
      }
      break;
    case 3: // vanne closeing
      Yx[i].RampeY.update();
      PixelONBool(LEDInt.getState(),Yx[i].LedID,PixelIOAct);
      if (Yx[i].RampeY.isFinished()){
        Yx[i].State=0;
      }
      if (Yx[i].VannePinStatu)
      {
        Yx[i].State=2;
        Yx[i].RampeY.go(100,Yx[i].TimeStart);
      }
      break;
    default: // erreur
      PixelONBool(LEDInt.getState(),Yx[i].LedID,PixelRouge);
      break;
    }
  }  

//calcul du débit entrant
if (Cuve.Level<=Cuve.MaxLevel)
{
  QinTot=MPompe[0].debit+MPompe[1].debit+MPompe[2].debit;
} else{
  QinTot=0;
}

// calacul du débit sortant
if(Cuve.Level>50){
  Cuvevidable=true;
}

if (Cuve.Level<5){
  Cuvevidable=false;
}

QoutTot=0;
if (MPompe[3].State!=0 && Cuve.Level>1){
  if (Cuvevidable)
  {
    if (Yx[6].State==2){
      QoutTot=MPompe[3].regime*2;
      Cuve.Level--;
    } 
    if (Yx[7].State==2){
      QoutTot=QoutTot+(MPompe[3].regime*2);
      Cuve.Level--;
    } 
    if (Yx[8].State==2){
      QoutTot=QoutTot+(MPompe[3].regime*2);
      Cuve.Level--;
    } 
    if (Yx[9].State==2){
      QoutTot=QoutTot+(MPompe[3].regime*2);
      Cuve.Level--;
    } 
  }
}
if (Cuve.Level>0){
  if (Yx[6].State==2 || Yx[7].State==2 || Yx[8].State==2 || Yx[9].State==2){
    QoutTot=random(1,5);
    Cuve.Level--;
  }
}
printQ(QinTot,5*linespace);
printQ(QoutTot,6*linespace);

//Sondes B0-B3 de niveau
  //B0
  B0Active=pixels.getPixelColor(Cuve.ArrLedLevel[0])!=0 || !digitalRead(B0PinIN);
  mcpMCU.digitalWrite(B0PinIN,B0Active);
  PixelONBool(B0Active,13,PixelIOAct);

  //B1
  B1Active=pixels.getPixelColor(Cuve.ArrLedLevel[2])!=0 || !digitalRead(B1PinIN);
  mcpMCU.digitalWrite(B1PinIN,B1Active);
  PixelONBool(B1Active,12,PixelIOAct);

  //B2
  B2Active=pixels.getPixelColor(Cuve.ArrLedLevel[7])!=0 || !digitalRead(B2PinIN);
  mcpMCU.digitalWrite(B2PinIN,B2Active);
  PixelONBool(B2Active,11,PixelIOAct);

  //B3
  B3Active=pixels.getPixelColor(Cuve.ArrLedLevel[9])!=0 || !digitalRead(B3PinIN);
  mcpMCU.digitalWrite(B3PinIN,B3Active);
  PixelONBool(B3Active,10,PixelIOAct);


  if (QinTot>4000) //env 1485*3
  {
    TimeStamp=200;
  }
  if (QinTot>8500) // env 1485*3*2
  {
    TimeStamp=100;
  }
  if (QinTot>12000) // env 1485*3*3
  {
    TimeStamp=40;
  }

  //B6
  if (!mcpTankIO.digitalRead(B6PBPin)||QinTot>500){
    OutB6.high();    
  }else{
    OutB6.low();
  }

  //B7
  if (QinTot>500){
    OutB7.blink(TimeStamp,60);    
  }else{
    if (!mcpTankIO.digitalRead(B7BPPin)){
      OutB7.high();
    }else
    {
       OutB7.low();
    } 
  }

  PixelONBool(OutB6.getState(),18,PixelIOAct);
  PixelONBool(OutB7.getState(),19,PixelIOAct);

  //=======
  // Flow in
  if (millis()>PreviusMillisIN+TimeStamp && QinTot>10)
  {
    PreviusMillisIN=millis();
    FlowCunterIn++;
  }
  if (QinTot<10)
  {
    FlowCunterIn=0;
  }
  switch (FlowCunterIn)
  {
  case 0:
    pixels.setPixelColor(LEDFlowIn[0],PixelOff);
    pixels.setPixelColor(LEDFlowIn[1],PixelOff);
    pixels.setPixelColor(LEDFlowIn[2],PixelOff);
    break;
  case 1:
    pixels.setPixelColor(LEDFlowIn[0],PixelEau);
    pixels.setPixelColor(LEDFlowIn[1],PixelOff);
    pixels.setPixelColor(LEDFlowIn[2],PixelOff);
    break;
  
  case 2:
    pixels.setPixelColor(LEDFlowIn[0],PixelEau/2);
    pixels.setPixelColor(LEDFlowIn[1],PixelEau);
    pixels.setPixelColor(LEDFlowIn[2],PixelOff);
    break;
  
  case 3:
    pixels.setPixelColor(LEDFlowIn[0],PixelOff);
    pixels.setPixelColor(LEDFlowIn[1],PixelEau/2);
    pixels.setPixelColor(LEDFlowIn[2],PixelEau);
    break;
  
   case 4:
    pixels.setPixelColor(LEDFlowIn[0],PixelOff);
    pixels.setPixelColor(LEDFlowIn[1],PixelOff);
    pixels.setPixelColor(LEDFlowIn[2],PixelEau/2);
    FlowCunterIn=0;
    break;
  
  default:
    break;
  }

   // Flow Out

  if (QoutTot>2500){ //env 1485*3
    TimeStamp=220;
  }
  if (QoutTot>5800){ //env 1485*3
    TimeStamp=160;
  }
  if (QoutTot>8500){ // env 1485*3*2
    TimeStamp=100;
  }
  if (QoutTot>11500){ // env 1485*3*3
    TimeStamp=40;
  }

  if (millis()>PreviusMillisOUT+TimeStamp && QoutTot>10)
  {
    PreviusMillisOUT=millis();
    FlowCunterOut++;
  }
  if (QoutTot<10)
  {
    FlowCunterOut=0;
  }
  
  switch (FlowCunterOut)
  {
  case 0:
    pixels.setPixelColor(LEDFlowOut[0],PixelOff);
    pixels.setPixelColor(LEDFlowOut[1],PixelOff);
    pixels.setPixelColor(LEDFlowOut[2],PixelOff);
    break;
  case 1:
    pixels.setPixelColor(LEDFlowOut[0],PixelEau);
    pixels.setPixelColor(LEDFlowOut[1],PixelOff);
    pixels.setPixelColor(LEDFlowOut[2],PixelOff);
    break;
  
  case 2:
    pixels.setPixelColor(LEDFlowOut[0],PixelEau/2);
    pixels.setPixelColor(LEDFlowOut[1],PixelEau);
    pixels.setPixelColor(LEDFlowOut[2],PixelOff);
    break;
  
  case 3:
    pixels.setPixelColor(LEDFlowOut[0],PixelOff);
    pixels.setPixelColor(LEDFlowOut[1],PixelEau/2);
    pixels.setPixelColor(LEDFlowOut[2],PixelEau);
    break;
  
   case 4:
    pixels.setPixelColor(LEDFlowOut[0],PixelOff);
    pixels.setPixelColor(LEDFlowOut[1],PixelOff);
    pixels.setPixelColor(LEDFlowOut[2],PixelEau/2);
    FlowCunterOut=0;
    break;
  
  default:
    break;
  }

  //B50
  if (!mcpTankIO.digitalRead(B50BPPin)||QoutTot>500){
    OutB50.high();    
  }else{
    OutB50.low();
  }

  //B51
  if (QoutTot>500){
    OutB51.blink(TimeStamp,60);    
  }else{
    if (!mcpTankIO.digitalRead(B51BPPin)){
      OutB51.high();
    }else
    {
       OutB51.low();
    } 
  }

  PixelONBool(OutB50.getState(),14,PixelIOAct);
  PixelONBool(OutB51.getState(),15,PixelIOAct);
  
  
  //Surveillance du systhème
  if(MPompe[0].Error || MPompe[1].Error || MPompe[2].Error || MPompe[3].Error){
    OutStatu.blink(100,500);
  }else{
    OutStatu.high();
  }
}