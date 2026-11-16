import React, { useState, useEffect } from 'react';
import { Calendar, X, Clock, CheckCircle2, AlertCircle, User, Star, Award, Shield, Phone, ArrowRight } from 'lucide-react';

export default function BookSpecialistModal({ isOpen, onClose, doctors, patientName, onBookingConfirmed }) {
  const [selectedDoctorId, setSelectedDoctorId] = useState('');
  const [slots, setSlots] = useState([]);
  const [selectedSlotId, setSelectedSlotId] = useState('');
  const [phone, setPhone] = useState('+91-98200-11000');
  const [isLoadingSlots, setIsLoadingSlots] = useState(false);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [feedback, setFeedback] = useState(null);

  // Default to first doctor with available slots if possible
  useEffect(() => {
    if (doctors.length > 0 && !selectedDoctorId) {
      setSelectedDoctorId(doctors[0].id.toString());
    }
  }, [doctors]);

  useEffect(() => {
    if (selectedDoctorId) {
      fetchDoctorSlots(selectedDoctorId);
    }
  }, [selectedDoctorId]);

  const fetchDoctorSlots = async (docId) => {
    setIsLoadingSlots(true);
    setSelectedSlotId('');
    try {
      const res = await fetch(`/api/appointments/slots?doctor_id=${docId}`);
      const data = await res.json();
      const allSlots = data.slots || [];
      setSlots(allSlots);
      const firstAvailable = allSlots.find(s => s.available);
      if (firstAvailable) {
        setSelectedSlotId(firstAvailable.id.toString());
      }
    } catch (err) {
      console.error('Error fetching slots:', err);
    } finally {
      setIsLoadingSlots(false);
    }
  };

  const selectedDoctor = doctors.find(d => d.id.toString() === selectedDoctorId.toString()) || doctors[0];
  const selectedSlot = slots.find(s => s.id.toString() === selectedSlotId.toString());

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!selectedSlotId) {
      alert('Please select an available time slot.');
      return;
    }

    setIsSubmitting(true);
    setFeedback(null);
    try {
      const res = await fetch('/api/appointments', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          doctor_id: parseInt(selectedDoctorId),
          slot_id: parseInt(selectedSlotId),
          patient_name: patientName,
          patient_phone: phone,
          notes: 'Booked via React Patient Portal'
        })
      });
      const data = await res.json();
      if (res.status === 201) {
        setFeedback({
          type: 'success',
          message: `Appointment Confirmed! Booking Code: ${data.booking_code}`
        });
        if (onBookingConfirmed) onBookingConfirmed(data);
        setTimeout(() => {
          onClose();
          setFeedback(null);
        }, 1600);
      } else {
        setFeedback({
          type: 'error',
          message: data.error || 'Concurrent booking conflict: this slot was just taken by another patient.'
        });
      }
    } catch (err) {
      setFeedback({ type: 'error', message: 'Failed to communicate with server.' });
    } finally {
      setIsSubmitting(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="modal-backdrop" onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className="modal-box" style={{ maxWidth: '820px', width: '95%', padding: '28px' }}>
        <button type="button" className="modal-close-btn" onClick={onClose}>
          <X size={18} />
        </button>

        {/* Modal Header */}
        <div style={{ marginBottom: '22px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '4px' }}>
            <div style={{ background: '#e0f2fe', width: '36px', height: '36px', borderRadius: '10px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
              <Calendar size={20} color="var(--primary)" />
            </div>
            <div>
              <h2 style={{ fontSize: '18px', fontWeight: 800, color: 'var(--text-main)' }}>
                Book Specialist Doctor Appointment
              </h2>
              <p style={{ fontSize: '12px', color: 'var(--text-muted)' }}>
                Select your preferred specialist and choose an available consultation time slot.
              </p>
            </div>
          </div>
        </div>

        {feedback && (
          <div style={{
            padding: '12px 16px',
            borderRadius: '10px',
            marginBottom: '18px',
            fontSize: '13px',
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
            background: feedback.type === 'success' ? '#dcfce7' : '#fee2e2',
            color: feedback.type === 'success' ? '#15803d' : '#b91c1c',
            border: `1px solid ${feedback.type === 'success' ? '#86efac' : '#fca5a5'}`
          }}>
            {feedback.type === 'success' ? <CheckCircle2 size={18} /> : <AlertCircle size={18} />}
            <span>{feedback.message}</span>
          </div>
        )}

        {/* Clean 2-Column Booking Layout */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1.1fr', gap: '24px', alignItems: 'start' }}>
          
          {/* Column 1: Specialist Selection */}
          <div>
            <div style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-main)', marginBottom: '10px', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Award size={15} color="var(--primary)" />
              <span>1. Choose Specialist Doctor</span>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px', maxHeight: '380px', overflowY: 'auto', paddingRight: '4px' }}>
              {doctors.map(doc => {
                const isSelected = selectedDoctorId.toString() === doc.id.toString();
                return (
                  <div
                    key={doc.id}
                    onClick={() => setSelectedDoctorId(doc.id.toString())}
                    style={{
                      background: isSelected ? '#f0f9ff' : '#ffffff',
                      border: `2px solid ${isSelected ? 'var(--primary)' : 'var(--card-border)'}`,
                      borderRadius: '12px',
                      padding: '12px 14px',
                      cursor: 'pointer',
                      transition: 'all 0.15s ease',
                      boxShadow: isSelected ? '0 4px 12px rgba(2, 132, 199, 0.12)' : 'none'
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                      <div style={{ display: 'flex', gap: '10px' }}>
                        <div style={{
                          width: '40px',
                          height: '40px',
                          borderRadius: '10px',
                          background: isSelected ? 'var(--primary)' : '#e2e8f0',
                          color: isSelected ? '#ffffff' : 'var(--text-sub)',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          fontWeight: 800,
                          fontSize: '15px'
                        }}>
                          {doc.name.split(' ').map(n => n[0]).join('').slice(1, 3)}
                        </div>
                        <div>
                          <div style={{ fontSize: '14px', fontWeight: 800, color: 'var(--text-main)' }}>
                            {doc.name}
                          </div>
                          <div style={{ fontSize: '11px', color: 'var(--primary)', fontWeight: 600 }}>
                            {doc.speciality}
                          </div>
                          <div style={{ fontSize: '11px', color: 'var(--text-muted)', marginTop: '2px' }}>
                            ⭐ {doc.rating || '4.9'} · {doc.experience || 10}+ yrs exp
                          </div>
                        </div>
                      </div>
                      <div style={{ textAlign: 'right' }}>
                        <div style={{ fontSize: '13px', fontWeight: 800, color: '#16a34a' }}>
                          {doc.fee ? (doc.fee.startsWith('$') ? doc.fee.replace('$', '₹') : doc.fee) : '₹500'}
                        </div>
                        <div style={{ fontSize: '10px', color: 'var(--text-muted)' }}>consult fee</div>
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Column 2: Date & Slot Selection + Contact */}
          <form onSubmit={handleSubmit} style={{ background: '#f8fafc', padding: '18px', borderRadius: '14px', border: '1px solid var(--card-border)' }}>
            <div style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-main)', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Clock size={15} color="var(--primary)" />
              <span>2. Select Time Slot</span>
            </div>

            {/* Doctor Summary Banner */}
            {selectedDoctor && (
              <div style={{
                background: '#ffffff',
                border: '1px solid var(--card-border)',
                borderRadius: '10px',
                padding: '10px 14px',
                marginBottom: '14px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}>
                <div>
                  <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Selected Specialist:</div>
                  <div style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-main)' }}>{selectedDoctor.name}</div>
                </div>
                <span className="sub-tag" style={{ background: '#e0f2fe', color: 'var(--primary-dark)', borderColor: '#bae6fd' }}>
                  {selectedDoctor.speciality}
                </span>
              </div>
            )}

            {/* Slots Grid */}
            <div style={{ marginBottom: '16px' }}>
              <div style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-sub)', marginBottom: '8px' }}>
                Available Consultation Slots:
              </div>

              {isLoadingSlots ? (
                <div style={{ fontSize: '12px', color: 'var(--text-muted)', textAlign: 'center', padding: '16px' }}>
                  Fetching live appointment slots...
                </div>
              ) : slots.length === 0 ? (
                <div style={{ fontSize: '12px', color: 'var(--accent-red)', padding: '12px', background: '#fef2f2', borderRadius: '8px', textAlign: 'center' }}>
                  No slots configured for this doctor.
                </div>
              ) : (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', maxHeight: '180px', overflowY: 'auto' }}>
                  {slots.map(s => {
                    const isSlotSelected = selectedSlotId.toString() === s.id.toString();
                    return (
                      <button
                        key={s.id}
                        type="button"
                        disabled={!s.available}
                        onClick={() => setSelectedSlotId(s.id.toString())}
                        style={{
                          padding: '9px 10px',
                          borderRadius: '8px',
                          border: `1.5px solid ${!s.available ? '#e2e8f0' : isSlotSelected ? '#16a34a' : 'var(--card-border)'}`,
                          background: !s.available ? '#f1f5f9' : isSlotSelected ? '#dcfce7' : '#ffffff',
                          color: !s.available ? '#94a3b8' : isSlotSelected ? '#15803d' : 'var(--text-main)',
                          fontSize: '11px',
                          fontWeight: isSlotSelected ? 800 : 600,
                          cursor: s.available ? 'pointer' : 'not-allowed',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'space-between',
                          transition: 'all 0.15s ease'
                        }}
                      >
                        <span style={{ fontFamily: 'var(--font-mono)' }}>{s.slot}</span>
                        {!s.available ? (
                          <span style={{ fontSize: '9px', color: '#94a3b8' }}>Booked</span>
                        ) : isSlotSelected ? (
                          <CheckCircle2 size={13} color="#16a34a" />
                        ) : null}
                      </button>
                    );
                  })}
                </div>
              )}
            </div>

            {/* Patient Contact Info */}
            <div className="form-group" style={{ marginBottom: '16px' }}>
              <label style={{ display: 'flex', alignItems: 'center', gap: '5px' }}>
                <Phone size={12} />
                <span>Patient Phone Number</span>
              </label>
              <input
                type="text"
                className="form-control"
                value={phone}
                onChange={(e) => setPhone(e.target.value)}
                placeholder="+91-98200-11000"
                required
              />
            </div>

            {/* Submit Actions */}
            <div style={{ display: 'flex', gap: '10px' }}>
              <button
                type="button"
                className="btn-action"
                onClick={onClose}
                style={{ width: '35%', justifyContent: 'center', background: '#e2e8f0', color: 'var(--text-main)' }}
              >
                Cancel
              </button>
              <button
                type="submit"
                className="btn-submit-main"
                disabled={isSubmitting || !selectedSlotId}
                style={{
                  width: '65%',
                  margin: 0,
                  background: 'linear-gradient(135deg, #16a34a, #15803d)',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '6px'
                }}
              >
                {isSubmitting ? 'Reserving Slot...' : 'Confirm Appointment'}
                <ArrowRight size={15} />
              </button>
            </div>
          </form>

        </div>
      </div>
    </div>
  );
}
