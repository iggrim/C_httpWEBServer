#include "win_unix.h"
//

const char *get_content_type(const char* path) {
    const char *last_dot = strrchr(path, '.');
	/* Функция strchr ищет последнее вхождения символа,
	код которого указан в аргументе ch, в строке,
	на которую указывает аргумент str.*/
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".csv") == 0) return "text/csv";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }

    return "application/octet-stream";
}


SOCKET create_socket(const char* host, const char *port) {
    printf("Configuring local address...\n");
    struct addrinfo hints; // ----------struct addrinfo---|
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address; // ---struct addrinfo---

	/* getaddrinfo(...)
	 Нулевое значение может принимать либо параметр host, либо port,
	 но не оба одновременно. host указывает на сетевой адрес или в числовом виде
	 (десятично-точечный формат для IPv4, шестнадцатеричный для IPv6), или в виде
	 сетевого имени машины, поиск и разрешение адреса которой будут затем
	 произведены. Если поле ai_flags параметра hints содержит флаг AI_NUMERICHOST,
	 то параметр node должен быть числовым сетевым адресом.
	 Флаг AI_NUMERICHOST запрещает любой потенциально долгий поиск сетевого
	 адреса машины. */
    // Первый параметр — имя хоста или IP-адрес для соединения.
    // у нас host = 0 - это любой доступный интефейс
	getaddrinfo(host, port, &hints, &bind_address); 

    printf("Creating socket...\n");
    SOCKET socket_listen; // серверный сокет
    // socket_listen равен, например 3
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Binding socket to local address...\n");
	// int bind(int s, char * name, int namelen)
	// связать сокет с адресом и портом, полученным из getaddrinfo()
	// int bind(int sockfd, struct sockaddr *bind_address, int addrlen); 
	// bind_address это структура типа addrinfo (приводится к структуре sockaddr), 
	// а bind_address->ai_addr - это структура sockaddr одно из полей
    // в структуре addrinfo
	// struct sockaddr содержит  информацию об адресе - порт и IP address	
	// int bind(int s, char * name, int namelen)
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return socket_listen; // серверный сокет
}


#define MAX_REQUEST_SIZE 2047

/*структура client_info позволяет нам хранить информацию о каждом подключенном
 клиенте. Адрес клиента хранится в поле address, длина адреса - в address_length,
 а сокет - в поле socket. Все данные, полученные от клиента, хранятся в символьном
 массиве request;
 received указывает количество байтов, хранящихся в этом массиве.
 Поле next - это указатель, который  позволяет  нам хранить структуры client_info
 в связанном списке*/
struct client_info { // client_info - наша кастомная структура

	/*<sys/socket.h> делает доступным тип socklen_t, который является непрозрачным
	целым типом без знака длиной не менее 32 бит. Чтобы предотвратить проблемы с
	переносимостью, рекомендуется, чтобы приложения не использовали значения
	больше 2^32 - 1.
	<sys/socket.h> заголовок также определяет беззнаковое интегральный
	тип sa_family_t .*/
	socklen_t address_length;

    struct sockaddr_storage address; 
	// В структуре SOCKADDR_STORAGE хранится информация об адресе сокета IPv4
    // или IPv6. Будет приводится к типу struct sockaddr*
    SOCKET socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct client_info *next;
};
/* Поскольку массив является объектом с ---автоматическим классом хранения---, 
информация обычно размещается в стеке. Крупный массив такого рода требует области 
памяти приличного размера, что может вызвать проблемы. Если во время выполнения 
вы получаете сообщение об ошибке, уведомляющее о переполнении стека, то возможно, 
ваш компилятор, скорее всего, использует стандартный размер для стека, который 
слишком мал для этого примера. Чтобы исправить положение, вы можете с помощью опций 
компилятора установить размер стека в 10000, обеспечив достаточное место для 
данного массива структур, или же сделать массив статическим либо внешним
(тогда он не будет размещаться в стеке)*/


