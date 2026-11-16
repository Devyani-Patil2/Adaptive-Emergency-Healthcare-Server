#include "common.h"
#include "thread_pool.h"
#include "database.h"
#include <signal.h>
#include <sys/types.h>

static volatile bool server_running = true;
static int server_socket_fd = -1;
static DynamicThreadPool *pool = NULL;

void handle_signal(int sig) {
    (void)sig;
    printf("\n[SERVER] Shutdown signal caught. Initiating clean termination...\n");
    server_running = false;
    if (server_socket_fd != -1) {
        shutdown(server_socket_fd, SHUT_RDWR);
        close(server_socket_fd);
        server_socket_fd = -1;
    }
}

/* Background thread to periodically display server and OS metrics */
void* monitor_routine(void *arg) {
    (void)arg;
    while (server_running) {
        sleep(3);
        if (pool && server_running) {
            thread_pool_print_stats(pool);
        }
    }
    return NULL;
}

/* Simple helper to parse query string / form-urlencoded key-value pairs or JSON */
static void extract_param(const char *src, const char *key, char *dest, size_t dest_len) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "%s=", key);
    const char *pos = strstr(src, search_key);
    if (!pos) {
        // Try JSON key: "key":"value" or "key":123
        snprintf(search_key, sizeof(search_key), "\"%s\"", key);
        pos = strstr(src, search_key);
        if (!pos) return;
        pos = strchr(pos, ':');
        if (!pos) return;
        pos++;
        while (*pos == ' ' || *pos == '\"') pos++;
        size_t i = 0;
        while (*pos && *pos != '\"' && *pos != ',' && *pos != '}' && *pos != '\r' && *pos != '\n' && i < dest_len - 1) {
            dest[i++] = *pos++;
        }
        dest[i] = '\0';
        return;
    }
    pos += strlen(search_key);
    size_t i = 0;
    while (*pos && *pos != '&' && *pos != ' ' && *pos != '\r' && *pos != '\n' && i < dest_len - 1) {
        dest[i++] = *pos++;
    }
    dest[i] = '\0';
}

