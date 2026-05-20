// quantizer-panel.jsx — Snap direction, strength, output range.

const SNAP_DIRS   = ['Nearest', 'Up', 'Down'];
const SNAP_ICONS  = ['⇅', '↑', '↓'];

export function QuantizerPanel({ state, sendParam, paper = window.PAPER }) {
  const {
    quantDir      = 0,
    quantStrength = 1.0,
    outputLo      = 0,
    outputHi      = 127,
  } = state;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
      {/* Snap direction */}
      <div>
        <Label paper={paper}>Snap</Label>
        <div style={{ display: 'flex', gap: 4, marginTop: 4 }}>
          {SNAP_DIRS.map((d, i) => (
            <button key={i}
              onClick={() => sendParam('quantDir', i)}
              style={chipStyle(i === quantDir, paper)}>
              {SNAP_ICONS[i]} {d}
            </button>
          ))}
        </div>
      </div>

      {/* Snap strength */}
      <SliderRow label="Strength" value={quantStrength} min={0} max={1} step={0.01}
        format={v => Math.round(v * 100) + '%'}
        onChange={v => sendParam('quantStrength', v)} paper={paper} />

      {/* Output range */}
      <div>
        <Label paper={paper}>Output range</Label>
        <div style={{ display: 'flex', gap: 8, marginTop: 4, alignItems: 'center' }}>
          <NoteInput label="Lo" value={outputLo}
            onChange={v => sendParam('outputLo', v)} paper={paper} />
          <span style={{ color: paper?.ink50 || '#6B5E55', fontSize: 12 }}>–</span>
          <NoteInput label="Hi" value={outputHi}
            onChange={v => sendParam('outputHi', v)} paper={paper} />
        </div>
      </div>
    </div>
  );
}

// ── Primitives ────────────────────────────────────────────────────────────────

function Label({ paper, children }) {
  return (
    <div style={{
      fontSize: 10, textTransform: 'uppercase', letterSpacing: '0.07em',
      color: paper?.ink50 || '#6B5E55', fontFamily: 'InterTight, system-ui',
    }}>
      {children}
    </div>
  );
}

function chipStyle(active, paper) {
  return {
    padding: '4px 10px', fontSize: 11, borderRadius: 5, cursor: 'pointer',
    border: `1px solid ${active ? (paper?.ink || '#2D2620') : (paper?.rule || '#D4CAB8')}`,
    background: active ? (paper?.ink || '#2D2620') : (paper?.card || '#FAF8F4'),
    color: active ? (paper?.card || '#FAF8F4') : (paper?.ink || '#2D2620'),
    fontFamily: 'InterTight, system-ui',
    transition: 'background 100ms',
  };
}

function SliderRow({ label, value, min, max, step, format, onChange, paper }) {
  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 3 }}>
        <Label paper={paper}>{label}</Label>
        <span style={{ fontSize: 11, color: paper?.ink70 || '#574E44',
          fontFamily: 'InterTight, system-ui' }}>
          {format ? format(value) : value}
        </span>
      </div>
      <input type="range" min={min} max={max} step={step}
        value={value}
        onChange={e => onChange(parseFloat(e.target.value))}
        style={{ width: '100%', accentColor: paper?.ink || '#2D2620' }} />
    </div>
  );
}

const NOTE_NAMES = ['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'];
function noteName(n) {
  return NOTE_NAMES[n % 12] + Math.floor(n / 12 - 1);
}

function NoteInput({ label, value, onChange, paper }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 2 }}>
      <Label paper={paper}>{label}</Label>
      <div style={{ fontSize: 11, color: paper?.ink || '#2D2620',
        fontFamily: 'Domine, Georgia, serif', fontStyle: 'italic' }}>
        {noteName(value)}
      </div>
      <input type="range" min={0} max={127} step={1} value={value}
        onChange={e => onChange(parseInt(e.target.value))}
        style={{ width: 80, accentColor: paper?.amber || '#C4873A' }} />
    </div>
  );
}
