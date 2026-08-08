import { NavLink, Route, Routes } from 'react-router-dom';
import Dashboard from './pages/Dashboard.jsx';
import GatewayDetail from './pages/GatewayDetail.jsx';
import BatteryDetail from './pages/BatteryDetail.jsx';
import Firmware from './pages/Firmware.jsx';

export default function App() {
  return (
    <div className="app-shell">
      <aside className="sidebar">
        <h1>蓄電池監視ポータル</h1>
        <nav>
          <NavLink to="/" end>ダッシュボード</NavLink>
          <NavLink to="/firmware">ファームウェア</NavLink>
        </nav>
      </aside>
      <main className="content">
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/gateways/:gatewayId" element={<GatewayDetail />} />
          <Route path="/gateways/:gatewayId/children/:childId" element={<BatteryDetail />} />
          <Route path="/firmware" element={<Firmware />} />
        </Routes>
      </main>
    </div>
  );
}