// будем хранить ---корень связанного списка--- в
// глобальной переменной clients. // client_info - наша кастомная структура
static struct client_info *clients = 0; 


/*get_client() принимает переменную SOCKET и ищет(клиента по сокету) в нашем
 связанном списке(clients) соответствующую структуру данных client_info
 если не находит, то создает под нового клиента пустую структуру
 ---т.к. s=-1 то здесь всегда создается новая структура---
 если s =-1, то -1 не является допустимым спецификатором сокета,
 поэтому get_client() создает новую структуру
 client_info.*/
// client_info - наша кастомная структура
struct client_info *get_client(SOCKET s) {
    // первоначально когда сработал сервреный сокет сюда передаем s равное -1
	// структура client_info определена чуть выше по коду
	// вначале адрес clients равен нулю

	// clients - глобальная переменная, определена выше по коду равна нулю
	// client_info - наша кастомная структура
	struct client_info *ci = clients;

	// если связанная структура clients была ранее создана для клиента то:
	while(ci) { // если ci не ноль

		// если в связанной структуре находим сокет какого-либо клента
        if (ci->socket == s)
            break;  // выходим из цикла и не создаем новую структуру
			
        ci = ci->next; // ci->next указывает на ноль
    }
	
	// здесь struct client_info *ci = clients всегда равна 0(ноль)
    if (ci) return ci; // возвращаем существующую структуру
	// но в ---для первого клиента--- ci->next всегда будет равно нулю,
	// поэтому ---всегда--- будет создаваться ---новая связаная--- структура.
	
    // если структура еще не создана(не ci), то под клиента создаем новую структуру
	// здесь выделяется новый адрес под структуру
    // client_info - наша кастомная структура
	struct client_info *n =   // например - 0x55555555a700
        (struct client_info*) calloc(1, sizeof(struct client_info));
		/* calloc выделяет память под массив данных, предварительно
		 инициализируя её нулями. принимает 2 аргумента.
		 malloc выделяет память без инициализации. принимает один аргумент.
		 calloc обнуляет выделенную память перед тем, как возвратить
		 указатель, а malloc этого не делает. Внутри calloc, наверняка
		 вызывает malloc для выделения памяти, а потом memset для
		 обнуления. Так что calloc это просто надстройка над malloc
		 для удобства. */
    if (!n) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    // определить размер n->address типа struct sockaddr_storage address;
    n->address_length = sizeof(n->address);
	// В структуре SOCKADDR_STORAGE хранится информация об адресе
	// сокета IPv4 или IPv6 . Будет приводится к типу struct sockaddr*
	/*В поле n->next устанавливается текущий корень глобального связанного списка, 
	а для корня глобального связанного списка, clients, устанавливается значение n. 
	Это решает задачу добавления ---новой структуры--- данных в ---начало--- 
	связанного списка(clients). как бы сдвигаем первого клиента вниз(или вправо)*/
    n->next = clients; 
	// n->next равен предыдущему адресу clients, первый раз это ноль
    // т.е. указатель n->next нового узла будет смотреть на предыдущий узел
    // в списке  // принцип LIFO
	
	clients = n; // присваиваем структуре clients адрес нового узла списка
    return n; // новый узел для клента
}


void drop_client(struct client_info *client) {
	// Все сокеты в системах Unix также являются стандартными файловыми дескрипторами.
	// По этой причине сокеты в системах Unix могут быть закрыты с помощью стандартной
	// функции close(). В Windows вместо этого используется специальная функция
	// закрытия - closesocket()
    CLOSESOCKET(client->socket);

    // **p - это адрес на адрес структуры client_info
    // client_info - наша кастомная структура
    struct client_info **p = &clients; // а нельзя сразу *p = clients ???

    while(*p) {
		// продвигаемся по p = &clients через *p = client->next и ищем client
        if (*p == client) {
        	// меняем адрес начала структуры client_info через глобальную
        	// переменную clients
            *p = client->next;
            
			/* Функция free() возвращает память, на которую указывает параметр
			ptr, назад в кучу. В резуль­тате эта память может выделяться снова.
			Обязательным условием использования функции free() является то, что 
			освобождаемая память должна была быть предварительно выделена с
			использованием одной из следующих функций: malloc(), realloc() или
			calloc(). Использование неверного указателя при вызове этой функции
			обычно ведет к разрушению механизма управления памятью и краху системы.*/
			free(client);
            return;
        }
        p = &(*p)->next;
    }

    fprintf(stderr, "drop_client not found.\n");
    exit(1);
}

