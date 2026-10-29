#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdbool.h>
#include <ctype.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 8080
#define BUFFER_SIZE 4096
#define MAX_SAMPLES 50000

typedef struct {
    int client_id;
    int request_type; // 0=Emergency (P0), 1=Telemedicine (P1), 2=Vaccine (P2), 3=Routine Appointment (P3)
    char server_ip[64];
    int server_port;
    long long latency_ms;
    int http_status;
    bool success;
} ClientRequest;

static long long get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

static int compare_longs(const void *a, const void *b) {
    long long la = *(const long long*)a;
    long long lb = *(const long long*)b;
    if (la < lb) return -1;
    if (la > lb) return 1;
    return 0;
}

void* execute_client_request(void *arg) {
    ClientRequest *req = (ClientRequest*)arg;
    long long start = get_time_ms();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        req->success = false;
        req->http_status = 0;
        return NULL;
    }

    // Set connection and read timeout (3 seconds)
    struct timeval tv_timeout;
    tv_timeout.tv_sec = 3;
    tv_timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv_timeout, sizeof(tv_timeout));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(req->server_port);
    inet_pton(AF_INET, req->server_ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        req->success = false;
        req->http_status = 0;
        close(sock);
        return NULL;
    }

    char message[BUFFER_SIZE];
    // Route by priority
    if (req->request_type == 0) {
        // P0: Emergency Info
        snprintf(message, sizeof(message),
                 "GET /api/emergency-info HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "User-Agent: ClientSimulator/2.0\r\n"
                 "Connection: close\r\n\r\n",
                 req->server_ip, req->server_port);
    } else if (req->request_type == 1) {
        // P1: Telemedicine Doctors
        snprintf(message, sizeof(message),
                 "GET /api/doctors HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "User-Agent: ClientSimulator/2.0\r\n"
                 "Connection: close\r\n\r\n",
                 req->server_ip, req->server_port);
    } else if (req->request_type == 2) {
        // P2: Vaccination & Hospitals
        snprintf(message, sizeof(message),
                 "GET /api/hospitals HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "User-Agent: ClientSimulator/2.0\r\n"
                 "Connection: close\r\n\r\n",
                 req->server_ip, req->server_port);
    } else {
        // P3: Routine Appointment booking attempt
        char body[256];
        int body_len = snprintf(body, sizeof(body),
                                "{\"doctor_id\":1,\"slot_id\":%d,\"patient_name\":\"Simulated Patient %d\",\"phone\":\"555-01%02d\"}",
                                (req->client_id % 5) + 1, req->client_id, req->client_id % 100);
        snprintf(message, sizeof(message),
                 "POST /api/appointments HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n\r\n%s",
                 req->server_ip, req->server_port, body_len, body);
    }

    send(sock, message, strlen(message), 0);

    char response[BUFFER_SIZE];
    int bytes = recv(sock, response, sizeof(response) - 1, 0);
    long long finish = get_time_ms();
    req->latency_ms = finish - start;

    if (bytes > 0) {
        response[bytes] = '\0';
        int status = 0;
        if (sscanf(response, "HTTP/1.1 %d", &status) == 1) {
            req->http_status = status;
            if (status >= 200 && status < 300) {
                req->success = true;
            } else if (status == 409) {
                // Legitimate double-booking conflict
                req->success = true; // Expected business response
            } else {
                req->success = false;
            }
        } else {
            req->http_status = 200;
            req->success = true;
        }
    } else {
        req->http_status = 0;
        req->success = false;
    }

    close(sock);
    return NULL;
}

typedef struct {
    int total_requests;
    int successful;
    int conflict_409;
    int overload_503;
    int errors;
    long long duration_ms;
    double throughput;
    long long min_latency;
    double avg_latency;
    long long p50_latency;
    long long p95_latency;
    long long max_latency;
} BenchmarkResults;

