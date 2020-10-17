#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "Nextion.h"
#include <SoftwareSerial.h>

// Master RX, TX, connect to Nextion TX, RX
SoftwareSerial HMISerial(10, 11);
/*
  Arduino -> Diğer Cihaz
  10(Rx)  ->   Tx
  11(Tx)  ->   Rx
*/

#define pin_Hiz A2
#define pin_Yon A3
#define led_Reset 4
#define led_Set 5
#define setButton 3


/*Declare a number object [page id:0,component id:3, component name: "n0"].*/
NexNumber n0 = NexNumber(0, 1, "n0");
NexNumber n1 = NexNumber(0, 2, "n1");
NexNumber n2 = NexNumber(0, 1, "n2");
//NexNumber n3 = NexNumber(0, 6, "n3");


byte buff[6];
byte hiz = 0;
byte yon = 0;
byte ats = 0;
byte rst = 0;
byte algilamaHassasiyetiGoster = 1;
byte algilamaHassasiyeti = 1;
int algilamaHassasiyetiTimerGiris = 1;
bool isDataRcvd = false;
uint32_t lastRstInteruptTime = 0;
uint8_t rstCounter = 0;
boolean rstInProgress = false;
boolean rstLedState = false;
uint32_t HassasiyetSeviye = 0;
uint32_t lastSetInteruptTime = 0;
boolean setInProgress = false;
uint8_t setCounter = 0;
boolean setLedState = false;
int setButtonDurum = 0;
boolean ilkAtisGeldimi = false;




void setup() {
  cli();

  //external int0-Reset Kesmesi
  DDRD &= ~(1 << DDD2);
  PORTD |= (1 << PORTD2);
  EICRA |= (1 << ISC00);
  EICRA |= (1 << ISC01);
  EIMSK |= (1 << INT0);

  //external int1-Set Kesmesi
  /*DDRD &= ~(1 << DDD3);
    PORTD |= (1 << PORTD3);
    EICRA |= (1 << ISC10);
    EICRA |= (1 << ISC11);
    EIMSK |= (1 << INT1);*/

  //timer1 reset onay
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B |= (1 << WGM12);   //CTC mod
  TCCR1B |= (1 << CS12);    //256 precaler
  OCR1A = 6250;             //100 ms


  //timer0 set onay
  /*TCCR0A = 0;
    TCCR0B = 0;
    TCCR0A |= (1 << WGM01);//CTC Mode
    TCCR0B |= ((1 << CS02) | (1 << CS00));//1024 prescaler
    OCR0A = 254;//~16ms peiod*/

  sei();


  Serial.begin(9600);
  HMISerial.begin(9600);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(setButton, INPUT);

  nexInit();

}


void loop() {
  monitoring();
  listening();

  //Set Butonuna basıldığında girilecek blok
  setButtonDurum = digitalRead(setButton);
  if ((setButtonDurum == HIGH) && (ilkAtisGeldimi == true)) {
    n2.getValue(&HassasiyetSeviye);
    algilamaHassasiyeti = HassasiyetSeviye;
    rst = 22;
  }

  sending();
}

void monitoring() {
  n0.setValue(ats);
  n1.setValue(algilamaHassasiyetiGoster);
}


void listening() {
  byte data;
  isDataRcvd = false;
  //tamponu bosalt ve son veriyi kaydet
  while (Serial.available()) {
    data = Serial.read();
    isDataRcvd = true;
  }
  if (isDataRcvd) {
    if (data != 251 && data != 252) {
      if (data < 100) {
        ats = data;
        if (data > 0) {
          ilkAtisGeldimi = true;//Setleme işlemi giriş değişkeni
        }
      }
    }
    else if (data == 251) {
      //reset onayi alindi
      ats = 0;
    }
    else if (data == 252) {
      //set onayi alindi
      algilamaHassasiyetiGoster = algilamaHassasiyeti;
      rst = 0;
      //algilamaHassasiyetiTimerGiris = 0;
      //algilamaHassasiyeti = 0;
    }
    //ekrani guncelle
  }
}

void sending() {
  hiz = map(analogRead(pin_Hiz), 0, 1023, 0, 250);
  yon = map(analogRead(pin_Yon), 0, 1023, 250, 0);
  buff[0] = 254;
  buff[1] = hiz;
  buff[2] = yon;
  buff[3] = algilamaHassasiyeti;
  buff[4] = rst;
  buff[5] = 255;
  Serial.write(buff, 6);
  delay(50);
}

//int0 reset kesmesi
ISR (INT0_vect)
{
  if ((millis() - lastRstInteruptTime > 300) && !rstInProgress && (ats != 0)) {
    TIMSK1 |= (1 << OCIE1A);
    TCNT1 = 0;
    rst = 1;
    rstCounter = 0;
    rstInProgress = true;
    digitalWrite(led_Reset, LOW);
    lastRstInteruptTime = millis();
    //Serial.println("Reset icin interupt baslatiliyor ..");
  }
}

//int1 set kesmesi
/*ISR (INT1_vect) {
  if ((millis() - lastSetInteruptTime > 300) && !setInProgress) {
    TIMSK0 |= (1 << OCIE0A);
    TCNT0 = 0;
    //n2.getValue(&HassasiyetSeviye);
    rst = 22;
    //algilamaHassasiyeti = HassasiyetSeviye;
    algilamaHassasiyeti = 3;
    setCounter = 0;
    setInProgress = true;
    digitalWrite(led_Set, LOW);
    lastSetInteruptTime = millis();
  }
  }*/

//Timer1 reset onay timer
ISR(TIMER1_COMPA_vect) {
  rstCounter++;
  if (rstCounter < 10) {
    if (ats == 0) {
      //Serial.println("resetleme islemi basarili.");
      TIMSK1 &= ~(1 << OCIE1A);
      rstInProgress = false;
      digitalWrite(led_Reset, LOW);
      rst = 0;
    } else {
      //Serial.println("onay gelmedi");
      //henuz onay gelmedi led yakip sondur
      rstLedState = !rstLedState;
      digitalWrite(led_Reset, rstLedState);

    }
  } else {
    TIMSK1 &= ~(1 << OCIE1A);   //interrupt'i devre disi birak
    rstInProgress = false;
    digitalWrite(led_Reset, HIGH);
    rst = 0;
    //Serial.println("Reset onay sinyali alinamadi, islem basarisiz.");
  }


}

//Timer0 set onay timer
/*ISR(TIMER0_COMPA_vect) {
  TIFR0 |= (1 << OCF0A);
  setCounter++;
  if (setCounter < 62) {
    if (algilamaHassasiyeti == 0) {
      //Serial.println("setleme islemi basarili.");
      TIMSK0 &= ~(1 << OCIE0A);
      setInProgress = false;
      digitalWrite(led_Set, LOW);
      rst = 0;
    }
    else {
      //Serial.println("onay gelmedi");
      //henuz onay gelmedi led yakip sondur
      setLedState = !setLedState;
      digitalWrite(led_Set, setLedState);
    }
  }
  else {
    TIMSK0 &= ~(1 << OCIE0A);//interrupt'i devre disi birak
    setInProgress = false;
    digitalWrite(led_Set, HIGH);
    rst = 0;
    //Serial.println("Set onay sinyali alinamadi, islem basarisiz.");
  }

  }*/
