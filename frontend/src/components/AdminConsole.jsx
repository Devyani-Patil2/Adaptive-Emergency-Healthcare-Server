import React, { useState } from 'react';
import { Cpu, Server, Activity, Users, Play, RefreshCw } from 'lucide-react';

export default function AdminConsole({ stats, threads, events, onRefreshData }) {
  const [benchmarkStatus, setBenchmarkStatus] = useState('');
  const [isRunningBench, setIsRunningBench] = useState(false);

  const runSimulation = async (type) => {
    setIsRunningBench(true);
    setBenchmarkStatus(`Triggering ${type} simulation on C server...`);
    try {
      // Simulate by calling /api/request rapidly
      const total = type === 'burst' ? 25 : 8;
      const promises = [];
      for (let i = 0; i < total; i++) {
        promises.push(fetch('/api/request', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            patient_name: `Sim Patient ${i + 1}`,
            age: 20 + i,
            symptoms: type === 'burst' ? 'Sudden mass casualty triage test' : 'Baseline test',
            priority: i % 4,
            required_resource: 'general_beds'
          })
        }));
      }
      await Promise.all(promises);
      setBenchmarkStatus(`Completed ${total} requests. Autoscaling telemetry updated!`);
      if (onRefreshData) onRefreshData();
    } catch (err) {
      setBenchmarkStatus('Simulation error.');
    } finally {
      setIsRunningBench(false);
    }
  };

  return (
    <div>
      <div style={{
        background: '#f1f5f9',
        border: '1px solid #cbd5e1',
        borderLeft: '4px solid var(--text-main)',
        borderRadius: '12px',
        padding: '14px 20px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '20px'
      }}>
        <div>
          <h2 style={{ fontSize: '17px', fontWeight: 800, color: 'var(--text-main)' }}>
            Operating System Concurrency Diagnostics Console
          </h2>
          <p style={{ fontSize: '12px', color: 'var(--text-muted)' }}>
            Dynamic Thread Pool (4-16 Workers), Semaphores & Priority Scheduling
          </p>
        </div>
        <button 
          type="button" 
          className="btn-action btn-refresh" 
          onClick={onRefreshData}
        >
          <RefreshCw size={13} />
          <span>Refresh Telemetry</span>
        </button>
      </div>

      {/* OS Engine Metrics Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '16px', marginBottom: '20px' }}>
        <div className="content-card" style={{ marginBottom: 0 }}>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 700, textTransform: 'uppercase' }}>
            Worker Threads
          </div>
          <div style={{ fontSize: '28px', fontWeight: 800, color: 'var(--primary)', margin: '4px 0' }}>
            {stats.current_threads || 4}
          </div>
          <div style={{ fontSize: '11px', color: 'var(--text-sub)' }}>
            Min: {stats.min_threads || 4} · Max: {stats.max_threads || 16}
          </div>
        </div>

        <div className="content-card" style={{ marginBottom: 0 }}>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 700, textTransform: 'uppercase' }}>
            Active / Idle
          </div>
          <div style={{ fontSize: '28px', fontWeight: 800, color: stats.busy_threads > 0 ? 'var(--accent-amber)' : 'var(--accent-green)', margin: '4px 0' }}>
            {stats.busy_threads || 0} / {stats.idle_threads || stats.current_threads || 4}
          </div>
          <div style={{ fontSize: '11px', color: 'var(--text-sub)' }}>
            Busy vs Standby
          </div>
        </div>

        <div className="content-card" style={{ marginBottom: 0 }}>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 700, textTransform: 'uppercase' }}>
            Counting Semaphores
          </div>
          <div style={{ fontSize: '28px', fontWeight: 800, color: 'var(--accent-indigo)', margin: '4px 0', fontFamily: 'var(--font-mono)' }}>
            {stats.sem_empty !== undefined ? stats.sem_empty : 64} / {stats.sem_full !== undefined ? stats.sem_full : 0}
          </div>
          <div style={{ fontSize: '11px', color: 'var(--text-sub)' }}>
            sem_empty / sem_full
          </div>
        </div>

        <div className="content-card" style={{ marginBottom: 0 }}>
          <div style={{ fontSize: '11px', color: 'var(--text-muted)', fontWeight: 700, textTransform: 'uppercase' }}>
            Latency (P50 / P95)
          </div>
          <div style={{ fontSize: '24px', fontWeight: 800, color: 'var(--text-main)', margin: '4px 0', fontFamily: 'var(--font-mono)' }}>
            {stats.p50_latency_ms || 0} / {stats.p95_latency_ms || 0} ms
          </div>
          <div style={{ fontSize: '11px', color: 'var(--text-sub)' }}>
            Avg: {stats.avg_time || 0} ms
          </div>
        </div>
      </div>

      {/* Simulator Triggers */}
      <div className="content-card">
        <div className="card-header-clean">
          <h3>
            <Play size={18} color="var(--primary)" />
            <span>Interactive Concurrency Surge Simulator</span>
          </h3>
          <span className="sub-tag">Test Dynamic Thread Spawning</span>
        </div>

        <p style={{ fontSize: '13px', color: 'var(--text-sub)', marginBottom: '14px' }}>
          Dispatch concurrent requests to inspect worker thread scale-up and bounded queue semaphore behavior.
        </p>

        <div style={{ display: 'flex', gap: '12px', flexWrap: 'wrap', alignItems: 'center' }}>
          <button 
            type="button" 
            className="btn-action" 
            onClick={() => runSimulation('baseline')}
            disabled={isRunningBench}
            style={{ background: 'var(--primary-light)', color: 'var(--primary-dark)', padding: '9px 16px', fontSize: '13px' }}
          >
            Run Baseline Load (8 Requests)
          </button>
          <button 
            type="button" 
            className="btn-action" 
            onClick={() => runSimulation('burst')}
            disabled={isRunningBench}
            style={{ background: '#fee2e2', color: '#b91c1c', padding: '9px 16px', fontSize: '13px' }}
          >
            Trigger Outbreak Burst (25 Concurrent)
          </button>
          {benchmarkStatus && (
            <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--accent-green)' }}>
              {benchmarkStatus}
            </span>
          )}
        </div>
      </div>

      {/* Worker Threads Lifecycle Inspector */}
      <div className="content-card">
        <div className="card-header-clean">
          <h3>
            <Users size={18} color="var(--primary)" />
            <span>Worker Threads State Map</span>
          </h3>
          <span className="sub-tag">Scale-Up: {stats.scale_up || 0} · Scale-Down: {stats.scale_down || 0}</span>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(130px, 1fr))', gap: '10px' }}>
          {threads && threads.length > 0 ? (
            threads.map((w, idx) => (
              <div key={idx} style={{
                background: !w.alive ? '#f8fafc' : w.busy ? '#fef3c7' : '#dcfce7',
                border: `1px solid ${!w.alive ? '#e2e8f0' : w.busy ? '#fcd34d' : '#86efac'}`,
                borderRadius: '8px',
                padding: '10px',
                textAlign: 'center'
              }}>
                <div style={{ fontSize: '12px', fontWeight: 800 }}>Worker W{w.id + 1}</div>
                <div style={{ fontSize: '11px', color: 'var(--text-sub)' }}>
                  {!w.alive ? 'Terminated' : w.busy ? 'Working' : 'Idle Standby'}
                </div>
                <span style={{ fontSize: '9px', fontWeight: 700, color: w.baseline ? 'var(--primary-dark)' : 'var(--accent-purple)' }}>
                  {w.baseline ? 'Baseline' : 'Burst Dynamic'}
                </span>
              </div>
            ))
          ) : (
            <div style={{ fontSize: '12px', color: 'var(--text-muted)' }}>Workers telemetry updating...</div>
          )}
        </div>
      </div>
    </div>
  );
}
