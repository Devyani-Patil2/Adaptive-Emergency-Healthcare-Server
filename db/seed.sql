-- Initial demonstration records for Adaptive Emergency Healthcare Server

INSERT OR IGNORE INTO hospitals (name, address, distance_km, icu_beds_total, icu_beds_available, oxygen_percent, phone, is_emergency_active)
VALUES
('City General Trauma Center', 'Sector 4, MG Road, Central', 1.2, 20, 14, 100, '+91-20-2550-0101', 1),
('Apollo Emergency & Critical Care', 'Bandra Kurla Complex, West', 3.5, 16, 8, 98, '+91-20-2550-0102', 1),
('Metro Multi-Specialty Hospital', 'South Avenue, Tech Park', 5.1, 30, 22, 95, '+91-20-2550-0103', 1);

INSERT OR IGNORE INTO doctors (id, name, speciality, experience_years, fee_usd, rating, photo_url)
VALUES
(1, 'Dr. Ananya Sharma', 'Pediatric & Trauma', 12, 500, 4.9, 'https://images.unsplash.com/photo-1594824813589-9a7457788e34?w=100&auto=format&fit=crop&q=80'),
(2, 'Dr. Rajesh Kulkarni', 'General Surgery & ICU', 10, 800, 4.8, 'https://images.unsplash.com/photo-1622253692010-333f2da6031d?w=100&auto=format&fit=crop&q=80'),
(3, 'Dr. Priya Iyer', 'Gastroenterology', 7, 400, 4.9, 'https://images.unsplash.com/photo-1559839734-2b71ea197ec2?w=100&auto=format&fit=crop&q=80'),
(4, 'Dr. Arvind Menon', 'Interventional Cardiology', 15, 1000, 5.0, 'https://images.unsplash.com/photo-1537368910025-700350fe46c7?w=100&auto=format&fit=crop&q=80');

INSERT OR IGNORE INTO appointment_slots (doctor_id, day_of_week, start_time, end_time, is_booked)
VALUES
(1, 'Tue', '10:00 AM', '11:00 AM', 0),
(1, 'Tue', '11:00 AM', '12:00 PM', 0),
(1, 'Thu', '10:00 AM', '11:00 AM', 0),
(1, 'Thu', '12:00 PM', '01:00 PM', 0),
(2, 'Mon', '02:00 PM', '03:00 PM', 0),
(2, 'Mon', '03:00 PM', '04:00 PM', 0),
(2, 'Wed', '02:00 PM', '03:00 PM', 0),
(2, 'Wed', '04:00 PM', '05:00 PM', 0),
(3, 'Daily', '09:00 AM', '10:00 AM', 0),
(3, 'Daily', '10:00 AM', '11:00 AM', 0),
(3, 'Daily', '11:00 AM', '12:00 PM', 0),
(4, 'Fri', '10:00 AM', '10:30 AM', 0),
(4, 'Sat', '10:00 AM', '10:30 AM', 0),
(4, 'Sat', '02:00 PM', '02:30 PM', 0);

INSERT OR IGNORE INTO hospital_resources (resource_type, display_name, total_quantity, occupied_quantity, unit)
VALUES
('oxygen_cylinders', 'High-Flow Oxygen Cylinders', 10, 4, 'Cylinders'),
('blood_units', 'Blood Bank Emergency Reserve (O-)', 8, 3, 'Units'),
('icu_beds', 'Critical Care ICU Beds', 6, 2, 'Beds'),
('ventilators', 'Mechanical Ventilators', 5, 2, 'Units'),
('general_beds', 'General Ward Beds', 20, 8, 'Beds'),
('doctors_duty', 'Emergency Doctors on Duty', 8, 3, 'Physicians');

INSERT OR IGNORE INTO system_events (event_type, severity, message, related_id)
VALUES
('SYSTEM_START', 'INFO', 'Adaptive Healthcare Server initialized with 4 baseline worker threads.', 'SYS-01'),
('AUTOSCALER_READY', 'SUCCESS', 'Dynamic thread pool ready: min 4, max 16, bounded queue cap 64.', 'POOL-01'),
('DB_READY', 'INFO', 'SQLite database persistence engine mounted with ACID transaction protection.', 'DB-01'),
('RESOURCE_UPDATE', 'INFO', 'Hospital inventory initialized: 10 Oxygen Cylinders, 8 Blood Units, 6 ICU Beds, 5 Ventilators, 20 General Beds.', 'RES-01');
