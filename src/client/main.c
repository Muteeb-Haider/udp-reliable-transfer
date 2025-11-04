#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/platform.h"
#include "../common/protocol.h"
#include "../common/util.h"
#include "../common/crc32.h"



typedef struct {
    char host[256];
    int port;
    char file[1024];
    size_t chunk;
    uint16_t window;
    int timeout_ms;
    int max_retries;
} Args;

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s --host <host> --port <port> --file <path> "
            "[--chunk 1024] [--window 8] [--timeout 300] [--max-retries 20]\n", prog);
}

static int parse_args(int argc, char** argv, Args* args) {
    /* Initialize defaults */
    strcpy(args->host, "127.0.0.1");
    args->port = 9000;
    args->file[0] = '\0';
    args->chunk = 1024;
    args->window = 8;
    args->timeout_ms = 300;
    args->max_retries = 20;
    
    for (int i = 1; i < argc; i++) {
        char* a = argv[i];
        
        if (strcmp(a, "--host") == 0 && i+1 < argc) {
            strncpy(args->host, argv[++i], sizeof(args->host) - 1);
            args->host[sizeof(args->host) - 1] = '\0';
        } else if (strcmp(a, "--port") == 0 && i+1 < argc) {
            args->port = atoi(argv[++i]);
        } else if (strcmp(a, "--file") == 0 && i+1 < argc) {
            strncpy(args->file, argv[++i], sizeof(args->file) - 1);
            args->file[sizeof(args->file) - 1] = '\0';
        } else if (strcmp(a, "--chunk") == 0 && i+1 < argc) {
            args->chunk = (size_t)atol(argv[++i]);
        } else if (strcmp(a, "--window") == 0 && i+1 < argc) {
            args->window = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(a, "--timeout") == 0 && i+1 < argc) {
            args->timeout_ms = atoi(argv[++i]);
        } else if (strcmp(a, "--max-retries") == 0 && i+1 < argc) {
            args->max_retries = atoi(argv[++i]);
        } else if (strncmp(a, "--", 2) == 0) {
            fprintf(stderr, "Unknown flag: %s\n", a);
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", a);
            usage(argv[0]);
            return 0;
        }
    }
    
    if (strlen(args->file) == 0) {
        fprintf(stderr, "Missing required --file argument\n");
        usage(argv[0]);
        return 0;
    }
    return 1;
}

static void cleanup_resources(uint8_t** chunks, size_t total, uint8_t* data, 
                             SOCKET_TYPE sock, struct addrinfo* res) {
    if (chunks) {
        for (size_t i = 0; i < total; i++) {
            if (chunks[i]) {
                free(chunks[i]);
            }
        }
        free(chunks);
    }
    if (data) free(data);
    if (sock != INVALID_SOCKET_TYPE) CLOSE_SOCKET(sock);
    if (res) freeaddrinfo(res);
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", result);
        return 1;
    }
#endif

    Args args;
    if (!parse_args(argc, argv, &args)) {
#ifdef _WIN32



        WSACleanup();
#endif
        return 1;
    }

    /* Read file */
    FILE* fp = fopen(args.file, "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open file: %s\n", args.file);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (filesize < 0) {
        fprintf(stderr, "Error getting file size\n");
        fclose(fp);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    /* Calculate total packets */
    size_t total = (filesize + args.chunk - 1) / args.chunk;
    
    /* Read entire file */
    uint8_t* data = malloc(filesize);
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(fp);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    if (fread(data, 1, filesize, fp) != (size_t)filesize) {
        fprintf(stderr, "Failed to read file\n");
        free(data);
        fclose(fp);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    fclose(fp);
    
    /* Resolve host */
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", args.port);
    
    if (getaddrinfo(args.host, port_str, &hints, &res) != 0) {
        fprintf(stderr, "Failed to resolve host\n");
        free(data);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    SOCKET_TYPE sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET_TYPE) {
#ifdef _WIN32
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        freeaddrinfo(res);
        free(data);
        WSACleanup();
#else
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
        freeaddrinfo(res);
        free(data);
#endif
        return 1;
    }

    /* Non-blocking receive */
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
        fprintf(stderr, "ioctlsocket failed: %d\n", WSAGetLastError());
        closesocket(sock);
        freeaddrinfo(res);
        free(data);
        WSACleanup();
        return 1;
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "fcntl F_GETFL failed: %s\n", strerror(errno));
        CLOSE_SOCKET(sock);
        freeaddrinfo(res);
        free(data);
        return 1;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "fcntl F_SETFL failed: %s\n", strerror(errno));
        CLOSE_SOCKET(sock);
        freeaddrinfo(res);
        free(data);
        return 1;
    }