// client_info - наша кастомная структура
const char *get_client_address(struct client_info *ci) {
    static char address_buffer[100];
    getnameinfo((struct sockaddr*)&ci->address,
            ci->address_length,
            address_buffer, sizeof(address_buffer), 0, 0,
            NI_NUMERICHOST);
    return address_buffer;
}


// если есть клиенты перебираем их для вычислени max_socket
fd_set wait_on_clients(SOCKET server) {
	/*Функция select оперирует структурами данных fd_set , представляющими собой 
	множества открытых файловых дескрипторов. Для обработки этих множеств 
	определен набор макросов.
		typedef struct fd_set {
			unsigned int count;
			int fd[FD_SETSIZE];
		} fd_set;
	fd_set - структура содержащая массив с индетификаторами сокетов. В Winsock, 
	значение элемента - сокет, в Unix - похоже индекс
	*/
    fd_set reads;  //fd_set тип

    // в цикле while(1) в main() каждый раз обнуляем структуру reads и заново
    FD_ZERO(&reads);

    // серверный сокет заносим в reads
    FD_SET(server, &reads); // 8 // макрос FD_SET

    SOCKET max_socket = server; // 3

    // clients изначально объявлена как static struct client_info *clients = 0;
    // далее в коде при создании структуры под клиента было присвоено - clients = n;
    // изначально присваиваем структуре client_info *ci  --нулевой адрес--
    // client_info - наша кастомная структура
    struct client_info *ci = clients;
    /* после создания структуры под клента, clients уже не равна нулю
	 struct client_info {
	 // <sys/socket.h> делает доступным тип socklen_t , который является
	 непрозрачным  целым типом без знака длиной не менее 32 бит. Чтобы
	 предотвратить проблемы с переносимостью, рекомендуется, чтобы приложения
	 не использовали значения больше  2^32 - 1.
     socklen_t address_length;
    struct sockaddr_storage address; 
	// В структуре SOCKADDR_STORAGE хранится информация об адресе сокета IPv4
	 или IPv6.
	// Будет приводится к типу struct sockaddr*
    SOCKET socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct client_info *next;
	};
	
	clients - это static struct client_info *clients = 0; 
	clients изначально инициализирован нулем. Чтобы упростить код,
	храним корень связанного списка в глобальной переменной clients.
	*/

    // если структура struct client_info *ci = clients не пустая
    // т.е. ранее уже выделили память под структуру и записали туда данные
    while(ci) {
    	// пробегаемся по структуре ci и заносим ci->socket( соект клиента)
    	// в reads 	// ci первоначально(для сервера) ноль, поэтому пропускаем
    	// ci применяем только для установки в fd_set reads
        FD_SET(ci->socket, &reads); // 24      if (ci->socket > max_socket)
            max_socket = ci->socket; // 4 // и для установки max_socket
        ci = ci->next; // равно ноль
    }
    // select блокирует выполнение кода ожидая когда сработает сокет
    // ранее записанный в структуру reads
    // если сокетов больше 10, то выходим
    if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    // для отладки нужно вывести номер сокета который сработал
    return reads; // 8  // первоначально сработал серверный сокет(3) в reads, 
	// затем клентский(4) возвращаем fd_set reads // 16 - клиентский сокет
}

