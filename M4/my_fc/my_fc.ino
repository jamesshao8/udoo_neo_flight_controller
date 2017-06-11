#include <Wire.h>
#include "FXAS21002C.h"
#include <Servo.h>


#define M_pi 3.14159265359

FXAS21002C gyro = FXAS21002C(0x20); // SA0=1 0x21

double phi, theta, psi;

//phi = pitch 
//theta = roll 
//psi = yaw 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//PID gain and limit settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float pid_p_gain_roll = 1.3;               //Gain setting for the roll P-controller (1.3)
float pid_i_gain_roll = 0.05;              //Gain setting for the roll I-controller (0.3)
float pid_d_gain_roll = 15;                //Gain setting for the roll D-controller (15)
int pid_max_roll = 400;                    //Maximum output of the PID-controller (+/-)

float pid_p_gain_pitch = pid_p_gain_roll;  //Gain setting for the pitch P-controller.
float pid_i_gain_pitch = pid_i_gain_roll;  //Gain setting for the pitch I-controller.
float pid_d_gain_pitch = pid_d_gain_roll;  //Gain setting for the pitch D-controller.
int pid_max_pitch = pid_max_roll;          //Maximum output of the PID-controller (+/-)

float pid_p_gain_yaw = 4.0;                //Gain setting for the pitch P-controller. //4.0
float pid_i_gain_yaw = 0.02;               //Gain setting for the pitch I-controller. //0.02
float pid_d_gain_yaw = 0.0;                //Gain setting for the pitch D-controller.
int pid_max_yaw = 400;                     //Maximum output of the PID-controller (+/-)


//serial buffers
char byteRead;
char buffer[64];
volatile uint8_t buffptr = 0;
char csBuffer[3];
char tempBuffer[10];

// servo stuff
uint8_t esc0_pin = 4;
uint8_t esc1_pin = 5;
uint8_t esc2_pin = 6;
uint8_t esc3_pin = 7;


Servo esc0;
Servo esc1;
Servo esc2;
Servo esc3;


int minPulse = 1000;
int maxPulse = 2000;
int esc0_pulse = 1500;
int esc1_pulse = 1500;
int esc2_pulse = 1500;
int esc3_pulse = 1500;

int front_left = 1000;
int front_right = 1000;
int rear_left = 1000;
int rear_right = 1000;

double throttle = 0;

double target_pitch=0;
double target_roll=0;
double target_yaw=0;

double gyro_pitch, gyro_roll, gyro_yaw;

float pid_error_temp;
float pid_i_mem_roll, pid_roll_setpoint, gyro_roll_input, pid_output_roll, pid_last_roll_d_error;
float pid_i_mem_pitch, pid_pitch_setpoint, gyro_pitch_input, pid_output_pitch, pid_last_pitch_d_error;
float pid_i_mem_yaw, pid_yaw_setpoint, gyro_yaw_input, pid_output_yaw, pid_last_yaw_d_error;

uint32_t lastWrite,lastUpdate;

/**
 * Modes:
 * 0 = standby -> send no pulse or idle
 * 1 = calculate pulse length based on aircraft orientation
 * 2 = send pulse length set by the operator
*/
int mode = 0;
int checksum,checksumIn;

void setup()
{
  // initialize digital pin 13 as an output
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  Serial.begin(115200);
  Wire1.begin();

  // Initialize the FXAS21002C
  gyro.init();
  
  esc0.attach(esc0_pin);
  esc1.attach(esc1_pin);
  esc2.attach(esc2_pin);
  esc3.attach(esc3_pin);

  esc0.writeMicroseconds(minPulse);
  esc1.writeMicroseconds(minPulse);
  esc2.writeMicroseconds(minPulse);
  esc3.writeMicroseconds(minPulse);

  delay(10000); //10s delay
  // Gyro resolution?
  gyro.getGres();

  //calibrate gyro
  digitalWrite(13, HIGH);
  gyro.calibrate();
  digitalWrite(13, LOW);

  pid_i_mem_roll = 0;
  pid_last_roll_d_error = 0;
  pid_i_mem_pitch = 0;
  pid_last_pitch_d_error = 0;
  pid_i_mem_yaw = 0;
  pid_last_yaw_d_error = 0;
  
  lastWrite = micros();
  lastUpdate = micros();
}

