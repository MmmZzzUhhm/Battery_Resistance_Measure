import { NavLink, Route, Routes } from 'react-router-dom';
import Dashboard from './pages/Dashboard.jsx';
import GatewayDetail from './pages/GatewayDetail.jsx';
import BatteryDetail from './pages/BatteryDetail.jsx';
import Firmware from './pages/Firmware.jsx';
import CameraList from './pages/CameraList.jsx';
import CameraDetail from './pages/CameraDetail.jsx';

export default function App() {
  return (
    <div className="app-shell">
      <aside className="sidebar">
        <h1>ニッスイ八王子工場<br />キュービクル監視システム</h1>
        <nav>
          <div className="nav-section-label">鉛蓄電池内部抵抗測定</div>
          <NavLink to="/" end>ダッシュボード</NavLink>
          <NavLink to="/firmware">ファームウェア</NavLink>
          <div className="nav-section-label">薄型カウンタカメラ</div>
          <NavLink to="/cameras">カメラ一覧</NavLink>
        </nav>
      </aside>
      <main className="content">
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/gateways/:gatewayId" element={<GatewayDetail />} />
          <Route path="/gateways/:gatewayId/children/:childId" element={<BatteryDetail />} />
          <Route path="/firmware" element={<Firmware />} />
          <Route path="/cameras" element={<CameraList />} />
          <Route path="/cameras/:cameraId" element={<CameraDetail />} />
        </Routes>
      </main>
    </div>
  );
}