// client_info - наша кастомная структура
void send_400(struct client_info *client) {
    const char *c400 = "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n\r\nBad Request";
    send(client->socket, c400, strlen(c400), 0);
    drop_client(client);
}

// client_info - наша кастомная структура
void send_404(struct client_info *client) {
    const char *c404 = "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n\r\nNot Found";
    send(client->socket, c404, strlen(c404), 0);
    drop_client(client);
}


// client_info - наша кастомная структура
void serve_resource(struct client_info *client, const char *path) {

    printf("serve_resource %s %s\n", get_client_address(client), path);
    // если пришел один слеш, то делаемp ath = "/index.html"
    if (strcmp(path, "/") == 0) path = "/index.html";

    if (strlen(path) > 100) {
        send_400(client);
        return;
    }

    if (strstr(path, "..")) { // защита
        send_404(client);
        return;
    }

    char full_path[128];
    sprintf(full_path, "public%s", path); 
	// пишем в массив full_path строку, например "public/page2.html"

#if defined(_WIN32)
    char *p = full_path;
    while (*p) {
        if (*p == '/') *p = '\\';
        ++p;
    }
#endif
    // открываем файл по маршруту full_path
    // "rb"	Открывает двоичный файл для чтения
    FILE *fp = fopen(full_path, "rb");

    if (!fp) {
        send_404(client);
        return;
    }

    fseek(fp, 0L, SEEK_END);
	/*int fseek(FILE *stream, long int offset, int whence); 	
	Чтобы небольшая константа интерпретировалась как значение типа 	
	long, к ней можно дописать букву l строчная буква L) или L. 
	
	Функция fseek( ) позволяет трактовать файл подобно массиву и переходить 
	непосредственно к любому байту в файле, открытом с помощью fopen().
	Функция fseek() устанавливает позицию в потоке данных, заданным аргументом
	stream. Относительно установленной позиции будет осуществляться чтение и
	запись данных.Точка отсчета устанавливаемой позиции определяется аргументом
	whence, который может принимать значения:
		 SEEK_SET – смещение отсчитывается от начала файла
         SEEK_CUR – смещение отсчитывается от текущей позиции
         SEEK_END – смещение отсчитывается от конца файла */
	
    size_t cl = ftell(fp); // cl для заголовка Content-Length (размер файла)
	/*Функция ftell определяет текущую позицию в потоке данных, на который 
	указывает аргумент stream.
	Для двоичных потоков данных позиция соответствует количеству байтов от 
	начала файла. Для текстовых потоков данных получаемое число может не 
	совпадать с количеством байтов от начала файла и должно использоваться 
	только функцией fseek() для установки потока данных в ту же позицию. */
    rewind(fp);

    const char *ct = get_content_type(full_path);  // ---------------------

#define BSIZE 1024
    char buffer[BSIZE];

    sprintf(buffer, "HTTP/1.1 200 OK\r\n"); // пишем строку в буфер
	
	// отсылаем заголовки(содержимое буфера) клиенту
	// int send(int s, char * buf, int len, int flags);
    send(client->socket, buffer, strlen(buffer), 0); 

    sprintf(buffer, "Connection: close\r\n");
	
	// отсылаем заголовки клиенту
	// int send(int s, char * buf, int len, int flags);
    send(client->socket, buffer, strlen(buffer), 0); 

    sprintf(buffer, "Content-Length: %u\r\n", cl);
	// отсылаем заголовки клиенту
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Type: %s\r\n", ct);
	// отсылаем заголовки клиенту
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "\r\n"); // конец строки
	// отсылаем заголовки клиенту
    send(client->socket, buffer, strlen(buffer), 0);

    // читаем содержимое файла(часть файла размером BSIZE) через открытый
    // дескриптор fp в буфер
	int r = fread(buffer, 1, BSIZE, fp); 

	// читаем и отправляем файл чанками
	while (r) { //
		// int send(int s, char * buf, int len, int flags);
		// пересылаем данные из файла клиенту,
		// браузер отобразил страницу пока без картинки
        send(client->socket, buffer, r, 0);
        r = fread(buffer, 1, BSIZE, fp);
        // т.к. буфер был меньше BSIZE поэтому прочитали за один раз выше по коду и
        // r равно нулю
		/*size_t fread(void *ptr, size_t size, size_t n, FILE * stream); 
		считывает n элементов данных,  каждый длиной  size  байтов, из
		потока  stream  в блок с адресной ссылкой ptr.
		*/
    }
	// Функция fclose() используется для закрытия потока, ранее открытого
	// с помощью fopen().
    fclose(fp);
    drop_client(client);
}



