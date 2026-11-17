import React, { useState, useEffect } from 'react';
import Header from './components/Header';
import RoleSelect from './components/RoleSelect';
import PatientPortal from './components/PatientPortal';
import DoctorPortal from './components/DoctorPortal';
import AdminConsole from './components/AdminConsole';

export default function App() {
  const [role, setRole] = useState(null); // 'patient', 'doctor', 'admin', or null
  const [patientName, setPatientName] = useState('Rahul Sharma');
  const [doctorId, setDoctorId] = useState('1');
  const [isRefreshing, setIsRefreshing] = useState(false);

  // Live telemetry & data state
  const [doctors, setDoctors] = useState([]);
  const [appointments, setAppointments] = useState([]);
  const [requests, setRequests] = useState([]);
  const [resources, setResources] = useState({});
  const [stats, setStats] = useState({});
  const [threads, setThreads] = useState([]);
  const [events, setEvents] = useState([]);

  // Fetch initial & recurrent data
  const fetchData = async () => {
    setIsRefreshing(true);
    try {
      const [docRes, apptRes, reqRes, resRes, statsRes, threadRes] = await Promise.allSettled([
        fetch('/api/doctors').then(r => r.json()),
        fetch('/api/appointments').then(r => r.json()),
        fetch('/api/requests').then(r => r.json()),
        fetch('/api/resources').then(r => r.json()),
        fetch('/api/stats').then(r => r.json()),
        fetch('/api/threads').then(r => r.json())
      ]);

      if (docRes.status === 'fulfilled' && docRes.value.doctors) {
        setDoctors(docRes.value.doctors);
      }
      if (apptRes.status === 'fulfilled' && apptRes.value.appointments) {
        setAppointments(apptRes.value.appointments);
      }
      if (reqRes.status === 'fulfilled' && reqRes.value.requests) {
        setRequests(reqRes.value.requests);
      }
      if (resRes.status === 'fulfilled') {
        const resData = resRes.value;
        // API returns either an array directly or {resources: [...]}
        const resArray = Array.isArray(resData) ? resData : (resData.resources || []);
        // Convert array to keyed object for DoctorPortal
        const resObj = {};
        resArray.forEach(r => { resObj[r.resource_type] = r; });
        setResources(resObj);
      }
      if (statsRes.status === 'fulfilled') {
        setStats(statsRes.value);
        if (statsRes.value.workers) {
          setThreads(statsRes.value.workers);
        }
      }
      if (threadRes.status === 'fulfilled' && threadRes.value.workers) {
        setThreads(threadRes.value.workers);
      }
    } catch (err) {
      console.error('Error fetching telemetry:', err);
    } finally {
      setTimeout(() => setIsRefreshing(false), 300);
    }
  };

  useEffect(() => {
    fetchData();
    const timer = setInterval(fetchData, 2500);
    return () => clearInterval(timer);
  }, []);

  const handleSelectRole = (selectedRole, meta = {}) => {
    if (meta.patientName) setPatientName(meta.patientName);
    if (meta.doctorId) setDoctorId(meta.doctorId);
    setRole(selectedRole);
  };

  const handleLogout = () => {
    setRole(null);
  };

  return (
    <div>
      <Header 
        role={role}
        patientName={patientName}
        onRefresh={fetchData}
        onLogout={handleLogout}
        isRefreshing={isRefreshing}
      />

      <main className="main-container">
        {!role && (
          <RoleSelect 
            onSelectRole={handleSelectRole}
            doctors={doctors}
          />
        )}

        {role === 'patient' && (
          <PatientPortal 
            patientName={patientName}
            doctors={doctors}
            appointments={appointments}
            requests={requests}
            onRefreshData={fetchData}
          />
        )}

        {role === 'doctor' && (
          <DoctorPortal 
            doctorId={doctorId}
            doctors={doctors}
            requests={requests}
            appointments={appointments}
            resources={resources}
            onRefreshData={fetchData}
          />
        )}

        {role === 'admin' && (
          <AdminConsole 
            stats={stats}
            threads={threads}
            events={events}
            onRefreshData={fetchData}
          />
        )}
      </main>
    </div>
  );
}
