#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>



#include "../common/platform.h"
#include "../common/protocol.h"
#include "../common/util.h"
#include "../common/crc32.h"

typedef struct {
    int port;
    char outdir[1024];
    uint16_t window;
} Args;

typedef struct {
    char key[256];
    FILE* ofs;
    char filename[256];
    size_t expected;
    size_t total;
    size_t received;
    int active;
    char target_path[1024];
    uint32_t session_id;  // Unique session identifier
    uint64_t last_activity; // Track last activity time for cleanup
} Session;

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s [--port 9000] [--out ./server_data] [--window 8]\n", prog);
}

static int parse_args(int argc, char** argv, Args* a) {
    /* Initialize defaults */
    a->port = 9000;
    strcpy(a->outdir, "./server_data");
    a->window = 8;
    
    for (int i = 1; i < argc; i++) {
        char* s = argv[i];
        if (strcmp(s, "--port") == 0 && i+1 < argc) {
            a->port = atoi(argv[++i]);
        } else if (strcmp(s, "--out") == 0 && i+1 < argc) {
            strncpy(a->outdir, argv[++i], sizeof(a->outdir) - 1);
            a->outdir[sizeof(a->outdir) - 1] = '\0';
        } else if (strcmp(s, "--window") == 0 && i+1 < argc) {
            a->window = (uint16_t)atoi(argv[++i]);
        } else {
            usage(argv[0]);
            return 0;
        }
    }
    return 1;
}

static char* addr_key(const struct sockaddr_in* a) {
    static char key[256];
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &a->sin_addr, ip, sizeof(ip));
    snprintf(key, sizeof(key), "%s:%d", ip, ntohs(a->sin_port));
    return key;
}

static Session* find_session(Session* sessions, int* session_count, const char* key) {
    for (int i = 0; i < *session_count; i++) {
        if (strcmp(sessions[i].key, key) == 0) {
            return &sessions[i];
        }
    }
    return NULL;
}

static void remove_session(Session* sessions, int* session_count, int index) {
    if (index < *session_count - 1) {
        memmove(&sessions[index], &sessions[index + 1], 
                (*session_count - index - 1) * sizeof(Session));
    }
    (*session_count)--;
}

static void cleanup_old_session(Session* sessions, int* session_count, const char* key) {
    for (int i = 0; i < *session_count; i++) {
        if (strcmp(sessions[i].key, key) == 0) {
            // Close any open file
            if (sessions[i].ofs) {
                fclose(sessions[i].ofs);
                sessions[i].ofs = NULL;
            }
            // Remove the session
            remove_session(sessions, session_count, i);
            break;
        }
    }
}

static void cleanup_inactive_sessions(Session* sessions, int* session_count) {
    uint64_t now = ms_since(0);
    for (int i = *session_count - 1; i >= 0; i--) {
        // Clean up sessions that have been inactive for more than 30 seconds
        if (now - sessions[i].last_activity > 30000) {
            if (sessions[i].ofs) {
                fclose(sessions[i].ofs);
                sessions[i].ofs = NULL;
            }
            remove_session(sessions, session_count, i);
        }
    }
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
    
    MKDIR(args.outdir);

    SOCKET_TYPE sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET_TYPE) {
#ifdef _WIN32
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        WSACleanup();
#else
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
#endif
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(args.port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
#ifdef _WIN32
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
#else
        fprintf(stderr, "bind failed: %s\n", strerror(errno));
        CLOSE_SOCKET(sock);
#endif
        return 1;
    }

    /* Non-blocking */
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
        fprintf(stderr, "ioctlsocket failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "fcntl F_GETFL failed: %s\n", strerror(errno));
        CLOSE_SOCKET(sock);
        return 1;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "fcntl F_SETFL failed: %s\n", strerror(errno));
        CLOSE_SOCKET(sock);
        return 1;
    }
