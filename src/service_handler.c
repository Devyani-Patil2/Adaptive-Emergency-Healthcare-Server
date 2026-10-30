#include "service_handler.h"
#include "thread_pool.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Helper to reliably send all bytes over TCP socket */
static bool send_all(int sock, const char *buf, size_t len) {
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t bytes = send(sock, buf + total_sent, len - total_sent, 0);
        if (bytes <= 0) {
            return false;
        }
        total_sent += bytes;
    }
    return true;
}

/* Helper to format and send standard HTTP response */
static void send_http_response(int sock, int status_code, const char *status_text,
                               const char *content_type, const char *body) {
    if (sock < 0) return;
    size_t body_len = body ? strlen(body) : 0;
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n\r\n",
        status_code, status_text, content_type, body_len);

    send_all(sock, header, header_len);
    if (body_len > 0) {
        send_all(sock, body, body_len);
    }
}

/* Serve dashboard static HTML with basic path traversal protection */
static void serve_html_file(int sock, const char *filepath) {
    if (sock < 0) return;
    if (strstr(filepath, "..")) {
        send_http_response(sock, 403, "Forbidden", "text/plain", "403 Forbidden");
        return;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        f = fopen("../web/index.html", "rb");
    }
    if (!f) {
        send_http_response(sock, 404, "Not Found", "text/plain", "404 Not Found");
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *html_buf = malloc(fsize + 1);
    if (!html_buf) {
        fclose(f);
        send_http_response(sock, 500, "Internal Server Error", "text/plain", "500 Server Error");
        return;
    }

    size_t n = fread(html_buf, 1, fsize, f);
    fclose(f);
    html_buf[n] = '\0';

    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n", n);

    send_all(sock, header, hlen);
    send_all(sock, html_buf, n);
    free(html_buf);
}

void process_healthcare_task(Task *task, int worker_id, DynamicThreadPool *pool) {
    long long start_ms = current_timestamp_ms();
    long long queue_wait = start_ms - ((long long)task->arrival_time.tv_sec * 1000 + 
                                       task->arrival_time.tv_usec / 1000);
    if (queue_wait < 0) queue_wait = 0;

    int client_sock = task->client_socket;
    bool request_success = true;

    // Handle OPTIONS Preflight
    if (strcmp(task->http_method, "OPTIONS") == 0) {
        send_http_response(client_sock, 204, "No Content", "text/plain", "");
        close(client_sock);
        thread_pool_record_completion(pool, (int)queue_wait, true);
        return;
    }

    // Route: GET / and GET /index.html
    if (strcmp(task->http_method, "GET") == 0 &&
        (strcmp(task->path, "/") == 0 || strcmp(task->path, "/index.html") == 0)) {
        serve_html_file(client_sock, "web/index.html");
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), true);
        return;
    }

    // Route: GET /api/stats and GET /metrics
    if (strcmp(task->http_method, "GET") == 0 &&
        (strcmp(task->path, "/api/stats") == 0 || strcmp(task->path, "/metrics") == 0)) {
        char json_buf[4096];
        thread_pool_get_stats_json(pool, json_buf, sizeof(json_buf));
        send_http_response(client_sock, 200, "OK", "application/json", json_buf);
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), true);
        return;
    }

    // Route: GET /api/status
    if (strcmp(task->http_method, "GET") == 0 && strcmp(task->path, "/api/status") == 0) {
        char status_buf[512];
        snprintf(status_buf, sizeof(status_buf),
            "{\"status\":\"ONLINE\",\"version\":\"2.0\",\"architecture\":\"Adaptive Dynamic Thread Pool\","
            "\"concurrency_model\":\"POSIX Threads + Producer-Consumer Bounded Buffer\","
            "\"database\":\"SQLite Persistent Storage\",\"min_workers\":%d,\"max_workers\":%d}",
            pool->min_threads, pool->max_threads);
        send_http_response(client_sock, 200, "OK", "application/json", status_buf);
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), true);
        return;
    }

    // Route: GET /api/hospitals
    if (strcmp(task->http_method, "GET") == 0 && strcmp(task->path, "/api/hospitals") == 0) {
        char json_buf[2048];
        db_get_hospitals_json(json_buf, sizeof(json_buf));
        send_http_response(client_sock, 200, "OK", "application/json", json_buf);
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), true);
        return;
    }

    // Route: GET /api/doctors
    if (strcmp(task->http_method, "GET") == 0 && strcmp(task->path, "/api/doctors") == 0) {
        char json_buf[2048];
        db_get_doctors_json(json_buf, sizeof(json_buf));
        send_http_response(client_sock, 200, "OK", "application/json", json_buf);
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), true);
        return;
    }

    // Route: GET /api/appointments/slots
    if (strcmp(task->http_method, "GET") == 0 && strcmp(task->path, "/api/appointments/slots") == 0) {
        char json_buf[3072];
        db_get_slots_json(task->doctor_id, json_buf, sizeof(json_buf));
        send_http_response(client_sock, 200, "OK", "application/json", json_buf);
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), true);
        return;
    }

    // Route: GET /api/emergency-info
    if (strcmp(task->http_method, "GET") == 0 && strcmp(task->path, "/api/emergency-info") == 0) {
        char json_buf[1024];
        db_get_emergency_info_json(json_buf, sizeof(json_buf));
        send_http_response(client_sock, 200, "OK", "application/json", json_buf);
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), true);
        return;
    }

    // Route: GET /api/appointments (List recent)
    if (strcmp(task->http_method, "GET") == 0 && strcmp(task->path, "/api/appointments") == 0) {
        char json_buf[4096];
        db_get_appointments_json(25, json_buf, sizeof(json_buf));
        send_http_response(client_sock, 200, "OK", "application/json", json_buf);
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), true);
        return;
    }

    // Route: GET /api/appointments/{id}
    if (strcmp(task->http_method, "GET") == 0 && strncmp(task->path, "/api/appointments/", 18) == 0) {
        int appt_id = atoi(task->path + 18);
        char json_buf[1024];
        db_get_appointment_by_id_json(appt_id, json_buf, sizeof(json_buf));
        if (strstr(json_buf, "Appointment not found")) {
            send_http_response(client_sock, 404, "Not Found", "application/json", json_buf);
            request_success = false;
        } else {
            send_http_response(client_sock, 200, "OK", "application/json", json_buf);
        }
        close(client_sock);
        long long end_ms = current_timestamp_ms();
        thread_pool_record_completion(pool, (int)(end_ms - start_ms + queue_wait), request_success);
        return;
    }

    // Route: POST /api/appointments (Atomic SQLite appointment booking)
    if (strcmp(task->http_method, "POST") == 0 && strcmp(task->path, "/api/appointments") == 0) {
        // Measure execution time
        long long proc_start = current_timestamp_ms();
        char booking_code[64] = {0};
        char err_msg[256] = {0};

        int book_rc = db_book_appointment(task->doctor_id, task->slot_id,
                                          task->patient_name, task->patient_phone,
                                          task->notes, worker_id, (int)queue_wait,
                                          booking_code, sizeof(booking_code),
                                          err_msg, sizeof(err_msg));

        long long proc_end = current_timestamp_ms();
        int turnaround_ms = (int)(proc_end - start_ms + queue_wait);

        if (book_rc == 0) {
            // Success: HTTP 201 Created
            char resp_buf[1024];
            snprintf(resp_buf, sizeof(resp_buf),
                "{\"status\":\"CONFIRMED\",\"booking_code\":\"%s\",\"patient_name\":\"%s\","
                "\"doctor_id\":%d,\"slot_id\":%d,\"worker_id\":%d,\"priority\":%d,\"priority_desc\":\"%s\","
                "\"queue_wait_ms\":%lld,\"execution_ms\":%lld,\"turnaround_ms\":%d}",
                booking_code, task->patient_name, task->doctor_id, task->slot_id,
                worker_id, (int)task->type, request_type_to_str(task->type),
                queue_wait, (proc_end - proc_start), turnaround_ms);

            send_http_response(client_sock, 201, "Created", "application/json", resp_buf);
            request_success = true;
        } else if (book_rc == 1) {
            // Conflict: HTTP 409 Conflict (slot already booked)
            char resp_buf[512];
            snprintf(resp_buf, sizeof(resp_buf),
                "{\"status\":\"CONFLICT\",\"error\":\"%s\",\"slot_id\":%d,\"worker_id\":%d,\"priority\":%d,\"priority_desc\":\"%s\"}",
                err_msg, task->slot_id, worker_id, (int)task->type, request_type_to_str(task->type));
            send_http_response(client_sock, 409, "Conflict", "application/json", resp_buf);
            request_success = false;
        } else if (book_rc == -1) {
            // Validation error: HTTP 400 Bad Request
            char resp_buf[512];
            snprintf(resp_buf, sizeof(resp_buf),
                "{\"status\":\"BAD_REQUEST\",\"error\":\"%s\",\"worker_id\":%d}",
                err_msg, worker_id);
            send_http_response(client_sock, 400, "Bad Request", "application/json", resp_buf);
            request_success = false;
        } else {
            // Database internal error: HTTP 500
            char resp_buf[512];
            snprintf(resp_buf, sizeof(resp_buf),
                "{\"status\":\"INTERNAL_ERROR\",\"error\":\"%s\",\"worker_id\":%d}",
                err_msg, worker_id);
            send_http_response(client_sock, 500, "Internal Server Error", "application/json", resp_buf);
            request_success = false;
        }

        close(client_sock);
        thread_pool_record_completion(pool, turnaround_ms, request_success);
        return;
    }

    // Route: POST /api/request or POST /api/healthcare (Triage simulated processing)
    if (strcmp(task->http_method, "POST") == 0 &&
        (strcmp(task->path, "/api/request") == 0 || strcmp(task->path, "/api/healthcare") == 0)) {
        
        int sim_work_ms = 20;
        const char *action_desc = "General Request Processed";

        switch (task->type) {
            case REQ_EMERGENCY_ACCIDENT:
                sim_work_ms = 35;
                action_desc = "ACCIDENT/TRAUMA: Red Triage, OR & Trauma Team Dispatched Immediately";
                break;
            case REQ_EMERGENCY_INFO:
                sim_work_ms = 45;
                action_desc = "EMERGENCY: ICU Oxygen & Bed Allocated";
                break;
            case REQ_TELEMEDICINE:
                sim_work_ms = 70;
                action_desc = "TELEMEDICINE: Video Session Channel Created";
                break;
            case REQ_VACCINATION:
                sim_work_ms = 90;
                action_desc = "VACCINATION: Booster Dose Inventory Reserved";
                break;
            case REQ_APPOINTMENT:
            default:
                sim_work_ms = 110;
                action_desc = "ROUTINE OPD: Consultation Query Handled";
                break;
        }

        usleep(sim_work_ms * 1000);
        long long finish_ms = current_timestamp_ms();
        int turnaround = (int)(finish_ms - start_ms + queue_wait);

        char resp_buf[1024];
        snprintf(resp_buf, sizeof(resp_buf),
            "{\"status\":\"SUCCESS\",\"worker_id\":%d,\"priority\":\"%s\",\"action\":\"%s\","
            "\"queue_wait_ms\":%lld,\"execution_ms\":%d,\"turnaround_ms\":%d}",
            worker_id, request_type_to_str(task->type), action_desc,
            queue_wait, sim_work_ms, turnaround);

        send_http_response(client_sock, 200, "OK", "application/json", resp_buf);
        close(client_sock);
        thread_pool_record_completion(pool, turnaround, true);
        return;
    }

    // Unknown Route -> HTTP 404 Not Found
    char not_found_buf[256];
    snprintf(not_found_buf, sizeof(not_found_buf),
        "{\"error\":\"Endpoint not found\",\"path\":\"%s\",\"method\":\"%s\"}",
        task->path, task->http_method);
    send_http_response(client_sock, 404, "Not Found", "application/json", not_found_buf);
    close(client_sock);

    long long finish_ms = current_timestamp_ms();
    thread_pool_record_completion(pool, (int)(finish_ms - start_ms + queue_wait), false);
}
