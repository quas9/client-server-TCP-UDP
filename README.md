

# Testing programm
###    For starting **clients**:

C-programmed:

    ./tcpclient 127.0.0.1:port file_name.txt
    ./udpclient 127.0.0.1:port file_name.txt

Ruby-programmed:

    ruby ./tcpclient.rb 127.0.0.1:port file_name.txt
    ruby ./udpclient.rb 127.0.0.1:port file_name.txt
    
###    For starting **servers**:

C-programmed:

    ./tcpserver port
    ./udpclient port_start port_end

Ruby-programmed:

    ruby ./tcpclient.rb port
    ruby ./udpclient.rb port_start port_end


## Files

File with information can have any name. File of server response is named **"msg.txt"**


## Format of input file

date time phone_number message_content

 **example**: 

 - [ ] 28.03.2016 11:19:23 +79163849299 msg1 
 - [ ] 21.03.2016 11:19:23 +79163849299 msg2
               