#endif

    Session sessions[100]; /* Max 100 concurrent sessions */
    memset(sessions, 0, sizeof(sessions));  // Initialize all sessions to zero
    int session_count = 0;
    uint64_t last_cleanup = ms_since(0);

    char* time_str = now_time();
    printf("[%s] Server listening on UDP %d\n", time_str, args.port);
    free(time_str);

    while (1) {
        uint8_t buf[64 * 1024];
        struct sockaddr_in from;
        int fromlen = sizeof(from);
        int n = recvfrom(sock, (char*)buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
        if (n <= 0) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                Sleep(5); // Windows equivalent of usleep(5000)
                // Periodic cleanup of inactive sessions
                uint64_t now = ms_since(0);
                if (now - last_cleanup > 10000) { // Every 10 seconds
                    cleanup_inactive_sessions(sessions, &session_count);
                    last_cleanup = now;
                }
                continue;
            }
            fprintf(stderr, "recvfrom failed: %d\n", error);
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(5000); // 5ms sleep
                // Periodic cleanup of inactive sessions
                uint64_t now = ms_since(0);
                if (now - last_cleanup > 10000) { // Every 10 seconds
                    cleanup_inactive_sessions(sessions, &session_count);
                    last_cleanup = now;
                }
                continue;
            }
            fprintf(stderr, "recvfrom failed: %s\n", strerror(errno));