void loop()
{
    //250hz update frequency
    if(micros() - lastUpdate >= 4000)
    {
      lastUpdate = micros();
      gyro_signalen();
      gyro_roll_input = (gyro_roll_input * 0.8) + (gyro_roll * 0.2);            //Gyro pid input is deg/sec.
      gyro_pitch_input = (gyro_pitch_input * 0.8) + (gyro_pitch * 0.2);         //Gyro pid input is deg/sec.
      gyro_yaw_input = (gyro_yaw_input * 0.8) + (gyro_yaw * 0.2); 

      pid_pitch_setpoint = target_pitch;
      pid_roll_setpoint = target_roll;
      pid_yaw_setpoint = target_yaw;

      calculate_pid();
      updateServo(); //update pwn signals
    }
    
    if(micros() - lastWrite >= 500000)
    {
      lastWrite = micros();

      Serial.print("$QCEUL,");
      Serial.print(gyro_pitch);
      Serial.print(",");
      Serial.print(gyro_roll);
      Serial.print(",");
      Serial.print(gyro_yaw);
      Serial.print(",");
      Serial.print(front_left);
      Serial.print(",");
      Serial.print(checksum);
      Serial.print(",");
      Serial.print(checksumIn);
      Serial.print(",");
      Serial.println("*00\n");
    }
    
    //Check serial port
    if (Serial.available())
    {
      //Serial.println("Reading");
      // read the most recent byte
      byteRead = Serial.read();
      if(byteRead == '\n')
      {
        decodeMessage();
        buffptr = 0;

        //reset buffers
        for(int8_t i=0;i<64;i++)
        {
          buffer[i] = 0;
          if(i<10)
          {
            tempBuffer[i] = 0;
          }
        }
      }
      else
      {
        buffer[buffptr % 64] = byteRead;
        buffptr++;
      }
    }
  
}



void updateServo()
{
  switch (mode){
    case 1:
    {
      front_right = throttle - pid_output_pitch + pid_output_roll - pid_output_yaw;
      rear_right = throttle + pid_output_pitch + pid_output_roll + pid_output_yaw;
      rear_left = throttle + pid_output_pitch - pid_output_roll - pid_output_yaw;
      front_left = throttle - pid_output_pitch - pid_output_roll + pid_output_yaw;


      esc0.writeMicroseconds(min(maxPulse,max(minPulse,front_right)));  //4
      esc1.writeMicroseconds(min(maxPulse,max(minPulse,rear_right)));   //5
      esc2.writeMicroseconds(min(maxPulse,max(minPulse,rear_left)));    //6
      esc3.writeMicroseconds(min(maxPulse,max(minPulse,front_left)));   //7
      break;
    }
    case 2:
    {
      //stop the quadcotper and reset
      pid_i_mem_roll = 0;
      pid_last_roll_d_error = 0;
      pid_i_mem_pitch = 0;
      pid_last_pitch_d_error = 0;
      pid_i_mem_yaw = 0;
      pid_last_yaw_d_error = 0;
          
      esc0.writeMicroseconds(esc0_pulse);
      esc1.writeMicroseconds(esc1_pulse);
      esc2.writeMicroseconds(esc2_pulse);
      esc3.writeMicroseconds(esc3_pulse);
      break;
    }
    default:
    {
      esc0.writeMicroseconds(1000);
      esc1.writeMicroseconds(1000);
      esc2.writeMicroseconds(1000);
      esc3.writeMicroseconds(1000);
      break;
    }
  }
}

