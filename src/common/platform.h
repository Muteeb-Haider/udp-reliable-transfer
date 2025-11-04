#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <direct.h>
    #include <sys/timeb.h>
    #include <winsock.h>
    #define SOCKET_TYPE SOCKET
    #define INVALID_SOCKET_TYPE INVALID_SOCKET
    #define CLOSE_SOCKET closesocket
    #define MKDIR(dir) _mkdir(dir)
    #define GET_TIME(tb) _ftime(tb)
    #define TIME_TYPE struct _timeb
    #define MILLISECONDS(tb) (tb.time * 1000 + tb.millitm)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <sys/time.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <errno.h>
    #define SOCKET_TYPE int
    #define INVALID_SOCKET_TYPE -1
    #define CLOSE_SOCKET close
    #define MKDIR(dir) mkdir(dir, 0755)
    #define GET_TIME(tb) gettimeofday(&tb, NULL)
    #define TIME_TYPE struct timeval
    #define MILLISECONDS(tb) (tb.tv_sec * 1000 + tb.tv_usec / 1000)
#endif

#endif /* PLATFORM_H */