/* Parses HTTP request buffer into structured Task */
static bool parse_http_request(int client_sock, const char *raw_data, int data_len, Task *task) {
    (void)data_len;
    memset(task, 0, sizeof(Task));
    task->client_socket = client_sock;
    gettimeofday(&task->arrival_time, NULL);
    task->type = REQ_P3_ROUTINE; // default routine priority

    // Parse Request Line: METHOD PATH HTTP/X.X
    char method[16] = {0};
    char path[128] = {0};
    if (sscanf(raw_data, "%15s %127s", method, path) != 2) {
        return false;
    }
    snprintf(task->http_method, sizeof(task->http_method), "%s", method);
    snprintf(task->path, sizeof(task->path), "%s", path);

    // Extract query string parameters if any
    char *query = strchr(task->path, '?');
    if (query) {
        *query = '\0';
        char doc_id_str[16] = {0};
        extract_param(query + 1, "doctor_id", doc_id_str, sizeof(doc_id_str));
        if (strlen(doc_id_str) > 0) task->doctor_id = atoi(doc_id_str);
    }

    // Extract request body for POST requests
    const char *body_start = strstr(raw_data, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        snprintf(task->body, sizeof(task->body), "%s", body_start);
    }

    const char *payload = (strlen(task->body) > 0) ? task->body : raw_data;
    char doc_str[32] = {0};
    char slot_str[32] = {0};
    char patient_str[64] = {0};
    char phone_str[32] = {0};
    char age_str[16] = {0};
    char gender_str[16] = {0};
    char symp_str[128] = {0};
    char vit_str[128] = {0};
    char reason_str[128] = {0};
    char res_str[64] = {0};
    char pat_id_str[32] = {0};
    char req_id_str[32] = {0};

    extract_param(payload, "doctor_id", doc_str, sizeof(doc_str));
    if (strlen(doc_str) > 0) task->doctor_id = atoi(doc_str);

    extract_param(payload, "slot_id", slot_str, sizeof(slot_str));
    if (strlen(slot_str) > 0) task->slot_id = atoi(slot_str);

    extract_param(payload, "patient_name", patient_str, sizeof(patient_str));
    if (strlen(patient_str) == 0) extract_param(payload, "patient", patient_str, sizeof(patient_str));
    if (strlen(patient_str) == 0) {
        static const char *default_names[] = {
            "Rahul Sharma", "Devyani Patil", "Aarav Mehta", "Pooja Verma",
            "Siddharth Rao", "Ananya Deshmukh", "Aditya Kulkarni", "Neha Iyer"
        };
        snprintf(task->patient_name, sizeof(task->patient_name), "%s", default_names[rand() % 8]);
    } else {
        snprintf(task->patient_name, sizeof(task->patient_name), "%s", patient_str);
    }

    extract_param(payload, "phone", phone_str, sizeof(phone_str));
    snprintf(task->patient_phone, sizeof(task->patient_phone), "%s", strlen(phone_str) > 0 ? phone_str : "+91-98200-11000");

    extract_param(payload, "age", age_str, sizeof(age_str));
    task->patient_age = (strlen(age_str) > 0) ? atoi(age_str) : 34;

    extract_param(payload, "gender", gender_str, sizeof(gender_str));
    snprintf(task->patient_gender, sizeof(task->patient_gender), "%s", strlen(gender_str) > 0 ? gender_str : "Other");

    extract_param(payload, "symptoms", symp_str, sizeof(symp_str));
    snprintf(task->symptoms, sizeof(task->symptoms), "%s", strlen(symp_str) > 0 ? symp_str : "General checkup and consultation");

    extract_param(payload, "vitals", vit_str, sizeof(vit_str));
    snprintf(task->vitals, sizeof(task->vitals), "%s", strlen(vit_str) > 0 ? vit_str : "BP: 120/80, SpO2: 98%, HR: 72");

    extract_param(payload, "priority_reason", reason_str, sizeof(reason_str));
    snprintf(task->priority_reason, sizeof(task->priority_reason), "%s", strlen(reason_str) > 0 ? reason_str : "Triage assessment completed by authorized hospital staff");

    extract_param(payload, "required_resource", res_str, sizeof(res_str));
    snprintf(task->required_resource, sizeof(task->required_resource), "%s", strlen(res_str) > 0 ? res_str : "none");

    extract_param(payload, "patient_id", pat_id_str, sizeof(pat_id_str));
    if (strlen(pat_id_str) > 0) {
        snprintf(task->patient_id, sizeof(task->patient_id), "%s", pat_id_str);
    } else {
        long p_num = rand() % 9000 + 1000;
        snprintf(task->patient_id, sizeof(task->patient_id), "PAT-%ld", p_num);
    }

    extract_param(payload, "request_id", req_id_str, sizeof(req_id_str));
    if (strlen(req_id_str) > 0) {
        snprintf(task->request_id, sizeof(task->request_id), "%s", req_id_str);
    } else {
        long r_num = rand() % 9000 + 2000;
        snprintf(task->request_id, sizeof(task->request_id), "REQ-%ld", r_num);
    }

    // Triage Priority Determination (P0 to P3)
    // Section 3.2: Configurable demonstration rules + Authorized staff confirmation
    char prio_str[16] = {0};
    extract_param(payload, "priority", prio_str, sizeof(prio_str));
    if (strlen(prio_str) > 0) {
        int p = atoi(prio_str);
        if (p >= 0 && p <= 3) {
            task->type = (RequestType)p;
        } else {
            task->type = REQ_P3_ROUTINE;
        }
    } else {
        // Triage keyword evaluation:
        // Life-threatening symptoms -> P0
        if (strstr(raw_data, "unconscious") || strstr(raw_data, "cardiac") ||
            strstr(raw_data, "severe bleeding") || strstr(raw_data, "head trauma") ||
            strstr(raw_data, "CRITICAL") || strstr(raw_data, "critical") ||
            (strstr(raw_data, "accident") && strstr(raw_data, "severe"))) {
            task->type = REQ_P0_CRITICAL;
        } else if (strstr(raw_data, "serious injury") || strstr(raw_data, "severe pain") ||
                   strstr(raw_data, "chest pain") || strstr(raw_data, "shortness of breath") ||
                   strstr(raw_data, "URGENT") || strstr(raw_data, "urgent")) {
            task->type = REQ_P1_HIGH;
        } else if (strstr(raw_data, "stable") || strstr(raw_data, "telemedicine") ||
                   strstr(raw_data, "fever") || strstr(raw_data, "vaccine")) {
            task->type = REQ_P2_MODERATE;
        } else {
            task->type = REQ_P3_ROUTINE;
        }
    }

    // Persist patient & request in SQLite for traceability
    if (strcmp(task->path, "/api/request") == 0 || strcmp(task->path, "/api/healthcare") == 0) {
        db_record_patient(task->patient_id, task->patient_name, task->patient_age, task->patient_gender, task->patient_phone);
        db_record_request(task->request_id, task->patient_id, task->patient_name,
                          (int)task->type, request_type_to_str(task->type),
                          task->symptoms, task->vitals, task->priority_reason, task->required_resource);
        
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), "Patient %s registered with %s", task->patient_name, request_type_to_str(task->type));
        db_log_event(task->type == REQ_P0_CRITICAL ? "P0_EMERGENCY" : "PATIENT_REGISTERED",
                     task->type == REQ_P0_CRITICAL ? "CRITICAL" : "INFO",
                     log_msg, task->request_id);
    }

    return true;
}

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    int min_threads = DEFAULT_MIN_THREADS;
    int max_threads = DEFAULT_MAX_THREADS;
    int idle_timeout = DEFAULT_IDLE_TIMEOUT_SEC;
    const char *db_path = DB_DEFAULT_PATH;

    if (argc >= 2) port = atoi(argv[1]);
    if (argc >= 3) min_threads = atoi(argv[2]);
    if (argc >= 4) max_threads = atoi(argv[3]);
    if (argc >= 5) idle_timeout = atoi(argv[4]);
    if (argc >= 6) db_path = argv[5];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN); // Prevent SIGPIPE from terminating server on client disconnect

    printf("===================================================================\n");
    printf("   ADAPTIVE EMERGENCY HEALTHCARE SERVER (DYNAMIC THREAD POOL)     \n");
    printf("   Course Project: Operating Systems                              \n");
    printf("===================================================================\n");
    printf("Configuration: Port=%d | Min Workers=%d | Max Workers=%d | Timeout=%ds\n",
           port, min_threads, max_threads, idle_timeout);

    // 1. Initialize SQLite Database
    if (db_init(db_path) != 0) {
        fprintf(stderr, "[FATAL] Failed to initialize SQLite database.\n");
        return 1;
    }

    // 2. Initialize Dynamic Thread Pool (Producer-Consumer Bounded Queue)
    pool = thread_pool_create(min_threads, max_threads, idle_timeout);
    if (!pool) {
        fprintf(stderr, "[FATAL] Failed to create dynamic thread pool.\n");
        db_close();
        return 1;
    }

    // 3. Setup Master Server TCP Socket (Socket Programming)
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("[FATAL] Socket creation failed");
        thread_pool_destroy(pool);
        db_close();
        return 1;
    }

    int opt = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[FATAL] Socket bind failed");
        close(server_socket_fd);
        thread_pool_destroy(pool);
        db_close();
        return 1;
    }

    if (listen(server_socket_fd, 128) < 0) {
        perror("[FATAL] Socket listen failed");
        close(server_socket_fd);
        thread_pool_destroy(pool);
        db_close();
        return 1;
    }

    // 4. Launch Background Metrics Monitor Thread
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_routine, NULL);

    printf("[SERVER] Listening on port %d. Ready for patient traffic & tests.\n\n", port);

    // 5. Main Accept Loop (Master Producer Thread)
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_sock = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (!server_running) break;
            continue;
        }

        // Apply read and write timeouts to prevent hung connections
        struct timeval tv_timeout;
        tv_timeout.tv_sec = 5;
        tv_timeout.tv_usec = 0;
        setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout));
        setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &tv_timeout, sizeof(tv_timeout));

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0) {
            close(client_sock);
            continue;
        }

        Task task;
        if (!parse_http_request(client_sock, buffer, bytes_received, &task)) {
            const char *bad_req = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"Malformed HTTP request\"}";
            send(client_sock, bad_req, strlen(bad_req), 0);
            close(client_sock);
            continue;
        }

        // Producer submits task into the bounded queue
        // If queue is full (overload), thread_pool_submit returns false
        if (!thread_pool_submit(pool, task)) {
            // Server Overload -> HTTP 503 Service Unavailable
            const char *overload_resp = 
                "HTTP/1.1 503 Service Unavailable\r\n"
                "Content-Type: application/json\r\n"
                "Retry-After: 2\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n\r\n"
                "{\"status\":\"OVERLOAD\",\"error\":\"Server queue full (64/64). Autoscaling capacity reached. Overload protection applied.\"}";

            send(client_sock, overload_resp, strlen(overload_resp), 0);
            close(client_sock);
        }
    }

    // 6. Graceful Shutdown & Resource Cleanup
    printf("[SERVER] Stopping monitor thread and joining workers...\n");
    pthread_join(monitor_tid, NULL);
    thread_pool_destroy(pool);
    db_close();
    printf("[SERVER] Shutdown completed successfully. Clean exit.\n");

    return 0;
}