void decodeMessage()
{
  //Serial.println("Decode messsage");
  //check for start char '$'
  int8_t startIndex = -1;
  int8_t stopIndex = -1;
  int8_t it;
  for(it=0;it<128;it++) {
    if(buffer[it]=='$') {startIndex = it; break;}
  }
  for(it=0;it<128;it++) {
    if(buffer[it]=='*') {stopIndex = it; break;}
  }
  if(128-stopIndex < 2) {
    return;
  }
  if(startIndex == -1 || stopIndex == -1) {
    return;
  }
  csBuffer[0] = buffer[stopIndex+1];
  csBuffer[1] = buffer[stopIndex+2];
  csBuffer[2] = buffer[stopIndex+3];
  checksumIn = (int)strtod(csBuffer,NULL);
  
  if(checksumIn !=0)
  {
    calculateChecksum(startIndex,stopIndex);
    if (checksumIn !=checksum)
    {
      return;
    }
  }

  /**Decode messages*/

  /**Set Target Angles*/
  /*"$QCSTA,roll,pitch,yaw,*CS"*/
  if (strncmp(buffer+startIndex+1,"QCSTA",5)==0) {
    //Serial.println("Set Target Angles message");

    //find 1:st roll
    startIndex+=7;
    for(it=startIndex;it<=stopIndex;it++) {
      if(buffer[it] == ',' || buffer[it] == '*') {
        strncpy (tempBuffer, buffer+startIndex, it-startIndex );
        double roll_tmp = max(0,min(20,strtod(tempBuffer,NULL)));
        target_roll = (roll_tmp - 10)*10;
        break;
      }
    }

    //find 2:nd pitch
    startIndex = it+1;
    for(it=startIndex;it<=stopIndex;it++) {
      if(buffer[it] == ',' || buffer[it] == '*') {
        strncpy (tempBuffer, buffer+startIndex, it-startIndex );
        double pitch_tmp = max(0,min(20,strtod(tempBuffer,NULL)));
        target_pitch = (pitch_tmp - 10)*10;
        break;
      }
    }

    //find 3:rd yaw
    startIndex = it+1;
    for(it=startIndex;it<=stopIndex;it++) {
      if(buffer[it] == ',' || buffer[it] == '*') {
        strncpy (tempBuffer, buffer+startIndex, it-startIndex );
        double yaw_tmp = max(0,min(20,strtod(tempBuffer,NULL)));
        target_yaw = (yaw_tmp - 10)*10;
        break;
      }
    }
    //find 4:th throttle
    startIndex = it+1;
    for(it=startIndex;it<=stopIndex;it++) {
      if(buffer[it] == ',' || buffer[it] == '*') {
        strncpy (tempBuffer, buffer+startIndex, it-startIndex );
        double thr_tmp =  max(0,min(100,strtod(tempBuffer,NULL)));       
        throttle = 1000 + thr_tmp*10;   
        
        break;
      }
    }

    mode = 1; //set mode: fly!
  }

  /**set pulse length*/
  /*"$QCPUL,pulse1,pulse2,pulse3,pulse4,*CS"*/
  if (strncmp(buffer+startIndex+1,"QCPUL",5)==0) {
    //Serial.println("Set pulse length message received");
  
    //find 1:st pulse
    startIndex+=7;
    for(it=startIndex;it<=stopIndex;it++) {
      if(buffer[it] == ',' || buffer[it] == '*') {
        strncpy (tempBuffer, buffer+startIndex, it-startIndex );
        esc0_pulse = max(minPulse,min(maxPulse,(int)strtol(tempBuffer,NULL,10)));
        break;
      }
    }

    //find 2:nd pulse
    startIndex = it+1;
    for(it=startIndex;it<=stopIndex;it++) {
      if(buffer[it] == ',' || buffer[it] == '*') {
        strncpy (tempBuffer, buffer+startIndex, it-startIndex );
        esc1_pulse = max(minPulse,min(maxPulse,(int)strtol(tempBuffer,NULL,10)));
        break;
      }
    }

    //find 3:rd pulse
    startIndex = it+1;
    for(it=startIndex;it<=stopIndex;it++) {
      if(buffer[it] == ',' || buffer[it] == '*') {
        strncpy (tempBuffer, buffer+startIndex, it-startIndex );
        esc2_pulse = max(minPulse,min(maxPulse,(int)strtol(tempBuffer,NULL,10)));
        break;
      }
    }

    //find 4:th pulse
    startIndex = it+1;
    for(it=startIndex;it<=stopIndex;it++) {
      if(buffer[it] == ',' || buffer[it] == '*') {
        strncpy (tempBuffer, buffer+startIndex, it-startIndex );
        esc3_pulse = max(minPulse,min(maxPulse,(int)strtol(tempBuffer,NULL,10)));
        break;
      }
    }
    mode = 2; //send these pulses
  }
}

