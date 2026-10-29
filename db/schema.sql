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

CREATE INDEX IF NOT EXISTS idx_slots_doc_avail ON appointment_slots(doctor_id, is_booked);
CREATE INDEX IF NOT EXISTS idx_appointments_code ON appointments(booking_code);
