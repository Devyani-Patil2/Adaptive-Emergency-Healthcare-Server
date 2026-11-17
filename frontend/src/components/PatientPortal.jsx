import React, { useState } from 'react';
import { AlertTriangle, Calendar, ShieldCheck, UserCheck, Activity, Clock, CheckCircle, HeartPulse, User, Phone, Stethoscope, AlertCircle, FileText, CheckCircle2 } from 'lucide-react';
import BookSpecialistModal from './BookSpecialistModal';

export default function PatientPortal({ patientName, doctors, appointments, requests, onRefreshData }) {
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [patientData, setPatientData] = useState({
    name: patientName || 'Rahul Sharma',
    age: 34,
    gender: 'Male',
    phone: '+91-98200-11000',
    emergencyType: 'accident',
    consciousness: 'unresponsive',
    symptoms: 'Severe vehicular collision on highway, heavy impact to chest and head, bleeding from forehead',
    conditions: ['Hypertension']
  });

  const [isSubmitting, setIsSubmitting] = useState(false);

  // Clinical emergency categories (Patient-friendly descriptions)
  const emergencyCategories = [
    {
      id: 'accident',
      label: 'Accident & Severe Trauma',
      icon: '🚨',
      priority: 0,
      resource: 'icu_beds',
      defaultSymptoms: 'Severe highway vehicular crash, severe multi-trauma, head impact',
      description: 'Collisions, falls from height, severe bodily injury'
    },
    {
      id: 'cardiac',
      label: 'Chest Pain / Heart Emergency',
      icon: '🫀',
      priority: 0,
      resource: 'icu_beds',
      defaultSymptoms: 'Crushing central chest pain radiating to left arm, cold sweats, breathlessness',
      description: 'Suspected heart attack, angina, sudden cardiac distress'
    },
    {
      id: 'respiratory',
      label: 'Severe Breathing Trouble',
      icon: '🫁',
      priority: 0,
      resource: 'oxygen_cylinders',
      defaultSymptoms: 'Gasping for air, acute asthma attack, cyanosis around lips, SpO2 very low',
      description: 'Choking, asthma, respiratory failure'
    },
    {
      id: 'bleeding',
      label: 'Heavy Uncontrolled Bleeding',
      icon: '🩸',
      priority: 0,
      resource: 'blood_units',
      defaultSymptoms: 'Arterial or deep laceration bleeding that does not stop with pressure',
      description: 'Deep cuts, hemorrhage, acute blood loss'
    },
    {
      id: 'fracture',
      label: 'Fracture / Acute Bone Injury',
      icon: '🩹',
      priority: 1,
      resource: 'general_beds',
      defaultSymptoms: 'Deformed limb, intense bone pain, inability to bear weight, swelling',
      description: 'Broken bones, joint dislocations, severe sprains'
    },
    {
      id: 'fever',
      label: 'High Fever & Sudden Infection',
      icon: '🤒',
      priority: 2,
      resource: 'general_beds',
      defaultSymptoms: 'Persistent temperature over 103F, severe chills, acute weakness, dehydration',
      description: 'Infection, dengue/malaria suspicion, delirium'
    },
    {
      id: 'general',
      label: 'General Illness / OPD Checkup',
      icon: '🩺',
      priority: 3,
      resource: 'none',
      defaultSymptoms: 'Routine checkup, mild stomach upset, prescription refill, blood pressure review',
      description: 'Mild symptoms, non-urgent consultations'
    }
  ];

  const handleCategorySelect = (cat) => {
    setPatientData(prev => ({
      ...prev,
      emergencyType: cat.id,
      symptoms: cat.defaultSymptoms
    }));
  };

  // Determine priority and inferred resource automatically
  const activeCategory = emergencyCategories.find(c => c.id === patientData.emergencyType) || emergencyCategories[0];
  let calculatedPriority = activeCategory.priority;
  let inferredResource = activeCategory.resource;

  // Unconscious patient is always escalated to P0 Critical!
  if (patientData.consciousness === 'unresponsive') {
    calculatedPriority = 0;
    if (inferredResource === 'none') inferredResource = 'general_beds';
  } else if (patientData.consciousness === 'drowsy' && calculatedPriority > 1) {
    calculatedPriority = 1;
  }

  const getPriorityLabel = (p) => {
    if (p === 0) return 'P0 – Immediate Critical Care (Top Queue Precedence)';
    if (p === 1) return 'P1 – High Priority (Urgent Emergency Care)';
    if (p === 2) return 'P2 – Moderate Priority (Monitored Ward Bed)';
    return 'P3 – Routine Consultation (Outpatient Care)';
  };

  // Live filter for the logged-in patient's real requests
  const normalizedPatientName = (patientName || '').trim().toLowerCase();

  const myRequests = (requests || []).filter(r => 
    (r.patient_name || '').trim().toLowerCase() === normalizedPatientName
  );

  // Active Emergency Requests: ONLY show requests currently in hospital care (not yet discharged or completed)
  const activeEmergencyRequests = myRequests.filter(r => {
    const s = (r.status || '').toUpperCase();
    return s !== 'DISCHARGED' && s !== 'COMPLETED';
  });

  // Completed / Discharged Emergency Requests: permanently preserved for History (NOT deleted!)
  const completedEmergencyRequests = myRequests.filter(r => {
    const s = (r.status || '').toUpperCase();
    return s === 'DISCHARGED' || s === 'COMPLETED';
  });

  // Live filter for the logged-in patient's real appointments
  const myAppointments = (appointments || []).filter(a => 
    (a.patient_name || '').trim().toLowerCase() === normalizedPatientName
  );

  // Active Upcoming Appointments (e.g. Scheduled / Confirmed)
  const activeAppointments = myAppointments.filter(a => {
    const s = (a.status || '').toUpperCase();
    return s !== 'COMPLETED' && s !== 'DISCHARGED' && s !== 'CANCELLED';
  });

  // Completed / Past Consultations for History (NOT deleted!)
  const completedAppointments = myAppointments.filter(a => {
    const s = (a.status || '').toUpperCase();
    return s === 'COMPLETED' || s === 'DISCHARGED' || s === 'CANCELLED';
  });

  // Unified Patient Medical History: ONLY completed, discharged, and past records (Zero active duplication)
  const completedPatientHistory = [
    ...completedEmergencyRequests.map(r => ({
      id: r.request_id,
      type: 'Emergency Triage',
      icon: '🚨',
      title: r.priority_name || `P${r.priority} Emergency Case`,
      priority: r.priority,
      details: r.symptoms || 'Acute Triage Case',
      careUnit: r.required_resource ? r.required_resource.replace('_', ' ').toUpperCase() : 'GENERAL',
      allocCode: r.alloc_code,
      status: r.status || 'DISCHARGED',
      date: r.created_at || 'Recent',
      isEmergency: true
    })),
    ...completedAppointments.map(a => ({
      id: a.booking_code,
      type: 'Specialist Consult',
      icon: '📅',
      title: a.doctor_name || `Doctor #${a.doctor_id}`,
      priority: 3,
      details: `${a.speciality ? a.speciality + ' · ' : ''}${a.slot_time || (a.day_of_week ? a.day_of_week + ': ' + a.start_time : 'Scheduled Slot')}`,
      careUnit: 'OPD CLINIC',
      allocCode: a.booking_code,
      status: a.status || 'COMPLETED',
      date: a.created_at || 'Recent',
      isEmergency: false
    }))
  ];

  // Helper for live doctor care status badge
  const getDoctorStatusInfo = (status) => {
    switch (status) {
      case 'QUEUED':
      case 'IN_TRIAGE':
        return {
          label: '🕒 Queued in Triage (Awaiting Doctor Attendance)',
          bg: '#fef3c7',
          color: '#b45309',
          border: '#fcd34d'
        };
      case 'ATTENDED':
        return {
          label: '🩺 Attended by Physician (Patient Under Examination)',
          bg: '#e0f2fe',
          color: '#0369a1',
          border: '#7dd3fc'
        };
      case 'O2_VENT_DEPLOYED':
        return {
          label: '🫁 High-Flow O2 & Ventilator Deployed (Active Life Support)',
          bg: '#f3e8ff',
          color: '#7e22ce',
          border: '#d8b4fe'
        };
      case 'INTER_HOSPITAL_TRANSFER':
        return {
          label: '🚑 Priority Ambulance Transfer Initiated (Trauma Center)',
          bg: '#e0e7ff',
          color: '#4338ca',
          border: '#a5b4fc'
        };
      case 'STEPDOWN_BED_CONVERTED':
        return {
          label: '🛏️ Stepdown Monitored Bed Converted',
          bg: '#dcfce7',
          color: '#15803d',
          border: '#86efac'
        };
      case 'DISCHARGED':
        return {
          label: '✅ Discharged & Stabilized (Care Completed)',
          bg: '#dcfce7',
          color: '#166534',
          border: '#86efac'
        };
      default:
        return {
          label: `📋 ${status || 'In Progress'}`,
          bg: '#f1f5f9',
          color: '#475569',
          border: '#cbd5e1'
        };
    }
  };

  const handleCaseSubmit = async (e) => {
    e.preventDefault();
    setIsSubmitting(true);
    try {
      const enrichedSymptoms = `${patientData.symptoms} | Alertness: ${patientData.consciousness.toUpperCase()} | Medical History: ${patientData.conditions.join(', ')}`;
      const priorityReason = `Automated Triage: ${activeCategory.label} (Alertness: ${patientData.consciousness})`;

      const res = await fetch('/api/request', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          patient_name: patientData.name,
          age: parseInt(patientData.age),
          gender: patientData.gender,
          phone: patientData.phone,
          symptoms: enrichedSymptoms,
          priority: calculatedPriority,
          required_resource: inferredResource,
          priority_reason: priorityReason
        })
      });
      const data = await res.json();
      if (onRefreshData) onRefreshData();
      alert(`✅ Emergency Request Registered in Hospital System!\n\nPatient: ${data.patient_name} (${data.request_id})\nTriage Category: ${data.priority_label}\nAssigned Unit: ${data.worker_name}\nHospital Care: ${data.resource_status}\n\n👉 Your active request is now live in the Active Emergency Requests section below.`);
    } catch (err) {
      alert('Error registering emergency request.');
    } finally {
      setIsSubmitting(false);
    }
  };

  const handleCancelAppointment = async (bookingCode) => {
    if (!window.confirm(`Cancel active appointment ${bookingCode}? It will be moved to your history records.`)) return;
    try {
      const res = await fetch('/api/appointments/action', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ booking_code: bookingCode, action: 'CANCELLED' })
      });
      if (res.ok && onRefreshData) {
        onRefreshData();
      }
    } catch (err) {
      alert('Error updating appointment.');
    }
  };

  return (
    <div>
      {/* Top Banner with Patient Info & Book Specialist Action */}
      <div style={{
        background: '#e0f2fe',
        border: '1px solid #bae6fd',
        borderLeft: '4px solid var(--primary)',
        borderRadius: '14px',
        padding: '16px 22px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '20px'
      }}>
        <div>
          <h2 style={{ fontSize: '18px', fontWeight: 800, color: 'var(--primary-dark)' }}>
            Patient Emergency Intake & Triage Portal
          </h2>
          <p style={{ fontSize: '12px', color: 'var(--text-sub)' }}>
            Active Profile: <strong>{patientName}</strong> · Real-Time Hospital Telemetry Sync Active
          </p>
        </div>
        <div style={{ display: 'flex', gap: '10px', alignItems: 'center' }}>
          <button 
            type="button" 
            className="btn-submit-main" 
            onClick={() => setIsModalOpen(true)}
            style={{ width: 'auto', margin: 0, padding: '9px 18px', fontSize: '13px', background: 'linear-gradient(135deg, #16a34a, #15803d)', display: 'inline-flex', alignItems: 'center', gap: '6px' }}
          >
            <Calendar size={15} />
            <span>Book Specialist Doctor</span>
          </button>
          <span className="prio-badge prio-p2">Session Active</span>
        </div>
      </div>

      {/* Main Form: Clean Standard Emergency Registration Form */}
      <div className="content-card">
        <div className="card-header-clean">
          <h3>
            <AlertTriangle size={18} color="var(--primary)" />
            <span>Emergency & Acute Triage Registration</span>
          </h3>
          <span className="sub-tag">Intelligent Clinical Triage</span>
        </div>

        <form onSubmit={handleCaseSubmit}>
          
          {/* Row 1: Emergency Type Dropdown & Alertness Dropdown */}
          <div className="form-row-2">
            <div className="form-group">
              <label style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-sub)' }}>
                Emergency / Health Condition
              </label>
              <select
                className="form-control"
                style={{ fontWeight: 700 }}
                value={patientData.emergencyType}
                onChange={(e) => {
                  const cat = emergencyCategories.find(c => c.id === e.target.value);
                  if (cat) handleCategorySelect(cat);
                }}
              >
                {emergencyCategories.map(cat => (
                  <option key={cat.id} value={cat.id}>
                    {cat.icon} {cat.label} ({cat.description})
                  </option>
                ))}
              </select>
            </div>

            <div className="form-group">
              <label style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-sub)' }}>
                Patient Alertness & Consciousness
              </label>
              <select
                className="form-control"
                style={{ fontWeight: 700 }}
                value={patientData.consciousness}
                onChange={(e) => setPatientData({ ...patientData, consciousness: e.target.value })}
              >
                <option value="unresponsive">🔴 Unconscious / Unresponsive (P0 Critical Care)</option>
                <option value="drowsy">🟡 Drowsy / Disoriented / Confused</option>
                <option value="alert">🟢 Conscious & Fully Alert</option>
              </select>
            </div>
          </div>

          {/* Row 2: Patient Demographics */}
          <div className="form-row-2">
            <div className="form-group">
              <label>Patient Full Name</label>
              <input 
                type="text" 
                className="form-control" 
                value={patientData.name} 
                onChange={(e) => setPatientData({ ...patientData, name: e.target.value })}
                required 
              />
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              <div className="form-group">
                <label>Age</label>
                <input 
                  type="number" 
                  className="form-control" 
                  value={patientData.age} 
                  onChange={(e) => setPatientData({ ...patientData, age: e.target.value })}
                  required 
                />
              </div>
              <div className="form-group">
                <label>Gender</label>
                <select 
                  className="form-control"
                  value={patientData.gender}
                  onChange={(e) => setPatientData({ ...patientData, gender: e.target.value })}
                >
                  <option value="Male">Male</option>
                  <option value="Female">Female</option>
                  <option value="Other">Other</option>
                </select>
              </div>
            </div>
          </div>

          {/* Row 3: Contact Phone & Medical History */}
          <div className="form-row-2">
            <div className="form-group">
              <label>Contact Phone Number</label>
              <input 
                type="text" 
                className="form-control" 
                value={patientData.phone} 
                onChange={(e) => setPatientData({ ...patientData, phone: e.target.value })}
                placeholder="+91-98200-11000"
                required 
              />
            </div>
            <div className="form-group">
              <label>Known Medical History / Pre-existing Condition</label>
              <select
                className="form-control"
                value={patientData.conditions[0] || 'None'}
                onChange={(e) => setPatientData({ ...patientData, conditions: [e.target.value] })}
              >
                <option value="None">None / No Prior Major Illness</option>
                <option value="Diabetes">Diabetes Mellitus</option>
                <option value="Hypertension">Hypertension (High Blood Pressure)</option>
                <option value="Asthma / COPD">Asthma / Chronic Respiratory Condition</option>
                <option value="Heart Condition">Cardiac / Heart Disease</option>
                <option value="Blood Thinners">On Blood Thinners / Anticoagulants</option>
              </select>
            </div>
          </div>

          {/* Row 4: Symptoms Details */}
          <div className="form-group" style={{ marginBottom: '20px' }}>
            <label>Reported Medical Symptoms & Clinical Condition Details</label>
            <textarea 
              rows={2}
              className="form-control"
              value={patientData.symptoms}
              onChange={(e) => setPatientData({ ...patientData, symptoms: e.target.value })}
              required
            />
          </div>

          {/* Automated System Acuity Assessment Banner */}
          <div style={{
            background: calculatedPriority === 0 ? '#fff5f5' : calculatedPriority === 1 ? '#fffbeb' : '#f0fdf4',
            border: `1px solid ${calculatedPriority === 0 ? '#fca5a5' : calculatedPriority === 1 ? '#fcd34d' : '#bbf7d0'}`,
            borderRadius: '10px',
            padding: '14px 18px',
            marginBottom: '20px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center'
          }}>
            <div>
              <div style={{ fontSize: '11px', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 700 }}>
                Automated Clinical Triage Assessment
              </div>
              <div style={{ fontSize: '14px', fontWeight: 800, color: calculatedPriority === 0 ? 'var(--accent-red)' : calculatedPriority === 1 ? 'var(--accent-amber)' : 'var(--accent-green)' }}>
                {getPriorityLabel(calculatedPriority)}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Assigned Care Pathway:</div>
              <div style={{ fontSize: '13px', fontWeight: 700, color: 'var(--text-main)' }}>
                {inferredResource === 'icu_beds' ? 'Critical Care ICU Bed' :
                 inferredResource === 'oxygen_cylinders' ? 'High-Flow Oxygen & Resuscitation' :
                 inferredResource === 'blood_units' ? 'Emergency Blood Bank Transfusion' :
                 inferredResource === 'general_beds' ? 'Acute Monitored Ward Bed' : 'Outpatient Consultation'}
              </div>
            </div>
          </div>

          <button 
            type="submit" 
            className="btn-submit-main"
            disabled={isSubmitting}
            style={{
              background: 'linear-gradient(135deg, #2563eb, #1d4ed8)',
              boxShadow: '0 4px 14px rgba(37, 99, 235, 0.3)'
            }}
          >
            {isSubmitting ? 'Registering Acute Case...' : 'Submit Emergency Case to Triage Command'}
          </button>
        </form>
      </div>

      {/* SECTION 1: ONLY ACTIVE EMERGENCY REQUESTS */}
      <div className="content-card" style={{ border: '2px solid #bae6fd' }}>
        <div className="card-header-clean">
          <h3>
            <ShieldCheck size={18} color="var(--primary)" />
            <span>My Active Emergency Requests & Live Care Status</span>
          </h3>
          <span className="sub-tag" style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span style={{ width: '8px', height: '8px', borderRadius: '50%', background: '#16a34a', display: 'inline-block' }}></span>
            {activeEmergencyRequests.length} Active in Care
          </span>
        </div>

        {activeEmergencyRequests.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '32px 20px', color: 'var(--text-muted)' }}>
            <CheckCircle2 size={36} color="#16a34a" style={{ margin: '0 auto 8px', opacity: 0.7 }} />
            <h4 style={{ fontSize: '14px', fontWeight: 700, color: 'var(--text-main)', marginBottom: '4px' }}>
              No Active Emergencies in Progress for {patientName}
            </h4>
            <p style={{ fontSize: '12px', color: 'var(--text-muted)', maxWidth: '460px', margin: '0 auto' }}>
              All submitted emergency cases have been attended and stabilized. See the Complete Patient History table below for past discharged records.
            </p>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            {activeEmergencyRequests.map((req, idx) => {
              const statusInfo = getDoctorStatusInfo(req.status);
              return (
                <div 
                  key={idx} 
                  style={{
                    background: '#ffffff',
                    border: '1px solid var(--card-border)',
                    borderRadius: '12px',
                    padding: '16px 20px',
                    boxShadow: '0 2px 8px rgba(0,0,0,0.03)'
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px', borderBottom: '1px solid #f1f5f9', paddingBottom: '10px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                      <span style={{ fontFamily: 'var(--font-mono)', fontWeight: 800, color: 'var(--primary)', fontSize: '15px' }}>
                        {req.request_id}
                      </span>
                      <span className={`prio-badge prio-p${req.priority}`}>
                        {req.priority_name || `P${req.priority}`}
                      </span>
                      <span style={{ fontSize: '12px', color: 'var(--text-muted)' }}>
                        Patient ID: <strong>{req.patient_id}</strong>
                      </span>
                    </div>

                    {/* Live Doctor Status Badge */}
                    <div style={{
                      padding: '6px 14px',
                      borderRadius: '20px',
                      fontSize: '12px',
                      fontWeight: 800,
                      background: statusInfo.bg,
                      color: statusInfo.color,
                      border: `1px solid ${statusInfo.border}`,
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px'
                    }}>
                      <span>{statusInfo.label}</span>
                    </div>
                  </div>

                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '14px', fontSize: '13px' }}>
                    <div>
                      <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Assigned Care Worker:</div>
                      <div style={{ fontWeight: 700, color: 'var(--text-main)' }}>
                        Worker W{req.worker_id !== undefined ? req.worker_id + 1 : 1}
                      </div>
                    </div>
                    <div>
                      <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Care Resource Pathway:</div>
                      <div style={{ fontWeight: 700, color: '#15803d' }}>
                        {req.required_resource || 'Outpatient Consultation'}
                        {req.alloc_code ? ` (${req.alloc_code})` : ''}
                      </div>
                    </div>
                    <div>
                      <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Registered At:</div>
                      <div style={{ fontFamily: 'var(--font-mono)', fontSize: '12px' }}>
                        {req.created_at || 'Just now'}
                      </div>
                    </div>
                  </div>

                  {req.symptoms && (
                    <div style={{ marginTop: '10px', fontSize: '12px', color: 'var(--text-sub)', background: '#f8fafc', padding: '8px 12px', borderRadius: '8px' }}>
                      <strong>Symptoms:</strong> {req.symptoms}
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* SECTION 2: ONLY ACTIVE / UPCOMING SPECIALIST APPOINTMENTS */}
      <div className="content-card">
        <div className="card-header-clean">
          <h3>
            <Calendar size={18} color="var(--primary)" />
            <span>My Active Scheduled Specialist Consultations</span>
          </h3>
          <span className="sub-tag">
            {activeAppointments.length} Upcoming Bookings
          </span>
        </div>

        {activeAppointments.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '28px 20px', color: 'var(--text-muted)' }}>
            <Calendar size={32} color="#94a3b8" style={{ margin: '0 auto 8px' }} />
            <h4 style={{ fontSize: '14px', fontWeight: 700, color: 'var(--text-main)', marginBottom: '4px' }}>
              No Active Appointments Found for {patientName}
            </h4>
            <p style={{ fontSize: '12px', color: 'var(--text-muted)', maxWidth: '440px', margin: '0 auto' }}>
              You have no upcoming consultations scheduled. Click "Book Specialist Doctor" above to reserve a time slot.
            </p>
          </div>
        ) : (
          <div style={{ overflowX: 'auto' }}>
            <table className="clean-table">
              <thead>
                <tr>
                  <th>Booking Code</th>
                  <th>Patient Name</th>
                  <th>Doctor</th>
                  <th>Time Slot</th>
                  <th>Status</th>
                  <th>Turnaround</th>
                  <th>Action</th>
                </tr>
              </thead>
              <tbody>
                {activeAppointments.map((a, idx) => (
                  <tr key={idx}>
                    <td style={{ fontFamily: 'var(--font-mono)', fontWeight: 700, color: 'var(--primary)' }}>
                      {a.booking_code}
                    </td>
                    <td>{a.patient_name}</td>
                    <td>{a.doctor_name || `Doctor #${a.doctor_id}`} {a.speciality ? `(${a.speciality})` : ''}</td>
                    <td>{a.slot_time || `${a.day_of_week || 'Day'}: ${a.start_time || ''} - ${a.end_time || ''}`}</td>
                    <td>
                      <span className="prio-badge prio-p2">
                        {a.status || 'CONFIRMED'}
                      </span>
                    </td>
                    <td>{a.turnaround_ms ? `${a.turnaround_ms} ms` : 'Immediate'}</td>
                    <td>
                      <button
                        type="button"
                        onClick={() => handleCancelAppointment(a.booking_code)}
                        style={{
                          background: '#fee2e2',
                          color: '#b91c1c',
                          border: '1px solid #fca5a5',
                          borderRadius: '6px',
                          padding: '4px 10px',
                          fontSize: '11px',
                          fontWeight: 700,
                          cursor: 'pointer'
                        }}
                      >
                        Cancel
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* SECTION 3: COMPREHENSIVE PATIENT COMPLETED & DISCHARGED HISTORY */}
      <div className="content-card" style={{ border: '2px solid #e2e8f0' }}>
        <div className="card-header-clean">
          <h3>
            <FileText size={18} color="var(--primary)" />
            <span>Patient Medical History & Completed Consultations</span>
          </h3>
          <span className="sub-tag">
            Completed Records: {completedPatientHistory.length}
          </span>
        </div>

        {completedPatientHistory.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '32px 20px', color: 'var(--text-muted)' }}>
            <FileText size={32} color="#94a3b8" style={{ margin: '0 auto 8px' }} />
            <h4 style={{ fontSize: '14px', fontWeight: 700, color: 'var(--text-main)', marginBottom: '4px' }}>
              No Completed History Records for {patientName}
            </h4>
            <p style={{ fontSize: '12px', color: 'var(--text-muted)' }}>
              Active cases currently undergoing care appear above. When a case is discharged or consultation completed, it is permanently archived here.
            </p>
          </div>
        ) : (
          <div style={{ overflowX: 'auto' }}>
            <table className="clean-table">
              <thead>
                <tr>
                  <th>Service Type</th>
                  <th>Reference ID</th>
                  <th>Clinical Details / Description</th>
                  <th>Allocated Unit / Resource</th>
                  <th>Final Outcome / Status</th>
                  <th>Date Recorded</th>
                </tr>
              </thead>
              <tbody>
                {completedPatientHistory.map((item, idx) => {
                  const statusInfo = getDoctorStatusInfo(item.status);
                  return (
                    <tr key={idx} style={{ background: idx % 2 === 0 ? '#ffffff' : '#f8fafc' }}>
                      <td>
                        <span style={{ display: 'inline-flex', alignItems: 'center', gap: '6px', fontWeight: 700, fontSize: '12px' }}>
                          <span>{item.icon}</span>
                          <span>{item.type}</span>
                        </span>
                      </td>
                      <td style={{ fontFamily: 'var(--font-mono)', fontWeight: 800, color: 'var(--primary)' }}>
                        {item.id}
                      </td>
                      <td style={{ maxWidth: '300px' }}>
                        <div style={{ fontWeight: 700, fontSize: '12px', color: 'var(--text-main)' }}>
                          {item.title}
                        </div>
                        <div style={{ fontSize: '11px', color: 'var(--text-muted)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                          {item.details}
                        </div>
                      </td>
                      <td>
                        <span style={{ fontSize: '12px', fontWeight: 600, color: '#15803d' }}>
                          {item.careUnit}
                        </span>
                      </td>
                      <td>
                        <span style={{
                          padding: '4px 10px',
                          borderRadius: '12px',
                          fontSize: '11px',
                          fontWeight: 700,
                          background: statusInfo.bg,
                          color: statusInfo.color,
                          border: `1px solid ${statusInfo.border}`,
                          display: 'inline-block'
                        }}>
                          {statusInfo.label.split('(')[0].trim()}
                        </span>
                      </td>
                      <td style={{ fontFamily: 'var(--font-mono)', fontSize: '11px', color: 'var(--text-muted)' }}>
                        {item.date}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Book Specialist Modal Component */}
      <BookSpecialistModal 
        isOpen={isModalOpen}
        onClose={() => setIsModalOpen(false)}
        doctors={doctors}
        patientName={patientData.name}
        onBookingConfirmed={() => {
          if (onRefreshData) onRefreshData();
        }}
      />
    </div>
  );
}
