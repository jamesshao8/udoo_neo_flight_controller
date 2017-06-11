#!/usr/bin/env python
import socket
import time
import pygame

start = 1
def main():
    global start
    address = ('192.168.3.18', 5555)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    pygame.init()
    pygame.joystick.init()
    W, H = 320, 240
    screen = pygame.display.set_mode((W, H))
    clock = pygame.time.Clock()
    running = True
    command_to_send=0
    command_last=0
    enable=0
   

    while running:
        joystick_count = pygame.joystick.get_count()
        #print joystick_count

        for i in range(joystick_count):
            joystick = pygame.joystick.Joystick(i)
            joystick.init()
    
            name = joystick.get_name()
            axes = joystick.get_numaxes()

            for i in range( axes ):
                axis = joystick.get_axis( i )
                #print "Axis",i,"value", axis
                if i==0:
                    roll = int(axis*(3.14/30)*100)+10
                if i==1:
                    pitch = int(axis*(3.14/30)*100)+10
                if i==2:
                    yaw = int(axis*(3.14/30)*100)+10
                if i==3:
                    throttle = int((-axis+1)/2*100)
            
            buttons = joystick.get_numbuttons()
            for i in range( buttons ):
                button = joystick.get_button( i )
                #print "Button",i, "value: ",button
                if i==4 and button==1:
                    enable=1 
                if i==9 and button==1:
                    enable=0
                    string="$QCPUL,1000,1000,1000,1000,*00\n"
                    sock.sendto(string,address)

            string_check="$QCSTA,"+str(roll)+","+str(pitch)+","+str(yaw)+","+str(throttle)+","+str(enable)+","
            string_check_list=list(string_check)
            checksum=0
            for item in string_check_list:
                checksum = checksum ^ ord(item)
            print checksum
            if checksum<100:
                checksum_str = '0'+str(checksum)
            else:
                checksum_str = str(checksum)
            #string="$QCSTA,"+str(roll)+","+str(pitch)+","+str(yaw)+","+str(throttle)+","+str(enable)+",*00\n"
            string="$QCSTA,"+str(roll)+","+str(pitch)+","+str(yaw)+","+str(throttle)+","+str(enable)+",*"+checksum_str+"\n"
            if throttle !=50:
                start = 2
                sock.sendto(string,address)
            else:
                if start == 2:
                    sock.sendto(string,address)
                else:
                    print "Sorry, Cant fly yet!"

  
            

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False 
         
      
              


        time.sleep(0.2)

    print "Ok."



if __name__ == '__main__':
    main()

