# Task
   **Задание 1**. 
   Необходимо реализовать сетевую программу (tcpclient.cpp), которая считывала бы все эти сообщения из текстового файла и передавала их на удаленный сервер, прослушивающий TCP порт. Протокол взаимодействия клиента и сервера указан в вашем варианте задания.  
      
   Адрес и порт сервера, а также имя входного файла указываются как аргументы командной строки при запуске клиента, например:  
    tcpclient 192.168.50.7:9000 file1.txt  
    где tcpclient - имя исполняемого бинарного файла клиента (получен путем компиляции tcpclient.cpp), 192.168.50.7 - IPv4 адрес сервера, 9000 - прослушиваемый сервером TCP-порт, file1.txt - файл с сообщениями.  
      
   **Примечания**:  
- Клиент должен быть реализован для ОС Windows, будет компилироваться и проверяться компилятором Microsoft Visual Studio 2010  
- Клиент может распечатывать некоторые отладочные сообщения в stdout, но этим не стоит злоупотреблять: большой stdout не будет сохраняться в результате теста. В stdout рекомендуется распечатывать только важные сообщения, в том числе об ошибках: подключения, неожиданного обрыва связи и т.д. Также stdout не сохраняется, если программа "зависла" и не завершается в течение отведенного ей разумного интервала времени - в этом случае ее аварийно останавливают и тест получает статус "timeout" без сохранения stdout.  
- Клиенту не стоит создавать какие-либо посторонние файлы в текущем каталоге, т.к. в тестах будет запускаться несколько экземпляров клиентов и попытки открыть файл с одним и тем же именем в разных экземплярах запущенной программы клинта может порождать конфликты доступа к одному и тому же файлу.  
- Если клиенту не удалось подключиться к серверу, то он должен подождать 100 мс и повторить попытку подключения. После 10 неудачных попыток клиент распечатывает ошибку в stdout и завершается.  
- Клиент подключается только в начале работы и использует установленное подключение для передачи всех сообщений из файла, т.е. НЕ переподключается к серверу для передачи каждого отдельного сообщения из входного файла.  
- Сокет клиента работает в блокирующем режиме.  
- Для отладки клиентской программы можно воспользоваться эмулятором сервера: tcpserveremul.rb (Эмулятор сервера написан на ruby, он же будет использоваться системой для тестов вашей клиентской программы. Для запуска сервера необходимо указывать номер прослушиваемого TCP-порта как первый аргумент, например: ruby tcpserveremul.rb 9000).  
- Если возникают проблемы с подключениями, то рекомендуется временно, на время разработки, отключить (или добавить используемые порты в исключения) Windows Firewall / iptables и прочих сетевых средств защиты, которые могут блокировать исходящие и входящие сетевые подключения.  
      
Строки исходного файла, считываемый клиентской программой, имеют следующий формат:        
    После успешного подключения клиент отправляет на сервер 3 байта: коды символов 'p', 'u' и 't'. Этим самым он сообщает серверу, что далее будут передаваться сообщения. Далее клиент передает на сервер сообщения, один за одним. Каждое сообщение из исходного файла клиент отправляет на сервер в следующем виде:  
      
- 4 байта - номер сообщения в исходном файле, целое 32-битное число в СЕТЕВОМ порядке байтов, индексация от 0.  
- 4 байта - значение UNIX-time, передается в СЕТЕВОМ порядке байтов;  
- 12 байт - номер телефона;  
- 4 байта - длина поля Message в символах, в СЕТЕВОМ порядке байтов,  
- N байт - символы самого поля Message.   

  На каждое принятое сообщение сервер присылает клиенту два байта: коды символов 'o' и 'k'. Это подтверждает успешный прием сообщения сервером. Клиент может и не дожидаться получения "ok" от сервера на каждое сообщение, а отправлять сообщения подряд, а уже после их отправки дождаться от сервера получения нескольких "ok" подряд. Их придет столько же, сколько сообщений отправил клиент.
  После передачи последнего сообщения клиент дожидается финального "ok", и завершается. Важно, что клиент НЕ ДОЛЖЕН завершиться раньше, чем получит от сервера подтверждение по последнему сообщению.
  Сервер завершается, если клиент отправит на сервер сообщение содержащее в поле Message лишь 4 байта - служебное слово "stop".  
      
    **Задание 2**.
    Необходимо реализовать на языке C/C++ сервер tcpserver.cpp, который можно было бы использовать вместо эмулятора (tcpserveremul.rb). Сервер должен прослушивать TCP-порт, принимать входящие подключения от клиентов, а затем - принимать сообщения, передаваемые клиентами. На каждое полученное сообщение сервер отправляет клиенту подтверждение "ok", согласно спецификации протокола. Все полученные сообщения сервер распечатывает в файл msg.txt. Каждое сообщение в msg.txt предваряется данными о клиенте: IP-адрес, двоеточие, порт клиента, пробел.  
    Далее следует сообщение, которое распечатывается в таком же виде, как и было во входном файле клиента. Если от какого-либо клиента пришло сообщение с текстом "stop", то сервер, после отправки этому клиенту подтверждения "ok", закрывает все открытые соединения (отключает всех клиентов) и завершает работу. Номер прослушиваемого порта передается серверу при запуске как первый аргумент командной строки, например: tcpserver 9000 где tcpserver - имя бинарного файла программы, 9000 - номер TCP порта для прослушивания.  
      
    **Примечания**:  
- Сокеты сервера работают в неблокирующем режиме.  
- Сервер является однопоточной программой. Сервер должен обслуживать несколько одновременно подключенных клиентов.  
- Механизм параллельного обслуживания: **poll**
- Сервер должен быть реализован для ОС Linux, будет компилироваться и проверяться компилятором g++ 5.x  
- Если при запуске серверу не удалось открыть TCP-порт для прослушивания, то сервер распечатывает сообщение об ошибке в stdout и завершается.  
- Сервер может, а вслучае ошибок - и должен, использовать stdout для вывода диагностической информации, но этим, как и в случае с клиентом, не стоит злоупотреблять.  
- Номер сообщения, используемый в протоколе, носит только служебный характер и в файле msg.txt не фиксируется.  

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


# Files
###    Format of answer in output file
 File of server response is named **"msg.txt"**
 
client_ip:port date time phone_number message_content

 **example**: 
 
 - [ ] some_ip:port 28.03.2016 11:19:23 +79163849299 msg1 
 - [ ] some_ip:port 21.03.2016 11:19:23 +79163849299 msg2

### Format of input file
File with information can have any name.

date time phone_number message_content

 **example**: 

 - [ ] 28.03.2016 11:19:23 +79163849299 msg1 
 - [ ] 21.03.2016 11:19:23 +79163849299 msg2
               

