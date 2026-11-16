import React from 'react';
import { RefreshCw, LogOut, User, Stethoscope, Cpu } from 'lucide-react';

export default function Header({ role, patientName, onRefresh, onLogout, isRefreshing }) {
  const getRoleIcon = () => {
    if (role === 'patient') return <User size={14} />;
    if (role === 'doctor') return <Stethoscope size={14} />;
    if (role === 'admin') return <Cpu size={14} />;
    return null;
  };

  const getRoleLabel = () => {
    if (role === 'patient') return patientName ? `Patient: ${patientName}` : 'Patient Portal';
    if (role === 'doctor') return 'Physician Command Center';
    if (role === 'admin') return 'OS Concurrency Diagnostics';
    return '';
  };

  return (
    <header className="app-header">
      <div className="header-brand" onClick={onLogout}>
        <div className="header-logo">+</div>
        <div className="header-title">
          <h1>HealthOS Emergency Server</h1>
          <p>Adaptive Dynamic Thread Pooling & Priority Healthcare Server</p>
        </div>
      </div>

      <div className="header-right">
        {role && (
          <div className="user-role-badge">
            {getRoleIcon()}
            <span>{getRoleLabel()}</span>
          </div>
        )}

        {role && (
          <button 
            className="btn-action btn-refresh" 
            onClick={onRefresh}
            title="Polls C engine immediately for live thread allocations, queue depth, and resource tallies"
          >
            <RefreshCw size={13} className={isRefreshing ? 'spin-icon' : ''} />
            <span>Refresh Telemetry</span>
          </button>
        )}

        {role && (
          <button 
            className="btn-action btn-logout" 
            onClick={onLogout}
            title="Log out and return to portal selection"
          >
            <LogOut size={13} />
            <span>Log Out</span>
          </button>
        )}
      </div>
    </header>
  );
}