int main() {   //--------------------------------------------------------------------------

#if defined(_WIN32) // для винды
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif // для Unix
    // создаем серверный сокет - server // SOCKET socket_listen;
    // серверный сокет
    SOCKET server = create_socket(0, "8080"); // 0 - это любой доступный интефейс

    while(1) {

        fd_set reads; // «набор» файловых дескрипторов
		/*fd_set - структура содержащая массив с индетификаторами сокетов. 
		В Winsock, значение элемента - сокет, в Unix - похоже индекс*/

        // передаем серверный сокет для его прослушки в структуре данных fd_set
        // функцией select (оперирует структурами данных fd_set)
        // --select блокирует выполнение кода ожидая когда сработает сокет--
        // получаем заполненную структуру fd_set
        reads = wait_on_clients(server);   // ------------------

        // --если вернулся reads, то значит сработал какой-то сокет--

        // с помощью FD_ISSET можно проверить, были ли изменения по данному
        // сокету (проверять нужно в каждом массиве
        // если сработал серверный сокет сразу создаем структуру под клиента
        // вызывая get_client(-1)
		// (иначе, сработал клиентский сокет, тогда готовимся получать данные)
		if (FD_ISSET(server, &reads)) {
		  struct client_info *client = get_client(-1);                          //---------------------
/*-1 не является допустимым спецификатором сокета, поэтому
get_client() создает новую структуру client_info.
и адрес выделенной памяти под структуру client_info назначается
переменной  client


Ранее в коде была определена кастомная структура client_info:
struct client_info {
    socklen_t address_length;
    struct sockaddr_storage address;
    // В структуре SOCKADDR_STORAGE хранится информация
	   об адресе сокета IPv4 или IPv6. Будет приводится к типу struct sockaddr*

    SOCKET socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct client_info *next;
};
*/

/* функция accept(...) используется сервером для принятия связи на сокет.
 Сокет должен быть уже слушающим в момент вызова функции. Если сервер
 устанавливает связь с клиентом, то функция accept --возвращает новый
 сокет-дескриптор--, через который и происходит общение клиента с сервером.
 Пока устанавливается связь клиента с сервером, функция accept
 --блокирует-- другие запросы связи с данным сервером, а после установления
 связи "прослушивание" запросов возобновляется.
 -- accept создает новый сокет--- по запросу от клиента.
 Второй и третий аргументы заполняются соответствующими значениями
 в момент установления связи клиента с сервером и позволяют серверу
 точно определить, с каким именно клиентом он общается.
 Прототип: int accept(int s, char * name, int* anamelen);*/

		  	// описание функции accept(...) чуть выше по коду
            // Пишем сокет клиента в подготовленную структуру client->socket
            // client->socket = например 4, если сокет сервера = 3,
		    // затем второй клиент 5 и т.д
		    // struct client_info *client // client получили из get_client, = n
			client->socket = accept(server,
                    (struct sockaddr*) &(client->address),
                    &(client->address_length));
/*параметр client->address это структура типа sockaddr_storage содержащая 
адрес IPv4 или IPv6 --клиента---  приводится к типу struct sockaddr. 
sockaddr_storage, созданна достаточно большой, чтобы содержать обе IPv4 и IPv6
структуры. Судя по некоторым вызовам вы не знаете наперёд каким адресом
загружать вашу структуру sockaddr: IPv4 или IPv6. Передайте эту параллельную
структуру, подобную struct sockaddr, только больше, и приведите к нужному типу
исходный сокет продолжает прослушивать новые соединения, но новый сокет,
возвращаемый функцией accept(), может использоваться для отправки и получения
данных через вновь установленное соединение.
Структура SOCKADDR используется для хранения IP-адреса компьютера, участвующего
в обмене данными через сокеты Windows*/

            if (!ISVALIDSOCKET(client->socket)) {
                fprintf(stderr, "accept() failed. (%d)\n",
                        GETSOCKETERRNO());
                return 1;
            }

            printf("New connection from %s.\n",
                    get_client_address(client));
        } // end if if (FD_ISSET(server, &reads))
		
        // иначе ===сработал клиентский сокет===, готовимся получать данные

		// изначально static struct client_info *clients = 0;
		// далее по коду // присваиваем структуре clients
		// clients = n; адрес нового узла списка
		// присваиваем адрес структуре client заполненной функцией
		// accept(...)
		struct client_info *client = clients;
		/*ранее определили кастомную структуру
		struct client_info {
		socklen_t address_length;
		struct sockaddr_storage address;
		SOCKET socket;
		SSL *ssl;
		char request[MAX_REQUEST_SIZE + 1];
		int received;
		struct client_info *next;
		};
		и присвоили static struct client_info *clients = 0;
		*/
        
		// проходим по узлам связанного списка с данными сокета клиента
		// перебирем клентов в цикле, ищем клиента чей сокет сработал
		// продвигаемся по узлам через client->next, при client равным нулю
		// выходим
		while(client) {  // цикл while(client) в цикле while(1)
			// ранее struct client_info *client = clients;
			// *next = client->next - указатель на связанный список, если есть,
			// а если нет, то ноль
			// используется в конце цикла while(client)
			struct client_info *next = client->next;

            // перебирем клентов (*client = clients) в цикле, ищем клиента
			// (в связанном списке ) чей сокет сработал
			//
			if (FD_ISSET(client->socket, &reads)) { 

                if (MAX_REQUEST_SIZE == client->received) {// первый раз - ноль
                    send_400(client); 
					// Код состояния ответа "HTTP 400 Bad Request" 
					// указывает, что сервер не смог понять запрос из-за 
					// недействительного синтаксиса. Клиент не должен повторять
                    // этот запрос без изменений.
                    continue;
                }

                // int recv(int s, char * buf, int len, int flags); 
				//buf-адрес буфера, len - длинна буфера

                int r = recv(client->socket, // получаем данные от клиента
                	// меняем адрес конца буфера client->request для записи
                	// даных от клиента при повторных получениях данных
					client->request + client->received,
					/* client->request - это адрес буфера + длинна полученных
					данных(первый раз - ноль), готовим адрес для записи следующей
					порции запроса от клиента */
					// #define MAX_REQUEST_SIZE 2047
                    MAX_REQUEST_SIZE - client->received, 0);

                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n",
                            get_client_address(client));
                    drop_client(client);

                } else {
                	// инкремент, потому-что данные могут приходить чанками через
                	// интервалы  // пишем в стуктуру клиента для дальнейшего применения
                	// в recv(...)
                	client->received += r;
                    // request - это массив символов char request[MAX_REQUEST_SIZE + 1];
                    // добавили ноль в конец строки в массиве, обозначили одну строку
                    client->request[client->received] = 0;

					/*The web client must send a blank line after the HTTP request 
					header. This blank line is how the web server knows that the 
					HTTP request is finished*/
                    // strstr – поиск первого вхождения строки А в строку В
                    // т.е. конец переданных заголовков.
                    char *q = strstr(client->request, "\r\n\r\n");

                    // заголовок и тело HTTP очерчены пустой строкой, следовательно,
                    // если strstr() находит пустую строку (\ r \ n \ r \ n), мы знаем,
                    // что HTTP-заголовок получен, и можем начать его анализ.
                    if (q) {
                        *q = 0;

                        // strncmp сравнивает первые num символов строки string1
                        // с первыми num символами строки string2. У нас "GET /"
                        // - из 5 символов (char - 1 байт) в строке запроса
						// "GET / HTTP/1.1\r\nHost: localhost:8080\r\nUser-... " 
						// или например из строки запроса 
						// "GET /page2.html HTTP/1.1\r\nHost: localhost:8080\r\nUser-Agent
                        // strncmp - возвращает значение:
                        // 0 – если первые n символов сравниваемых строк идентичны.
						if (strncmp("GET /", client->request, 5)) {
                            send_400(client); // если "GET /" не равно client->request на пяти символах
                        } else { // если 0 то равны
                        	// к адресу начала структуры добавляем 4 символа (4 байта)
                            char *path = client->request + 4;
							/* находим то, что за GET - "/ HTTP/1.1\r\nHost: и т.д
							localhost:8080\r\nUser-Agent: ....." т.е. указатель на слэш - "/"
							или 
							например "/page2.html HTTP/1.1\r\nHost: localhost:80..." */

                            // находим первый слеш в строке *path
                            char *end_path = strstr(path, " ");
							/* strstr – поиск первого вхождения --строки А-- в строку В,
							возвращает указатель на первое вхождение строки A в строку B.
							не индекс а вхождение*/
                            if (!end_path) { 
							/* end_path это - " "(пробел за слешем) при запросе 
							"GET / HTTP/1.1\r\nHost: localhost:8080\r\nUser-Agent: Mozilla/5.0 
							или " "(пробел за "/page2.html")  при запросе 
							"GET /page2.html HTTP/1.1\r\nHost: localhost:80..." */
                                send_400(client);
                            } else {
                                *end_path = 0;
                                serve_resource(client, path); 
								/* path - 'это указатель на слэш - "/"
								в начале строки "/ HTTP/1.1\r\nHost:
								localhost:8080\r\nUser-Agent: ....." или например 
								/page2.html в строке "/page2.html HTTP/1.1\r\nHost: 
								localhost:80..."*/
                            }
                        }
                    } //if (q)
                }
            }
			// первоначльно сработал сервреный сокет, клиента обнуляем т.к.
			// поле next в структуре под клиента равно нулю
            client = next; // т.е. цикл работает пока client не будет равен нулю
        }

    } //while(1)


    printf("\nClosing socket...\n");
    CLOSESOCKET(server);