#endif
            continue;
        }

        char* key = addr_key(&from);
        Packet p;
        if (unpack(buf, n, &p) == 0) {
            if (p.ptype == PT_HANDSHAKE) {
                /* Convert payload to string */
                char* meta = malloc(p.payload_size + 1);
                if (!meta) {
                    free_packet(&p);
                    continue;
                }
                memcpy(meta, p.payload, p.payload_size);
                meta[p.payload_size] = '\0';
                
                int parts_count;
                char** parts = split(meta, '|', &parts_count);
                if (!parts || parts_count < 5) {
                    Packet err;
                    memset(&err, 0, sizeof(err));
                    err.magic0 = 'R';
                    err.magic1 = 'U';
                    err.version = VERSION;
                    err.ptype = PT_ERROR;
                    const char* msg = "bad handshake";
                    err.payload = (uint8_t*)msg;
                    err.payload_size = strlen(msg);
                    
                    size_t eb_size;
                    uint8_t* eb = pack(&err, &eb_size);
                    if (eb) {
                        sendto(sock, (char*)eb, (int)eb_size, 0, (struct sockaddr*)&from, fromlen);
                        free(eb);
                    }
                    
                    if (parts) free_split_result(parts, parts_count);
                    free(meta);
                    free_packet(&p);
                    continue;
                }
                
                // Clean up any existing session for this client
                cleanup_old_session(sessions, &session_count, key);
                
                if (session_count >= 100) {
                    fprintf(stderr, "Too many sessions\n");
                    if (parts) free_split_result(parts, parts_count);
                    free(meta);
                    free_packet(&p);
                    continue;
                }
                
                Session* s = &sessions[session_count];
                memset(s, 0, sizeof(Session));  // Initialize all fields to zero
                strncpy(s->key, key, sizeof(s->key) - 1);
                s->key[sizeof(s->key) - 1] = '\0';
                strncpy(s->filename, parts[0], sizeof(s->filename) - 1);
                s->filename[sizeof(s->filename) - 1] = '\0';
                s->total = (size_t)atoll(parts[2]);
                s->expected = 0;
                s->received = 0;
                s->active = 1;
                s->last_activity = ms_since(0);
                s->session_id = (uint32_t)ms_since(0);  // Use timestamp as unique ID
                s->ofs = NULL;  // Explicitly set file handle to NULL
                
                // Create unique filename to avoid conflicts
                char unique_filename[512];
                snprintf(unique_filename, sizeof(unique_filename), "%s_%u_%s", 
                        s->filename, s->session_id, key);
                snprintf(s->target_path, sizeof(s->target_path), "%s/%s", args.outdir, unique_filename);
                
                s->ofs = fopen(s->target_path, "wb");
                if (!s->ofs) {
                    fprintf(stderr, "Failed to create file: %s\n", s->target_path);
                    if (parts) free_split_result(parts, parts_count);
                    free(meta);
                    free_packet(&p);
                    continue;
                }
                
                session_count++;

                Packet ack;
                memset(&ack, 0, sizeof(ack));
                ack.magic0 = 'R';
                ack.magic1 = 'U';
                ack.version = VERSION;
                ack.ptype = PT_HANDSHAKE_ACK;
                ack.total = s->total;
                ack.window = args.window;
                
                size_t b_size;
                uint8_t* b = pack(&ack, &b_size);
                if (b) {
                    sendto(sock, (char*)b, (int)b_size, 0, (struct sockaddr*)&from, fromlen);
                    free(b);
                }
                
                time_str = now_time();
                printf("[%s] %s handshake for %s total=%zu -> %s\n", 
                       time_str, s->key, s->filename, s->total, s->target_path);
                free(time_str);
                
                if (parts) free_split_result(parts, parts_count);
                free(meta);
            }
            else if (p.ptype == PT_DATA) {
                Session* s = find_session(sessions, &session_count, key);
                if (!s) {
                    Packet err;
                    memset(&err, 0, sizeof(err));
                    err.magic0 = 'R';
                    err.magic1 = 'U';
                    err.version = VERSION;
                    err.ptype = PT_ERROR;
                    const char* msg = "no session";
                    err.payload = (uint8_t*)msg;
                    err.payload_size = strlen(msg);
                    
                    size_t eb_size;
                    uint8_t* eb = pack(&err, &eb_size);
                    if (eb) {
                        sendto(sock, (char*)eb, (int)eb_size, 0, (struct sockaddr*)&from, fromlen);
                        free(eb);
                    }
                    free_packet(&p);
                    continue;
                }
                
                // Update activity time
                s->last_activity = ms_since(0);
                
                /* Validate checksum */
                uint32_t chk = ru_crc32(p.payload, p.payload_size);
                if (chk != p.checksum) {
                    /* drop corrupted packet, send cumulative ack for last in-order */
                    Packet ack;
                    memset(&ack, 0, sizeof(ack));
                    ack.magic0 = 'R';
                    ack.magic1 = 'U';
                    ack.version = VERSION;
                    ack.ptype = PT_ACK;
                    ack.seq = (s->expected > 0) ? s->expected - 1 : 0;  /* ACK the last in-order packet */
                    
                    size_t b_size;
                    uint8_t* b = pack(&ack, &b_size);
                    if (b) {
                        sendto(sock, (char*)b, (int)b_size, 0, (struct sockaddr*)&from, fromlen);
                        free(b);
                    }
                    free_packet(&p);
                    continue;
                }
                
                if (p.seq == s->expected) {
                    size_t written = fwrite(p.payload, 1, p.payload_size, s->ofs);
                    if (written != p.payload_size) {
                        fprintf(stderr, "Failed to write data: expected %zu, wrote %zu\n", 
                               p.payload_size, written);
                    }
                    s->expected++;
                    s->received++;
                    
                    // Try to write any buffered packets that are now in order
                    // For simplicity, we'll just write in-order packets as they arrive
                    // In a more sophisticated implementation, we'd buffer out-of-order packets
                }
                
                /* cumulative ACK for last in-order */
                Packet ack;
                memset(&ack, 0, sizeof(ack));
                ack.magic0 = 'R';
                ack.magic1 = 'U';
                ack.version = VERSION;
                ack.ptype = PT_ACK;
                ack.seq = (s->expected > 0) ? s->expected - 1 : 0;  /* ACK the last in-order packet */
                
                size_t b_size;
                uint8_t* b = pack(&ack, &b_size);
                if (b) {
                    sendto(sock, (char*)b, (int)b_size, 0, (struct sockaddr*)&from, fromlen);
                    free(b);
                }
            }
            else if (p.ptype == PT_FIN) {
                Session* s = find_session(sessions, &session_count, key);
                if (s) {
                    if (s->ofs) {
                        fflush(s->ofs);
                        fclose(s->ofs);
                        s->ofs = NULL;
                    }
                    
                    time_str = now_time();
                    printf("[%s] %s transfer complete %zu/%zu packets -> %s\n", 
                           time_str, s->key, s->received, s->total, s->target_path);
                    free(time_str);
                    
                    /* Find index and remove */
                    for (int i = 0; i < session_count; i++) {
                        if (&sessions[i] == s) {
                            remove_session(sessions, &session_count, i);
                            break;
                        }
                    }
                }
                
                Packet a;
                memset(&a, 0, sizeof(a));
                a.magic0 = 'R';
                a.magic1 = 'U';
                a.version = VERSION;
                a.ptype = PT_FIN_ACK;
                
                size_t b_size;
                uint8_t* b = pack(&a, &b_size);
                if (b) {
                    sendto(sock, (char*)b, (int)b_size, 0, (struct sockaddr*)&from, fromlen);
                    free(b);
                }
            }
            /* ignore others */
            
            free_packet(&p);
        } else {
            fprintf(stderr, "Failed to unpack packet\n");
        }
    }
    
    /* Cleanup */
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].ofs) {
            fclose(sessions[i].ofs);
        }
    }
    
    CLOSE_SOCKET(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
