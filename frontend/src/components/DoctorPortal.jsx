import React, { useState } from 'react';
import { Activity, Stethoscope, AlertCircle, CheckCircle, Calendar, X, HeartPulse, CheckCircle2, Lock, UserCheck } from 'lucide-react';

export default function DoctorPortal({ doctorId, doctors, requests, appointments, resources, onRefreshData }) {
  const [isProcessing, setIsProcessing] = useState(false);
  const [selectedPatient, setSelectedPatient] = useState(null);
  const [filterMode, setFilterMode] = useState('all'); // 'all' or 'my'

  const activeDoc = (doctors || []).find(d => d.id.toString() === (doctorId || '1').toString()) || {
    id: '1',
    name: 'Dr. Ananya Sharma',
    speciality: 'Trauma Care Lead'
  };

  const handleAction = async (requestId, action, resourceType, allocCode) => {
    setIsProcessing(true);
    try {
      const res = await fetch('/api/requests/action', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          request_id: requestId,
          action,
          resource_type: resourceType,
          allocation_code: allocCode
        })
      });
      const data = await res.json();
      if (res.ok) {
        if (selectedPatient && selectedPatient.request_id === requestId) {
          setSelectedPatient(null);
        }
        if (onRefreshData) onRefreshData();
      }
    } catch (err) {
      alert('Error updating patient case.');
    } finally {
      setIsProcessing(false);
    }
  };

  const handleAppointmentAction = async (bookingCode, action) => {
    setIsProcessing(true);
    try {
      const res = await fetch('/api/appointments/action', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ booking_code: bookingCode, action })
      });
      if (res.ok && onRefreshData) {
        onRefreshData();
      }
    } catch (err) {
      alert('Error updating appointment.');
    } finally {
      setIsProcessing(false);
    }
  };

  const handleResetQueue = async () => {
    if (!window.confirm('Reset emergency queue and restore all bed capacities?')) return;
    try {
      await fetch('/api/queue/reset', { method: 'POST' });
      if (onRefreshData) onRefreshData();
    } catch (err) {
      alert('Error resetting queue.');
    }
  };

  const activeRequests = (requests || []).filter(r => r.status !== 'DISCHARGED');
  const dischargedRequests = (requests || []).filter(r => r.status === 'DISCHARGED');
  const criticalP0Count = activeRequests.filter(r => r.priority === 0).length;
  const attendedCount = activeRequests.filter(r => r.status === 'ATTENDED' || r.status === 'O2_VENT_DEPLOYED').length;
  
  // Check if a patient is allotted to the currently logged in doctor
  const isPatientOfActiveDoc = (r) => {
    const docIdNum = parseInt(activeDoc.id, 10);
    // worker_id: 0 -> Doctor 1, 1 -> Doctor 2, 2 -> Doctor 3, 3 -> Doctor 4
    if (r.worker_id !== undefined && r.worker_id >= 0) {
      return (r.worker_id + 1) === docIdNum;
    }
    if (r.attending_doctor) {
      return r.attending_doctor.toLowerCase().includes(activeDoc.name.toLowerCase());
    }
    return false;
  };

  const myActiveRequests = activeRequests.filter(isPatientOfActiveDoc);

  // Filtered requests according to view mode
  const displayedRequests = filterMode === 'my' ? myActiveRequests : activeRequests;

  const activeAppointments = (appointments || []).filter(a => {
    const s = (a.status || '').toUpperCase();
    return s !== 'COMPLETED' && s !== 'DISCHARGED' && s !== 'CANCELLED';
  });

  return (
    <div>
      {/* Top Banner */}
      <div style={{
        background: '#f0fdf4',
        border: '1px solid #bbf7d0',
        borderLeft: '4px solid var(--accent-green)',
        borderRadius: '12px',
        padding: '14px 20px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '20px'
      }}>
        <div>
          <h2 style={{ fontSize: '17px', fontWeight: 800, color: '#166534', margin: 0 }}>
            Emergency Command & Triage Console
          </h2>
          <p style={{ fontSize: '12px', color: '#15803d', margin: '4px 0 0 0' }}>
            Logged In Physician: <strong>{activeDoc.name}</strong> ({activeDoc.speciality}) · Core Unit W{activeDoc.id}
          </p>
        </div>
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <button 
            type="button" 
            className="btn-action" 
            onClick={handleResetQueue}
            style={{ background: '#fee2e2', color: '#b91c1c', border: '1px solid #fca5a5', padding: '6px 12px', fontSize: '12px', fontWeight: 700 }}
          >
            Clear Board & Reset Beds
          </button>
          <span className="prio-badge prio-p2">Online</span>
        </div>
      </div>

      {/* KPI Clinical Overview Bar */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))',
        gap: '14px',
        marginBottom: '20px'
      }}>
        <div style={{
          background: '#ffffff',
          border: '1px solid var(--card-border)',
          borderRadius: '12px',
          padding: '16px',
          boxShadow: '0 2px 6px rgba(0,0,0,0.02)'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
            <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', textTransform: 'uppercase' }}>Active Emergencies</span>
            <AlertCircle size={16} color="var(--primary)" />
          </div>
          <div style={{ fontSize: '24px', fontWeight: 800, color: 'var(--primary)' }}>
            {activeRequests.length}
          </div>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
            Hospital Triage Queue
          </div>
        </div>

        <div style={{
          background: criticalP0Count > 0 ? '#fff5f5' : '#ffffff',
          border: `1px solid ${criticalP0Count > 0 ? '#fca5a5' : 'var(--card-border)'}`,
          borderRadius: '12px',
          padding: '16px',
          boxShadow: '0 2px 6px rgba(0,0,0,0.02)'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
            <span style={{ fontSize: '11px', fontWeight: 700, color: criticalP0Count > 0 ? 'var(--accent-red)' : 'var(--text-muted)', textTransform: 'uppercase' }}>Critical P0 Cases</span>
            <HeartPulse size={16} color="var(--accent-red)" />
          </div>
          <div style={{ fontSize: '24px', fontWeight: 800, color: 'var(--accent-red)' }}>
            {criticalP0Count}
          </div>
          <div style={{ fontSize: '11px', color: criticalP0Count > 0 ? 'var(--accent-red)' : 'var(--text-muted)' }}>
            Immediate OR / Resuscitation Precedence
          </div>
        </div>

        <div style={{
          background: '#ffffff',
          border: '1px solid var(--card-border)',
          borderRadius: '12px',
          padding: '16px',
          boxShadow: '0 2px 6px rgba(0,0,0,0.02)'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
            <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', textTransform: 'uppercase' }}>My Assigned Cases</span>
            <UserCheck size={16} color="#0284c7" />
          </div>
          <div style={{ fontSize: '24px', fontWeight: 800, color: '#0284c7' }}>
            {myActiveRequests.length}
          </div>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
            Under {activeDoc.name}
          </div>
        </div>

        <div style={{
          background: '#ffffff',
          border: '1px solid var(--card-border)',
          borderRadius: '12px',
          padding: '16px',
          boxShadow: '0 2px 6px rgba(0,0,0,0.02)'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
            <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', textTransform: 'uppercase' }}>Discharged & Stabilized</span>
            <CheckCircle2 size={16} color="#16a34a" />
          </div>
          <div style={{ fontSize: '24px', fontWeight: 800, color: '#16a34a' }}>
            {dischargedRequests.length}
          </div>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
            Permanently Preserved in History
          </div>
        </div>
      </div>

      {/* Hospital Resources Inventory Grid */}
      <div className="content-card">
        <div className="card-header-clean">
          <h3>
            <Activity size={18} color="var(--primary)" />
            <span>Critical Hospital Resource Inventory</span>
          </h3>
          <span className="sub-tag">Atomic SQLite Locks</span>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: '14px' }}>
          {resources && Object.keys(resources).length > 0 ? (
            Object.entries(resources).map(([key, val]) => (
              <div key={key} style={{
                background: '#f8fafc',
                border: '1px solid var(--card-border)',
                borderRadius: '10px',
                padding: '14px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '11px', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 700 }}>
                  {key.replace('_', ' ')}
                </div>
                <div style={{ fontSize: '24px', fontWeight: 800, color: val.available === 0 ? 'var(--accent-red)' : 'var(--accent-green)', margin: '6px 0' }}>
                  {val.available} / {val.total}
                </div>
                <div style={{ fontSize: '11px', color: 'var(--text-sub)' }}>
                  {val.in_use} In Use
                </div>
              </div>
            ))
          ) : (
            <div style={{ fontSize: '12px', color: 'var(--text-muted)' }}>Loading inventory telemetry...</div>
          )}
        </div>
      </div>

      {/* Live Priority Triage Board */}
      <div className="content-card">
        <div className="card-header-clean" style={{ flexWrap: 'wrap', gap: '10px' }}>
          <div>
            <h3>
              <AlertCircle size={18} color="var(--accent-red)" />
              <span>Live Acuity Triage Queue (P0 Critical First)</span>
            </h3>
            <span style={{ fontSize: '12px', color: 'var(--text-muted)' }}>
              Anti-Starvation Aging & Parallel Doctor Core Allocation Active
            </span>
          </div>

          {/* Filter Toggle: All Cases vs My Cases */}
          <div style={{ display: 'flex', gap: '6px', background: '#f1f5f9', padding: '4px', borderRadius: '8px' }}>
            <button
              type="button"
              onClick={() => setFilterMode('all')}
              style={{
                border: 'none',
                background: filterMode === 'all' ? '#ffffff' : 'transparent',
                color: filterMode === 'all' ? 'var(--primary)' : 'var(--text-muted)',
                fontWeight: 700,
                fontSize: '12px',
                padding: '6px 12px',
                borderRadius: '6px',
                cursor: 'pointer',
                boxShadow: filterMode === 'all' ? '0 1px 3px rgba(0,0,0,0.1)' : 'none'
              }}
            >
              All Hospital Cases ({activeRequests.length})
            </button>
            <button
              type="button"
              onClick={() => setFilterMode('my')}
              style={{
                border: 'none',
                background: filterMode === 'my' ? '#ffffff' : 'transparent',
                color: filterMode === 'my' ? '#16a34a' : 'var(--text-muted)',
                fontWeight: 700,
                fontSize: '12px',
                padding: '6px 12px',
                borderRadius: '6px',
                cursor: 'pointer',
                boxShadow: filterMode === 'my' ? '0 1px 3px rgba(0,0,0,0.1)' : 'none'
              }}
            >
              My Patients ({myActiveRequests.length})
            </button>
          </div>
        </div>

        {displayedRequests.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '32px', color: 'var(--text-muted)', fontSize: '13px' }}>
            {filterMode === 'my' 
              ? `No active emergency cases currently allotted to ${activeDoc.name}. Switch to "All Hospital Cases" to see other triage beds.`
              : 'No patients in triage queue. All emergency cases attended and stabilized.'}
          </div>
        ) : (
          <div style={{ overflowX: 'auto' }}>
            <table className="clean-table">
              <thead>
                <tr>
                  <th>Request ID</th>
                  <th>Priority</th>
                  <th>Patient Details</th>
                  <th>Symptoms & Vitals</th>
                  <th>Resource Status</th>
                  <th>Allotted Physician / Core</th>
                  <th>Current State</th>
                  <th>Clinical Actions</th>
                </tr>
              </thead>
              <tbody>
                {displayedRequests.map((r, idx) => {
                  const isMyCase = isPatientOfActiveDoc(r);
                  return (
                    <tr key={idx} style={{ background: r.priority === 0 ? '#fff5f5' : isMyCase ? '#f0fdf4' : 'transparent' }}>
                      <td style={{ fontFamily: 'var(--font-mono)', fontWeight: 700, color: 'var(--primary)' }}>
                        {r.request_id}
                      </td>
                      <td>
                        <span className={`prio-badge prio-p${r.priority}`}>
                          {r.priority_label || `P${r.priority}`}
                        </span>
                      </td>
                      <td>
                        <div 
                          style={{ cursor: 'pointer', color: 'var(--text-main)', fontWeight: 700, display: 'flex', alignItems: 'center', gap: '6px' }}
                          onClick={() => setSelectedPatient(r)}
                        >
                          <span>{r.patient_name}</span>
                          {isMyCase && (
                            <span style={{ fontSize: '10px', background: '#dcfce7', color: '#15803d', padding: '1px 5px', borderRadius: '4px', fontWeight: 800 }}>
                              Your Patient
                            </span>
                          )}
                        </div>
                        <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
                          {r.patient_id} · Age {r.age || 34}
                        </div>
                      </td>
                      <td style={{ maxWidth: '240px' }}>
                        <div style={{ fontSize: '12px' }}>{r.symptoms}</div>
                        <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontFamily: 'var(--font-mono)' }}>
                          {r.vitals || 'BP: 120/80, SpO2: 98%'}
                        </div>
                      </td>
                      <td>
                        <div style={{ fontSize: '12px', fontWeight: 600, color: r.resource_status?.includes('SHORTAGE') ? 'var(--accent-red)' : 'var(--accent-green)' }}>
                          {r.required_resource || 'None'}
                        </div>
                        <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
                          {r.allocation_code || r.resource_status || 'Pending'}
                        </div>
                      </td>
                      <td>
                        <div style={{ fontWeight: 800, fontSize: '12px', color: isMyCase ? '#15803d' : 'var(--primary)' }}>
                          {r.attending_doctor || `Physician Core W${r.worker_id !== undefined ? r.worker_id + 1 : 1}`}
                        </div>
                      </td>
                      <td>
                        <span style={{
                          fontSize: '11px',
                          fontWeight: 700,
                          padding: '4px 8px',
                          borderRadius: '4px',
                          background: r.status === 'ATTENDED' ? '#e0f2fe' : r.status === 'O2_VENT_DEPLOYED' ? '#f3e8ff' : '#fef3c7',
                          color: r.status === 'ATTENDED' ? '#0369a1' : r.status === 'O2_VENT_DEPLOYED' ? '#7e22ce' : '#b45309'
                        }}>
                          {r.status === 'QUEUED' ? 'QUEUED in Triage' : r.status}
                        </span>
                      </td>
                      <td>
                        {/* Only the assigned doctor can modify their patient's clinical state */}
                        {isMyCase ? (
                          <div style={{ display: 'flex', gap: '5px', flexWrap: 'wrap' }}>
                            <button 
                              type="button" 
                              className="btn-action" 
                              disabled={isProcessing}
                              onClick={() => handleAction(r.request_id, 'ATTENDED', r.required_resource, r.allocation_code)}
                              style={{ background: '#dcfce7', color: '#15803d', padding: '4px 8px', fontSize: '11px', fontWeight: 700 }}
                            >
                              Attended
                            </button>
                            <button 
                              type="button" 
                              className="btn-action" 
                              disabled={isProcessing}
                              onClick={() => handleAction(r.request_id, 'DEPLOY_O2', 'oxygen_cylinders', r.allocation_code)}
                              style={{ background: '#e0f2fe', color: '#0369a1', padding: '4px 8px', fontSize: '11px', fontWeight: 700 }}
                            >
                              Deploy O2
                            </button>
                            <button 
                              type="button" 
                              className="btn-action" 
                              disabled={isProcessing}
                              onClick={() => handleAction(r.request_id, 'DISCHARGED', r.required_resource, r.allocation_code)}
                              style={{ background: '#fee2e2', color: '#b91c1c', padding: '4px 8px', fontSize: '11px', fontWeight: 700 }}
                            >
                              Discharge
                            </button>
                          </div>
                        ) : (
                          <span style={{
                            fontSize: '11px',
                            fontWeight: 700,
                            color: '#64748b',
                            background: '#f1f5f9',
                            border: '1px solid #cbd5e1',
                            borderRadius: '6px',
                            padding: '4px 8px',
                            display: 'inline-flex',
                            alignItems: 'center',
                            gap: '4px',
                            whiteSpace: 'nowrap'
                          }}>
                            <Lock size={12} />
                            <span>Assigned to {r.attending_doctor ? r.attending_doctor.split('(')[0].trim() : 'Other Doctor'}</span>
                          </span>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Specialist Outpatient Consultations Board */}
      <div className="content-card">
        <div className="card-header-clean">
          <h3>
            <Calendar size={18} color="var(--primary)" />
            <span>Scheduled Outpatient Specialist Consultations</span>
          </h3>
          <span className="sub-tag">
            {activeAppointments.length} Active Appointments
          </span>
        </div>

        {activeAppointments.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '24px', color: 'var(--text-muted)', fontSize: '13px' }}>
            No pending outpatient consultations.
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
                  <th>Clinical Action</th>
                </tr>
              </thead>
              <tbody>
                {activeAppointments.map((a, idx) => (
                  <tr key={idx}>
                    <td style={{ fontFamily: 'var(--font-mono)', fontWeight: 700, color: 'var(--primary)' }}>
                      {a.booking_code}
                    </td>
                    <td>
                      <strong>{a.patient_name}</strong>
                      {a.phone && <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>{a.phone}</div>}
                    </td>
                    <td>{a.doctor || a.doctor_name || `Doctor #${a.doctor_id}`} {a.speciality ? `(${a.speciality})` : ''}</td>
                    <td>{a.time || a.slot_time || `${a.day || a.day_of_week || 'Day'}: ${a.start_time || ''}`}</td>
                    <td>
                      <span className="prio-badge prio-p2">
                        {a.status || 'CONFIRMED'}
                      </span>
                    </td>
                    <td>
                      <div style={{ display: 'flex', gap: '6px' }}>
                        <button
                          type="button"
                          className="btn-action"
                          disabled={isProcessing}
                          onClick={() => handleAppointmentAction(a.booking_code, 'COMPLETED')}
                          style={{ background: '#dcfce7', color: '#15803d', padding: '5px 10px', fontSize: '11px', fontWeight: 700 }}
                        >
                          Mark Completed
                        </button>
                        <button
                          type="button"
                          className="btn-action"
                          disabled={isProcessing}
                          onClick={() => handleAppointmentAction(a.booking_code, 'CANCELLED')}
                          style={{ background: '#fee2e2', color: '#b91c1c', padding: '5px 10px', fontSize: '11px', fontWeight: 700 }}
                        >
                          Cancel
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Patient Clinical Inspection Modal */}
      {selectedPatient && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: 'rgba(15, 23, 42, 0.5)',
          backdropFilter: 'blur(3px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          padding: '20px'
        }}>
          <div style={{
            background: '#ffffff',
            borderRadius: '14px',
            maxWidth: '520px',
            width: '100%',
            boxShadow: '0 20px 40px rgba(0,0,0,0.2)',
            overflow: 'hidden'
          }}>
            <div style={{
              background: '#0284c7',
              color: '#ffffff',
              padding: '14px 18px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <div>
                <h3 style={{ fontSize: '15px', fontWeight: 800, margin: 0 }}>
                  Patient Clinical Details: {selectedPatient.patient_name}
                </h3>
                <span style={{ fontSize: '11px', opacity: 0.9 }}>
                  {selectedPatient.patient_id} · {selectedPatient.request_id}
                </span>
              </div>
              <button 
                type="button" 
                onClick={() => setSelectedPatient(null)}
                style={{ background: 'transparent', border: 'none', color: '#ffffff', cursor: 'pointer' }}
              >
                <X size={18} />
              </button>
            </div>

            <div style={{ padding: '18px' }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '14px' }}>
                <div style={{ background: '#f8fafc', padding: '10px', borderRadius: '8px' }}>
                  <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Allotted Doctor:</div>
                  <div style={{ fontWeight: 800, color: 'var(--primary)', fontSize: '12px' }}>
                    {selectedPatient.attending_doctor || `Worker W${selectedPatient.worker_id !== undefined ? selectedPatient.worker_id + 1 : 1}`}
                  </div>
                </div>
                <div style={{ background: '#f8fafc', padding: '10px', borderRadius: '8px' }}>
                  <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Allocated Resource:</div>
                  <div style={{ fontWeight: 800, color: '#15803d', fontSize: '12px' }}>
                    {selectedPatient.required_resource || 'General Ward'}
                  </div>
                </div>
              </div>

              <div style={{ marginBottom: '14px' }}>
                <div style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', marginBottom: '3px' }}>
                  Symptoms
                </div>
                <div style={{ fontSize: '12px', background: '#f1f5f9', padding: '8px 12px', borderRadius: '6px' }}>
                  {selectedPatient.symptoms}
                </div>
              </div>

              <div style={{ marginBottom: '16px' }}>
                <div style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', marginBottom: '3px' }}>
                  Reported Vitals
                </div>
                <div style={{ fontFamily: 'var(--font-mono)', fontSize: '12px', background: '#f8fafc', border: '1px solid #e2e8f0', padding: '8px 12px', borderRadius: '6px' }}>
                  {selectedPatient.vitals || 'BP: 120/80 mmHg | SpO2: 98% | Pulse: 72 bpm'}
                </div>
              </div>

              <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end', borderTop: '1px solid #e2e8f0', paddingTop: '14px' }}>
                {isPatientOfActiveDoc(selectedPatient) ? (
                  <>
                    <button
                      type="button"
                      className="btn-action"
                      onClick={() => handleAction(selectedPatient.request_id, 'ATTENDED', selectedPatient.required_resource, selectedPatient.allocation_code)}
                      style={{ background: '#dcfce7', color: '#15803d', fontWeight: 700 }}
                    >
                      Mark Attended
                    </button>
                    <button
                      type="button"
                      className="btn-action"
                      onClick={() => handleAction(selectedPatient.request_id, 'DISCHARGED', selectedPatient.required_resource, selectedPatient.allocation_code)}
                      style={{ background: '#fee2e2', color: '#b91c1c', fontWeight: 700 }}
                    >
                      Discharge Patient
                    </button>
                  </>
                ) : (
                  <span style={{ fontSize: '12px', color: '#64748b', fontWeight: 600 }}>
                    🔒 Actions locked: Allotted to {selectedPatient.attending_doctor ? selectedPatient.attending_doctor.split('(')[0].trim() : 'another doctor'}
                  </span>
                )}
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