#endif

    char* time_str = now_time();
    printf("[%s] Client connecting to %s:%d sending %s (%ld bytes, %zu packets)\n",
           time_str, args.host, args.port, args.file, filesize, total);
    free(time_str);

    /* HANDSHAKE */
    Packet hs;
    memset(&hs, 0, sizeof(hs));
    hs.magic0 = 'R';
    hs.magic1 = 'U';
    hs.version = VERSION;
    hs.ptype = PT_HANDSHAKE;
    
    /* Extract filename */
    char* fname = strrchr(args.file, '/');
    if (!fname) fname = strrchr(args.file, '\\');
    if (!fname) fname = args.file;
    else fname++; /* Skip the separator */
    
    /* Create metadata string */
    char meta[1024];
    snprintf(meta, sizeof(meta), "%s|%ld|%zu|%zu|%d",
             fname, filesize, total, args.chunk, args.window);
    
    hs.payload_size = strlen(meta);
    hs.payload = (uint8_t*)meta;
    
    /* Note: meta is a stack variable, so we don't need to free it */
    
    size_t packed_size;
    uint8_t* buf = pack(&hs, &packed_size);
    if (!buf) {
        fprintf(stderr, "Failed to pack handshake\n");
        cleanup_resources(NULL, 0, data, sock, res);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    struct sockaddr_storage peer;
    memcpy(&peer, res->ai_addr, res->ai_addrlen);
    int peerlen = (int)res->ai_addrlen;

    int tries = 0;
    int hs_ackd = 0;
    while (tries < args.max_retries && !hs_ackd) {
        sendto(sock, (char*)buf, (int)packed_size, 0, (struct sockaddr*)&peer, peerlen);
        
        uint64_t t0 = ms_since(0);
        while (ms_since(t0) < (uint64_t)args.timeout_ms) {
            uint8_t rbuf[2048];
            struct sockaddr_storage from;
            int fromlen = sizeof(from);
            int rn = recvfrom(sock, (char*)rbuf, sizeof(rbuf), 0, (struct sockaddr*)&from, &fromlen);
            if (rn <= 0) {
#ifdef _WIN32
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    Sleep(5); // Windows equivalent of usleep(5000)
                    continue;
                }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(5000); // 5ms sleep
                    continue;
                }
#endif
                continue;
            }
            
            Packet p;
            if (unpack(rbuf, rn, &p) == 0) {
                if (p.ptype == PT_HANDSHAKE_ACK) {
                    hs_ackd = 1;
                    free_packet(&p);
                    break;
                }
                free_packet(&p);
            }
        }
        tries++;
    }
    
    free(buf);
    
    if (!hs_ackd) {
        fprintf(stderr, "Handshake failed\n");
        cleanup_resources(NULL, 0, data, sock, res);
#ifdef _WIN32
        WSACleanup();
#endif
        return 2;
    }
    
    time_str = now_time();
    printf("[%s] Handshake ACK received\n", time_str);
    free(time_str);

    /* Prepare chunks */
    uint8_t** chunks = malloc(total * sizeof(uint8_t*));
    size_t* chunk_sizes = malloc(total * sizeof(size_t));
    if (!chunks || !chunk_sizes) {
        fprintf(stderr, "Memory allocation failed\n");
        cleanup_resources(NULL, 0, data, sock, res);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    for (size_t i = 0; i < total; i++) {
        size_t off = i * args.chunk;
        size_t len = (off + args.chunk > (size_t)filesize) ? (size_t)filesize - off : args.chunk;
        chunks[i] = malloc(len);
        if (!chunks[i]) {
            fprintf(stderr, "Memory allocation failed\n");
            /* Cleanup */
            for (size_t j = 0; j < i; j++) {
                if (chunks[j]) {
                    free(chunks[j]);
                }
            }
            free(chunks);
            free(chunk_sizes);
            cleanup_resources(NULL, 0, data, sock, res);
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }
        memcpy(chunks[i], data + off, len);
        chunk_sizes[i] = len;
    }

    size_t base = 0;
    size_t nextseq = 0;
    int retries = 0;
    int timer_running = 0;
    uint64_t timer_t0 = 0;

    /* Main GBN send loop */
    while (base < total) {
        while (nextseq < total && nextseq < base + args.window) {
            /* Send data packet */
            Packet d;
            memset(&d, 0, sizeof(d));
            d.magic0 = 'R';
            d.magic1 = 'U';
            d.version = VERSION;
            d.ptype = PT_DATA;
            d.seq = nextseq;
            d.total = total;
            d.window = args.window;
            d.payload = chunks[nextseq];
            d.payload_size = chunk_sizes[nextseq];
            
            size_t d_packed_size;
            uint8_t* d_buf = pack(&d, &d_packed_size);
            if (d_buf) {
                sendto(sock, (char*)d_buf, (int)d_packed_size, 0, (struct sockaddr*)&peer, peerlen);
                free(d_buf);
            }
            
            if (base == nextseq && !timer_running) {
                timer_running = 1;
                timer_t0 = ms_since(0);
            }
            nextseq++;
        }

        /* Receive ACKs (non-blocking) */
        uint8_t rbuf[2048];
        struct sockaddr_storage from;
        int fromlen = sizeof(from);
        int rn = recvfrom(sock, (char*)rbuf, sizeof(rbuf), 0, (struct sockaddr*)&from, &fromlen);
        if (rn > 0) {
            Packet p;
            if (unpack(rbuf, rn, &p) == 0) {
                if (p.ptype == PT_ACK) {
                    if (p.seq >= base) {
                        /* Move base forward */
                        size_t old_base = base;
                        base = p.seq + 1;
                        
                        /* Free chunks we no longer need */
                        for (size_t s = old_base; s < base; s++) {
                            free(chunks[s]);
                            chunks[s] = NULL; /* Mark as freed */
                        }
                        
                        if (base == nextseq) {
                            timer_running = 0;
                        } else {
                            timer_running = 1;
                            timer_t0 = ms_since(0);
                        }
                    }
                }
                free_packet(&p);
            }
        }
        
        /* Check timeout */
        if (timer_running && ms_since(timer_t0) > (uint64_t)args.timeout_ms) {
            /* Timeout - retransmit from base */
            timer_running = 0;
            retries++;
            
            if (retries > args.max_retries) {
                fprintf(stderr, "Max retries exceeded\n");
                /* Cleanup */
                for (size_t i = 0; i < total; i++) {
                    if (chunks[i]) {
                        free(chunks[i]);
                    }
                }
                free(chunks);
                free(chunk_sizes);
                cleanup_resources(NULL, 0, data, sock, res);
#ifdef _WIN32
                WSACleanup();
#endif
                return 3;
            }
            
            /* Retransmit all packets in window */
            for (size_t s = base; s < nextseq; s++) {
                if (chunks[s]) {  // Only retransmit if chunk still exists
                    Packet d;
                    memset(&d, 0, sizeof(d));
                    d.magic0 = 'R';
                    d.magic1 = 'U';
                    d.version = VERSION;
                    d.ptype = PT_DATA;
                    d.seq = s;
                    d.total = total;
                    d.window = args.window;
                    d.payload = chunks[s];
                    d.payload_size = chunk_sizes[s];
                    
                    size_t d_packed_size;
                    uint8_t* d_buf = pack(&d, &d_packed_size);
                    if (d_buf) {
                        sendto(sock, (char*)d_buf, (int)d_packed_size, 0, (struct sockaddr*)&peer, peerlen);
                        free(d_buf);
                    }
                }
            }
            timer_running = 1;
            timer_t0 = ms_since(0);
        }
    }

    /* FIN */
    Packet fin;
    memset(&fin, 0, sizeof(fin));
    fin.magic0 = 'R';
    fin.magic1 = 'U';
    fin.version = VERSION;
    fin.ptype = PT_FIN;
    
    size_t fin_packed_size;
    uint8_t* bfin = pack(&fin, &fin_packed_size);
    if (!bfin) {
        fprintf(stderr, "Failed to pack FIN\n");
        /* Cleanup */
        for (size_t i = 0; i < total; i++) {
            if (chunks[i]) {
                free(chunks[i]);
            }
        }
        free(chunks);
        free(chunk_sizes);
        cleanup_resources(NULL, 0, data, sock, res);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    int fin_ok = 0;
    tries = 0;
    while (tries < args.max_retries && !fin_ok) {
        sendto(sock, (char*)bfin, (int)fin_packed_size, 0, (struct sockaddr*)&peer, peerlen);
        
        uint64_t t0 = ms_since(0);
        while (ms_since(t0) < (uint64_t)args.timeout_ms) {
            uint8_t rbuf2[2048];
            struct sockaddr_storage from2;
            int fromlen2 = sizeof(from2);
            int rn2 = recvfrom(sock, (char*)rbuf2, sizeof(rbuf2), 0, (struct sockaddr*)&from2, &fromlen2);
            if (rn2 <= 0) {
#ifdef _WIN32
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    Sleep(5); // Windows equivalent of usleep(5000)
                    continue;
                }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(5000); // 5ms sleep
                    continue;
                }
#endif
                continue;
            }
            
            Packet p;
            if (unpack(rbuf2, rn2, &p) == 0) {
                if (p.ptype == PT_FIN_ACK) {
                    fin_ok = 1;
                    free_packet(&p);
                    break;
                }
                free_packet(&p);
            }
        }
        tries++;
    }
    
    free(bfin);
    
    if (!fin_ok) {
        fprintf(stderr, "FIN not acknowledged\n");
        /* Cleanup */
        for (size_t i = 0; i < total; i++) {
            if (chunks[i]) {
                free(chunks[i]);
            }
        }
        free(chunks);
        free(chunk_sizes);
        cleanup_resources(NULL, 0, data, sock, res);
#ifdef _WIN32
        WSACleanup();
#endif
        return 4;
    }

    time_str = now_time();
    printf("[%s] Transfer complete (%zu packets)\n", time_str, total);
    free(time_str);
    
    /* Cleanup */
    for (size_t i = 0; i < total; i++) {
        if (chunks[i]) {
            free(chunks[i]);
        }
    }
    free(chunks);
    free(chunk_sizes);
    cleanup_resources(NULL, 0, data, sock, res);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
