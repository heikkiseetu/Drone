/*/////////////////////////////////////////////////////////////////////////////////////
  Reference for gyro
  http://www.brokking.net/imu.html
///////////////////////////////////////////////////////////////////////////////////////
  Connections
///////////////////////////////////////////////////////////////////////////////////////
  VCC   -  5V
  GND   -  GND
  SDA   -  A4
  SCL   -  A5
                          MIN   MID   MAX FM values 
  Ch1   -  D2 Throttle    xx    xx    xx
  Ch2   -  D3 Roll        xx    xx    xx
  Ch3   -  D4 Pitch       xx    xx    xx 
  Ch4   -  D5 Yaw         xx    xx    xx
  LFesc -  D6
  RFesc -  D9
  LBesc -  D10
  RBesc -  D11
  LED   -  D13
/////////////////////////////////////////////////////////////////////////////////////*/
//-------------------------------//-----------------------------------------//
//Include I2C & servolibrary 
#include <Wire.h>
//-------------------------------//-----------------------------------------//
//Global variables
  // MPU 9255
  int gyroX, gyroY, gyroZ;
  long accX, accY, accZ, accTotalVector;
  long gyroXcal, gyroYcal, gyroZcal;
  unsigned long loopTimer;
  float anglePitchOutput, angleRollOutput;

// FM channels
  float Ch1, Ch2, Ch3, Ch4; // Primary channels
  //int Ch2Low  = 1000; 
  //int Ch3Low  = 1000; 
  //int Ch2Mid  = 1500; 
  //int Ch3Mid  = 1500;
  //int Ch2High = 2000;  
  //int Ch3High = 2000;
  
// ESC  
  float LFescOut, LBescOut, RFescOut, RBescOut; 
  
// PID constants
  unsigned long previousTime
  double controlX, controlY;
 
//-------------------------------//-----------------------------------------//

void setup() {
  Wire.begin();                                                     //Start I2C as master
  Serial.begin(57600);                                              //Use only for debugging
  pinMode(13, OUTPUT);                                              //Set output 13 (LED) as output
 // Pins for FM channels
  pinMode(1, INPUT);  
  pinMode(2, INPUT);  
  pinMode(3, INPUT);   
  pinMode(4, INPUT);   
//pinMode(pinCh5, INPUT);   // Additional
//pinMode(pinCh6, INPUT);   // Additional
//pinMode(pinCh7, INPUT);   // Additional

 // Pins for ESCs
  //int pinRFesc = 6;
  //int pinRBesc = 9; 
  //int pinLFesc = 10; 
  //int pinLBesc = 11; 
  pinMode(6, OUTPUT); 
  pinMode(9, OUTPUT); 
  pinMode(10, OUTPUT); 
  pinMode(11, OUTPUT);   
  
  setup_mpu_9255_registers();                                       //Setup the registers of the MPU-9255 (500dfs / +/-8g) and start the gyro

  digitalWrite(13, HIGH);                                           //Set digital output 13 high to indicate startup
  for (int cal_int = 0; cal_int < 2000 ; cal_int ++){               //Run this code 2000 times
   if(cal_int % 125 == 0)Serial.print(".");                         //Print a dot every 125 readings
    read_mpu_9255_data();                                           //Read the raw acc and gyro data from the MPU-9255
    gyroXcal += gyroX;                                              //Add the gyro x-axis offset to the gyroXcal variable
    gyroYcal += gyroY;                                              //Add the gyro y-axis offset to the gyroYcal variable
    gyroZcal += gyroZ;                                              //Add the gyro z-axis offset to the gyroZcal variable
    delay(3);                                                       //Delay 3us to simulate the 250Hz program loop
   }
  gyroXcal /= 2000;                                                 //Divide the gyro_x_cal variable by 2000 to get the avarage offset
  gyroYcal /= 2000;                                                 //Divide the gyro_y_cal variable by 2000 to get the avarage offset
  gyroZcal /= 2000;                                                 //Divide the gyro_z_cal variable by 2000 to get the avarage offset

  loopTimer = millis();                                             //Reset the loop timer
  previousTime = 0;                                                 //Setup PID timestamp
  digitalWrite(13, LOW);                                            //Indicate setup is done
}// End setup
//-------------------------------//-----------------------------------------//

////Main loop:
void loop(){
  
  read_mpu_9255_data();                                             //Read the raw acc and gyro data from the MPU-6050
  gyroDataProcessing();                                             //Process the raw data from gyro
  FMinput();                                                        //Read the FM signal
  PID();                                                            //Call the PID algorithm
  output();                                                         //Output to esc's
 
  // Serial prints to the monitor
  Serial.print("Pitch : ");
  Serial.print(anglePitchOutput);
  Serial.print("Roll : ");
  Serial.print(angleRollOutput);
  //Serial.print(" RFesc:");
  //Serial.print(RFesc);
  //Serial.print(" LFesc:");
  //Serial.println(LFesc);

  while(millis() - loopTimer < 8);                                   //Wait until the loop_timer reaches 4000us (250Hz) before starting the next loop
  loopTimer = millis();                                              //Reset the loop timer

}// End loop
//-------------------------------//-----------------------------------------//

