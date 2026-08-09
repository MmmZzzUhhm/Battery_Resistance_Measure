const BASE = '/api/v1';

async function req(path, opts = {}) {
  const res = await fetch(BASE + path, {
    headers: { 'Content-Type': 'application/json' },
    ...opts,
  });
  const text = await res.text();
  const data = text ? JSON.parse(text) : null;
  if (!res.ok) throw new Error(data?.error || `HTTP ${res.status}`);
  return data;
}

export const api = {
  gateways: () => req('/gateways'),
  children: (gatewayId) => req(`/gateways/${encodeURIComponent(gatewayId)}/children`),
  updateChild: (gatewayId, childId, body) =>
    req(`/gateways/${encodeURIComponent(gatewayId)}/children/${encodeURIComponent(childId)}`, {
      method: 'PUT', body: JSON.stringify(body),
    }),
  measurements: (gatewayId, params) => {
    const qs = new URLSearchParams(params).toString();
    return req(`/gateways/${encodeURIComponent(gatewayId)}/measurements?${qs}`);
  },
  firmwareList: () => req('/firmware'),
  firmwareUpload: (version, file) => {
    const form = new FormData();
    form.append('version', version);
    form.append('file', file);
    return fetch(`${BASE}/firmware`, { method: 'POST', body: form }).then((r) => r.json());
  },
  setFirmwareTarget: (gatewayId, childId, version) =>
    req(`/gateways/${encodeURIComponent(gatewayId)}/children/${encodeURIComponent(childId)}/firmware-target`, {
      method: 'PUT', body: JSON.stringify({ version }),
    }),
  clearFirmwareTarget: (gatewayId, childId) =>
    req(`/gateways/${encodeURIComponent(gatewayId)}/children/${encodeURIComponent(childId)}/firmware-target`, {
      method: 'DELETE',
    }),
  cameras: () => req('/cameras'),
  cameraCaptures: (cameraId, params) => {
    const qs = new URLSearchParams(params).toString();
    return req(`/cameras/${encodeURIComponent(cameraId)}/captures?${qs}`);
  },
  cameraCaptureImageUrl: (captureId) => `${BASE}/camera-captures/${encodeURIComponent(captureId)}/image`,
};