#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");
    return 0;
	
} // end main ------------------------------------------------------------------

/*
socket() создаёт конечную точку соединения и возвращает дескриптор. 
Прототип

#include <sys/types.h>
#include <sys/socket.h>
int socket(int domain, int type, int protocol);
Функция возвращает −1 в случае ошибки. Иначе, она возвращает целое число, 
представляющее присвоенный дескриптор.

socket() принимает три аргумента:
    domain, указывающий семейство протоколов создаваемого сокета. 
	        Этот параметр задает правила использования именования 
			и формат адреса. Например:
        	PF_INET для сетевого протокола IPv4 или PF_INET6 для IPv6.
        	PF_UNIX для локальных сокетов (используя файл).
    type (тип) один из:
        	SOCK_STREAM надёжная потокоориентированная служба (TCP) (сервис)
		                или потоковый сокет
        	SOCK_DGRAM служба датаграмм (UDP) или датаграммный сокет
        	SOCK_SEQPACKET надёжная служба последовательных пакетов
        	SOCK_RAW Сырой сокет — сырой протокол поверх сетевого уровня.
    protocol определяет используемый транспортный протокол. Самые 
	         распространённые — это IPPROTO_TCP, IPPROTO_SCTP, IPPROTO_UDP, 
			 IPPROTO_DCCP. Эти протоколы указаны в <netinet/in.h>. 
			 Значение «0» может быть использовано для выбора протокола по 
			 умолчанию из указанного семейства (domain) и типа (type).
*/
/*
int connect(int sockfd, struct sockaddr *serv_addr, int addrlen);

Здесь sockfd - сокет, который будет использоваться для обмена данными с сервером, 
serv_addr содержит указатель на структуру с адресом сервера, а addrlen - длину 
этой структуры. Обычно сокет не требуется предварительно привязывать(через bind) к локальному 
адресу, так как функция connect сделает это за вас, подобрав подходящий 
свободный порт. Вы можете принудительно назначить клиентскому сокету некоторый 
номер порта, используя bind перед вызовом connect. 
*/