/// FM signal from the transmitter 
void FMinput(){
  Ch1 = (pulseIn(2, HIGH, 25000));
  Ch2 = (pulseIn(3, HIGH, 250000));
  Ch3 = (pulseIn(4, HIGH, 2500000));
  Ch4 = (pulseIn(5, HIGH, 25000000));
//Ch5 =  pulseIn(6, HIGH, 250000000);   // Additional channel
//Ch6 =  pulseIn(7, HIGH, 2500000000);  // Additional channel
//Ch7 =  pulseIn(8, HIGH, 25000000000); // Additional channel
}// End FMinput
//-------------------------------//-----------------------------------------//
void gyroDataProcessing(){
  // Local variables
  float anglePitch, angleRoll;
  //boolean setGyroAngles;
  float angleRollAcc, anglePitchAcc;
  
  gyroX -= gyroXcal;                                                 //Subtract the offset calibration value from the raw gyro_x value
  gyroY -= gyroYcal;                                                 //Subtract the offset calibration value from the raw gyro_y value
  gyroZ -= gyroZcal;                                                 //Subtract the offset calibration value from the raw gyro_z value
  
  //Gyro angle calculations
  //0.000122137 = 1 / (125Hz / 65.5)
  anglePitch += gyroX * 0.000122137;                                 //Calculate the traveled pitch angle and add this to the angle_pitch variable 
  angleRoll  += gyroY * 0.000122137;                                 //Calculate the traveled roll angle and add this to the angle_roll variable 
  
  //0.000002132 = 0.000122137 * (3.142(PI) / 180degr) The Arduino sin function is in radians
  anglePitch += angleRoll * sin(gyroZ * 0.000002132);                //If the IMU has yawed transfer the roll angle to the pitch angel
  angleRoll -= anglePitch * sin(gyroZ * 0.000002132);                //If the IMU has yawed transfer the pitch angle to the roll angel
  
  //Accelerometer angle calculations
  accTotalVector = sqrt((accX*accX)+(accY*accY)+(accZ*accZ));        //Calculate the total accelerometer vector
  //57.296 = 1 / (3.142 / 180) The Arduino asin function is in radians
  anglePitchAcc = asin((float)accY/accTotalVector)* 57.296;          //Calculate the pitch angle
  angleRollAcc = asin((float)accX/accTotalVector)* -57.296;          //Calculate the roll angle
  
  //Place the MPU-6050 spirit level and note the values in the following two lines for calibration
  anglePitchAcc -= 0.0;                                              //Accelerometer calibration value for pitch
  angleRollAcc -= 0.0;                                               //Accelerometer calibration value for roll

  if(setGyroAngles){                                                 //If the IMU is already started
    anglePitch = anglePitch * 0.9996 + anglePitchAcc * 0.0004;       //Correct the drift of the gyro pitch angle with the accelerometer pitch angle
    angleRoll = angleRoll * 0.9996 + angleRollAcc * 0.0004;          //Correct the drift of the gyro roll angle with the accelerometer roll angle
   }
   else{                                                             //At first start
     anglePitch = anglePitchAcc;                                     //Set the gyro pitch angle equal to the accelerometer pitch angle 
     angleRoll = angleRollAcc;                                       //Set the gyro roll angle equal to the accelerometer roll angle 
     setGyroAngles = true;                                           //Set the IMU started flag
   }
  
  //Complementary filter:
  anglePitchOutput = anglePitchOutput * 0.9 + anglePitch * 0.1;      //Take 90% of the output pitch value and add 10% of the raw pitch value
  angleRollOutput = angleRollOutput * 0.9 + angleRoll * 0.1;         //Take 90% of the output roll value and add 10% of the raw roll value
  
}// End gyroDataProcessing
//-------------------------------//-----------------------------------------//
/// PID loop
void PID(){
  //Local variables
  int Kp = 1;
  int Ki = 1;
  int Kd = 1;
  double elapsedTime;
  double errorX, errorY, cumErrorX, cumErrorY, rateErrorX, rateErrorY;
  double lastErrorX, lastErrorY; 
  float setpointX, setpointY;
  unsigned long currentTime;
  int Ch2Low  = 1000; 
  int Ch3Low  = 1000; 
  int Ch2High = 2000;  
  int Ch3High = 2000;
  
  setpointX = map(Ch2, Ch2Low, Ch2High, -20, 20);                   //Setpoint between -20 and 20 degrees depending on the signal
  setpointY = map(Ch3, Ch3Low, Ch3High, -20, 20);                   //Setpoint between -20 and 20 degrees depending on the signal
  
  currentTime = loopTimer;                                          //Get current time
  elapsedTime = (double)(previousTime - currentTime);

  errorX = setpointX - angleRollOutput ;                            // X:Roll  Tarkista kanava ja akseli!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  errorY = setpointY - anglePitchOutput;                            // Y:Pitch Tarkista kanava ja akseli!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  cumErrorX += errorX* elapsedTime;                                 // Cumulative x-error for integrating term
  cumErrorY += errorY* elapsedTime;                                 // Cumulative y-error for integrating temr
  rateErrorX = (errorX - lastErrorX) / elapsedTime;                 // Rate of x-error for derivative term
  rateErrorY = (errorY - lastErrorY) / elapsedTime;                 // Rate of y-error for derivative term

  controlX = Kp*errorX + Ki*cumErrorX + Kd*rateErrorX;              // ControlX
  controlY = Kp*errorY + Ki*cumErrorY + Kd*rateErrorY;              // ControlY
 
  lastErrorX = errorX;                                              //Update x-axis error
  lastErrorY = errorY;                                              //Update y-axis error  
  
  previousTime = currentTime; //Time update
}// End PID
//-------------------------------//-----------------------------------------//

