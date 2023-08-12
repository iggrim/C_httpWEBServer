# Простой WEB Server на Си

Создаем серверный сокет 
![Создаем серверный сокет](img/1_создаем_серверный_сокет.png)

Объявляем и инициализируем нашу структуру hints
![](img/2_создаем_структуру_hints.png)

![](img/3_инициализируем_структуру_hints.png)

Вызываем функцию __getaddrinfo(...)__ для преобразования доменного имени, имени хоста и IP-адреса из удобочитаеммого текстового представления в структурированный двоичный формат для сетевого API операционной системы
![](img/4_getaddrinfo.png)

Создаем серверный сокет.
![](img/5_socket.png)

