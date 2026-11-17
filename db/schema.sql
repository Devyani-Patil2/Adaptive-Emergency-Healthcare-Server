-- Adaptive Emergency Healthcare Server Database Schema
-- Demonstrates persistent storage for SQLite-backed healthcare transactions

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS hospitals (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    address TEXT NOT NULL,
    distance_km REAL NOT NULL,
    icu_beds_total INTEGER NOT NULL,
    icu_beds_available INTEGER NOT NULL,
    oxygen_percent INTEGER NOT NULL,
    phone TEXT NOT NULL,
    is_emergency_active INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS doctors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    speciality TEXT NOT NULL,
    experience_years INTEGER NOT NULL,
    fee_usd INTEGER NOT NULL,
    rating REAL NOT NULL DEFAULT 4.9,
    photo_url TEXT
);

CREATE TABLE IF NOT EXISTS appointment_slots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    doctor_id INTEGER NOT NULL,
    day_of_week TEXT NOT NULL,
    start_time TEXT NOT NULL,
    end_time TEXT NOT NULL,
    is_booked INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(doctor_id) REFERENCES doctors(id) ON DELETE CASCADE,
    UNIQUE(doctor_id, day_of_week, start_time)
);

CREATE TABLE IF NOT EXISTS appointments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    booking_code TEXT NOT NULL UNIQUE,
    doctor_id INTEGER NOT NULL,
    slot_id INTEGER NOT NULL,
    patient_name TEXT NOT NULL,
    patient_phone TEXT NOT NULL,
    notes TEXT,
    status TEXT NOT NULL DEFAULT 'CONFIRMED',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    handled_by_worker INTEGER NOT NULL,
    turnaround_ms INTEGER NOT NULL,
    FOREIGN KEY(doctor_id) REFERENCES doctors(id),
    FOREIGN KEY(slot_id) REFERENCES appointment_slots(id)
);

-- Core Healthcare Application Tables (Supporting Concurrency Demos)
CREATE TABLE IF NOT EXISTS patients (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_id TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    age INTEGER NOT NULL,
    gender TEXT,
    phone TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS requests (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    request_id TEXT NOT NULL UNIQUE,
    patient_id TEXT NOT NULL,
    patient_name TEXT NOT NULL,
    priority INTEGER NOT NULL,
    priority_name TEXT NOT NULL,
    symptoms TEXT,
    vitals TEXT,
    priority_reason TEXT,
    required_resource TEXT,
    status TEXT NOT NULL DEFAULT 'QUEUED',
    worker_id INTEGER DEFAULT -1,
    queue_wait_ms INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS hospital_resources (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    resource_type TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    total_quantity INTEGER NOT NULL,
    occupied_quantity INTEGER NOT NULL DEFAULT 0,
    unit TEXT NOT NULL DEFAULT 'Units'
);

CREATE TABLE IF NOT EXISTS resource_allocations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    allocation_code TEXT NOT NULL UNIQUE,
    patient_id TEXT NOT NULL,
    patient_name TEXT NOT NULL,
    resource_type TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'ALLOCATED',
    allocated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    released_at TIMESTAMP
);

CREATE TABLE IF NOT EXISTS system_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL,
    message TEXT NOT NULL,
    related_id TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_slots_doc_avail ON appointment_slots(doctor_id, is_booked);
CREATE INDEX IF NOT EXISTS idx_appointments_code ON appointments(booking_code);
CREATE INDEX IF NOT EXISTS idx_requests_priority ON requests(priority, status);
CREATE INDEX IF NOT EXISTS idx_events_created ON system_events(created_at DESC);