/// Outputs to ESC's
void output(){
  int Ch2Mid  = 1500; 
  int Ch3Mid  = 1500;
  float LFesc, LBesc, RFesc, RBesc; 
  //float LFescOut, LBescOut, RFescOut, RBescOut; 
  //int pinRFesc = 6;
  //int pinRBesc = 7;
  //int pinLFesc = 8;
  //int pinLBesc = 9;
  //Ch1 = base value
  
  if(Ch2 < Ch2Mid){  // Roll vasemmalle 
    RFesc = Ch1 + Ch1*controlX*0,1;
    RBesc = Ch1 + Ch1*controlX*0,1;  }

  if(Ch2 > Ch2Mid){ // Roll oikealle
    LFesc = Ch1 + Ch1*controlX*(-1)*0,1;
    LBesc = Ch1 + Ch1*controlX*(-1)*0,1; }

  if(Ch3 < Ch3Mid){  // Yaw ylös
    RFesc = Ch1 + Ch1*controlY*0,1;
    LFesc = Ch1 + Ch1*controlY*0,1; } 
  
  if(Ch3 > Ch3Mid){ // Yaw alas
    RBesc = Ch1 + Ch1*controlY*(-1)*0,1;
    LBesc = Ch1 + Ch1*controlY*(-1)*0,1; }

     

} // End output 
//-------------------------------//-----------------------------------------//

// Data from the MPU 9255
void read_mpu_9255_data(){                                           //Subroutine for reading the raw gyro and accelerometer data
  Wire.beginTransmission(0x68);                                      //Start communicating with the MPU-6050
  Wire.write(0x3B);                                                  //Send the requested starting register
  Wire.endTransmission();                                            //End the transmission
  Wire.requestFrom(0x68,16);                                         //Request 14 bytes from the MPU-6050
  while(Wire.available() < 16);                                      //Wait until all the bytes are received
  accX  = Wire.read()<<8|Wire.read();                                //Add the low and high byte to the acc_x variable
  accY  = Wire.read()<<8|Wire.read();                                //Add the low and high byte to the acc_y variable
  accZ  = Wire.read()<<8|Wire.read();                                //Add the low and high byte to the acc_z variable
  temp  = Wire.read()<<8|Wire.read();                                //Add the low and high byte to the temperature variable
  gyroX = Wire.read()<<8|Wire.read();                                //Add the low and high byte to the gyro_x variable
  gyroY = Wire.read()<<8|Wire.read();                                //Add the low and high byte to the gyro_y variable
  gyroZ = Wire.read()<<8|Wire.read();                                //Add the low and high byte to the gyro_z variable
} // End data read
//-------------------------------//-----------------------------------------//

// Configurations for the MPU 9255
void setup_mpu_9255_registers(){
  //Activate the MPU-9255
  Wire.beginTransmission(0x68);                                      //Start communicating with the MPU-9255
  Wire.write(0x6B);                                                  //Send the requested starting register
  Wire.write(0x00);                                                  //Set the requested starting register
  Wire.endTransmission();                                            //End the transmission
  //Configure the accelerometer (+/-8g)
  Wire.beginTransmission(0x68);                                      //Start communicating with the MPU-6050
  Wire.write(0x1C);                                                  //Send the requested starting register
  Wire.write(0x10);                                                  //Set the requested starting register
  Wire.endTransmission();                                            //End the transmission
  //Configure the gyro (500dps full scale)
  Wire.beginTransmission(0x68);                                      //Start communicating with the MPU-6050
  Wire.write(0x1B);                                                  //Send the requested starting register
  Wire.write(0x08);                                                  //Set the requested starting register
  Wire.endTransmission();                                            //End the transmission
} // End registers
//-------------------------------//-----------------------------------------//