int calculateChecksum(uint8_t start,uint8_t stop)
{
    int c = 0;
    for(uint8_t i=start;i<stop;i++)
    {
      c ^= buffer[i];
    }


    checksum = c;
    return c;
}

void gyro_signalen(){
  gyro.readGyroData();
  gyro_pitch = (M_pi/180)*gyro.gyroData.x*gyro.gRes;
  gyro_pitch *= -1;
  gyro_roll = (M_pi/180)*gyro.gyroData.y*gyro.gRes;
  gyro_roll *= -1;
  gyro_yaw = (M_pi/180)*gyro.gyroData.z*gyro.gRes;
  gyro_yaw *= -1;

}

void calculate_pid(){
  //Roll calculations
  pid_error_temp = gyro_roll_input - pid_roll_setpoint;
  pid_i_mem_roll += pid_i_gain_roll * pid_error_temp;
  if(pid_i_mem_roll > pid_max_roll)pid_i_mem_roll = pid_max_roll;
  else if(pid_i_mem_roll < pid_max_roll * -1)pid_i_mem_roll = pid_max_roll * -1;
  
  pid_output_roll = pid_p_gain_roll * pid_error_temp + pid_i_mem_roll + pid_d_gain_roll * (pid_error_temp - pid_last_roll_d_error);
  if(pid_output_roll > pid_max_roll)pid_output_roll = pid_max_roll;
  else if(pid_output_roll < pid_max_roll * -1)pid_output_roll = pid_max_roll * -1;
  
  pid_last_roll_d_error = pid_error_temp;
  
  //Pitch calculations
  pid_error_temp = gyro_pitch_input - pid_pitch_setpoint;
  pid_i_mem_pitch += pid_i_gain_pitch * pid_error_temp;
  if(pid_i_mem_pitch > pid_max_pitch)pid_i_mem_pitch = pid_max_pitch;
  else if(pid_i_mem_pitch < pid_max_pitch * -1)pid_i_mem_pitch = pid_max_pitch * -1;
  
  pid_output_pitch = pid_p_gain_pitch * pid_error_temp + pid_i_mem_pitch + pid_d_gain_pitch * (pid_error_temp - pid_last_pitch_d_error);
  if(pid_output_pitch > pid_max_pitch)pid_output_pitch = pid_max_pitch;
  else if(pid_output_pitch < pid_max_pitch * -1)pid_output_pitch = pid_max_pitch * -1;
    
  pid_last_pitch_d_error = pid_error_temp;
    
  //Yaw calculations
  pid_error_temp = gyro_yaw_input - pid_yaw_setpoint;
  pid_i_mem_yaw += pid_i_gain_yaw * pid_error_temp;
  if(pid_i_mem_yaw > pid_max_yaw)pid_i_mem_yaw = pid_max_yaw;
  else if(pid_i_mem_yaw < pid_max_yaw * -1)pid_i_mem_yaw = pid_max_yaw * -1;
  
  pid_output_yaw = pid_p_gain_yaw * pid_error_temp + pid_i_mem_yaw + pid_d_gain_yaw * (pid_error_temp - pid_last_yaw_d_error);
  if(pid_output_yaw > pid_max_yaw)pid_output_yaw = pid_max_yaw;
  else if(pid_output_yaw < pid_max_yaw * -1)pid_output_yaw = pid_max_yaw * -1;
    
  pid_last_yaw_d_error = pid_error_temp;
}


