# PitchFold — UI Design Considerations

## Core principle: collapsible sections in one display

Rather than a tab-per-feature model, the long-term goal is a single scrollable (or height-adaptive) view where each section can be collapsed to a header bar. This lets performers configure their workspace density on the fly — expand what they need, collapse the rest.

### Proposed sections (all collapsible)

| Section | Default state | Contents |
|---------|---------------|----------|
| **Scale bank** | Expanded | ScaleBank tabs: Common / families / Explorer |
| **Visualize** | Expanded | Chromatic wheel + Neutral lattice (or PickPCS ring when Explorer is active) |
| **Root** | Expanded | 12-button root picker |
| **Quantizer** | Expanded | Snap direction chips + Output range slider |
| **Tools** | Collapsed | All / None / Invert, direct mask input |

When the Explorer (PickPCS) tab is active inside Scale bank, the Visualize section auto-collapses — the ring IS the visualization.

---

## Open design questions

### Layout / space

- **Fixed height vs. scrollable**: AUv3 windows have fixed dimensions. Should we compress everything to fit without scrolling, or allow vertical scroll within the plugin view?  
  - Candidate: fixed height ≤ 600 px, each collapsed section = 28 px header. 5 sections × 28 = 140 px overhead; ~460 px for expanded content.
- **iPad / mobile single-column**: all sections stack vertically. Desktop / wide: Scale bank + Visualize side-by-side, Quantizer to the right.

### Explorer / visualizations coexistence

- When the PickPCS Explorer tab is open, should Wheel + Lattice be hidden entirely (Explorer is a superset), or shown as a compact summary strip?  
  - Inclination: hide — avoid redundancy, give Explorer more vertical space.

### Quantizer placement

- Currently lives in a separate column beside the Scale tab content.  
  - Option A: merge into the Scale collapsible view (fewer top-level tabs).  
  - Option B: keep as its own collapsible section at the bottom, always accessible regardless of which scale view is active.  
  - Inclination: Option B — snap and range affect *all* modes equally, not just the scale-selection step.

### Root picker

- Appears in both the Scale section (for setting the tonal centre) and implicitly in the PickPCS ring (clicking segments changes root).  
  - Consider: clicking a root in the ring should sync the standalone root picker, and vice versa.

### Performance density modes

- A "Performance" toggle could collapse everything except Scale bank + Root, maximising the bank buttons' tap targets.  
- A "Full" toggle restores all sections.

---

## Completed / in progress

- [x] ScaleBank: Common tab + family tabs unified
- [x] Explorer tab in ScaleBank (PickPCS concentric-ring browser)
- [x] PickPCS top-level tab (dedicated ring view, larger canvas)
- [ ] Collapsible section infrastructure
- [ ] Quantizer migration into Scale view
- [ ] Root picker sync with Explorer ring
- [ ] Performance density mode
- [ ] Dark mode implementation
