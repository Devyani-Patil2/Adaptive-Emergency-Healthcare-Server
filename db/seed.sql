-- Demonstration Seed Data for Adaptive Emergency Healthcare Server
-- NOTE: For academic demonstration only. Not actual real-world clinical data.

DELETE FROM appointments;
DELETE FROM appointment_slots;
DELETE FROM doctors;
DELETE FROM hospitals;

-- Hospitals
INSERT INTO hospitals (name, address, distance_km, icu_beds_total, icu_beds_available, oxygen_percent, phone, is_emergency_active)
VALUES
('City General Hospital', 'Rua Sao Salvador 57, Sector 4', 1.2, 20, 14, 100, '+1-800-555-0101', 1),
('Apollo Emergency Care', 'Largo Prazeres 120, Central', 3.5, 16, 8, 98, '+1-800-555-0102', 1),
('Metro Trauma Center', 'R Velhas 92, South Avenue', 5.1, 30, 22, 95, '+1-800-555-0103', 1);

-- Doctors
INSERT INTO doctors (id, name, speciality, experience_years, fee_usd, rating, photo_url)
VALUES
(1, 'Dr. Ananya Sharma', 'Pediatric', 12, 500, 4.9, 'https://images.unsplash.com/photo-1594824813589-9a7457788e34?w=100&auto=format&fit=crop&q=80'),
(2, 'Dr. Rajesh Kulkarni', 'Surgical', 10, 800, 4.8, 'https://images.unsplash.com/photo-1622253692010-333f2da6031d?w=100&auto=format&fit=crop&q=80'),
(3, 'Dr. Priya Iyer', 'Gastroenterology', 7, 400, 4.9, 'https://images.unsplash.com/photo-1559839734-2b71ea197ec2?w=100&auto=format&fit=crop&q=80'),
(4, 'Dr. Arvind Menon', 'Cardiology', 15, 1000, 5.0, 'https://images.unsplash.com/photo-1537368910025-700350fe46c7?w=100&auto=format&fit=crop&q=80');

-- Appointment Slots
INSERT INTO appointment_slots (doctor_id, day_of_week, start_time, end_time, is_booked)
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
