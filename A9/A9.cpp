#include "A9.hpp"
#include <iostream>
#include <string.h>
#include <time.h>

clock_t last_command_time=0;

A9::A9()
{
  std::cout << "A9 constructor" << std::endl;
  
    
  serialport_M4.setSerialPort("/dev/ttyMCC",115200);
  serialport_M4.AddObserver(*this);
  serialport_M4.start();

  //serialport_GPS.setSerialPort("/dev/ttymxc5",9600);
  //serialport_GPS.AddObserver(*this);
  //serialport_GPS.start();
  //serialport_GPS.send("$PMTK220,5000*1B\r\n"); // position data every 5s
  //serialport_GPS.send("$PMTK300,10000,0,0,0,0*2C\r\n"); //fix data every 10s
  //serialport_GPS.send("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"); //GPRMC and GGA
  //serialport_GPS.send("$PGCMD,33,0*6D\r\n"); //don't send antenna message

  network.AddObserver(*this);
  network.start(5555);
}
A9::~A9()
{
  std::cout << "A9 destructor" << std::endl;
}

void A9::update()
{
  if(serialport_M4.hasChanged())
  {
    std::cout << "From M4: " << serialport_M4.getLine() << std::endl;
  }
  if(serialport_GPS.hasChanged())
  {
    //std::cout << "From gps:" << serialport_GPS.getLine() << std::endl;
    gps.decodeMessage(serialport_GPS.getLine());
  }
    
  
    
  if(network.hasChanged())
  {
    clock_t current_command_time=clock();
    std::string s = network.getMessage();
    std::cout << "From Android: " << s <<"interval: "<<current_command_time-last_command_time << std::endl;
    
    
    //std::string toSend = "$QCPUL";
    //std::string toSend = "$QCSTA";
    //toSend += s.substr (0,20);
    //toSend += s.substr (0,18);
    //toSend += ",*00\n";
    //std::cout << "toSend: " << toSend << std::endl;
    //serialport_M4.send(toSend);
    if (current_command_time-last_command_time>100)
    {
        serialport_M4.send(s);
        last_command_time=current_command_time;
    }
    else
    {
        std::cout<<"ignored"<<std::endl;
    }
    
    //serialport_M4.send("$QCPUL,1100,1100,1100,1100,*00\n");

    //serialport_M4.send("$QCSTA,0.000,0.000,0.000,*00\n");
  }
}