/*
При установке или получении параметров сокета на IPPROTO_IP уровне приложения 
C/C++ Winsock, предназначенного для Windows, включение правильного заголовка и 
файла библиотеки в проект программы имеет решающее значение. Если файлы заголовка 
и библиотеки не совпадают должным образом setsockopt или getsockopt могут 
завершиться ошибкой выполнения 10042 ( WSAENOPROTOOPT). В некоторых случаях, даже 
если API возвращается успешно, значение параметра, которое вы устанавливаете или 
получаете, может не соответствовать вашим ожиданиям.

Чтобы избежать этой проблемы, соблюдайте следующие правила:

    Программа, которая включает, Winsock.h должна связываться только с Wsock32.lib.
    Программа , которая включает в себя Ws2tcpip.hможет связать либо с Ws2_32.lib или Wsock32.lib.

    Ws2tcpip.h должен быть явно включен после Winsock2.h, чтобы использовать 
	           параметры сокета на этом уровне.

*/

/*
struct addrinfo { 
  int              ai_flags;      // AI_PASSIVE, AI_CANONNAME,  т.д. 
  int              ai_family;     // AF_INET, AF_INET6, AF_UNSPEC 
  int              ai_socktype;   // SOCK_STREAM, SOCK_DGRAM 
  int              ai_protocol;   // используйте 0 для"any" 
  size_t           ai_addrlen;    // размер ai_addr в байтах  
  struct sockaddr  *ai_addr;      // struct sockaddr_in или _in6 
  char             *ai_canonname; // полное каноническое имя хоста 
  struct addrinfo  *ai_next;      // связанный список, следующий 
 }; 
*/

