import React, { useState } from 'react';
import { User, Stethoscope, Cpu, ArrowRight } from 'lucide-react';

export default function RoleSelect({ onSelectRole, doctors }) {
  const [patientName, setPatientName] = useState('Rahul Sharma');
  const [selectedDoctorId, setSelectedDoctorId] = useState('1');

  return (
    <div style={{ maxWidth: '900px', margin: '40px auto' }}>
      <div style={{ textAlign: 'center', marginBottom: '32px' }}>
        <h2 style={{ fontSize: '26px', fontWeight: 800, color: 'var(--text-main)', marginBottom: '8px' }}>
          Select Clinical Portal
        </h2>
        <p style={{ color: 'var(--text-muted)', fontSize: '14px' }}>
          Access patient emergency care intake or the physician triage command center.
        </p>
      </div>

      <div className="role-grid">
        {/* Patient Portal Card */}
        <div className="role-card" onClick={() => onSelectRole('patient', { patientName })}>
          <div className="role-icon-box" style={{ background: '#e0f2fe', color: '#0369a1' }}>
            <User size={24} />
          </div>
          <h3 style={{ fontSize: '18px', fontWeight: 800, marginBottom: '6px' }}>Patient Portal</h3>
          <p style={{ fontSize: '13px', color: 'var(--text-sub)', marginBottom: '16px' }}>
            Register acute emergency cases, report vital symptoms, book doctor consultations, and receive treatment passes.
          </p>

          <div className="form-group" onClick={(e) => e.stopPropagation()}>
            <label>Patient Full Name</label>
            <input 
              type="text" 
              className="form-control" 
              value={patientName} 
              onChange={(e) => setPatientName(e.target.value)} 
            />
          </div>

          <button 
            type="button" 
            className="btn-submit-main" 
            style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px' }}
            onClick={() => onSelectRole('patient', { patientName })}
          >
            <span>Enter Patient Portal</span>
            <ArrowRight size={16} />
          </button>
        </div>

        {/* Doctor Portal Card */}
        <div className="role-card" onClick={() => onSelectRole('doctor', { doctorId: selectedDoctorId })}>
          <div className="role-icon-box" style={{ background: '#f0fdf4', color: '#15803d' }}>
            <Stethoscope size={24} />
          </div>
          <h3 style={{ fontSize: '18px', fontWeight: 800, marginBottom: '6px' }}>Doctor & Emergency Center</h3>
          <p style={{ fontSize: '13px', color: 'var(--text-sub)', marginBottom: '16px' }}>
            Monitor prioritized triage queues (P0 Critical first), allocate ICU beds, deploy oxygen, and record clinical decisions.
          </p>

          <div className="form-group" onClick={(e) => e.stopPropagation()}>
            <label>Attending Physician</label>
            <select 
              className="form-control" 
              value={selectedDoctorId} 
              onChange={(e) => setSelectedDoctorId(e.target.value)}
              style={{ fontWeight: 700 }}
            >
              {doctors.length > 0 ? (
                doctors.map(d => (
                  <option key={d.id} value={d.id}>{d.name} ({d.speciality})</option>
                ))
              ) : (
                <>
                  <option value="1">Dr. Ananya Sharma (Trauma Lead)</option>
                  <option value="2">Dr. Rajesh Kulkarni (Surgery & ICU)</option>
                  <option value="3">Dr. Priya Iyer (Gastroenterology)</option>
                  <option value="4">Dr. Arvind Menon (Cardiology Lead)</option>
                </>
              )}
            </select>
          </div>

          <button 
            type="button" 
            className="btn-submit-main" 
            style={{ background: 'linear-gradient(135deg, #16a34a, #15803d)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px' }}
            onClick={() => onSelectRole('doctor', { doctorId: selectedDoctorId })}
          >
            <span>Enter Doctor Command Center</span>
            <ArrowRight size={16} />
          </button>
        </div>
      </div>

      <div style={{ textAlign: 'center', marginTop: '28px' }}>
        <button 
          onClick={() => onSelectRole('admin', {})}
          style={{ background: 'none', border: 'none', color: 'var(--text-muted)', fontSize: '12px', textDecoration: 'underline', cursor: 'pointer', display: 'inline-flex', alignItems: 'center', gap: '4px' }}
        >
          <Cpu size={14} /> Open Advanced OS Concurrency Diagnostics Console
        </button>
      </div>
    </div>
  );
}
