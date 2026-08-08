import { useMemo, useRef, useState } from 'react';

// 汎用の軽量ラインチャート (単一Y軸、複数系列)。
// 依存ライブラリを増やさないための自前SVG実装。dataviz方針:
// 2pxの線、控えめなグリッド、ホバー時のクロスヘア+ツールチップ、系列ごとの凡例。
export default function LineChart({ series, yLabel, height = 260, formatY = (v) => v.toFixed(2) }) {
  const ref = useRef(null);
  const [hoverX, setHoverX] = useState(null);
  const width = 720;
  const padding = { top: 12, right: 16, bottom: 28, left: 56 };
  const plotW = width - padding.left - padding.right;
  const plotH = height - padding.top - padding.bottom;

  const allPoints = series.flatMap((s) => s.points);
  const xs = allPoints.map((p) => p.x);
  const ys = allPoints.map((p) => p.y);
  const xMin = Math.min(...xs), xMax = Math.max(...xs);
  const yMinRaw = Math.min(...ys), yMaxRaw = Math.max(...ys);
  const yPad = (yMaxRaw - yMinRaw) * 0.1 || 1;
  const yMin = yMinRaw - yPad, yMax = yMaxRaw + yPad;

  const xScale = (x) => padding.left + ((x - xMin) / (xMax - xMin || 1)) * plotW;
  const yScale = (y) => padding.top + plotH - ((y - yMin) / (yMax - yMin || 1)) * plotH;

  const yTicks = useMemo(() => {
    const n = 4;
    return Array.from({ length: n + 1 }, (_, i) => yMin + ((yMax - yMin) * i) / n);
  }, [yMin, yMax]);

  function handleMove(e) {
    const rect = ref.current.getBoundingClientRect();
    const px = ((e.clientX - rect.left) / rect.width) * width;
    const x = xMin + ((px - padding.left) / plotW) * (xMax - xMin);
    setHoverX(x);
  }

  const hoverEntries = hoverX == null ? [] : series.map((s) => {
    let nearest = s.points[0];
    for (const p of s.points) {
      if (Math.abs(p.x - hoverX) < Math.abs(nearest.x - hoverX)) nearest = p;
    }
    return { ...s, point: nearest };
  });

  return (
    <div>
      <svg
        ref={ref}
        viewBox={`0 0 ${width} ${height}`}
        style={{ width: '100%', height: 'auto', display: 'block' }}
        onMouseMove={handleMove}
        onMouseLeave={() => setHoverX(null)}
      >
        {yTicks.map((t, i) => (
          <g key={i}>
            <line
              x1={padding.left} x2={width - padding.right}
              y1={yScale(t)} y2={yScale(t)}
              stroke="var(--gridline)" strokeWidth="1"
            />
            <text x={padding.left - 8} y={yScale(t)} dy="0.32em" textAnchor="end"
              fontSize="11" fill="var(--text-muted)">{formatY(t)}</text>
          </g>
        ))}
        <line
          x1={padding.left} x2={padding.left} y1={padding.top} y2={padding.top + plotH}
          stroke="var(--baseline)" strokeWidth="1"
        />
        <line
          x1={padding.left} x2={width - padding.right} y1={padding.top + plotH} y2={padding.top + plotH}
          stroke="var(--baseline)" strokeWidth="1"
        />

        {series.map((s) => (
          <path
            key={s.id}
            d={s.points.map((p, i) => `${i === 0 ? 'M' : 'L'}${xScale(p.x)},${yScale(p.y)}`).join(' ')}
            fill="none"
            stroke={s.color}
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          />
        ))}

        {hoverX != null && (
          <line
            x1={xScale(hoverX)} x2={xScale(hoverX)} y1={padding.top} y2={padding.top + plotH}
            stroke="var(--text-muted)" strokeWidth="1" strokeDasharray="3,3"
          />
        )}
        {hoverEntries.map((e) => (
          <circle key={e.id} cx={xScale(e.point.x)} cy={yScale(e.point.y)} r="4"
            fill="var(--surface-1)" stroke={e.color} strokeWidth="2" />
        ))}

        <text x={12} y={14} fontSize="11" fill="var(--text-muted)">{yLabel}</text>
      </svg>

      {hoverX != null && (
        <div style={{
          fontSize: '.8rem', color: 'var(--text-secondary)', marginTop: 4,
          display: 'flex', gap: 16, flexWrap: 'wrap',
        }}>
          <span>{new Date(hoverX * 1000).toLocaleString()}</span>
          {hoverEntries.map((e) => (
            <span key={e.id}>
              <span className="swatch" style={{ background: e.color }} /> {e.label}: {formatY(e.point.y)}
            </span>
          ))}
        </div>
      )}

      {series.length > 1 && (
        <div className="legend">
          {series.map((s) => (
            <span key={s.id}><span className="swatch" style={{ background: s.color }} />{s.label}</span>
          ))}
        </div>
      )}
    </div>
  );
}