/*
  struct in_addr { 
  uint32_t  s_addr;  // Интернет адрес  // это 32-битный int (4 байта) 
};
*/

/*
В структуре SOCKADDR_STORAGE хранится информация об адресе сокета.
typedef struct sockaddr_storage {
  short   ss_family;  // Семейство ---адресов--- сокета, например AF_INET.
  char    __ss_pad1[_SS_PAD1SIZE];  // __ss_pad1 Зарезервировано. Определяется 
                                    как 48-битный блокнот, который гарантирует, 
									что SOCKADDR_STORAGE достигает 64-битного 
									выравнивания.

  __int64 __ss_align;  // Зарезервировано. Используется компилятором для 
                          выравнивания структуры
  char    __ss_pad2[_SS_PAD2SIZE];  // Зарезервировано. Используется компилятором 
                                       для выравнивания структуры
} SOCKADDR_STORAGE, *PSOCKADDR_STORAGE;

sockaddr_storage, созданна достаточно большой, чтобы содержать обе IPv4 и IPv6 
структуры. Судя по некоторым вызовам вы не знаете наперёд каким адресом  загружать 
вашу структуру  sockaddr: IPv4 или IPv6.Так передайте эту параллельную структуру, 
подобную struct sockaddr, только больше, и приведите к нужному типу.
*/

/*
Структура SOCKADDR используется для хранения IP-адреса компьютера, участвующего в 
обмене данными через сокеты Windows.
struct sockaddr {
    unsigned short sa_family;
    char           sa_data[14];
};
sa_family будет либо AF_INET (IPv4) или AF_INET6 
sa_data  содержит ----адрес назначения и номер порта---- для сокета.
*/