BenchmarkResults run_traffic_phase(const char *phase_name, const char *server_ip, int server_port,
                                   int total_requests, int concurrency, int delay_ms_between_batches) {
    printf("\n===================================================================\n");
    printf(">>> RUNNING: %s\n", phase_name);
    printf("    Target: %s:%d | Total Requests: %d | Concurrency: %d | Batch Delay: %dms\n",
           server_ip, server_port, total_requests, concurrency, delay_ms_between_batches);
    printf("===================================================================\n");

    pthread_t *threads = malloc(sizeof(pthread_t) * total_requests);
    ClientRequest *reqs = malloc(sizeof(ClientRequest) * total_requests);

    long long phase_start = get_time_ms();
    int dispatched = 0;

    while (dispatched < total_requests) {
        int batch_size = (total_requests - dispatched < concurrency) ? (total_requests - dispatched) : concurrency;

        for (int i = 0; i < batch_size; i++) {
            int idx = dispatched + i;
            reqs[idx].client_id = idx;
            strncpy(reqs[idx].server_ip, server_ip, sizeof(reqs[idx].server_ip));
            reqs[idx].server_port = server_port;

            // Priority distribution: 20% Emergency (P0), 30% Telemedicine (P1), 30% Vaccine (P2), 20% Routine (P3)
            int r = rand() % 100;
            if (r < 20) reqs[idx].request_type = 0;
            else if (r < 50) reqs[idx].request_type = 1;
            else if (r < 80) reqs[idx].request_type = 2;
            else reqs[idx].request_type = 3;

            pthread_create(&threads[idx], NULL, execute_client_request, &reqs[idx]);
        }

        dispatched += batch_size;
        if (delay_ms_between_batches > 0) {
            usleep(delay_ms_between_batches * 1000);
        }
    }

    // Await all thread completions
    BenchmarkResults res;
    memset(&res, 0, sizeof(BenchmarkResults));
    res.total_requests = total_requests;

    long long *valid_latencies = malloc(sizeof(long long) * total_requests);
    int latency_count = 0;
    long long total_lat = 0;
    res.min_latency = 999999;
    res.max_latency = 0;

    for (int i = 0; i < total_requests; i++) {
        pthread_join(threads[i], NULL);
        if (reqs[i].http_status >= 200 && reqs[i].http_status < 300) {
            res.successful++;
        } else if (reqs[i].http_status == 409) {
            res.conflict_409++;
        } else if (reqs[i].http_status == 503) {
            res.overload_503++;
        } else {
            res.errors++;
        }

        if (reqs[i].latency_ms > 0) {
            valid_latencies[latency_count++] = reqs[i].latency_ms;
            total_lat += reqs[i].latency_ms;
            if (reqs[i].latency_ms < res.min_latency) res.min_latency = reqs[i].latency_ms;
            if (reqs[i].latency_ms > res.max_latency) res.max_latency = reqs[i].latency_ms;
        }
    }

    long long phase_end = get_time_ms();
    res.duration_ms = phase_end - phase_start;
    if (res.duration_ms <= 0) res.duration_ms = 1;

    res.throughput = (double)(res.successful + res.conflict_409) / (res.duration_ms / 1000.0);
    res.avg_latency = latency_count > 0 ? (double)total_lat / latency_count : 0.0;
    if (res.min_latency == 999999) res.min_latency = 0;

    if (latency_count > 0) {
        qsort(valid_latencies, latency_count, sizeof(long long), compare_longs);
        res.p50_latency = valid_latencies[(int)(latency_count * 0.50)];
        int p95_i = (int)(latency_count * 0.95);
        if (p95_i >= latency_count) p95_i = latency_count - 1;
        res.p95_latency = valid_latencies[p95_i];
    }

    printf("\n--- Performance Metrics: %s ---\n", phase_name);
    printf("  Total Dispatched   : %d requests\n", res.total_requests);
    printf("  HTTP 2xx Success   : %d (%.1f%%)\n", res.successful, (double)res.successful / total_requests * 100.0);
    printf("  HTTP 409 Conflicts : %d (Atomic slot double-booking guards)\n", res.conflict_409);
    printf("  HTTP 503 Overloads : %d (Bounded buffer rejection guards)\n", res.overload_503);
    printf("  Network/Sock Errors: %d\n", res.errors);
    printf("  Wallclock Duration : %lld ms\n", res.duration_ms);
    printf("  Server Throughput  : %.2f req/sec\n", res.throughput);
    printf("  Min Latency        : %lld ms\n", res.min_latency);
    printf("  Avg Latency        : %.2f ms\n", res.avg_latency);
    printf("  P50 Latency (Median): %lld ms\n", res.p50_latency);
    printf("  P95 Latency (Tail) : %lld ms\n", res.p95_latency);
    printf("  Max Latency        : %lld ms\n", res.max_latency);

    free(threads);
    free(reqs);
    free(valid_latencies);
    return res;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    char server_ip[64] = DEFAULT_SERVER_IP;
    int server_port = DEFAULT_SERVER_PORT;
    int mode = 0; // 0=Full 3-stage, 1=Baseline, 2=Surge, 3=Comparative
    int custom_reqs = 60;
    int custom_concurrency = 15;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            server_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc) {
            strncpy(server_ip, argv[++i], sizeof(server_ip) - 1);
            server_ip[sizeof(server_ip) - 1] = '\0';
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "baseline") == 0) mode = 1;
            else if (strcmp(argv[i], "surge") == 0) mode = 2;
            else if (strcmp(argv[i], "compare") == 0) mode = 3;
            else mode = atoi(argv[i]);
        } else if (strcmp(argv[i], "--requests") == 0 && i + 1 < argc) {
            custom_reqs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--concurrency") == 0 && i + 1 < argc) {
            custom_concurrency = atoi(argv[++i]);
        } else if (isdigit(argv[i][0])) {
            mode = atoi(argv[i]);
        }
    }

    printf("===================================================================\n");
    printf("   HEALTHCARE CLIENT TRAFFIC SIMULATOR & OS BENCHMARK HARNESS      \n");
    printf("===================================================================\n");

    if (mode == 1) {
        run_traffic_phase("Baseline OPD Traffic (Normal Load)", server_ip, server_port, 25, 3, 150);
    } else if (mode == 2) {
        run_traffic_phase("Outbreak / Vaccine Surge (Burst Load)", server_ip, server_port, custom_reqs, custom_concurrency, 15);
    } else if (mode == 3) {
        printf("\nStarting Comparative OS Evaluation:\n");
        BenchmarkResults r1 = run_traffic_phase("STAGE 1: Baseline OPD Traffic (Normal Load)", server_ip, server_port, 20, 2, 150);
        printf("\n>> Pausing 1 second before burst...\n");
        sleep(1);
        BenchmarkResults r2 = run_traffic_phase("STAGE 2: Sudden Mass Outbreak Surge (Burst Load)", server_ip, server_port, 80, 20, 10);
        
        printf("\n===================================================================\n");
        printf("   BENCHMARK COMPARISON SUMMARY                                    \n");
        printf("===================================================================\n");
        printf("  Metric                 | Baseline OPD Load     | Outbreak Surge Load\n");
        printf("  -----------------------+-----------------------+---------------------\n");
        printf("  Dispatched Requests    | %-21d | %-20d\n", r1.total_requests, r2.total_requests);
        printf("  Throughput (req/s)     | %-21.2f | %-20.2f\n", r1.throughput, r2.throughput);
        printf("  Avg Latency (ms)       | %-21.2f | %-20.2f\n", r1.avg_latency, r2.avg_latency);
        printf("  P50 Latency (ms)       | %-21lld | %-20lld\n", r1.p50_latency, r2.p50_latency);
        printf("  P95 Latency (ms)       | %-21lld | %-20lld\n", r1.p95_latency, r2.p95_latency);
        printf("  Max Latency (ms)       | %-21lld | %-20lld\n", r1.max_latency, r2.max_latency);
        printf("  503 Dropped (Overload) | %-21d | %-20d\n", r1.overload_503, r2.overload_503);
        printf("===================================================================\n");
    } else {
        printf("\nExecuting Complete 3-Stage Stress Test:\n");
        run_traffic_phase("STAGE 1: Baseline OPD Traffic (Normal Load)", server_ip, server_port, 20, 3, 150);
        
        printf("\n>> Simulating sudden disease outbreak and vaccine drive rush in 2 seconds...\n");
        sleep(2);
        run_traffic_phase("STAGE 2: Sudden Mass Outbreak Surge (Burst Traffic)", server_ip, server_port, 70, 20, 15);

        printf("\n>> Traffic burst completed. Waiting 5s for autoscaler cooldown & dynamic thread pruning...\n");
        sleep(5);
        run_traffic_phase("STAGE 3: Post-Surge Verification Traffic", server_ip, server_port, 15, 2, 100);
    }

    return 0;
}
